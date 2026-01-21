#include "ae_internal.h"

static void ae_dsp_lowpass(float *samples, size_t n, float cutoff,
                           float sample_rate, float *state) {
  if (!samples || n == 0)
    return;
  float x = *state;
  float rc = 1.0f / (2.0f * (float)M_PI * cutoff);
  float dt = 1.0f / sample_rate;
  float alpha = dt / (rc + dt);
  for (size_t i = 0; i < n; ++i) {
    x = x + alpha * (samples[i] - x);
    samples[i] = x;
  }
  *state = x;
}

static void ae_dsp_highpass(float *samples, size_t n, float cutoff,
                            float sample_rate, float *state) {
  if (!samples || n == 0)
    return;
  float lp = *state;
  float rc = 1.0f / (2.0f * (float)M_PI * cutoff);
  float dt = 1.0f / sample_rate;
  float alpha = dt / (rc + dt);
  for (size_t i = 0; i < n; ++i) {
    lp = lp + alpha * (samples[i] - lp);
    samples[i] = samples[i] - lp;
  }
  *state = lp;
}

void ae_dsp_apply_brightness(float *samples, size_t n, float brightness,
                             float sample_rate, float *lp_state,
                             float *hp_state) {
  float bright_norm = ae_clamp(brightness, -1.0f, 1.0f);
  if (bright_norm < 0.0f) {
    float cutoff = 2000.0f + (bright_norm + 1.0f) * 6000.0f;
    ae_dsp_lowpass(samples, n, cutoff, sample_rate, lp_state);
  } else if (bright_norm > 0.0f) {
    float cutoff = 1000.0f + bright_norm * 6000.0f;
    ae_dsp_highpass(samples, n, cutoff, sample_rate, hp_state);
  }
}

void ae_dsp_apply_lofi(float *left, float *right, size_t n, float amount) {
  if (!left || !right || n == 0 || amount <= 0.0f)
    return;
  float clamped = ae_clamp(amount, 0.0f, 1.0f);
  int bits = (int)(16.0f - clamped * 12.0f);
  if (bits < 4)
    bits = 4;
  float step = 1.0f / (float)(1 << bits);
  float noise_amp = clamped * 0.002f;
  for (size_t i = 0; i < n; ++i) {
    float noise = noise_amp * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    float l = left[i] + noise;
    float r = right[i] + noise;
    left[i] = floorf(l / step + 0.5f) * step;
    right[i] = floorf(r / step + 0.5f) * step;
  }
}

void ae_dsp_apply_width(float *left, float *right, size_t n, float width) {
  if (!left || !right || n == 0)
    return;
  float w = ae_clamp(width, 0.0f, 2.0f);
  for (size_t i = 0; i < n; ++i) {
    float mid = 0.5f * (left[i] + right[i]);
    float side = 0.5f * (left[i] - right[i]);
    side *= w;
    left[i] = mid + side;
    right[i] = mid - side;
  }
}

void ae_dsp_apply_precedence(ae_engine_t *engine, float *left, float *right,
                             size_t frames) {
  if (!engine || !left || !right || frames == 0)
    return;
  if (engine->precedence.delay_ms <= 0.0f)
    return;

  float gain = ae_db_to_linear(engine->precedence.level_db);
  float pan = ae_clamp(engine->precedence.pan, -1.0f, 1.0f);
  float pan_l = 0.5f * (1.0f - pan);
  float pan_r = 0.5f * (1.0f + pan);

  size_t delay_samples = (size_t)(engine->precedence.delay_ms * 0.001f *
                                  engine->config.sample_rate);
  if (delay_samples >= engine->precedence_size)
    delay_samples = engine->precedence_size - 1;

  for (size_t i = 0; i < frames; ++i) {
    size_t read_pos =
        (engine->precedence_index + engine->precedence_size - delay_samples) %
        engine->precedence_size;
    float delayed_l = engine->precedence_l[read_pos];
    float delayed_r = engine->precedence_r[read_pos];

    engine->precedence_l[engine->precedence_index] = left[i];
    engine->precedence_r[engine->precedence_index] = right[i];
    engine->precedence_index =
        (engine->precedence_index + 1) % engine->precedence_size;

    left[i] += gain * (delayed_l * pan_l);
    right[i] += gain * (delayed_r * pan_r);
  }
}

void ae_dsp_apply_doppler(const ae_doppler_params_t *doppler, const float *in_l,
                          const float *in_r, float *out_l, float *out_r,
                          size_t frames, float *phase) {
  if (!doppler || !in_l || !in_r || !out_l || !out_r || frames == 0)
    return;
  if (!doppler->enabled) {
    memcpy(out_l, in_l, frames * sizeof(float));
    memcpy(out_r, in_r, frames * sizeof(float));
    return;
  }

  float c = 343.0f;
  float ratio =
      (c + doppler->listener_velocity) / (c - doppler->source_velocity);
  ratio = ae_clamp(ratio, 0.5f, 2.0f);

  float local_phase = phase ? *phase : 0.0f;
  while (local_phase >= (float)frames)
    local_phase -= (float)frames;
  if (local_phase < 0.0f)
    local_phase = 0.0f;
  for (size_t i = 0; i < frames; ++i) {
    size_t idx = (size_t)local_phase;
    float frac = local_phase - (float)idx;
    float l0 = (idx < frames) ? in_l[idx] : 0.0f;
    float l1 = (idx + 1 < frames) ? in_l[idx + 1] : l0;
    float r0 = (idx < frames) ? in_r[idx] : 0.0f;
    float r1 = (idx + 1 < frames) ? in_r[idx + 1] : r0;
    out_l[i] = l0 + (l1 - l0) * frac;
    out_r[i] = r0 + (r1 - r0) * frac;
    local_phase += ratio;
    while (local_phase >= (float)frames)
      local_phase -= (float)frames;
  }
  if (phase)
    *phase = local_phase;
}

float ae_dsp_apply_envelope(ae_engine_t *engine, float sample) {
  if (!engine)
    return sample;
  ae_adsr_t env = engine->envelope;
  float sr = (float)engine->config.sample_rate;
  float attack = env.attack_ms * 0.001f;
  float decay = env.decay_ms * 0.001f;
  float release = env.release_ms * 0.001f;

  switch (engine->env_state) {
  case AE_ENV_IDLE:
    return sample;
  case AE_ENV_ATTACK:
    if (attack <= 0.0f) {
      engine->env_level = 1.0f;
      engine->env_state = AE_ENV_DECAY;
    } else {
      engine->env_level += 1.0f / (attack * sr);
      if (engine->env_level >= 1.0f) {
        engine->env_level = 1.0f;
        engine->env_state = AE_ENV_DECAY;
      }
    }
    break;
  case AE_ENV_DECAY:
    if (decay <= 0.0f) {
      engine->env_level = env.sustain_level;
      engine->env_state = AE_ENV_SUSTAIN;
    } else {
      float delta = (1.0f - env.sustain_level) / (decay * sr);
      engine->env_level -= delta;
      if (engine->env_level <= env.sustain_level) {
        engine->env_level = env.sustain_level;
        engine->env_state = AE_ENV_SUSTAIN;
      }
    }
    break;
  case AE_ENV_SUSTAIN:
    break;
  case AE_ENV_RELEASE:
    if (release <= 0.0f) {
      engine->env_level = 0.0f;
      engine->env_state = AE_ENV_IDLE;
    } else {
      engine->env_level -= env.sustain_level / (release * sr);
      if (engine->env_level <= 0.0f) {
        engine->env_level = 0.0f;
        engine->env_state = AE_ENV_IDLE;
      }
    }
    break;
  }

  return sample * engine->env_level;
}

/**
 * Wow and flutter effect (tape-style pitch wobble)
 *
 * @param samples Input/output samples
 * @param n Number of samples
 * @param depth Modulation depth (0.0-1.0, typical 0.001-0.005 for subtle)
 * @param rate_hz Modulation rate in Hz (typical 0.5-4.0)
 * @param sample_rate Sample rate in Hz
 * @param phase LFO phase state (caller-owned)
 */
void ae_dsp_apply_wow_flutter(float *samples, size_t n, float depth,
                              float rate_hz, float sample_rate, float *phase) {
  if (!samples || n == 0 || depth <= 0.0f || !phase)
    return;

  float lfo_phase = *phase;
  float phase_inc = 2.0f * (float)M_PI * rate_hz / sample_rate;
  float max_delay_samples =
      depth * sample_rate * 0.01f; /* Max 10ms delay variation */

  /* Simple delay buffer for interpolation */
  static float delay_buffer[4800]; /* 100ms at 48kHz */
  static size_t delay_index = 0;
  size_t delay_size = 4800;

  for (size_t i = 0; i < n; ++i) {
    /* LFO (combination of slow wow and faster flutter) */
    float wow = sinf(lfo_phase);
    float flutter =
        0.3f * sinf(lfo_phase * 5.7f);  /* Higher frequency flutter */
    float mod = (wow + flutter) * 0.5f; /* Combined modulation */

    /* Calculate delay time */
    float delay_samples = (1.0f + mod) * max_delay_samples + 1.0f;
    delay_samples = ae_clamp(delay_samples, 1.0f, (float)(delay_size - 2));

    /* Write to delay buffer */
    delay_buffer[delay_index] = samples[i];

    /* Read with interpolation */
    float read_pos = (float)delay_index - delay_samples;
    while (read_pos < 0.0f)
      read_pos += (float)delay_size;
    size_t idx0 = (size_t)read_pos % delay_size;
    size_t idx1 = (idx0 + 1) % delay_size;
    float frac = read_pos - floorf(read_pos);
    samples[i] = delay_buffer[idx0] * (1.0f - frac) + delay_buffer[idx1] * frac;

    delay_index = (delay_index + 1) % delay_size;
    lfo_phase += phase_inc;
    if (lfo_phase >= 2.0f * (float)M_PI)
      lfo_phase -= 2.0f * (float)M_PI;
  }

  *phase = lfo_phase;
}

/**
 * Tape saturation (warm analog-style distortion)
 *
 * @param samples Input/output samples
 * @param n Number of samples
 * @param drive Saturation amount (0.0-1.0)
 */
void ae_dsp_apply_tape_saturation(float *samples, size_t n, float drive) {
  if (!samples || n == 0 || drive <= 0.0f)
    return;

  float d = ae_clamp(drive, 0.0f, 1.0f);

  /* Pre-gain based on drive */
  float pre_gain = 1.0f + d * 3.0f;

  for (size_t i = 0; i < n; ++i) {
    float x = samples[i] * pre_gain;

    /* Soft clipping using tanh (tape-like saturation curve) */
    float saturated = tanhf(x);

    /* Blend between clean and saturated */
    float output = samples[i] * (1.0f - d) + saturated * d;

    /* Apply subtle high-frequency rolloff (tape head loss simulation) */
    samples[i] = output;
  }
}
