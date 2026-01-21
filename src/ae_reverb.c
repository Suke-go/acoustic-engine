#include "ae_internal.h"

static float ae_allpass_process(ae_allpass_t *ap, float input) {
  float buf = ap->buffer[ap->index];
  float output = -input + buf;
  ap->buffer[ap->index] = input + buf * ap->feedback;
  ap->index = (ap->index + 1) % ap->delay;
  return output;
}

static void ae_early_reflections_update(ae_early_reflections_t *early,
                                        float room_size, float sample_rate) {
  static const float base_ms[AE_ER_TAPS] = {7.0f, 11.0f, 17.0f, 23.0f,
                                            29.0f, 37.0f, 45.0f, 53.0f,
                                            61.0f, 73.0f, 89.0f, 101.0f};
  size_t count = AE_ER_TAPS;
  float scale = 0.6f + 0.8f * room_size;
  for (size_t i = 0; i < count; ++i) {
    float ms = base_ms[i] * scale;
    size_t delay = (size_t)(ms * 0.001f * sample_rate);
    if (delay >= early->size)
      delay = early->size - 1;
    early->delay_samples[i] = delay;
    early->gains[i] = 0.6f * powf(0.75f, (float)i);
    float pan = -0.8f + (1.6f * (float)i / (float)(count - 1));
    early->pans[i] = pan;
  }
  early->tap_count = count;
}

static void ae_hadamard_8(float *v) {
  float a0 = v[0] + v[1];
  float a1 = v[0] - v[1];
  float a2 = v[2] + v[3];
  float a3 = v[2] - v[3];
  float a4 = v[4] + v[5];
  float a5 = v[4] - v[5];
  float a6 = v[6] + v[7];
  float a7 = v[6] - v[7];

  float b0 = a0 + a2;
  float b1 = a1 + a3;
  float b2 = a0 - a2;
  float b3 = a1 - a3;
  float b4 = a4 + a6;
  float b5 = a5 + a7;
  float b6 = a4 - a6;
  float b7 = a5 - a7;

  v[0] = b0 + b4;
  v[1] = b1 + b5;
  v[2] = b2 + b6;
  v[3] = b3 + b7;
  v[4] = b0 - b4;
  v[5] = b1 - b5;
  v[6] = b2 - b6;
  v[7] = b3 - b7;
}

void ae_reverb_init(ae_engine_t *engine) {
  if (!engine)
    return;
  struct ae_reverb *reverb = &engine->reverb;
  reverb->sample_rate = (float)engine->config.sample_rate;
  reverb->lfo_phase = 0.0f;

  size_t max_delay = (size_t)(reverb->sample_rate * 0.1f) + 1;
  size_t max_er = (size_t)(reverb->sample_rate * 0.2f) + 1;

  for (size_t i = 0; i < AE_FDN_CHANNELS; ++i) {
    reverb->lines[i].buffer = (float *)calloc(max_delay, sizeof(float));
    reverb->lines[i].size = max_delay;
    reverb->lines[i].delay = max_delay;
    reverb->lines[i].index = 0;
    reverb->lines[i].feedback = 0.7f;
    reverb->lines[i].damping = 0.5f;
    reverb->lines[i].filter_state = 0.0f;
  }

  for (size_t i = 0; i < 2; ++i) {
    reverb->diffusion[i].buffer = (float *)calloc(max_delay, sizeof(float));
    reverb->diffusion[i].size = max_delay;
    reverb->diffusion[i].delay = max_delay;
    reverb->diffusion[i].index = 0;
    reverb->diffusion[i].feedback = 0.5f;
  }

  reverb->pre_delay_size = max_delay;
  reverb->pre_delay_delay = 1;
  reverb->pre_delay = (float *)calloc(reverb->pre_delay_size, sizeof(float));
  reverb->pre_delay_index = 0;

  reverb->early.buffer = (float *)calloc(max_er, sizeof(float));
  reverb->early.size = max_er;
  reverb->early.index = 0;
  reverb->early.tap_count = AE_ER_TAPS;
  ae_early_reflections_update(&reverb->early, 0.5f, reverb->sample_rate);

  ae_reverb_update_params(engine, 0.5f, 3.0f, 0.5f, 0.5f);
  ae_reverb_reset(engine);
}

void ae_reverb_reset(ae_engine_t *engine) {
  if (!engine)
    return;
  struct ae_reverb *reverb = &engine->reverb;
  for (size_t i = 0; i < AE_FDN_CHANNELS; ++i) {
    ae_clear_buffer(reverb->lines[i].buffer, reverb->lines[i].size);
    reverb->lines[i].index = 0;
    reverb->lines[i].filter_state = 0.0f;
  }
  for (size_t i = 0; i < 2; ++i) {
    ae_clear_buffer(reverb->diffusion[i].buffer, reverb->diffusion[i].size);
    reverb->diffusion[i].index = 0;
  }
  ae_clear_buffer(reverb->pre_delay, reverb->pre_delay_size);
  reverb->pre_delay_index = 0;
  ae_clear_buffer(reverb->early.buffer, reverb->early.size);
  reverb->early.index = 0;
}

void ae_reverb_update_params(ae_engine_t *engine, float room_size, float rt60,
                             float diffusion, float damping) {
  if (!engine)
    return;
  struct ae_reverb *reverb = &engine->reverb;
  static const size_t base_delays[AE_FDN_CHANNELS] = {1116, 1188, 1277, 1356,
                                                      1422, 1491, 1557, 1617};
  static const size_t base_diff[2] = {142, 107};

  float scale = 0.7f + 0.8f * room_size;
  float sr_scale = reverb->sample_rate / 44100.0f;

  reverb->room_size = room_size;
  reverb->rt60 = rt60;
  reverb->diffusion_amount = diffusion;
  reverb->damping = damping;

  size_t pre_delay =
      (size_t)(room_size * 0.08f * reverb->sample_rate);
  if (pre_delay < 1)
    pre_delay = 1;
  if (pre_delay >= reverb->pre_delay_size)
    pre_delay = reverb->pre_delay_size - 1;
  reverb->pre_delay_delay = pre_delay;

  for (size_t i = 0; i < AE_FDN_CHANNELS; ++i) {
    size_t delay = (size_t)(base_delays[i] * sr_scale * scale);
    if (delay < 1)
      delay = 1;
    if (delay >= reverb->lines[i].size)
      delay = reverb->lines[i].size - 1;
    reverb->lines[i].delay = delay;
    if (reverb->lines[i].index >= reverb->lines[i].delay)
      reverb->lines[i].index %= reverb->lines[i].delay;
    reverb->lines[i].damping = damping;
    reverb->lines[i].feedback =
        powf(10.0f, (-3.0f * (float)delay) / (rt60 * reverb->sample_rate));
  }

  for (size_t i = 0; i < 2; ++i) {
    size_t delay = (size_t)(base_diff[i] * sr_scale * scale);
    if (delay < 1)
      delay = 1;
    if (delay >= reverb->diffusion[i].size)
      delay = reverb->diffusion[i].size - 1;
    reverb->diffusion[i].delay = delay;
    reverb->diffusion[i].feedback = 0.5f + 0.4f * diffusion;
    if (reverb->diffusion[i].index >= reverb->diffusion[i].delay)
      reverb->diffusion[i].index %= reverb->diffusion[i].delay;
  }

  ae_early_reflections_update(&reverb->early, room_size, reverb->sample_rate);
}

void ae_reverb_process_block(ae_engine_t *engine, const float *input,
                             float *out_l, float *out_r, size_t frames) {
  if (!engine || !input || !out_l || !out_r || frames == 0)
    return;
  struct ae_reverb *reverb = &engine->reverb;
  float modulation = AE_ATOMIC_LOAD(&engine->modulation);

  for (size_t i = 0; i < frames; ++i) {
    size_t read_pos =
        (reverb->pre_delay_index + reverb->pre_delay_size -
         reverb->pre_delay_delay) %
        reverb->pre_delay_size;
    float pre = reverb->pre_delay[read_pos];
    reverb->pre_delay[reverb->pre_delay_index] = input[i];
    reverb->pre_delay_index =
        (reverb->pre_delay_index + 1) % reverb->pre_delay_size;

    float diffused = pre;
    diffused = ae_allpass_process(&reverb->diffusion[0], diffused);
    diffused = ae_allpass_process(&reverb->diffusion[1], diffused);

    float er_l = 0.0f;
    float er_r = 0.0f;
    reverb->early.buffer[reverb->early.index] = diffused;
    for (size_t t = 0; t < reverb->early.tap_count; ++t) {
      size_t tap =
          (reverb->early.index + reverb->early.size -
           reverb->early.delay_samples[t]) %
          reverb->early.size;
      float tap_val = reverb->early.buffer[tap] * reverb->early.gains[t];
      float pan = reverb->early.pans[t];
      float gain_l = 0.5f * (1.0f - pan);
      float gain_r = 0.5f * (1.0f + pan);
      er_l += tap_val * gain_l;
      er_r += tap_val * gain_r;
    }
    reverb->early.index = (reverb->early.index + 1) % reverb->early.size;

    float fdn_out[AE_FDN_CHANNELS];
    for (size_t c = 0; c < AE_FDN_CHANNELS; ++c) {
      ae_fdn_delay_t *line = &reverb->lines[c];
      float sample = line->buffer[line->index];
      line->filter_state =
          sample + (line->filter_state - sample) * line->damping;
      fdn_out[c] = line->filter_state;
    }

    float feedback_vec[AE_FDN_CHANNELS];
    memcpy(feedback_vec, fdn_out, sizeof(feedback_vec));
    ae_hadamard_8(feedback_vec);
    float norm = 1.0f / sqrtf((float)AE_FDN_CHANNELS);

    float mod = 1.0f +
                modulation * 0.01f *
                    sinf(2.0f * (float)M_PI * reverb->lfo_phase);
    reverb->lfo_phase += 0.25f / reverb->sample_rate;
    if (reverb->lfo_phase >= 1.0f)
      reverb->lfo_phase -= 1.0f;

    for (size_t c = 0; c < AE_FDN_CHANNELS; ++c) {
      ae_fdn_delay_t *line = &reverb->lines[c];
      float input_sample = diffused * mod;
      line->buffer[line->index] =
          input_sample + feedback_vec[c] * norm * line->feedback;
      line->index = (line->index + 1) % line->delay;
    }

    float wet_l = 0.25f * (fdn_out[0] + fdn_out[1] + fdn_out[2] + fdn_out[3]);
    float wet_r = 0.25f * (fdn_out[4] + fdn_out[5] + fdn_out[6] + fdn_out[7]);

    out_l[i] = er_l + wet_l;
    out_r[i] = er_r + wet_r;
  }
}

void ae_reverb_cleanup(ae_engine_t *engine) {
  if (!engine)
    return;
  struct ae_reverb *reverb = &engine->reverb;
  for (size_t i = 0; i < AE_FDN_CHANNELS; ++i) {
    free(reverb->lines[i].buffer);
    reverb->lines[i].buffer = NULL;
    reverb->lines[i].size = 0;
  }
  for (size_t i = 0; i < 2; ++i) {
    free(reverb->diffusion[i].buffer);
    reverb->diffusion[i].buffer = NULL;
    reverb->diffusion[i].size = 0;
  }
  free(reverb->pre_delay);
  reverb->pre_delay = NULL;
  reverb->pre_delay_size = 0;
  free(reverb->early.buffer);
  reverb->early.buffer = NULL;
  reverb->early.size = 0;
}
