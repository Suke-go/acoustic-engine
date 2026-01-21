/**
 * @file test_analysis.c
 * @brief Unit tests for audio analysis functions (room metrics, loudness, etc.)
 */

#include "../include/acoustic_engine.h"
#include "ae_test.h"
#include <math.h>
#include <string.h>


/*============================================================================
 * Tests: Clarity metrics
 *============================================================================*/

void test_c50_impulse_only(void) {
  /* IR with only direct sound (impulse at t=0) */
  float ir[4800]; /* 100ms at 48kHz */
  ae_test_generate_impulse(ir, 4800, 0);

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* All energy is early, C50 should be very high */
  AE_ASSERT(metrics.c50 > 50.0f);
  AE_TEST_PASS();
}

void test_c50_late_only(void) {
  /* IR with only late energy */
  float ir[4800];
  memset(ir, 0, sizeof(ir));
  ir[3000] = 1.0f; /* 62.5ms - after 50ms threshold */

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* All energy is late, C50 should be very negative */
  AE_ASSERT(metrics.c50 < -50.0f);
  AE_TEST_PASS();
}

void test_c50_balanced(void) {
  /* IR with equal early and late energy */
  float ir[4800];
  memset(ir, 0, sizeof(ir));
  ir[0] = 1.0f;    /* Direct sound */
  ir[3000] = 1.0f; /* Late reflection */

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* Equal energy -> C50 ≈ 0 dB */
  AE_ASSERT_FLOAT_EQ(metrics.c50, 0.0f, 1.0f);
  AE_TEST_PASS();
}

void test_c80_greater_than_c50(void) {
  /* C80 should always be >= C50 for same IR */
  float ir[9600]; /* 200ms */
  ae_test_generate_impulse(ir, 9600, 0);
  /* Add decaying tail */
  for (size_t i = 1; i < 9600; i++) {
    ir[i] = expf(-0.001f * i) * 0.1f;
  }

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 9600, 48000, &metrics), AE_OK);

  AE_ASSERT(metrics.c80 >= metrics.c50);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: D50 (Definition)
 *============================================================================*/

void test_d50_impulse_only(void) {
  float ir[4800];
  ae_test_generate_impulse(ir, 4800, 0);

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* All energy is early, D50 should be 1.0 */
  AE_ASSERT_FLOAT_EQ(metrics.d50, 1.0f, 0.01f);
  AE_TEST_PASS();
}

void test_d50_late_only(void) {
  float ir[4800];
  memset(ir, 0, sizeof(ir));
  ir[3000] = 1.0f;

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* All energy is late, D50 should be 0.0 */
  AE_ASSERT_FLOAT_EQ(metrics.d50, 0.0f, 0.01f);
  AE_TEST_PASS();
}

void test_d50_c50_relationship(void) {
  /* D50 and C50 are related: C50 = 10*log10(D50/(1-D50)) */
  float ir[4800];
  ae_test_generate_impulse(ir, 4800, 0);
  ir[3000] = 0.5f; /* Some late energy */

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  float c50_from_d50 =
      10.0f * log10f(metrics.d50 / (1.0f - metrics.d50 + 1e-10f));

  AE_ASSERT_FLOAT_EQ(metrics.c50, c50_from_d50, 0.5f);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Center time (Ts)
 *============================================================================*/

void test_ts_impulse_at_zero(void) {
  float ir[4800];
  ae_test_generate_impulse(ir, 4800, 0);

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* Energy is at t=0, Ts should be 0 */
  AE_ASSERT_FLOAT_EQ(metrics.ts_ms, 0.0f, 1.0f);
  AE_TEST_PASS();
}

void test_ts_impulse_at_50ms(void) {
  float ir[4800];
  ae_test_generate_impulse(ir, 4800, 2400); /* 50ms */

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 4800, 48000, &metrics), AE_OK);

  /* Energy is at t=50ms, Ts should be 50 */
  AE_ASSERT_FLOAT_EQ(metrics.ts_ms, 50.0f, 1.0f);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: EDT
 *============================================================================*/

void test_edt_short_decay(void) {
  /* Generate exponentially decaying IR */
  float ir[48000]; /* 1 second */
  for (size_t i = 0; i < 48000; i++) {
    float t = (float)i / 48000.0f;
    ir[i] = expf(-t * 10.0f); /* Fast decay */
  }

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 48000, 48000, &metrics), AE_OK);

  /* Should be relatively short (< 1s) */
  AE_ASSERT(metrics.edt > 0.0f);
  AE_ASSERT(metrics.edt < 1.0f);
  AE_TEST_PASS();
}

void test_edt_long_decay(void) {
  /* Generate slowly decaying IR */
  float ir[48000];
  for (size_t i = 0; i < 48000; i++) {
    float t = (float)i / 48000.0f;
    ir[i] = expf(-t * 1.0f); /* Slow decay */
  }

  ae_room_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_room_metrics(ir, 48000, 48000, &metrics), AE_OK);

  /* Should be longer than short decay test */
  AE_ASSERT(metrics.edt > 0.5f);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Level calculations
 *============================================================================*/

void test_peak_db_full_scale(void) {
  float samples[100];
  for (int i = 0; i < 100; i++)
    samples[i] = 0.0f;
  samples[50] = 1.0f; /* Full scale peak */

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {
      .samples = samples, .frame_count = 100, .channels = 1, .interleaved = true};
  ae_loudness_t loudness;
  AE_ASSERT_EQ(ae_analyze_loudness(engine, &buf, AE_WEIGHTING_NONE, &loudness),
               AE_OK);
  float peak = loudness.peak_db;
  ae_destroy_engine(engine);

  AE_ASSERT_FLOAT_EQ(peak, 0.0f, 0.1f);
  AE_TEST_PASS();
}

void test_peak_db_half_scale(void) {
  float samples[100];
  for (int i = 0; i < 100; i++)
    samples[i] = 0.0f;
  samples[50] = 0.5f;

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {
      .samples = samples, .frame_count = 100, .channels = 1, .interleaved = true};
  ae_loudness_t loudness;
  AE_ASSERT_EQ(ae_analyze_loudness(engine, &buf, AE_WEIGHTING_NONE, &loudness),
               AE_OK);
  float peak = loudness.peak_db;
  ae_destroy_engine(engine);

  AE_ASSERT_FLOAT_EQ(peak, -6.02f, 0.1f);
  AE_TEST_PASS();
}

void test_rms_db_sine(void) {
  float samples[4800];
  ae_test_generate_sine(samples, 4800, 1000.0f, 48000.0f, 1.0f);

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {.samples = samples,
                           .frame_count = 4800,
                           .channels = 1,
                           .interleaved = true};
  ae_loudness_t loudness;
  AE_ASSERT_EQ(ae_analyze_loudness(engine, &buf, AE_WEIGHTING_NONE, &loudness),
               AE_OK);
  float rms = loudness.rms_db;
  ae_destroy_engine(engine);

  /* RMS of sine = peak / sqrt(2) = -3.01 dB */
  AE_ASSERT_FLOAT_EQ(rms, -3.01f, 0.1f);
  AE_TEST_PASS();
}

void test_rms_db_silence(void) {
  float samples[100];
  memset(samples, 0, sizeof(samples));

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {
      .samples = samples, .frame_count = 100, .channels = 1, .interleaved = true};
  ae_loudness_t loudness;
  AE_ASSERT_EQ(ae_analyze_loudness(engine, &buf, AE_WEIGHTING_NONE, &loudness),
               AE_OK);
  float rms = loudness.rms_db;
  ae_destroy_engine(engine);

  AE_ASSERT(rms <= -100.0f);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: IACC
 *============================================================================*/

void test_iacc_identical_signals(void) {
  float left[1000], right[1000];
  ae_test_generate_sine(left, 1000, 1000.0f, 48000.0f, 1.0f);
  memcpy(right, left, 1000 * sizeof(float));

  float samples[2000];
  for (int i = 0; i < 1000; i++) {
    samples[i * 2] = left[i];
    samples[i * 2 + 1] = right[i];
  }
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {
      .samples = samples, .frame_count = 1000, .channels = 2, .interleaved = true};
  ae_perceptual_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_perceptual_metrics(engine, &buf, &metrics), AE_OK);
  float iacc = metrics.iacc_early;
  ae_destroy_engine(engine);

  /* Identical signals -> IACC = 1.0 */
  AE_ASSERT_FLOAT_EQ(iacc, 1.0f, 0.05f);
  AE_TEST_PASS();
}

void test_iacc_uncorrelated_signals(void) {
  float left[1000], right[1000];
  srand(12345);
  ae_test_generate_noise(left, 1000, 1.0f);
  srand(67890);
  ae_test_generate_noise(right, 1000, 1.0f);

  float samples[2000];
  for (int i = 0; i < 1000; i++) {
    samples[i * 2] = left[i];
    samples[i * 2 + 1] = right[i];
  }
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {
      .samples = samples, .frame_count = 1000, .channels = 2, .interleaved = true};
  ae_perceptual_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_perceptual_metrics(engine, &buf, &metrics), AE_OK);
  float iacc = metrics.iacc_early;
  ae_destroy_engine(engine);

  /* Uncorrelated noise -> IACC should be low */
  AE_ASSERT(iacc < 0.3f);
  AE_TEST_PASS();
}

void test_iacc_inverted_signals(void) {
  float left[1000], right[1000];
  ae_test_generate_sine(left, 1000, 1000.0f, 48000.0f, 1.0f);
  for (int i = 0; i < 1000; i++) {
    right[i] = -left[i]; /* Inverted */
  }

  float samples[2000];
  for (int i = 0; i < 1000; i++) {
    samples[i * 2] = left[i];
    samples[i * 2 + 1] = right[i];
  }
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);
  ae_audio_buffer_t buf = {
      .samples = samples, .frame_count = 1000, .channels = 2, .interleaved = true};
  ae_perceptual_metrics_t metrics;
  AE_ASSERT_EQ(ae_compute_perceptual_metrics(engine, &buf, &metrics), AE_OK);
  float iacc = metrics.iacc_early;
  ae_destroy_engine(engine);

  /* Inverted signals have high correlation (negative) */
  AE_ASSERT_FLOAT_EQ(iacc, 1.0f, 0.05f);
  AE_TEST_PASS();
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
  printf("Acoustic Engine - Analysis Tests\n");

  AE_TEST_SUITE_BEGIN("Clarity Metrics (C50/C80)");
  AE_RUN_TEST(test_c50_impulse_only);
  AE_RUN_TEST(test_c50_late_only);
  AE_RUN_TEST(test_c50_balanced);
  AE_RUN_TEST(test_c80_greater_than_c50);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Definition (D50)");
  AE_RUN_TEST(test_d50_impulse_only);
  AE_RUN_TEST(test_d50_late_only);
  AE_RUN_TEST(test_d50_c50_relationship);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Center Time (Ts)");
  AE_RUN_TEST(test_ts_impulse_at_zero);
  AE_RUN_TEST(test_ts_impulse_at_50ms);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Early Decay Time (EDT)");
  AE_RUN_TEST(test_edt_short_decay);
  AE_RUN_TEST(test_edt_long_decay);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Level Calculations");
  AE_RUN_TEST(test_peak_db_full_scale);
  AE_RUN_TEST(test_peak_db_half_scale);
  AE_RUN_TEST(test_rms_db_sine);
  AE_RUN_TEST(test_rms_db_silence);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("IACC");
  AE_RUN_TEST(test_iacc_identical_signals);
  AE_RUN_TEST(test_iacc_uncorrelated_signals);
  AE_RUN_TEST(test_iacc_inverted_signals);
  AE_TEST_SUITE_END();

  return ae_test_report();
}
