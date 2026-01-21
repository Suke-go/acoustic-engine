/**
 * @file test_simd.c
 * @brief SIMD accuracy verification tests
 */

#include "acoustic_engine.h"
#include "ae_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SIZE 256
#define TOLERANCE 1e-5f

/*============================================================================
 * Scalar Reference Implementations
 *============================================================================*/

static void scalar_add(float *dst, const float *a, const float *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] = a[i] + b[i];
  }
}

static void scalar_mul(float *dst, const float *a, const float *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] = a[i] * b[i];
  }
}

static void scalar_scale(float *dst, const float *src, float scale, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] = src[i] * scale;
  }
}

static void scalar_mac(float *dst, const float *a, const float *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] += a[i] * b[i];
  }
}

static int compare_buffers(const float *a, const float *b, size_t n,
                           float tolerance) {
  for (size_t i = 0; i < n; ++i) {
    if (fabsf(a[i] - b[i]) > tolerance) {
      return 0; /* Mismatch */
    }
  }
  return 1; /* Match */
}

/*============================================================================
 * Tests
 *============================================================================*/

void test_simd_add_accuracy(void) {
  float a[TEST_SIZE], b[TEST_SIZE];
  float simd_result[TEST_SIZE], scalar_result[TEST_SIZE];

  /* Initialize with random-ish data */
  for (int i = 0; i < TEST_SIZE; ++i) {
    a[i] = (float)(i * 0.001) - 0.5f;
    b[i] = (float)(i * 0.002) + 0.3f;
  }

  /* Compute both */
  ae_simd_add(simd_result, a, b, TEST_SIZE);
  scalar_add(scalar_result, a, b, TEST_SIZE);

  /* Compare */
  AE_ASSERT(compare_buffers(simd_result, scalar_result, TEST_SIZE, TOLERANCE));
  AE_TEST_PASS();
}

void test_simd_mul_accuracy(void) {
  float a[TEST_SIZE], b[TEST_SIZE];
  float simd_result[TEST_SIZE], scalar_result[TEST_SIZE];

  for (int i = 0; i < TEST_SIZE; ++i) {
    a[i] = (float)(i * 0.01) - 1.0f;
    b[i] = (float)((TEST_SIZE - i) * 0.01);
  }

  ae_simd_mul(simd_result, a, b, TEST_SIZE);
  scalar_mul(scalar_result, a, b, TEST_SIZE);

  AE_ASSERT(compare_buffers(simd_result, scalar_result, TEST_SIZE, TOLERANCE));
  AE_TEST_PASS();
}

void test_simd_scale_accuracy(void) {
  float src[TEST_SIZE];
  float simd_result[TEST_SIZE], scalar_result[TEST_SIZE];
  float scale = 2.5f;

  for (int i = 0; i < TEST_SIZE; ++i) {
    src[i] = sinf((float)i * 0.1f);
  }

  ae_simd_scale(simd_result, src, scale, TEST_SIZE);
  scalar_scale(scalar_result, src, scale, TEST_SIZE);

  AE_ASSERT(compare_buffers(simd_result, scalar_result, TEST_SIZE, TOLERANCE));
  AE_TEST_PASS();
}

void test_simd_mac_accuracy(void) {
  float a[TEST_SIZE], b[TEST_SIZE];
  float simd_result[TEST_SIZE], scalar_result[TEST_SIZE];

  /* Initialize destination with non-zero values */
  for (int i = 0; i < TEST_SIZE; ++i) {
    a[i] = (float)(i % 17) * 0.1f;
    b[i] = (float)(i % 13) * 0.05f;
    simd_result[i] = (float)(i % 7) * 0.01f;
    scalar_result[i] = simd_result[i]; /* Same initial state */
  }

  ae_simd_mac(simd_result, a, b, TEST_SIZE);
  scalar_mac(scalar_result, a, b, TEST_SIZE);

  AE_ASSERT(compare_buffers(simd_result, scalar_result, TEST_SIZE, TOLERANCE));
  AE_TEST_PASS();
}

void test_simd_unaligned_sizes(void) {
  /* Test with sizes that aren't multiples of SIMD width */
  int sizes[] = {1, 3, 7, 15, 17, 31, 33, 63, 65, 127, 129};
  int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

  float *a = (float *)malloc(256 * sizeof(float));
  float *b = (float *)malloc(256 * sizeof(float));
  float *simd_result = (float *)malloc(256 * sizeof(float));
  float *scalar_result = (float *)malloc(256 * sizeof(float));

  AE_ASSERT_NOT_NULL(a);
  AE_ASSERT_NOT_NULL(b);
  AE_ASSERT_NOT_NULL(simd_result);
  AE_ASSERT_NOT_NULL(scalar_result);

  for (int i = 0; i < 256; ++i) {
    a[i] = (float)i * 0.01f;
    b[i] = (float)(255 - i) * 0.01f;
  }

  for (int s = 0; s < num_sizes; ++s) {
    size_t n = (size_t)sizes[s];

    ae_simd_add(simd_result, a, b, n);
    scalar_add(scalar_result, a, b, n);

    AE_ASSERT(compare_buffers(simd_result, scalar_result, n, TOLERANCE));
  }

  free(a);
  free(b);
  free(simd_result);
  free(scalar_result);

  AE_TEST_PASS();
}

void test_simd_edge_cases(void) {
  float a[16] = {0};
  float b[16] = {0};
  float result[16] = {0};

  /* Test with zeros */
  ae_simd_add(result, a, b, 16);
  for (int i = 0; i < 16; ++i) {
    AE_ASSERT_FLOAT_EQ(result[i], 0.0f, 1e-10f);
  }

  /* Test with large values */
  for (int i = 0; i < 16; ++i) {
    a[i] = 1e30f;
    b[i] = 1e30f;
  }
  ae_simd_add(result, a, b, 16);
  AE_ASSERT(result[0] == 2e30f);

  /* Test with small values */
  for (int i = 0; i < 16; ++i) {
    a[i] = 1e-30f;
    b[i] = 1e-30f;
  }
  ae_simd_add(result, a, b, 16);
  AE_ASSERT(result[0] > 1e-30f);

  AE_TEST_PASS();
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
  printf("Acoustic Engine - SIMD Accuracy Tests\n");

  AE_TEST_SUITE_BEGIN("SIMD vs Scalar Accuracy");
  AE_RUN_TEST(test_simd_add_accuracy);
  AE_RUN_TEST(test_simd_mul_accuracy);
  AE_RUN_TEST(test_simd_scale_accuracy);
  AE_RUN_TEST(test_simd_mac_accuracy);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("SIMD Edge Cases");
  AE_RUN_TEST(test_simd_unaligned_sizes);
  AE_RUN_TEST(test_simd_edge_cases);
  AE_TEST_SUITE_END();

  return ae_test_report();
}
