/**
 * @file test_propagation.c
 * @brief Verification tests for physical propagation models
 */

#include "acoustic_engine.h"
#include "ae_test.h"
#include <math.h>
#include <stdio.h>

/*============================================================================
 * Francois-Garrison Seawater Absorption Tests
 *============================================================================*/

void test_francois_garrison_frequency_dependence(void) {
  /* Test that absorption increases with frequency */
  float T = 10.0f;  /* Temperature (°C) */
  float S = 35.0f;  /* Salinity (ppt) */
  float D = 100.0f; /* Depth (m) */

  float abs_1khz = ae_francois_garrison_absorption(1.0f, T, S, D);
  float abs_10khz = ae_francois_garrison_absorption(10.0f, T, S, D);
  float abs_100khz = ae_francois_garrison_absorption(100.0f, T, S, D);

  /* Absorption should monotonically increase with frequency */
  AE_ASSERT(abs_10khz > abs_1khz);
  AE_ASSERT(abs_100khz > abs_10khz);

  /* At 10 kHz, typical seawater: ~1 dB/km (order of magnitude check) */
  AE_ASSERT(abs_10khz > 0.5f && abs_10khz < 5.0f);

  AE_TEST_PASS();
}

void test_francois_garrison_temperature_dependence(void) {
  /* Test temperature effect on absorption */
  float f = 10.0f; /* 10 kHz */
  float S = 35.0f;
  float D = 100.0f;

  float abs_cold = ae_francois_garrison_absorption(f, 5.0f, S, D);
  float abs_warm = ae_francois_garrison_absorption(f, 20.0f, S, D);

  /* Temperature affects relaxation frequencies and absorption */
  /* Both should be positive and reasonably close (within factor of 2) */
  AE_ASSERT(abs_cold > 0.0f && abs_warm > 0.0f);
  AE_ASSERT(fabsf(abs_cold - abs_warm) / abs_cold < 2.0f);

  AE_TEST_PASS();
}

void test_francois_garrison_depth_dependence(void) {
  /* Test pressure/depth effect */
  float f = 10.0f;
  float T = 10.0f;
  float S = 35.0f;

  float abs_shallow = ae_francois_garrison_absorption(f, T, S, 10.0f);
  float abs_deep = ae_francois_garrison_absorption(f, T, S, 1000.0f);

  /* Both should be positive */
  AE_ASSERT(abs_shallow > 0.0f && abs_deep > 0.0f);

  AE_TEST_PASS();
}

/*============================================================================
 * ISO 9613-1 Atmospheric Absorption Tests
 *============================================================================*/

void test_iso9613_frequency_dependence(void) {
  float T = 20.0f;    /* 20°C */
  float rh = 50.0f;   /* 50% RH */
  float p = 101.325f; /* 1 atm */

  float abs_250 = ae_iso9613_absorption(250.0f, T, rh, p);
  float abs_1k = ae_iso9613_absorption(1000.0f, T, rh, p);
  float abs_8k = ae_iso9613_absorption(8000.0f, T, rh, p);

  /* Absorption increases with frequency */
  AE_ASSERT(abs_1k > abs_250);
  AE_ASSERT(abs_8k > abs_1k);

  /* Reference: ISO 9613-1 Table 1 at 8kHz, 20°C, 50%RH ≈ 0.15-0.25 dB/m */
  AE_ASSERT(abs_8k > 0.05f && abs_8k < 0.5f);

  AE_TEST_PASS();
}

void test_iso9613_humidity_effect(void) {
  float f = 4000.0f;
  float T = 20.0f;
  float p = 101.325f;

  float abs_dry = ae_iso9613_absorption(f, T, 20.0f, p);
  float abs_humid = ae_iso9613_absorption(f, T, 80.0f, p);

  /* Both should be positive */
  AE_ASSERT(abs_dry > 0.0f && abs_humid > 0.0f);

  AE_TEST_PASS();
}

/*============================================================================
 * Cave Acoustics Tests
 *============================================================================*/

void test_cave_modal_frequency(void) {
  float dimension = 15.0f; /* 15m cave */
  float temp = 20.0f;      /* c = 343 m/s approximately */

  float f1 = ae_cave_modal_frequency(dimension, 1, temp);
  float f2 = ae_cave_modal_frequency(dimension, 2, temp);
  float f3 = ae_cave_modal_frequency(dimension, 3, temp);

  /* f_n = n * c / (2 * L) → f1 ≈ 343 / 30 ≈ 11.4 Hz */
  AE_ASSERT(f1 > 10.0f && f1 < 13.0f);

  /* Mode frequencies should be integer multiples */
  AE_ASSERT_FLOAT_EQ(f2 / f1, 2.0f, 0.01f);
  AE_ASSERT_FLOAT_EQ(f3 / f1, 3.0f, 0.01f);

  AE_TEST_PASS();
}

void test_cave_flutter(void) {
  float wall_dist = 8.0f; /* 8m passage */
  float temp = 20.0f;

  float delay_ms, flutter_freq;
  ae_calculate_flutter(wall_dist, temp, &delay_ms, &flutter_freq);

  /* Expected: T = 2 * 8 / 343 ≈ 46.6 ms, f ≈ 21.4 Hz */
  AE_ASSERT(delay_ms > 40.0f && delay_ms < 55.0f);
  AE_ASSERT(flutter_freq > 18.0f && flutter_freq < 25.0f);

  AE_TEST_PASS();
}

void test_eyring_rt60(void) {
  /* Small room: V=50m³, S=80m², α=0.3 */
  float rt60_small = ae_eyring_rt60(50.0f, 80.0f, 0.3f);

  /* Large room: V=500m³, S=350m², α=0.3 */
  float rt60_large = ae_eyring_rt60(500.0f, 350.0f, 0.3f);

  /* Larger volume should give longer RT60 */
  AE_ASSERT(rt60_large > rt60_small);

  /* Reference: typical room RT60 0.3-3s */
  AE_ASSERT(rt60_small > 0.1f && rt60_small < 2.0f);

  AE_TEST_PASS();
}

void test_rock_wall_absorption(void) {
  float alpha_125 = ae_rock_wall_absorption(125.0f);
  float alpha_4k = ae_rock_wall_absorption(4000.0f);

  /* Low frequency: very low absorption (reflective) */
  AE_ASSERT(alpha_125 < 0.05f);

  /* High frequency: slightly more absorption */
  AE_ASSERT(alpha_4k > alpha_125);
  AE_ASSERT(alpha_4k < 0.15f);

  AE_TEST_PASS();
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
  printf("Acoustic Engine - Propagation Tests\n");

  AE_TEST_SUITE_BEGIN("Francois-Garrison Seawater");
  AE_RUN_TEST(test_francois_garrison_frequency_dependence);
  AE_RUN_TEST(test_francois_garrison_temperature_dependence);
  AE_RUN_TEST(test_francois_garrison_depth_dependence);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("ISO 9613-1 Atmospheric");
  AE_RUN_TEST(test_iso9613_frequency_dependence);
  AE_RUN_TEST(test_iso9613_humidity_effect);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Cave Acoustics");
  AE_RUN_TEST(test_cave_modal_frequency);
  AE_RUN_TEST(test_cave_flutter);
  AE_RUN_TEST(test_eyring_rt60);
  AE_RUN_TEST(test_rock_wall_absorption);
  AE_TEST_SUITE_END();

  return ae_test_report();
}
