/**
 * @file ae_dynamics.c
 * @brief Dynamics processing (compressor, limiter, de-esser)
 */

#include "ae_internal.h"

/**
 * Initialize dynamics processor
 */
void ae_dynamics_init(ae_dynamics_t *dyn) {
  if (!dyn)
    return;
  dyn->threshold_db = -20.0f;
  dyn->ratio = 4.0f;
  dyn->attack_ms = 10.0f;
  dyn->release_ms = 100.0f;
  dyn->knee_db = 6.0f;
  dyn->makeup_db = 0.0f;
  dyn->envelope = 0.0f;
  dyn->gain_reduction_db = 0.0f;
}

/**
 * Compute compressor gain for a given input level
 *
 * @param input_db Input level in dB
 * @param threshold_db Threshold in dB
 * @param ratio Compression ratio (e.g., 4.0 = 4:1)
 * @param knee_db Knee width in dB
 * @return Output level in dB
 */
static float compute_compressor_curve(float input_db, float threshold_db,
                                      float ratio, float knee_db) {
  float half_knee = knee_db * 0.5f;
  float knee_start = threshold_db - half_knee;
  float knee_end = threshold_db + half_knee;

  if (input_db <= knee_start) {
    /* Below knee: no compression */
    return input_db;
  } else if (input_db >= knee_end) {
    /* Above knee: full compression */
    return threshold_db + (input_db - threshold_db) / ratio;
  } else {
    /* In knee: smooth transition */
    float knee_factor = (input_db - knee_start) / knee_db;
    float slope = 1.0f + (1.0f / ratio - 1.0f) * knee_factor;
    return knee_start + (input_db - knee_start) * slope;
  }
}

/**
 * Process a single sample through the compressor
 *
 * @param dyn Dynamics processor state
 * @param sample Input sample
 * @param sample_rate Sample rate in Hz
 * @return Output sample
 */
float ae_compressor_process_sample(ae_dynamics_t *dyn, float sample,
                                   float sample_rate) {
  if (!dyn)
    return sample;

  /* Convert to dB */
  float input_abs = fabsf(sample);
  float input_db = (input_abs > 1e-10f) ? 20.0f * log10f(input_abs) : -100.0f;

  /* Envelope follower */
  float target_db = input_db;
  float attack_coeff = expf(-1.0f / (dyn->attack_ms * 0.001f * sample_rate));
  float release_coeff = expf(-1.0f / (dyn->release_ms * 0.001f * sample_rate));

  if (target_db > dyn->envelope) {
    dyn->envelope =
        attack_coeff * dyn->envelope + (1.0f - attack_coeff) * target_db;
  } else {
    dyn->envelope =
        release_coeff * dyn->envelope + (1.0f - release_coeff) * target_db;
  }

  /* Compute gain reduction */
  float output_db = compute_compressor_curve(dyn->envelope, dyn->threshold_db,
                                             dyn->ratio, dyn->knee_db);
  float gain_db = output_db - dyn->envelope + dyn->makeup_db;
  dyn->gain_reduction_db = dyn->envelope - output_db;

  /* Apply gain */
  float gain_linear = powf(10.0f, gain_db / 20.0f);
  return sample * gain_linear;
}

/**
 * Process a buffer through the compressor
 *
 * @param dyn Dynamics processor state
 * @param samples Input/output samples
 * @param frames Number of frames
 * @param sample_rate Sample rate in Hz
 */
void ae_compressor_process(ae_dynamics_t *dyn, float *samples, size_t frames,
                           float sample_rate) {
  if (!dyn || !samples || frames == 0)
    return;

  for (size_t i = 0; i < frames; ++i) {
    samples[i] = ae_compressor_process_sample(dyn, samples[i], sample_rate);
  }
}

/**
 * Stereo-linked compressor processing
 *
 * @param dyn Dynamics processor state
 * @param left Left channel samples
 * @param right Right channel samples
 * @param frames Number of frames
 * @param sample_rate Sample rate in Hz
 */
void ae_compressor_process_stereo(ae_dynamics_t *dyn, float *left, float *right,
                                  size_t frames, float sample_rate) {
  if (!dyn || !left || !right || frames == 0)
    return;

  for (size_t i = 0; i < frames; ++i) {
    /* Use max of L/R for detection */
    float input_abs = fmaxf(fabsf(left[i]), fabsf(right[i]));
    float input_db = (input_abs > 1e-10f) ? 20.0f * log10f(input_abs) : -100.0f;

    /* Envelope follower */
    float attack_coeff = expf(-1.0f / (dyn->attack_ms * 0.001f * sample_rate));
    float release_coeff =
        expf(-1.0f / (dyn->release_ms * 0.001f * sample_rate));

    if (input_db > dyn->envelope) {
      dyn->envelope =
          attack_coeff * dyn->envelope + (1.0f - attack_coeff) * input_db;
    } else {
      dyn->envelope =
          release_coeff * dyn->envelope + (1.0f - release_coeff) * input_db;
    }

    /* Compute gain */
    float output_db = compute_compressor_curve(dyn->envelope, dyn->threshold_db,
                                               dyn->ratio, dyn->knee_db);
    float gain_db = output_db - dyn->envelope + dyn->makeup_db;
    float gain_linear = powf(10.0f, gain_db / 20.0f);

    left[i] *= gain_linear;
    right[i] *= gain_linear;
  }
}

/**
 * Lookahead limiter processing
 *
 * @param samples Input/output samples
 * @param frames Number of frames
 * @param ceiling_db Ceiling level in dB (e.g., -0.3)
 * @param lookahead_samples Lookahead in samples
 * @param release_ms Release time in ms
 * @param sample_rate Sample rate in Hz
 * @param delay_buffer Delay buffer (caller-owned, size = lookahead_samples)
 * @param delay_index Delay buffer write index (caller-owned)
 * @param envelope Envelope state (caller-owned)
 */
void ae_limiter_process(float *samples, size_t frames, float ceiling_db,
                        size_t lookahead_samples, float release_ms,
                        float sample_rate, float *delay_buffer,
                        size_t *delay_index, float *envelope) {
  if (!samples || !delay_buffer || !delay_index || !envelope || frames == 0)
    return;

  float ceiling_linear = powf(10.0f, ceiling_db / 20.0f);
  float release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));

  for (size_t i = 0; i < frames; ++i) {
    /* Read from delay buffer */
    size_t read_idx =
        (*delay_index + lookahead_samples) % (lookahead_samples * 2);
    float delayed_sample = delay_buffer[read_idx];

    /* Write to delay buffer */
    delay_buffer[*delay_index] = samples[i];
    *delay_index = (*delay_index + 1) % (lookahead_samples * 2);

    /* Peak detection with lookahead */
    float peak = fabsf(samples[i]);
    if (peak > *envelope) {
      *envelope = peak;
    } else {
      *envelope *= release_coeff;
    }

    /* Compute gain reduction */
    float gain = 1.0f;
    if (*envelope > ceiling_linear) {
      gain = ceiling_linear / *envelope;
    }

    samples[i] = delayed_sample * gain;
  }
}

/**
 * Simple soft clipper
 *
 * @param sample Input sample
 * @param threshold Clipping threshold (0-1)
 * @return Clipped sample
 */
float ae_soft_clip(float sample, float threshold) {
  if (threshold <= 0.0f)
    return 0.0f;

  float abs_sample = fabsf(sample);
  if (abs_sample <= threshold) {
    return sample;
  }

  /* Soft knee using tanh */
  float sign = sample >= 0.0f ? 1.0f : -1.0f;
  float overshoot = abs_sample - threshold;
  float compressed = threshold + (1.0f - threshold) * tanhf(overshoot);
  return sign * compressed;
}
