/**
 * @file ae_propagation.c
 * @brief Physical propagation models (Francois-Garrison, ISO 9613-1, Cave)
 */

#include "ae_internal.h"

/**
 * Francois-Garrison ocean absorption model (dB/km)
 * Based on Francois & Garrison (1982)
 *
 * @param f_khz Frequency in kHz
 * @param T Temperature in Celsius
 * @param S Salinity in ppt (parts per thousand)
 * @param D Depth in meters
 * @return Absorption coefficient in dB/km
 */
float ae_francois_garrison_absorption(float f_khz, float T, float S, float D) {
  /* pH value (typically 8 for seawater) */
  float pH = 8.0f;

  /* Speed of sound approximation (Chen & Millero, 1977) */
  float c = 1449.2f + 4.6f * T - 0.055f * T * T + 0.00029f * T * T * T +
            (1.34f - 0.01f * T) * (S - 35.0f) + 0.016f * D;

  /* Boric acid contribution */
  float theta = 273.0f + T;
  float f1 = 2.8f * sqrtf(S / 35.0f) * powf(10.0f, 4.0f - 1245.0f / theta);
  float A1 = 8.86f / c * powf(10.0f, 0.78f * pH - 5.0f);
  float P1 = 1.0f;
  float alpha1 = A1 * P1 * f1 * f_khz * f_khz / (f1 * f1 + f_khz * f_khz);

  /* Magnesium sulfate contribution */
  float f2 = 8.17f * powf(10.0f, 8.0f - 1990.0f / theta) /
             (1.0f + 0.0018f * (S - 35.0f));
  float A2 = 21.44f * S / c * (1.0f + 0.025f * T);
  float P2 = 1.0f - 1.37e-4f * D + 6.2e-9f * D * D;
  float alpha2 = A2 * P2 * f2 * f_khz * f_khz / (f2 * f2 + f_khz * f_khz);

  /* Pure water contribution */
  float P3 = 1.0f - 3.83e-5f * D + 4.9e-10f * D * D;
  float A3;
  if (T <= 20.0f) {
    A3 = 4.937e-4f - 2.59e-5f * T + 9.11e-7f * T * T - 1.50e-8f * T * T * T;
  } else {
    A3 = 3.964e-4f - 1.146e-5f * T + 1.45e-7f * T * T - 6.5e-10f * T * T * T;
  }
  float alpha3 = A3 * P3 * f_khz * f_khz;

  /* Total absorption in dB/km */
  return alpha1 + alpha2 + alpha3;
}

/**
 * ISO 9613-1 atmospheric absorption model (dB/m)
 * Based on ISO 9613-1:1993
 *
 * @param f_hz Frequency in Hz
 * @param T_celsius Temperature in Celsius
 * @param humidity_pct Relative humidity in percent
 * @param pressure_kpa Atmospheric pressure in kPa
 * @return Absorption coefficient in dB/m
 */
float ae_iso9613_absorption(float f_hz, float T_celsius, float humidity_pct,
                            float pressure_kpa) {
  /* Reference values */
  float T0 = 293.15f;            /* Reference temperature (K) */
  float T01 = 273.16f;           /* Triple point of water (K) */
  float p0 = 101.325f;           /* Reference pressure (kPa) */
  float T = T_celsius + 273.15f; /* Absolute temperature (K) */
  float pa = pressure_kpa;       /* Absolute pressure (kPa) */
  float f = f_hz;

  /* Molar concentration of water vapor */
  float C = -6.8346f * powf(T01 / T, 1.261f) + 4.6151f;
  float psat = pa * powf(10.0f, C);
  float h = humidity_pct * psat / pa;

  /* Relaxation frequencies */
  float frO = (pa / p0) * (24.0f + 4.04e4f * h * (0.02f + h) / (0.391f + h));
  float frN =
      (pa / p0) * powf(T / T0, -0.5f) *
      (9.0f + 280.0f * h * expf(-4.170f * (powf(T / T0, -1.0f / 3.0f) - 1.0f)));

  /* Absorption coefficient in dB/m */
  float freq_sq = f * f;
  float term1 = 1.84e-11f * (p0 / pa) * sqrtf(T / T0);
  float term2 = powf(T / T0, -2.5f) *
                (0.01275f * expf(-2239.1f / T) / (frO + freq_sq / frO) +
                 0.1068f * expf(-3352.0f / T) / (frN + freq_sq / frN));

  float alpha = 8.686f * freq_sq * (term1 + term2);
  return alpha;
}

/**
 * Calculate cave modal resonance frequencies
 * Based on 1D approximation: f_n = n * c / (2 * L)
 *
 * @param dimension_m Major dimension of cave in meters
 * @param mode_number Mode number (1, 2, 3, ...)
 * @param temperature_c Temperature in Celsius (affects sound speed)
 * @return Resonance frequency in Hz
 */
float ae_cave_modal_frequency(float dimension_m, int mode_number,
                              float temperature_c) {
  /* Sound speed: c = 331.3 + 0.606 * T */
  float c = 331.3f + 0.606f * temperature_c;
  if (dimension_m <= 0.0f || mode_number < 1) {
    return 0.0f;
  }
  return (float)mode_number * c / (2.0f * dimension_m);
}

/**
 * Calculate flutter echo parameters
 *
 * @param wall_distance_m Distance between parallel walls in meters
 * @param temperature_c Temperature in Celsius
 * @param delay_ms_out Output: flutter delay in milliseconds
 * @param flutter_freq_out Output: flutter frequency in Hz
 */
void ae_calculate_flutter(float wall_distance_m, float temperature_c,
                          float *delay_ms_out, float *flutter_freq_out) {
  float c = 331.3f + 0.606f * temperature_c;
  if (wall_distance_m <= 0.0f || !delay_ms_out || !flutter_freq_out) {
    if (delay_ms_out)
      *delay_ms_out = 0.0f;
    if (flutter_freq_out)
      *flutter_freq_out = 0.0f;
    return;
  }

  /* Round-trip time */
  float T_flutter = 2.0f * wall_distance_m / c;
  *delay_ms_out = T_flutter * 1000.0f;
  *flutter_freq_out = 1.0f / T_flutter;
}

/**
 * Eyring RT60 calculation for low absorption spaces (caves)
 *
 * @param volume_m3 Volume in cubic meters
 * @param surface_m2 Surface area in square meters
 * @param avg_alpha Average absorption coefficient (0-1)
 * @return RT60 in seconds
 */
float ae_eyring_rt60(float volume_m3, float surface_m2, float avg_alpha) {
  if (volume_m3 <= 0.0f || surface_m2 <= 0.0f || avg_alpha <= 0.0f) {
    return 0.0f;
  }
  if (avg_alpha >= 1.0f) {
    return 0.0f; /* Fully absorbing, no reverb */
  }

  /* Eyring formula: RT60 = 0.161 * V / (-S * ln(1 - α)) */
  return 0.161f * volume_m3 / (-surface_m2 * logf(1.0f - avg_alpha));
}

/**
 * Frequency-dependent rock wall absorption (limestone approximation)
 *
 * @param f_hz Frequency in Hz
 * @return Absorption coefficient (0-1)
 */
float ae_rock_wall_absorption(float f_hz) {
  /* Approximate absorption curve for limestone */
  float alpha_125 = 0.02f;
  float alpha_4k = 0.08f;

  /* Logarithmic interpolation */
  float log_f = log10f(ae_clamp(f_hz, 125.0f, 8000.0f));
  float log_125 = log10f(125.0f);
  float log_4k = log10f(4000.0f);

  float t = (log_f - log_125) / (log_4k - log_125);
  t = ae_clamp(t, 0.0f, 1.0f);

  return alpha_125 + (alpha_4k - alpha_125) * t;
}

/**
 * Apply frequency-dependent absorption to an audio buffer
 *
 * @param samples Input/output samples
 * @param frames Number of frames
 * @param distance_m Distance in meters
 * @param absorption_db_per_m Absorption coefficient in dB/m (per frequency)
 * @param sample_rate Sample rate in Hz
 * @param filter_state Filter state for low-pass (caller-owned)
 */
void ae_apply_distance_absorption(float *samples, size_t frames,
                                  float distance_m, float absorption_db_per_km,
                                  float sample_rate, float *filter_state) {
  if (!samples || frames == 0)
    return;

  /* Total attenuation in dB */
  float attenuation_db = absorption_db_per_km * (distance_m / 1000.0f);

  /* High-frequency cutoff based on distance (simplified model) */
  /* At longer distances, high frequencies are attenuated more */
  float cutoff = 20000.0f / (1.0f + 0.01f * distance_m);
  cutoff = ae_clamp(cutoff, 1000.0f, 20000.0f);

  /* Simple 1-pole lowpass filter */
  float rc = 1.0f / (2.0f * (float)M_PI * cutoff);
  float dt = 1.0f / sample_rate;
  float alpha = dt / (rc + dt);

  float state = *filter_state;
  float linear_gain = powf(10.0f, -attenuation_db / 20.0f);

  for (size_t i = 0; i < frames; ++i) {
    state = state + alpha * (samples[i] - state);
    samples[i] = state * linear_gain;
  }

  *filter_state = state;
}
