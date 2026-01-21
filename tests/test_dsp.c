/**
 * @file test_dsp.c
 * @brief Unit tests for DSP primitives (Biquad, delays, etc.)
 */

#include "../include/acoustic_engine.h"
#include "ae_test.h"
#include <math.h>
#include <string.h>


/*============================================================================
 * Biquad filter implementation (for testing)
 *============================================================================*/

typedef struct {
  float b0, b1, b2;
  float a1, a2;
  float z1, z2;
} ae_biquad_t;

void ae_biquad_init(ae_biquad_t *bq) {
  bq->z1 = 0.0f;
  bq->z2 = 0.0f;
}

/* Low-pass filter design (normalized) */
void ae_biquad_lowpass(ae_biquad_t *bq, float freq, float q,
                       float sample_rate) {
  float w0 = 2.0f * 3.14159265f * freq / sample_rate;
  float alpha = sinf(w0) / (2.0f * q);
  float cos_w0 = cosf(w0);

  float a0 = 1.0f + alpha;
  bq->b0 = (1.0f - cos_w0) / 2.0f / a0;
  bq->b1 = (1.0f - cos_w0) / a0;
  bq->b2 = (1.0f - cos_w0) / 2.0f / a0;
  bq->a1 = -2.0f * cos_w0 / a0;
  bq->a2 = (1.0f - alpha) / a0;
}

/* High-pass filter design */
void ae_biquad_highpass(ae_biquad_t *bq, float freq, float q,
                        float sample_rate) {
  float w0 = 2.0f * 3.14159265f * freq / sample_rate;
  float alpha = sinf(w0) / (2.0f * q);
  float cos_w0 = cosf(w0);

  float a0 = 1.0f + alpha;
  bq->b0 = (1.0f + cos_w0) / 2.0f / a0;
  bq->b1 = -(1.0f + cos_w0) / a0;
  bq->b2 = (1.0f + cos_w0) / 2.0f / a0;
  bq->a1 = -2.0f * cos_w0 / a0;
  bq->a2 = (1.0f - alpha) / a0;
}

/* Process single sample (Direct Form II Transposed) */
float ae_biquad_process_sample(ae_biquad_t *bq, float in) {
  float out = bq->b0 * in + bq->z1;
  bq->z1 = bq->b1 * in - bq->a1 * out + bq->z2;
  bq->z2 = bq->b2 * in - bq->a2 * out;
  return out;
}

/* Process buffer */
void ae_biquad_process(ae_biquad_t *bq, const float *in, float *out, size_t n) {
  for (size_t i = 0; i < n; i++) {
    out[i] = ae_biquad_process_sample(bq, in[i]);
  }
}

/*============================================================================
 * Delay line implementation (for testing)
 *============================================================================*/

typedef struct {
  float *buffer;
  size_t buffer_size;
  size_t write_pos;
  size_t delay_samples;
} ae_delay_t;

ae_delay_t *ae_delay_create(size_t max_delay_samples) {
  ae_delay_t *delay = (ae_delay_t *)malloc(sizeof(ae_delay_t));
  if (!delay)
    return NULL;

  delay->buffer_size = max_delay_samples + 1;
  delay->buffer = (float *)calloc(delay->buffer_size, sizeof(float));
  if (!delay->buffer) {
    free(delay);
    return NULL;
  }
  delay->write_pos = 0;
  delay->delay_samples = max_delay_samples;
  return delay;
}

void ae_delay_destroy(ae_delay_t *delay) {
  if (delay) {
    free(delay->buffer);
    free(delay);
  }
}

void ae_delay_set_delay(ae_delay_t *delay, size_t samples) {
  if (samples < delay->buffer_size) {
    delay->delay_samples = samples;
  }
}

float ae_delay_process_sample(ae_delay_t *delay, float in) {
  /* Read from delayed position */
  size_t read_pos =
      (delay->write_pos + delay->buffer_size - delay->delay_samples) %
      delay->buffer_size;
  float out = delay->buffer[read_pos];

  /* Write new sample */
  delay->buffer[delay->write_pos] = in;
  delay->write_pos = (delay->write_pos + 1) % delay->buffer_size;

  return out;
}

/*============================================================================
 * Spectral analysis helpers (for testing)
 *============================================================================*/

/* Simple DFT for testing (not FFT) */
void ae_simple_dft(const float *in, float *mag, size_t n) {
  for (size_t k = 0; k < n / 2; k++) {
    float re = 0.0f, im = 0.0f;
    for (size_t t = 0; t < n; t++) {
      float angle = 2.0f * 3.14159265f * k * t / n;
      re += in[t] * cosf(angle);
      im -= in[t] * sinf(angle);
    }
    mag[k] = sqrtf(re * re + im * im) / n;
  }
}

/* Find peak frequency bin */
size_t ae_find_peak_bin(const float *mag, size_t n) {
  size_t peak = 0;
  float max_val = mag[0];
  for (size_t i = 1; i < n; i++) {
    if (mag[i] > max_val) {
      max_val = mag[i];
      peak = i;
    }
  }
  return peak;
}

/*============================================================================
 * Tests: Biquad filter
 *============================================================================*/

void test_biquad_passthrough(void) {
  /* Test unity gain at passband */
  ae_biquad_t bq;
  ae_biquad_lowpass(&bq, 10000.0f, 0.707f, 48000.0f);
  ae_biquad_init(&bq);

  /* 1kHz sine should pass through mostly unchanged */
  float in[256], out[256];
  ae_test_generate_sine(in, 256, 1000.0f, 48000.0f, 1.0f);
  ae_biquad_process(&bq, in, out, 256);

  /* Check RMS is close to original (allow for some filter settling) */
  float rms_in = ae_test_calculate_rms(in, 256);
  float rms_out = ae_test_calculate_rms(out + 64, 192); /* Skip transient */
  AE_ASSERT_FLOAT_EQ(rms_out, rms_in, 0.1f);
  AE_TEST_PASS();
}

void test_biquad_lowpass_attenuation(void) {
  ae_biquad_t bq;
  ae_biquad_lowpass(&bq, 1000.0f, 0.707f, 48000.0f);
  ae_biquad_init(&bq);

  /* 10kHz sine should be significantly attenuated */
  float in[256], out[256];
  ae_test_generate_sine(in, 256, 10000.0f, 48000.0f, 1.0f);
  ae_biquad_process(&bq, in, out, 256);

  float rms_in = ae_test_calculate_rms(in, 256);
  float rms_out = ae_test_calculate_rms(out + 64, 192);

  /* Should be attenuated by at least 20dB */
  AE_ASSERT(rms_out < rms_in * 0.1f);
  AE_TEST_PASS();
}

void test_biquad_highpass_attenuation(void) {
  ae_biquad_t bq;
  ae_biquad_highpass(&bq, 5000.0f, 0.707f, 48000.0f);
  ae_biquad_init(&bq);

  /* 100Hz sine should be significantly attenuated */
  float in[512], out[512];
  ae_test_generate_sine(in, 512, 100.0f, 48000.0f, 1.0f);
  ae_biquad_process(&bq, in, out, 512);

  float rms_in = ae_test_calculate_rms(in, 512);
  float rms_out = ae_test_calculate_rms(out + 128, 384);

  /* Should be attenuated by at least 30dB */
  AE_ASSERT(rms_out < rms_in * 0.03f);
  AE_TEST_PASS();
}

void test_biquad_stability(void) {
  ae_biquad_t bq;
  ae_biquad_lowpass(&bq, 5000.0f, 0.707f, 48000.0f);
  ae_biquad_init(&bq);

  /* Process many samples - should not blow up */
  float sample = 1.0f;
  for (int i = 0; i < 10000; i++) {
    sample = ae_biquad_process_sample(&bq, (i == 0) ? 1.0f : 0.0f);
  }

  AE_ASSERT(!isnan(sample));
  AE_ASSERT(!isinf(sample));
  AE_ASSERT(fabsf(sample) < 0.001f); /* Should have decayed */
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Delay line
 *============================================================================*/

void test_delay_create_destroy(void) {
  ae_delay_t *delay = ae_delay_create(1000);
  AE_ASSERT_NOT_NULL(delay);
  ae_delay_destroy(delay);
  AE_TEST_PASS();
}

void test_delay_exact_samples(void) {
  ae_delay_t *delay = ae_delay_create(100);
  AE_ASSERT_NOT_NULL(delay);
  ae_delay_set_delay(delay, 50);

  /* Send impulse */
  float out;
  for (int i = 0; i < 100; i++) {
    out = ae_delay_process_sample(delay, (i == 0) ? 1.0f : 0.0f);
    if (i == 50) {
      AE_ASSERT_FLOAT_EQ(out, 1.0f, 0.001f);
    } else if (i < 50) {
      AE_ASSERT_FLOAT_EQ(out, 0.0f, 0.001f);
    }
  }

  ae_delay_destroy(delay);
  AE_TEST_PASS();
}

void test_delay_change_delay(void) {
  ae_delay_t *delay = ae_delay_create(200);
  AE_ASSERT_NOT_NULL(delay);

  /* Start with 100 sample delay */
  ae_delay_set_delay(delay, 100);

  /* Fill buffer */
  for (int i = 0; i < 50; i++) {
    ae_delay_process_sample(delay, (float)i);
  }

  /* Change to 50 sample delay */
  ae_delay_set_delay(delay, 50);

  /* Next output should be from 50 samples ago */
  float out = ae_delay_process_sample(delay, 50.0f);
  AE_ASSERT_FLOAT_EQ(out, 0.0f, 0.001f); /* 50 samples back was 0 */

  ae_delay_destroy(delay);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Spectral analysis
 *============================================================================*/

void test_dft_dc(void) {
  float in[64];
  float mag[32];

  /* DC signal */
  for (int i = 0; i < 64; i++)
    in[i] = 1.0f;
  ae_simple_dft(in, mag, 64);

  /* Peak should be at bin 0 */
  AE_ASSERT_EQ(ae_find_peak_bin(mag, 32), 0);
  AE_TEST_PASS();
}

void test_dft_sine(void) {
  float in[64];
  float mag[32];

  /* Generate sine at bin 4 frequency */
  float freq = 4.0f * 48000.0f / 64.0f; /* 3000 Hz */
  ae_test_generate_sine(in, 64, freq, 48000.0f, 1.0f);
  ae_simple_dft(in, mag, 64);

  /* Peak should be at bin 4 */
  size_t peak = ae_find_peak_bin(mag, 32);
  AE_ASSERT(peak >= 3 && peak <= 5); /* Allow ±1 bin */
  AE_TEST_PASS();
}

void test_spectral_centroid_calculation(void) {
  float in[64];
  float mag[32];

  /* Low frequency sine */
  ae_test_generate_sine(in, 64, 500.0f, 48000.0f, 1.0f);
  ae_simple_dft(in, mag, 64);

  /* Calculate centroid */
  float sum_fm = 0.0f, sum_m = 0.0f;
  for (int k = 1; k < 32; k++) {
    float f = k * 48000.0f / 64.0f;
    sum_fm += f * mag[k] * mag[k];
    sum_m += mag[k] * mag[k];
  }
  float centroid = sum_m > 0 ? sum_fm / sum_m : 0.0f;

  /* Should be close to 500 Hz */
  AE_ASSERT_RANGE(centroid, 300.0f, 1200.0f);
  AE_TEST_PASS();
}

/*============================================================================
 * Tests: Buffer operations
 *============================================================================*/

void test_buffer_copy(void) {
  float src[256], dst[256];
  ae_test_generate_sine(src, 256, 1000.0f, 48000.0f, 1.0f);
  memcpy(dst, src, 256 * sizeof(float));

  for (int i = 0; i < 256; i++) {
    AE_ASSERT_FLOAT_EQ(dst[i], src[i], 0.0001f);
  }
  AE_TEST_PASS();
}

void test_buffer_scale(void) {
  float buf[256];
  ae_test_generate_sine(buf, 256, 1000.0f, 48000.0f, 1.0f);

  float scale = 0.5f;
  for (int i = 0; i < 256; i++) {
    buf[i] *= scale;
  }

  float rms = ae_test_calculate_rms(buf, 256);
  AE_ASSERT_FLOAT_EQ(rms, 0.5f / sqrtf(2.0f), 0.01f);
  AE_TEST_PASS();
}

void test_buffer_mix(void) {
  float a[256], b[256], out[256];
  ae_test_generate_sine(a, 256, 1000.0f, 48000.0f, 1.0f);
  ae_test_generate_sine(b, 256, 2000.0f, 48000.0f, 1.0f);

  for (int i = 0; i < 256; i++) {
    out[i] = 0.5f * a[i] + 0.5f * b[i];
  }

  /* Output should not clip */
  for (int i = 0; i < 256; i++) {
    AE_ASSERT(fabsf(out[i]) <= 1.0f);
  }
  AE_TEST_PASS();
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
  printf("Acoustic Engine - DSP Tests\n");

  AE_TEST_SUITE_BEGIN("Biquad Filter");
  AE_RUN_TEST(test_biquad_passthrough);
  AE_RUN_TEST(test_biquad_lowpass_attenuation);
  AE_RUN_TEST(test_biquad_highpass_attenuation);
  AE_RUN_TEST(test_biquad_stability);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Delay Line");
  AE_RUN_TEST(test_delay_create_destroy);
  AE_RUN_TEST(test_delay_exact_samples);
  AE_RUN_TEST(test_delay_change_delay);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Spectral Analysis");
  AE_RUN_TEST(test_dft_dc);
  AE_RUN_TEST(test_dft_sine);
  AE_RUN_TEST(test_spectral_centroid_calculation);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Buffer Operations");
  AE_RUN_TEST(test_buffer_copy);
  AE_RUN_TEST(test_buffer_scale);
  AE_RUN_TEST(test_buffer_mix);
  AE_TEST_SUITE_END();

  return ae_test_report();
}
