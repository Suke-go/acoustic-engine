/**
 * @file ae_test.h
 * @brief Simple test framework for Acoustic Engine
 */

#ifndef AE_TEST_H
#define AE_TEST_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Test result counters */
static int ae_test_pass = 0;
static int ae_test_fail = 0;
static int ae_test_skip = 0;

/* Current test name */
static const char *ae_current_test = NULL;

/* Color codes for terminal */
#ifdef _WIN32
#define AE_COLOR_GREEN ""
#define AE_COLOR_RED ""
#define AE_COLOR_YELLOW ""
#define AE_COLOR_RESET ""
#else
#define AE_COLOR_GREEN "\033[32m"
#define AE_COLOR_RED "\033[31m"
#define AE_COLOR_YELLOW "\033[33m"
#define AE_COLOR_RESET "\033[0m"
#endif

/*============================================================================
 * Test macros
 *============================================================================*/

#define AE_TEST_BEGIN(name)                                                    \
  do {                                                                         \
    ae_current_test = name;                                                    \
    printf("  [....] %s", name);                                               \
    fflush(stdout);                                                            \
  } while (0)

#define AE_TEST_PASS()                                                         \
  do {                                                                         \
    ae_test_pass++;                                                            \
    printf("\r  [%sPASS%s] %s\n", AE_COLOR_GREEN, AE_COLOR_RESET,              \
           ae_current_test);                                                   \
  } while (0)

#define AE_TEST_FAIL(msg)                                                      \
  do {                                                                         \
    ae_test_fail++;                                                            \
    printf("\r  [%sFAIL%s] %s: %s\n", AE_COLOR_RED, AE_COLOR_RESET,            \
           ae_current_test, msg);                                              \
  } while (0)

#define AE_TEST_SKIP(reason)                                                   \
  do {                                                                         \
    ae_test_skip++;                                                            \
    printf("\r  [%sSKIP%s] %s: %s\n", AE_COLOR_YELLOW, AE_COLOR_RESET,         \
           ae_current_test, reason);                                           \
  } while (0)

/*============================================================================
 * Assertion macros
 *============================================================================*/

#define AE_ASSERT(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      AE_TEST_FAIL("Assertion failed: " #cond);                                \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define AE_ASSERT_EQ(a, b)                                                     \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      char msg[256];                                                           \
      snprintf(msg, sizeof(msg), "Expected %d, got %d", (int)(b), (int)(a));   \
      AE_TEST_FAIL(msg);                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define AE_ASSERT_FLOAT_EQ(a, b, eps)                                          \
  do {                                                                         \
    float _a = (a), _b = (b), _eps = (eps);                                    \
    if (fabsf(_a - _b) > _eps) {                                               \
      char msg[256];                                                           \
      snprintf(msg, sizeof(msg), "Expected %.6f, got %.6f (eps=%.6f)", _b, _a, \
               _eps);                                                          \
      AE_TEST_FAIL(msg);                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define AE_ASSERT_RANGE(val, min, max)                                         \
  do {                                                                         \
    float _v = (val), _min = (min), _max = (max);                              \
    if (_v < _min || _v > _max) {                                              \
      char msg[256];                                                           \
      snprintf(msg, sizeof(msg), "Value %.6f out of range [%.6f, %.6f]", _v,   \
               _min, _max);                                                    \
      AE_TEST_FAIL(msg);                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define AE_ASSERT_NOT_NULL(ptr)                                                \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      AE_TEST_FAIL("Unexpected NULL pointer: " #ptr);                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define AE_ASSERT_NULL(ptr)                                                    \
  do {                                                                         \
    if ((ptr) != NULL) {                                                       \
      AE_TEST_FAIL("Expected NULL pointer: " #ptr);                            \
      return;                                                                  \
    }                                                                          \
  } while (0)

/*============================================================================
 * Test suite macros
 *============================================================================*/

#define AE_TEST_SUITE_BEGIN(name)                                              \
  do {                                                                         \
    printf("\n=== %s ===\n", name);                                            \
  } while (0)

#define AE_TEST_SUITE_END()                                                    \
  do {                                                                         \
    printf("\n");                                                              \
  } while (0)

#define AE_RUN_TEST(fn)                                                        \
  do {                                                                         \
    AE_TEST_BEGIN(#fn);                                                        \
    fn();                                                                      \
    if (ae_test_fail == 0 || ae_test_pass > 0) {                               \
      /* Test passed or fail was already reported */                           \
    }                                                                          \
  } while (0)

/*============================================================================
 * Test report
 *============================================================================*/

static int ae_test_report(void) {
  printf("\n========================================\n");
  printf("Test Results:\n");
  printf("  %sPassed: %d%s\n", AE_COLOR_GREEN, ae_test_pass, AE_COLOR_RESET);
  printf("  %sFailed: %d%s\n", ae_test_fail > 0 ? AE_COLOR_RED : "",
         ae_test_fail, AE_COLOR_RESET);
  printf("  %sSkipped: %d%s\n", ae_test_skip > 0 ? AE_COLOR_YELLOW : "",
         ae_test_skip, AE_COLOR_RESET);
  printf("========================================\n");

  return ae_test_fail > 0 ? 1 : 0;
}

/*============================================================================
 * Utility functions for testing
 *============================================================================*/

/* Generate sine wave for testing */
static void ae_test_generate_sine(float *buffer, size_t length, float freq,
                                  float sample_rate, float amplitude) {
  for (size_t i = 0; i < length; i++) {
    buffer[i] =
        amplitude * sinf(2.0f * 3.14159265f * freq * (float)i / sample_rate);
  }
}

/* Generate impulse for testing */
static void ae_test_generate_impulse(float *buffer, size_t length,
                                     size_t impulse_pos) {
  memset(buffer, 0, length * sizeof(float));
  if (impulse_pos < length) {
    buffer[impulse_pos] = 1.0f;
  }
}

/* Generate white noise for testing */
static void ae_test_generate_noise(float *buffer, size_t length,
                                   float amplitude) {
  for (size_t i = 0; i < length; i++) {
    buffer[i] = amplitude * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
  }
}

/* Calculate RMS of buffer */
static float ae_test_calculate_rms(const float *buffer, size_t length) {
  float sum = 0.0f;
  for (size_t i = 0; i < length; i++) {
    sum += buffer[i] * buffer[i];
  }
  return sqrtf(sum / (float)length);
}

/* Check if buffer is silent */
static bool ae_test_is_silent(const float *buffer, size_t length,
                              float threshold) {
  for (size_t i = 0; i < length; i++) {
    if (fabsf(buffer[i]) > threshold) {
      return false;
    }
  }
  return true;
}

#endif /* AE_TEST_H */
