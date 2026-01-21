/**
 * @file test_math.c
 * @brief Unit tests for mathematical utility functions
 */

#include "../include/acoustic_engine.h"
#include "ae_test.h"
#include <math.h>


/*============================================================================
 * Tests: dB conversion
 *============================================================================*/

void test_to_db_unity(void) {
  float result = ae_to_db(1.0f);
  AE_ASSERT_FLOAT_EQ(result, 0.0f, 0.001f);
  AE_TEST_PASS();
}

void test_to_db_half(void) {
  float result = ae_to_db(0.5f);
  AE_ASSERT_FLOAT_EQ(result, -6.02f, 0.1f); /* -6.02 dB */
  AE_TEST_PASS();
}

void test_to_db_zero(void) {
  float result = ae_to_db(0.0f);
  AE_ASSERT(result <= -120.0f);
  AE_TEST_PASS();
}

void test_to_db_negative(void) {
  float result = ae_to_db(-1.0f);
  AE_ASSERT(result <= -120.0f);
  AE_TEST_PASS();
}

void test_from_db_unity(void) {
  float result = ae_from_db(0.0f);
  AE_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f);
  AE_TEST_PASS();
}

void test_from_db_minus6(void) {
  float result = ae_from_db(-6.02f);
  AE_ASSERT_FLOAT_EQ(result, 0.5f, 0.01f);
  AE_TEST_PASS();
}

void test_db_roundtrip(void) {
  float values[] = {0.001f, 0.1f, 0.5f, 1.0f, 2.0f};
  for (int i = 0; i < 5; i++) {
    float db = ae_to_db(values[i]);
    float back = ae_from_db(db);
    AE_ASSERT_FLOAT_EQ(back, values[i], 0.001f);
  }
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Phon/Sone conversion
 *============================================================================*/

void test_phon_to_sone_40phon(void) {
  float result = ae_phon_to_sone(40.0f);
  AE_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f); /* 40 phon = 1 sone */
  AE_TEST_PASS();
}

void test_phon_to_sone_50phon(void) {
  float result = ae_phon_to_sone(50.0f);
  AE_ASSERT_FLOAT_EQ(result, 2.0f, 0.01f); /* 50 phon = 2 sone */
  AE_TEST_PASS();
}

void test_phon_to_sone_60phon(void) {
  float result = ae_phon_to_sone(60.0f);
  AE_ASSERT_FLOAT_EQ(result, 4.0f, 0.01f); /* 60 phon = 4 sone */
  AE_TEST_PASS();
}

void test_sone_to_phon_1sone(void) {
  float result = ae_sone_to_phon(1.0f);
  AE_ASSERT_FLOAT_EQ(result, 40.0f, 0.1f);
  AE_TEST_PASS();
}

void test_phon_sone_roundtrip(void) {
  float phons[] = {20.0f, 40.0f, 60.0f, 80.0f};
  for (int i = 0; i < 4; i++) {
    float sone = ae_phon_to_sone(phons[i]);
    float back = ae_sone_to_phon(sone);
    AE_ASSERT_FLOAT_EQ(back, phons[i], 0.5f);
  }
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Bark conversion
 *============================================================================*/

void test_hz_to_bark_1000hz(void) {
  float result = ae_hz_to_bark(1000.0f);
  AE_ASSERT_RANGE(result, 8.0f, 10.0f); /* ~9 Bark */
  AE_TEST_PASS();
}

void test_hz_to_bark_100hz(void) {
  float result = ae_hz_to_bark(100.0f);
  AE_ASSERT_RANGE(result, 0.5f, 2.0f); /* ~1 Bark */
  AE_TEST_PASS();
}

void test_hz_to_bark_10000hz(void) {
  float result = ae_hz_to_bark(10000.0f);
  AE_ASSERT_RANGE(result, 20.0f, 23.0f); /* ~21 Bark */
  AE_TEST_PASS();
}

void test_bark_roundtrip(void) {
  float freqs[] = {100.0f, 500.0f, 1000.0f, 4000.0f, 10000.0f};
  for (int i = 0; i < 5; i++) {
    float bark = ae_hz_to_bark(freqs[i]);
    float back = ae_bark_to_hz(bark);
    AE_ASSERT_FLOAT_EQ(back, freqs[i], freqs[i] * 0.1f); /* 10% tolerance */
  }
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Parameter validation
 *============================================================================*/

void test_validate_params_valid(void) {
  ae_main_params_t params = {.distance = 10.0f,
                             .room_size = 0.5f,
                             .brightness = 0.0f,
                             .width = 1.0f,
                             .dry_wet = 0.5f,
                             .intensity = 0.8f};
  AE_ASSERT_EQ(ae_validate_params(&params), AE_OK);
  AE_TEST_PASS();
}

void test_validate_params_null(void) {
  AE_ASSERT_EQ(ae_validate_params(NULL), AE_ERROR_INVALID_PARAM);
  AE_TEST_PASS();
}

void test_validate_params_distance_zero(void) {
  ae_main_params_t params = {.distance = 0.0f, /* Invalid */
                             .room_size = 0.5f,
                             .brightness = 0.0f,
                             .width = 1.0f,
                             .dry_wet = 0.5f,
                             .intensity = 0.8f};
  AE_ASSERT_EQ(ae_validate_params(&params), AE_ERROR_INVALID_PARAM);
  AE_TEST_PASS();
}

void test_validate_params_room_size_out_of_range(void) {
  ae_main_params_t params = {.distance = 10.0f,
                             .room_size = 1.5f, /* Invalid: > 1.0 */
                             .brightness = 0.0f,
                             .width = 1.0f,
                             .dry_wet = 0.5f,
                             .intensity = 0.8f};
  AE_ASSERT_EQ(ae_validate_params(&params), AE_ERROR_INVALID_PARAM);
  AE_TEST_PASS();
}

void test_validate_params_brightness_out_of_range(void) {
  ae_main_params_t params = {.distance = 10.0f,
                             .room_size = 0.5f,
                             .brightness = 2.0f, /* Invalid: > 1.0 */
                             .width = 1.0f,
                             .dry_wet = 0.5f,
                             .intensity = 0.8f};
  AE_ASSERT_EQ(ae_validate_params(&params), AE_ERROR_INVALID_PARAM);
  AE_TEST_PASS();
}

void test_validate_params_nan(void) {
  ae_main_params_t params = {.distance = NAN, /* Invalid */
                             .room_size = 0.5f,
                             .brightness = 0.0f,
                             .width = 1.0f,
                             .dry_wet = 0.5f,
                             .intensity = 0.8f};
  AE_ASSERT_EQ(ae_validate_params(&params), AE_ERROR_INVALID_PARAM);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Safe log
 *============================================================================*/

void test_safe_log10_positive(void) {
  float result = ae_safe_log10(100.0f);
  AE_ASSERT_FLOAT_EQ(result, 2.0f, 0.001f);
  AE_TEST_PASS();
}

void test_safe_log10_zero(void) {
  float result = ae_safe_log10(0.0f);
  AE_ASSERT(result <= -99.0f); /* Very negative */
  AE_ASSERT(!isnan(result));
  AE_ASSERT(!isinf(result));
  AE_TEST_PASS();
}

void test_safe_log10_negative(void) {
  float result = ae_safe_log10(-1.0f);
  AE_ASSERT(result <= -99.0f);
  AE_ASSERT(!isnan(result));
  AE_TEST_PASS();
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
  printf("Acoustic Engine - Math Tests\n");

  AE_TEST_SUITE_BEGIN("dB Conversion");
  AE_RUN_TEST(test_to_db_unity);
  AE_RUN_TEST(test_to_db_half);
  AE_RUN_TEST(test_to_db_zero);
  AE_RUN_TEST(test_to_db_negative);
  AE_RUN_TEST(test_from_db_unity);
  AE_RUN_TEST(test_from_db_minus6);
  AE_RUN_TEST(test_db_roundtrip);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Phon/Sone Conversion");
  AE_RUN_TEST(test_phon_to_sone_40phon);
  AE_RUN_TEST(test_phon_to_sone_50phon);
  AE_RUN_TEST(test_phon_to_sone_60phon);
  AE_RUN_TEST(test_sone_to_phon_1sone);
  AE_RUN_TEST(test_phon_sone_roundtrip);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Bark Conversion");
  AE_RUN_TEST(test_hz_to_bark_1000hz);
  AE_RUN_TEST(test_hz_to_bark_100hz);
  AE_RUN_TEST(test_hz_to_bark_10000hz);
  AE_RUN_TEST(test_bark_roundtrip);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Parameter Validation");
  AE_RUN_TEST(test_validate_params_valid);
  AE_RUN_TEST(test_validate_params_null);
  AE_RUN_TEST(test_validate_params_distance_zero);
  AE_RUN_TEST(test_validate_params_room_size_out_of_range);
  AE_RUN_TEST(test_validate_params_brightness_out_of_range);
  AE_RUN_TEST(test_validate_params_nan);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Safe Log");
  AE_RUN_TEST(test_safe_log10_positive);
  AE_RUN_TEST(test_safe_log10_zero);
  AE_RUN_TEST(test_safe_log10_negative);
  AE_TEST_SUITE_END();

  return ae_test_report();
}
