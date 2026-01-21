/**
 * @file ae_modfb.c
 * @brief Modulation Filterbank implementation
 *
 * Analyzes slow temporal envelopes (0.5 Hz - 256 Hz).
 * Used for Roughness (~70 Hz) and Fluctuation Strength (~4 Hz) calculation.
 *
 * Based on Dau et al. (1997) modulation filterbank model.
 */

#include "ae_internal.h"

/*============================================================================
 * Modulation Filter Channel
 *============================================================================*/
typedef struct {
  float cf; /* Center frequency (Hz) */
  float q;  /* Q factor (1 for f<10Hz, 2 for f>=10Hz) */
  float bw; /* Bandwidth (Hz) */

  /* 2nd-order bandpass filter coefficients (biquad) */
  float b0, b1, b2;
  float a1, a2;

  /* Filter state */
  float x1, x2; /* Input history */
  float y1, y2; /* Output history */
} ae_modfb_channel_t;

/*============================================================================
 * Modulation Filterbank Structure
 *============================================================================*/
struct ae_modfb {
  ae_modfb_config_t config;
  ae_modfb_channel_t *channels;
  float *center_freqs;
};

/*============================================================================
 * Calculate Bandpass Coefficients
 *
 * Standard biquad bandpass filter design
 *============================================================================*/
static void modfb_calculate_coeffs(ae_modfb_channel_t *ch, float sample_rate) {
  float w0 = 2.0f * (float)M_PI * ch->cf / sample_rate;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * ch->q);

  /* Bandpass filter (constant 0 dB peak gain) */
  float b0 = alpha;
  float b1 = 0.0f;
  float b2 = -alpha;
  float a0 = 1.0f + alpha;
  float a1 = -2.0f * cos_w0;
  float a2 = 1.0f - alpha;

  /* Normalize by a0 */
  ch->b0 = b0 / a0;
  ch->b1 = b1 / a0;
  ch->b2 = b2 / a0;
  ch->a1 = a1 / a0;
  ch->a2 = a2 / a0;
}

/*============================================================================
 * Calculate Center Frequencies (Logarithmic Spacing)
 *
 * Typical channels: [0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256] Hz
 *============================================================================*/
static void modfb_calculate_center_freqs(ae_modfb_t *mfb) {
  if (!mfb || mfb->config.n_channels == 0)
    return;

  float log_f_low = log10f(mfb->config.f_low);
  float log_f_high = log10f(mfb->config.f_high);
  float log_step =
      (log_f_high - log_f_low) / (float)(mfb->config.n_channels - 1);

  for (uint32_t i = 0; i < mfb->config.n_channels; ++i) {
    float log_f = log_f_low + i * log_step;
    float cf = powf(10.0f, log_f);
    mfb->center_freqs[i] = cf;

    ae_modfb_channel_t *ch = &mfb->channels[i];
    ch->cf = cf;

    /* Q factor: 1 for f < 10 Hz, 2 for f >= 10 Hz */
    ch->q = (cf < 10.0f) ? 1.0f : 2.0f;
    ch->bw = cf / ch->q;

    /* Calculate filter coefficients */
    modfb_calculate_coeffs(ch, (float)mfb->config.sample_rate);
  }
}

/*============================================================================
 * Create Modulation Filterbank
 *============================================================================*/
AE_API ae_modfb_t *ae_modfb_create(const ae_modfb_config_t *config) {
  if (!config || config->n_channels == 0 || config->sample_rate == 0)
    return NULL;

  ae_modfb_t *mfb = (ae_modfb_t *)calloc(1, sizeof(ae_modfb_t));
  if (!mfb)
    return NULL;

  mfb->config = *config;

  /* Set defaults if not specified */
  if (mfb->config.f_low <= 0.0f)
    mfb->config.f_low = 0.5f;
  if (mfb->config.f_high <= 0.0f)
    mfb->config.f_high = 256.0f;

  /* Allocate channels */
  mfb->channels = (ae_modfb_channel_t *)calloc(config->n_channels,
                                               sizeof(ae_modfb_channel_t));
  if (!mfb->channels) {
    free(mfb);
    return NULL;
  }

  /* Allocate center frequency array */
  mfb->center_freqs = (float *)calloc(config->n_channels, sizeof(float));
  if (!mfb->center_freqs) {
    free(mfb->channels);
    free(mfb);
    return NULL;
  }

  /* Calculate center frequencies and initialize channels */
  modfb_calculate_center_freqs(mfb);

  return mfb;
}

/*============================================================================
 * Destroy Modulation Filterbank
 *============================================================================*/
AE_API void ae_modfb_destroy(ae_modfb_t *mfb) {
  if (!mfb)
    return;

  free(mfb->center_freqs);
  free(mfb->channels);
  free(mfb);
}

/*============================================================================
 * Process Modulation Filterbank
 *
 * Input: Envelope signal (e.g., IHC output or adaptation loop output)
 * Output: Modulation-filtered signals for each channel
 *
 * This analyzes the temporal modulations in the 0.5-256 Hz range,
 * which are important for:
 * - Fluctuation Strength: ~4 Hz peak
 * - Roughness: ~70 Hz peak
 *============================================================================*/
AE_API ae_result_t ae_modfb_process(ae_modfb_t *mfb, const float *input,
                                    size_t n_samples, float **output) {
  if (!mfb || !input || !output || n_samples == 0)
    return AE_ERROR_INVALID_PARAM;

  for (uint32_t ch = 0; ch < mfb->config.n_channels; ++ch) {
    ae_modfb_channel_t *channel = &mfb->channels[ch];
    float *out = output[ch];

    for (size_t i = 0; i < n_samples; ++i) {
      float sample = input[i];

      /* Biquad bandpass filter (Direct Form I) */
      float y = channel->b0 * sample + channel->b1 * channel->x1 +
                channel->b2 * channel->x2 - channel->a1 * channel->y1 -
                channel->a2 * channel->y2;

      /* Update state */
      channel->x2 = channel->x1;
      channel->x1 = sample;
      channel->y2 = channel->y1;
      channel->y1 = y;

      out[i] = y;
    }
  }

  return AE_OK;
}

/*============================================================================
 * Get Center Frequencies
 *============================================================================*/
AE_API ae_result_t ae_modfb_get_center_frequencies(ae_modfb_t *mfb,
                                                   float *frequencies,
                                                   size_t *n_channels) {
  if (!mfb)
    return AE_ERROR_INVALID_PARAM;

  if (n_channels)
    *n_channels = mfb->config.n_channels;

  if (frequencies) {
    for (uint32_t i = 0; i < mfb->config.n_channels; ++i) {
      frequencies[i] = mfb->center_freqs[i];
    }
  }

  return AE_OK;
}

/*============================================================================
 * Helper: Find Peak Modulation Channel
 *
 * Useful for determining dominant modulation frequency
 *============================================================================*/
AE_API uint32_t ae_modfb_find_peak_channel(ae_modfb_t *mfb, float **output,
                                           size_t n_samples) {
  if (!mfb || !output || n_samples == 0)
    return 0;

  float max_energy = 0.0f;
  uint32_t peak_channel = 0;

  for (uint32_t ch = 0; ch < mfb->config.n_channels; ++ch) {
    float energy = 0.0f;
    for (size_t i = 0; i < n_samples; ++i) {
      energy += output[ch][i] * output[ch][i];
    }
    energy /= (float)n_samples;

    if (energy > max_energy) {
      max_energy = energy;
      peak_channel = ch;
    }
  }

  return peak_channel;
}
