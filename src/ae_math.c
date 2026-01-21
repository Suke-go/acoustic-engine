#include "ae_internal.h"

AE_API float ae_safe_log10(float x) {
  if (x <= 0.0f)
    return -100.0f;
  return log10f(fmaxf(x, AE_LOG_EPSILON));
}

AE_API float ae_to_db(float linear) {
  if (linear <= 0.0f)
    return -120.0f;
  float db = 20.0f * log10f(linear);
  return fmaxf(db, -120.0f);
}

AE_API float ae_from_db(float db) {
  if (db <= -120.0f)
    return 0.0f;
  return powf(10.0f, db / 20.0f);
}

AE_API float ae_phon_to_sone(float phon) {
  if (phon >= 40.0f) {
    return powf(2.0f, (phon - 40.0f) / 10.0f);
  }
  return powf(phon / 40.0f, 2.642f);
}

AE_API float ae_sone_to_phon(float sone) {
  if (sone >= 1.0f) {
    return 40.0f + 10.0f * log2f(sone);
  }
  return 40.0f * powf(sone, 0.378f);
}

AE_API float ae_hz_to_bark(float hz) {
  if (hz < 20.0f)
    hz = 20.0f;
  if (hz > 15500.0f)
    hz = 15500.0f;
  float z = 26.81f * hz / (1960.0f + hz) - 0.53f;
  if (z < 2.0f) {
    z = z + 0.15f * (2.0f - z);
  } else if (z > 20.1f) {
    z = z + 0.22f * (z - 20.1f);
  }
  return z;
}

AE_API float ae_bark_to_hz(float bark) {
  float target = bark;
  if (target < 0.0f)
    target = 0.0f;
  if (target > 24.0f)
    target = 24.0f;
  float low = 20.0f;
  float high = 15500.0f;
  for (int i = 0; i < 24; ++i) {
    float mid = 0.5f * (low + high);
    float z = ae_hz_to_bark(mid);
    if (z < target)
      low = mid;
    else
      high = mid;
  }
  return 0.5f * (low + high);
}

AE_API ae_result_t ae_validate_params(const ae_main_params_t *params) {
  if (!params)
    return AE_ERROR_INVALID_PARAM;
  if (isnan(params->distance) || params->distance <= 0.0f ||
      params->distance > 1000.0f)
    return AE_ERROR_INVALID_PARAM;
  if (isnan(params->room_size) || params->room_size < 0.0f ||
      params->room_size > 1.0f)
    return AE_ERROR_INVALID_PARAM;
  if (isnan(params->brightness) || params->brightness < -1.0f ||
      params->brightness > 1.0f)
    return AE_ERROR_INVALID_PARAM;
  if (isnan(params->width) || params->width < 0.0f || params->width > 2.0f)
    return AE_ERROR_INVALID_PARAM;
  if (isnan(params->dry_wet) || params->dry_wet < 0.0f ||
      params->dry_wet > 1.0f)
    return AE_ERROR_INVALID_PARAM;
  if (isnan(params->intensity) || params->intensity < 0.0f ||
      params->intensity > 1.0f)
    return AE_ERROR_INVALID_PARAM;
  return AE_OK;
}
