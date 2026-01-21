/**
 * @file ae_perceptual.c
 * @brief Perceptual profile analysis and perceptual-to-physical parameter
 * mapping
 *
 * Scientific references:
 * - Brightness/Timbre: Grey (1977), McAdams et al. (1995)
 * - Roughness/Fluctuation: Zwicker & Fastl (2007)
 * - Distance perception: Zahorik (2002), Bronkhorst & Houtgast (1999)
 * - Spaciousness (ASW/LEV): Bradley & Soulodre (1995), Ando (1998)
 * - Clarity: ISO 3382-1:2009
 */

#include "acoustic_engine.h"
#include <math.h>
#include <string.h>

/* Forward declarations for internal functions */
static float compute_spectral_flux_internal(const float *spectrum_prev,
                                            const float *spectrum_curr,
                                            size_t n_bins);
static float compute_attack_sharpness_internal(const float *samples,
                                               size_t n_samples,
                                               uint32_t sample_rate);

/* Normalization constants based on literature */
#define CENTROID_MIN_HZ 200.0f
#define CENTROID_MAX_HZ 8000.0f
#define ROUGHNESS_MAX_ASPER 2.5f
#define FLUCTUATION_MAX_VACIL 1.0f
#define SHARPNESS_MAX_ACUM 4.0f
#define DRR_NEAR_DB 10.0f
#define DRR_FAR_DB -20.0f
#define C50_MIN_DB -10.0f
#define C50_MAX_DB 10.0f

/* Clamp helper */
static inline float clampf(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

/* Sigmoid function for smooth normalization */
static inline float sigmoidf(float x) { return 1.0f / (1.0f + expf(-x)); }

/**
 * Analyze perceptual profile of audio signal
 */
AE_API ae_result_t ae_analyze_perceptual_profile(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    ae_perceptual_profile_t *out) {
  if (!engine || !signal || !out) {
    return AE_ERROR_INVALID_PARAM;
  }

  if (!signal->samples || signal->frame_count == 0) {
    return AE_ERROR_INVALID_PARAM;
  }

  memset(out, 0, sizeof(ae_perceptual_profile_t));

  /* === Spectral features (brightness, flux) === */
  ae_spectral_features_t spectral;
  ae_result_t res = ae_analyze_spectrum(engine, signal, &spectral);
  if (res == AE_OK) {
    out->spectral_centroid_hz = spectral.centroid_hz;

    /* Normalize brightness: Grey (1977), McAdams (1995) */
    float norm_centroid = (spectral.centroid_hz - CENTROID_MIN_HZ) /
                          (CENTROID_MAX_HZ - CENTROID_MIN_HZ);
    out->brightness = clampf(norm_centroid, 0.0f, 1.0f);

    /* Spectral flux - normalized by maximum typical value */
    out->spectral_flux = clampf(spectral.flux / 0.5f, 0.0f, 1.0f);
  }

  /* === Psychoacoustic features (roughness, fluctuation, sharpness) === */

  /* Roughness: Zwicker & Fastl (2007), peak at 70Hz modulation */
  float roughness_asper = 0.0f;
  res = ae_compute_roughness(engine, signal, &roughness_asper);
  if (res == AE_OK) {
    out->roughness_asper = roughness_asper;
    out->roughness_norm =
        clampf(roughness_asper / ROUGHNESS_MAX_ASPER, 0.0f, 1.0f);
  }

  /* Fluctuation strength: Fastl (1982), peak at 4Hz modulation */
  float fluctuation_vacil = 0.0f;
  res = ae_compute_fluctuation_strength(engine, signal, &fluctuation_vacil);
  if (res == AE_OK) {
    out->fluctuation_vacil = fluctuation_vacil;
    out->fluctuation_norm =
        clampf(fluctuation_vacil / FLUCTUATION_MAX_VACIL, 0.0f, 1.0f);
  }

  /* Sharpness: DIN 45692 */
  float sharpness_acum = 0.0f;
  res = ae_compute_sharpness(engine, signal, AE_SHARPNESS_DIN_45692,
                             &sharpness_acum);
  if (res == AE_OK) {
    out->sharpness_acum = sharpness_acum;
    out->sharpness_norm =
        clampf(sharpness_acum / SHARPNESS_MAX_ACUM, 0.0f, 1.0f);
  }

  /* === Spatial features (requires stereo) === */
  ae_perceptual_metrics_t metrics;
  res = ae_compute_perceptual_metrics(engine, signal, &metrics);
  if (res == AE_OK) {
    out->drr_db = metrics.drr_db;
    out->iacc_early = metrics.iacc_early;
    out->iacc_late = metrics.iacc_late;
    out->c50_db = metrics.clarity_c50;
    out->c80_db = metrics.clarity_c80;

    /* Perceived distance: Zahorik (2002)
     * DRR is approximately linear with log-distance
     * Near: DRR > 0dB, Far: DRR << 0dB */
    float drr_norm =
        (DRR_NEAR_DB - metrics.drr_db) / (DRR_NEAR_DB - DRR_FAR_DB);
    out->perceived_distance = clampf(drr_norm, 0.0f, 1.0f);

    /* Spaciousness: Bradley & Soulodre (1995)
     * Based on ASW (Apparent Source Width) which correlates with 1 - IACC_early
     */
    out->spaciousness = clampf(1.0f - metrics.iacc_early, 0.0f, 1.0f);

    /* Envelopment: Based on LEV which correlates with 1 - IACC_late */
    out->envelopment = clampf(1.0f - metrics.iacc_late, 0.0f, 1.0f);

    /* Clarity: ISO 3382, normalize C50 to 0-1 */
    float c50_norm =
        (metrics.clarity_c50 - C50_MIN_DB) / (C50_MAX_DB - C50_MIN_DB);
    out->clarity_norm = clampf(c50_norm, 0.0f, 1.0f);
  }

  /* === Attack sharpness (transient analysis) === */
  /* Simple envelope analysis for attack detection */
  out->attack_sharpness = compute_attack_sharpness_internal(
      signal->samples, signal->frame_count, AE_SAMPLE_RATE);

  return AE_OK;
}

/**
 * Set perceived distance - maps to physical parameters
 *
 * Based on Zahorik (2002): DRR is the primary cue for auditory distance.
 * Also applies high-frequency attenuation (air absorption).
 */
AE_API ae_result_t ae_set_perceived_distance(ae_engine_t *engine,
                                             float distance_perception) {
  if (!engine) {
    return AE_ERROR_INVALID_PARAM;
  }

  float d = clampf(distance_perception, 0.0f, 1.0f);

  /* Map to physical distance (exponential mapping) */
  /* 0.0 -> 1m, 0.5 -> 10m, 1.0 -> 100m */
  float physical_distance = 1.0f * powf(100.0f, d);

  /* Set physical parameters */
  ae_result_t res = ae_set_distance(engine, physical_distance);
  if (res != AE_OK)
    return res;

  /* Adjust dry/wet ratio based on DRR correlation */
  /* Near: more dry, Far: more wet */
  float dry_wet = 0.2f + 0.6f * d; /* 0.2 (near) to 0.8 (far) */
  res = ae_set_dry_wet(engine, dry_wet);
  if (res != AE_OK)
    return res;

  /* Adjust brightness (high frequency attenuation with distance) */
  /* Near: bright (0), Far: dark (-0.5) */
  float brightness = -0.5f * d;
  res = ae_set_brightness(engine, brightness);

  return res;
}

/**
 * Set perceived spaciousness - maps to physical parameters
 *
 * Based on Bradley & Soulodre (1995):
 * - ASW (Apparent Source Width) correlates with early lateral reflections
 * - LEV (Listener Envelopment) correlates with late lateral energy
 */
AE_API ae_result_t ae_set_perceived_spaciousness(ae_engine_t *engine,
                                                 float spaciousness) {
  if (!engine) {
    return AE_ERROR_INVALID_PARAM;
  }

  float s = clampf(spaciousness, 0.0f, 1.0f);

  /* Map to stereo width */
  /* 0.0 -> mono (0.0), 0.5 -> stereo (1.0), 1.0 -> ultra-wide (2.0) */
  float width = 2.0f * s;
  ae_result_t res = ae_set_width(engine, width);
  if (res != AE_OK)
    return res;

  /* Adjust room size to affect early reflection density */
  /* More spacious = larger perceived room */
  float room_size = 0.2f + 0.6f * s; /* 0.2 (intimate) to 0.8 (large) */
  res = ae_set_room_size(engine, room_size);

  return res;
}

/**
 * Set perceived clarity - maps to physical parameters
 *
 * Based on ISO 3382 C50/C80 metrics:
 * - High clarity = high DRR, short RT60
 * - Low clarity = low DRR, long RT60
 */
AE_API ae_result_t ae_set_perceived_clarity(ae_engine_t *engine,
                                            float clarity) {
  if (!engine) {
    return AE_ERROR_INVALID_PARAM;
  }

  float c = clampf(clarity, 0.0f, 1.0f);

  /* Dry/wet ratio: high clarity = more dry signal */
  float dry_wet = 0.7f - 0.5f * c; /* 0.2 (clear) to 0.7 (muddy) */
  ae_result_t res = ae_set_dry_wet(engine, dry_wet);
  if (res != AE_OK)
    return res;

  /* Room size affects RT60 */
  /* High clarity = smaller effective room */
  float room_size = 0.6f - 0.4f * c; /* 0.2 (clear) to 0.6 (reverberant) */
  res = ae_set_room_size(engine, room_size);
  if (res != AE_OK)
    return res;

  /* Brightness can also affect perceived clarity */
  /* Clearer signals tend to have more high-frequency content */
  float brightness = 0.3f * c - 0.15f; /* -0.15 (muddy) to +0.15 (clear) */
  res = ae_set_brightness(engine, brightness);

  return res;
}

/*============================================================================
 * Internal helper functions
 *============================================================================*/

/**
 * Compute attack sharpness from audio signal
 * Returns 0-1 based on the steepness of the initial transient
 */
static float compute_attack_sharpness_internal(const float *samples,
                                               size_t n_samples,
                                               uint32_t sample_rate) {
  if (!samples || n_samples == 0) {
    return 0.0f;
  }

  /* Find peak amplitude */
  float peak = 0.0f;
  size_t peak_idx = 0;
  for (size_t i = 0; i < n_samples; ++i) {
    float abs_val = fabsf(samples[i]);
    if (abs_val > peak) {
      peak = abs_val;
      peak_idx = i;
    }
  }

  if (peak < 1e-6f) {
    return 0.0f;
  }

  /* Find 10% threshold crossing before peak */
  float threshold_low = 0.1f * peak;
  size_t start_idx = 0;
  for (size_t i = 0; i < peak_idx; ++i) {
    if (fabsf(samples[i]) > threshold_low) {
      start_idx = i;
      break;
    }
  }

  /* Calculate attack time in seconds */
  float attack_samples = (float)(peak_idx - start_idx);
  float attack_time_s = attack_samples / (float)sample_rate;

  /* Normalize: 0ms -> 1.0 (sharp), 50ms -> 0.5, 200ms+ -> 0.0 (soft) */
  float attack_sharpness = 1.0f - clampf(attack_time_s * 5.0f, 0.0f, 1.0f);

  return attack_sharpness;
}

/**
 * Compute spectral flux between two spectra
 */
static float compute_spectral_flux_internal(const float *spectrum_prev,
                                            const float *spectrum_curr,
                                            size_t n_bins) {
  if (!spectrum_prev || !spectrum_curr || n_bins == 0) {
    return 0.0f;
  }

  float flux = 0.0f;
  for (size_t i = 0; i < n_bins; ++i) {
    float diff = spectrum_curr[i] - spectrum_prev[i];
    /* Half-wave rectification: only positive changes */
    if (diff > 0.0f) {
      flux += diff * diff;
    }
  }

  return sqrtf(flux);
}
