/**
 * @file ae_deesser.c
 * @brief De-esser implementation (sibilance reduction)
 */

#include "ae_internal.h"

/**
 * De-esser state structure
 */
typedef struct {
  float hp_state;       /* High-pass filter state */
  float envelope;       /* Envelope follower state */
  float gain_reduction; /* Current gain reduction */
  float threshold_db;   /* Detection threshold */
  float ratio;          /* Reduction ratio */
  float attack_ms;      /* Attack time */
  float release_ms;     /* Release time */
  float freq_low_hz;    /* Lower frequency bound */
  float freq_high_hz;   /* Upper frequency bound */
  bool wideband;        /* Wideband mode vs split-band */
} ae_deesser_state_t;

/* Default de-esser parameters */
static ae_deesser_state_t g_deesser_defaults = {.hp_state = 0.0f,
                                                .envelope = 0.0f,
                                                .gain_reduction = 0.0f,
                                                .threshold_db = -20.0f,
                                                .ratio = 4.0f,
                                                .attack_ms = 0.5f,
                                                .release_ms = 20.0f,
                                                .freq_low_hz = 4000.0f,
                                                .freq_high_hz = 10000.0f,
                                                .wideband = false};

/**
 * Initialize de-esser state
 */
void ae_deesser_init(ae_deesser_state_t *state) {
  if (!state)
    return;
  *state = g_deesser_defaults;
}

/**
 * High-pass filter for sibilance detection
 */
static float deesser_highpass(float sample, float cutoff_hz, float sample_rate,
                              float *state) {
  float rc = 1.0f / (2.0f * (float)M_PI * cutoff_hz);
  float dt = 1.0f / sample_rate;
  float alpha = rc / (rc + dt);

  float output = alpha * (*state + sample);
  *state = output - sample;
  return output;
}

/**
 * Process a single sample through the de-esser
 */
float ae_deesser_process_sample(ae_deesser_state_t *state, float sample,
                                float sample_rate) {
  if (!state)
    return sample;

  /* Detect sibilance using high-passed signal */
  float detection = deesser_highpass(sample, state->freq_low_hz, sample_rate,
                                     &state->hp_state);

  /* Rectify detection signal */
  float detection_abs = fabsf(detection);

  /* Convert to dB */
  float detection_db = 20.0f * log10f(detection_abs + AE_LOG_EPSILON);

  /* Envelope follower */
  float attack_coeff = expf(-1.0f / (state->attack_ms * 0.001f * sample_rate));
  float release_coeff =
      expf(-1.0f / (state->release_ms * 0.001f * sample_rate));

  if (detection_db > state->envelope) {
    state->envelope =
        attack_coeff * state->envelope + (1.0f - attack_coeff) * detection_db;
  } else {
    state->envelope =
        release_coeff * state->envelope + (1.0f - release_coeff) * detection_db;
  }

  /* Calculate gain reduction */
  float gain_db = 0.0f;
  if (state->envelope > state->threshold_db) {
    float overshoot = state->envelope - state->threshold_db;
    gain_db = -overshoot * (1.0f - 1.0f / state->ratio);
  }
  state->gain_reduction = -gain_db;

  float gain_linear = powf(10.0f, gain_db / 20.0f);

  if (state->wideband) {
    /* Wideband: reduce entire signal */
    return sample * gain_linear;
  } else {
    /* Split-band: only reduce high frequencies */
    float low_content = sample - detection;
    return low_content + detection * gain_linear;
  }
}

/**
 * Process a buffer through the de-esser
 */
void ae_deesser_process(ae_deesser_state_t *state, float *samples,
                        size_t frames, float sample_rate) {
  if (!state || !samples || frames == 0)
    return;

  for (size_t i = 0; i < frames; ++i) {
    samples[i] = ae_deesser_process_sample(state, samples[i], sample_rate);
  }
}

/**
 * Stereo-linked de-esser
 */
void ae_deesser_process_stereo(ae_deesser_state_t *state, float *left,
                               float *right, size_t frames, float sample_rate) {
  if (!state || !left || !right || frames == 0)
    return;

  for (size_t i = 0; i < frames; ++i) {
    /* Use max of L/R for detection */
    float mono = fmaxf(fabsf(left[i]), fabsf(right[i]));
    float gain = ae_deesser_process_sample(state, mono, sample_rate) /
                 (mono + AE_LOG_EPSILON);
    left[i] *= gain;
    right[i] *= gain;
  }
}
