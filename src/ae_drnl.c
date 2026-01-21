/**
 * @file ae_drnl.c
 * @brief DRNL (Dual-Resonance Nonlinear) filterbank implementation
 *
 * Based on the model by Meddis et al. (2001) for cochlear mechanics.
 * Parallel linear and nonlinear paths with "broken-stick" compression.
 */

#include "ae_internal.h"

/*============================================================================
 * DRNL Channel Structure
 *============================================================================*/
typedef struct {
  float cf; /* Center frequency (Hz) */

  /* Gammatone filter states for linear path (4th order) */
  float lin_state_re[4];
  float lin_state_im[4];

  /* Gammatone filter states for nonlinear path - first stage */
  float nlin1_state_re[4];
  float nlin1_state_im[4];

  /* Gammatone filter states for nonlinear path - second stage */
  float nlin2_state_re[4];
  float nlin2_state_im[4];

  /* Lowpass filter states */
  float lin_lp_state;
  float nlin_lp1_state;
  float nlin_lp2_state;

  /* Gammatone coefficients */
  float a;      /* Amplitude */
  float b;      /* Bandwidth factor */
  float cos_cf; /* cos(2*pi*cf/sr) */
  float sin_cf; /* sin(2*pi*cf/sr) */
} ae_drnl_channel_t;

/*============================================================================
 * DRNL Structure
 *============================================================================*/
struct ae_drnl {
  ae_drnl_config_t config;
  ae_drnl_channel_t *channels;
  float *center_freqs;
  float *output_buffer; /* Temporary output buffer per channel */
};

/*============================================================================
 * ERB Calculation (same as Gammatone)
 *============================================================================*/
static float drnl_erb(float f_hz) {
  return 24.7f * (4.37f * f_hz / 1000.0f + 1.0f);
}

static float drnl_hz_to_erb_rate(float f_hz) {
  return 21.4f * log10f(4.37f * f_hz / 1000.0f + 1.0f);
}

static float drnl_erb_rate_to_hz(float erb_rate) {
  return (powf(10.0f, erb_rate / 21.4f) - 1.0f) * 1000.0f / 4.37f;
}

/*============================================================================
 * Calculate Center Frequencies
 *============================================================================*/
static void drnl_calculate_center_freqs(ae_drnl_t *drnl) {
  if (!drnl || drnl->config.n_channels == 0)
    return;

  float erb_low = drnl_hz_to_erb_rate(drnl->config.f_low);
  float erb_high = drnl_hz_to_erb_rate(drnl->config.f_high);
  float erb_step = (erb_high - erb_low) / (float)(drnl->config.n_channels - 1);

  for (uint32_t i = 0; i < drnl->config.n_channels; ++i) {
    float erb = erb_low + i * erb_step;
    drnl->center_freqs[i] = drnl_erb_rate_to_hz(erb);

    /* Initialize channel coefficients */
    ae_drnl_channel_t *ch = &drnl->channels[i];
    ch->cf = drnl->center_freqs[i];

    float erb_cf = drnl_erb(ch->cf);
    ch->b = 2.0f * (float)M_PI * 1.019f * erb_cf / drnl->config.sample_rate;
    ch->a = 1.0f;

    float w = 2.0f * (float)M_PI * ch->cf / drnl->config.sample_rate;
    ch->cos_cf = cosf(w);
    ch->sin_cf = sinf(w);
  }
}

/*============================================================================
 * Broken-stick Compression
 *
 * y = sign(x) * min(a*|x|, b*|x|^c)
 * where c ≈ 0.25 for cochlear compression
 *============================================================================*/
static float broken_stick_compress(float x, float a, float b, float c) {
  float abs_x = fabsf(x);
  if (abs_x < AE_LOG_EPSILON)
    return 0.0f;

  float lin = a * abs_x;
  float nlin = b * powf(abs_x, c);
  float result = fminf(lin, nlin);

  return (x >= 0.0f) ? result : -result;
}

/*============================================================================
 * Single-pole Lowpass Filter
 *============================================================================*/
static float lowpass_process(float input, float *state, float alpha) {
  *state = alpha * (*state) + (1.0f - alpha) * input;
  return *state;
}

/*============================================================================
 * 4th-order Gammatone IIR Stage
 *============================================================================*/
static float gammatone_stage(float input, float *state_re, float *state_im,
                             float a, float b, float cos_w, float sin_w) {
  float decay = expf(-b);

  for (int stage = 0; stage < 4; ++stage) {
    float in_re = (stage == 0) ? input : state_re[stage - 1];
    float in_im = (stage == 0) ? 0.0f : state_im[stage - 1];

    /* Complex multiplication for frequency shift + decay */
    float new_re = decay * (cos_w * state_re[stage] - sin_w * state_im[stage]) +
                   (1.0f - decay) * in_re;
    float new_im = decay * (sin_w * state_re[stage] + cos_w * state_im[stage]) +
                   (1.0f - decay) * in_im;

    state_re[stage] = new_re;
    state_im[stage] = new_im;
  }

  /* Output is magnitude of final stage */
  float re = state_re[3];
  float im = state_im[3];
  return sqrtf(re * re + im * im) * a;
}

/*============================================================================
 * Create DRNL
 *============================================================================*/
AE_API ae_drnl_t *ae_drnl_create(const ae_drnl_config_t *config) {
  if (!config || config->n_channels == 0 || config->sample_rate == 0)
    return NULL;

  ae_drnl_t *drnl = (ae_drnl_t *)calloc(1, sizeof(ae_drnl_t));
  if (!drnl)
    return NULL;

  drnl->config = *config;

  /* Set defaults if not specified */
  if (drnl->config.compression_exp <= 0.0f)
    drnl->config.compression_exp = 0.25f;
  if (drnl->config.lin_gain <= 0.0f)
    drnl->config.lin_gain = 1.0f;
  if (drnl->config.nlin_a <= 0.0f)
    drnl->config.nlin_a = 1.0f;
  if (drnl->config.nlin_b <= 0.0f)
    drnl->config.nlin_b = 1.0f;

  /* Allocate channels */
  drnl->channels = (ae_drnl_channel_t *)calloc(config->n_channels,
                                               sizeof(ae_drnl_channel_t));
  if (!drnl->channels) {
    free(drnl);
    return NULL;
  }

  /* Allocate center frequency array */
  drnl->center_freqs = (float *)calloc(config->n_channels, sizeof(float));
  if (!drnl->center_freqs) {
    free(drnl->channels);
    free(drnl);
    return NULL;
  }

  /* Calculate center frequencies and initialize channels */
  drnl_calculate_center_freqs(drnl);

  return drnl;
}

/*============================================================================
 * Destroy DRNL
 *============================================================================*/
AE_API void ae_drnl_destroy(ae_drnl_t *drnl) {
  if (!drnl)
    return;

  free(drnl->center_freqs);
  free(drnl->channels);
  free(drnl->output_buffer);
  free(drnl);
}

/*============================================================================
 * Process DRNL
 *
 * Architecture:
 *   Input ─┬─→ [Linear Path] ──────────────────────────┬─→ Output
 *          │    GT → LP → Gain                         │
 *          │                                            │
 *          └─→ [Nonlinear Path] ───────────────────────┘
 *               GT → LP → Compression → GT → LP
 *============================================================================*/
AE_API ae_result_t ae_drnl_process(ae_drnl_t *drnl, const float *input,
                                   size_t n_samples, float **output) {
  if (!drnl || !input || !output || n_samples == 0)
    return AE_ERROR_INVALID_PARAM;

  float lp_alpha = 0.95f; /* Lowpass smoothing coefficient */
  float compression_exp = drnl->config.compression_exp;
  float lin_gain = drnl->config.lin_gain;
  float nlin_a = drnl->config.nlin_a;
  float nlin_b = drnl->config.nlin_b;

  for (uint32_t ch = 0; ch < drnl->config.n_channels; ++ch) {
    ae_drnl_channel_t *channel = &drnl->channels[ch];
    float *out = output[ch];

    for (size_t i = 0; i < n_samples; ++i) {
      float sample = input[i];

      /* === Linear Path === */
      /* Gammatone filter */
      float lin_gt = gammatone_stage(
          sample, channel->lin_state_re, channel->lin_state_im, channel->a,
          channel->b, channel->cos_cf, channel->sin_cf);
      /* Lowpass filter */
      float lin_lp = lowpass_process(lin_gt, &channel->lin_lp_state, lp_alpha);
      /* Apply gain */
      float lin_out = lin_lp * lin_gain;

      /* === Nonlinear Path === */
      /* First Gammatone */
      float nlin_gt1 = gammatone_stage(
          sample, channel->nlin1_state_re, channel->nlin1_state_im, channel->a,
          channel->b, channel->cos_cf, channel->sin_cf);
      /* First Lowpass */
      float nlin_lp1 =
          lowpass_process(nlin_gt1, &channel->nlin_lp1_state, lp_alpha);

      /* Broken-stick compression */
      float compressed =
          broken_stick_compress(nlin_lp1, nlin_a, nlin_b, compression_exp);

      /* Second Gammatone */
      float nlin_gt2 = gammatone_stage(
          compressed, channel->nlin2_state_re, channel->nlin2_state_im,
          channel->a, channel->b, channel->cos_cf, channel->sin_cf);
      /* Second Lowpass */
      float nlin_out =
          lowpass_process(nlin_gt2, &channel->nlin_lp2_state, lp_alpha);

      /* === Combine paths === */
      out[i] = lin_out + nlin_out;
    }
  }

  return AE_OK;
}

/*============================================================================
 * Get Center Frequencies
 *============================================================================*/
AE_API ae_result_t ae_drnl_get_center_frequencies(ae_drnl_t *drnl,
                                                  float *frequencies,
                                                  size_t *n_channels) {
  if (!drnl)
    return AE_ERROR_INVALID_PARAM;

  if (n_channels)
    *n_channels = drnl->config.n_channels;

  if (frequencies) {
    for (uint32_t i = 0; i < drnl->config.n_channels; ++i) {
      frequencies[i] = drnl->center_freqs[i];
    }
  }

  return AE_OK;
}
