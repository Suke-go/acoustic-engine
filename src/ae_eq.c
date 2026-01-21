/**
 * @file ae_eq.c
 * @brief Parametric EQ implementation (8-band)
 */

#include "ae_internal.h"

/*============================================================================
 * EQ Band Types
 *============================================================================*/
typedef enum {
  AE_EQ_PEAK,
  AE_EQ_LOW_SHELF,
  AE_EQ_HIGH_SHELF,
  AE_EQ_NOTCH,
  AE_EQ_LOWPASS,
  AE_EQ_HIGHPASS
} ae_eq_band_type_t;

/*============================================================================
 * Single EQ Band
 *============================================================================*/
typedef struct {
  ae_eq_band_type_t type;
  float frequency_hz; /* Center/corner frequency */
  float gain_db;      /* Gain in dB (for peak/shelf) */
  float q;            /* Q factor (0.1 - 30) */
  bool enabled;

  /* Biquad coefficients */
  float b0, b1, b2;
  float a1, a2;

  /* Filter state (stereo) */
  float x1_l, x2_l;
  float y1_l, y2_l;
  float x1_r, x2_r;
  float y1_r, y2_r;
} ae_eq_band_t;

/*============================================================================
 * 8-Band Parametric EQ
 *============================================================================*/
typedef struct {
  ae_eq_band_t bands[AE_MAX_EQ_BANDS];
  uint8_t band_count;
  float sample_rate;
} ae_parametric_eq_t;

/*============================================================================
 * Coefficient Calculation
 *============================================================================*/
static void eq_calculate_coeffs(ae_eq_band_t *band, float sample_rate) {
  if (!band || sample_rate <= 0.0f)
    return;

  float w0 = 2.0f * (float)M_PI * band->frequency_hz / sample_rate;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * band->q);
  float A = powf(10.0f, band->gain_db / 40.0f);

  float a0;

  switch (band->type) {
  case AE_EQ_PEAK:
    band->b0 = 1.0f + alpha * A;
    band->b1 = -2.0f * cos_w0;
    band->b2 = 1.0f - alpha * A;
    a0 = 1.0f + alpha / A;
    band->a1 = -2.0f * cos_w0;
    band->a2 = 1.0f - alpha / A;
    break;

  case AE_EQ_LOW_SHELF: {
    float sqrt_A = sqrtf(A);
    float sqrt_A_alpha = 2.0f * sqrt_A * alpha;
    band->b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + sqrt_A_alpha);
    band->b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
    band->b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - sqrt_A_alpha);
    a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + sqrt_A_alpha;
    band->a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
    band->a2 = (A + 1.0f) + (A - 1.0f) * cos_w0 - sqrt_A_alpha;
    break;
  }

  case AE_EQ_HIGH_SHELF: {
    float sqrt_A = sqrtf(A);
    float sqrt_A_alpha = 2.0f * sqrt_A * alpha;
    band->b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + sqrt_A_alpha);
    band->b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
    band->b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - sqrt_A_alpha);
    a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + sqrt_A_alpha;
    band->a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
    band->a2 = (A + 1.0f) - (A - 1.0f) * cos_w0 - sqrt_A_alpha;
    break;
  }

  case AE_EQ_NOTCH:
    band->b0 = 1.0f;
    band->b1 = -2.0f * cos_w0;
    band->b2 = 1.0f;
    a0 = 1.0f + alpha;
    band->a1 = -2.0f * cos_w0;
    band->a2 = 1.0f - alpha;
    break;

  case AE_EQ_LOWPASS:
    band->b0 = (1.0f - cos_w0) / 2.0f;
    band->b1 = 1.0f - cos_w0;
    band->b2 = (1.0f - cos_w0) / 2.0f;
    a0 = 1.0f + alpha;
    band->a1 = -2.0f * cos_w0;
    band->a2 = 1.0f - alpha;
    break;

  case AE_EQ_HIGHPASS:
    band->b0 = (1.0f + cos_w0) / 2.0f;
    band->b1 = -(1.0f + cos_w0);
    band->b2 = (1.0f + cos_w0) / 2.0f;
    a0 = 1.0f + alpha;
    band->a1 = -2.0f * cos_w0;
    band->a2 = 1.0f - alpha;
    break;

  default:
    band->b0 = 1.0f;
    band->b1 = band->b2 = 0.0f;
    band->a1 = band->a2 = 0.0f;
    return;
  }

  /* Normalize by a0 */
  if (a0 != 0.0f) {
    band->b0 /= a0;
    band->b1 /= a0;
    band->b2 /= a0;
    band->a1 /= a0;
    band->a2 /= a0;
  }
}

/*============================================================================
 * Band Processing
 *============================================================================*/
static float eq_process_band_sample(ae_eq_band_t *band, float sample, float *x1,
                                    float *x2, float *y1, float *y2) {
  float output = band->b0 * sample + band->b1 * (*x1) + band->b2 * (*x2) -
                 band->a1 * (*y1) - band->a2 * (*y2);

  *x2 = *x1;
  *x1 = sample;
  *y2 = *y1;
  *y1 = output;

  return output;
}

/*============================================================================
 * Public API
 *============================================================================*/
void ae_parametric_eq_init(ae_parametric_eq_t *eq, float sample_rate) {
  if (!eq)
    return;

  memset(eq, 0, sizeof(ae_parametric_eq_t));
  eq->sample_rate = sample_rate;
  eq->band_count = 0;

  /* Initialize all bands to bypass */
  for (int i = 0; i < AE_MAX_EQ_BANDS; ++i) {
    eq->bands[i].enabled = false;
    eq->bands[i].type = AE_EQ_PEAK;
    eq->bands[i].frequency_hz = 1000.0f;
    eq->bands[i].gain_db = 0.0f;
    eq->bands[i].q = 1.0f;
    eq->bands[i].b0 = 1.0f;
    eq->bands[i].b1 = eq->bands[i].b2 = 0.0f;
    eq->bands[i].a1 = eq->bands[i].a2 = 0.0f;
  }
}

void ae_parametric_eq_set_band(ae_parametric_eq_t *eq, uint8_t band_index,
                               ae_eq_band_type_t type, float freq_hz,
                               float gain_db, float q, bool enabled) {
  if (!eq || band_index >= AE_MAX_EQ_BANDS)
    return;

  ae_eq_band_t *band = &eq->bands[band_index];
  band->type = type;
  band->frequency_hz = ae_clamp(freq_hz, 20.0f, 20000.0f);
  band->gain_db = ae_clamp(gain_db, -24.0f, 24.0f);
  band->q = ae_clamp(q, 0.1f, 30.0f);
  band->enabled = enabled;

  eq_calculate_coeffs(band, eq->sample_rate);

  if (band_index >= eq->band_count) {
    eq->band_count = band_index + 1;
  }
}

void ae_parametric_eq_process(ae_parametric_eq_t *eq, float *left, float *right,
                              size_t frames) {
  if (!eq || !left || frames == 0)
    return;

  for (size_t i = 0; i < frames; ++i) {
    float sample_l = left[i];
    float sample_r = right ? right[i] : sample_l;

    for (uint8_t b = 0; b < eq->band_count; ++b) {
      ae_eq_band_t *band = &eq->bands[b];
      if (!band->enabled)
        continue;

      sample_l = eq_process_band_sample(band, sample_l, &band->x1_l,
                                        &band->x2_l, &band->y1_l, &band->y2_l);
      if (right) {
        sample_r = eq_process_band_sample(
            band, sample_r, &band->x1_r, &band->x2_r, &band->y1_r, &band->y2_r);
      }
    }

    left[i] = sample_l;
    if (right)
      right[i] = sample_r;
  }
}

void ae_parametric_eq_reset(ae_parametric_eq_t *eq) {
  if (!eq)
    return;

  for (int i = 0; i < AE_MAX_EQ_BANDS; ++i) {
    ae_eq_band_t *band = &eq->bands[i];
    band->x1_l = band->x2_l = 0.0f;
    band->y1_l = band->y2_l = 0.0f;
    band->x1_r = band->x2_r = 0.0f;
    band->y1_r = band->y2_r = 0.0f;
  }
}
