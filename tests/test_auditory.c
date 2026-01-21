/**
 * @file test_auditory.c
 * @brief Unit tests for auditory modeling features
 */

#include "../include/acoustic_engine.h"
#include "ae_test.h"
#include <math.h>
#include <string.h>

static size_t ae_find_peak_index(const float *data, size_t n) {
  size_t peak = 0;
  float max_val = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    float v = fabsf(data[i]);
    if (v > max_val) {
      max_val = v;
      peak = i;
    }
  }
  return peak;
}

static void ae_generate_am(float *buffer, size_t length, float carrier_hz,
                           float mod_hz, float sample_rate, float depth) {
  for (size_t i = 0; i < length; ++i) {
    float t = (float)i / sample_rate;
    float env = 1.0f + depth * sinf(2.0f * 3.14159265f * mod_hz * t);
    buffer[i] = env * sinf(2.0f * 3.14159265f * carrier_hz * t);
  }
}

void test_gammatone_impulse_response(void) {
  ae_gammatone_config_t config = {0};
  config.n_channels = 8;
  config.f_low = 80.0f;
  config.f_high = 8000.0f;
  config.filter_order = 4;
  config.sample_rate = 48000;

  ae_gammatone_t *gt = ae_gammatone_create(&config);
  AE_ASSERT_NOT_NULL(gt);

  float input[512];
  memset(input, 0, sizeof(input));
  input[0] = 1.0f;

  float *outputs[8];
  float *storage = (float *)calloc(config.n_channels * 512, sizeof(float));
  AE_ASSERT_NOT_NULL(storage);
  for (uint32_t ch = 0; ch < config.n_channels; ++ch) {
    outputs[ch] = storage + ch * 512;
  }

  AE_ASSERT_EQ(ae_gammatone_process(gt, input, 512, outputs), AE_OK);

  float centers[8];
  size_t n_centers = 0;
  AE_ASSERT_EQ(ae_gammatone_get_center_frequencies(gt, centers, &n_centers),
               AE_OK);
  AE_ASSERT_EQ((int)n_centers, 8);

  size_t prev_peak = 0;
  for (uint32_t ch = 0; ch < config.n_channels; ++ch) {
    float energy = 0.0f;
    for (size_t i = 0; i < 512; ++i)
      energy += fabsf(outputs[ch][i]);
    AE_ASSERT(energy > 0.0f);

    size_t peak = ae_find_peak_index(outputs[ch], 512);
    if (ch > 0 && centers[ch] < centers[ch - 1]) {
      AE_ASSERT(peak >= prev_peak);
    }
    prev_peak = peak;
  }

  free(storage);
  ae_gammatone_destroy(gt);
  AE_TEST_PASS();
}

void test_ihc_envelope_response(void) {
  float input[128];
  for (size_t i = 0; i < 128; ++i)
    input[i] = 1.0f;
  float output[128];

  ae_ihc_config_t config = {0};
  config.compression_exponent = 0.3f;
  config.lpf_cutoff_hz = 2000.0f;

  AE_ASSERT_EQ(ae_ihc_envelope(input, 128, &config, output), AE_OK);
  AE_ASSERT(output[0] >= 0.0f);
  AE_ASSERT(output[127] > output[0]);
  AE_TEST_PASS();
}

void test_adaptloop_forward_masking(void) {
  ae_adaptloop_config_t config = {0};
  config.n_stages = 5;
  config.time_constants[0] = 5.0f;
  config.time_constants[1] = 50.0f;
  config.time_constants[2] = 129.0f;
  config.time_constants[3] = 253.0f;
  config.time_constants[4] = 500.0f;
  config.min_output = 1e-5f;
  config.sample_rate = 48000;

  ae_adaptloop_t *al = ae_adaptloop_create(&config);
  AE_ASSERT_NOT_NULL(al);

  float input[60000];
  float output[60000];
  for (size_t i = 0; i < 60000; ++i)
    input[i] = (i < 5000) ? 1.0f : 0.0f;

  AE_ASSERT_EQ(ae_adaptloop_process(al, input, 60000, output), AE_OK);
  AE_ASSERT(output[4999] > output[50000]);
  AE_ASSERT(output[4999] > config.min_output);

  ae_adaptloop_destroy(al);
  AE_TEST_PASS();
}

void test_zwicker_loudness_consistency(void) {
  float samples[2048];
  ae_test_generate_sine(samples, 2048, 1000.0f, 48000.0f, 0.5f);
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  ae_audio_buffer_t buf = {.samples = samples,
                           .frame_count = 2048,
                           .channels = 1,
                           .interleaved = true};
  ae_zwicker_loudness_t loud;
  AE_ASSERT_EQ(ae_analyze_zwicker_loudness(engine, &buf,
                                           AE_LOUDNESS_METHOD_ISO_532_1, &loud),
               AE_OK);

  float sum = 0.0f;
  float max_val = 0.0f;
  uint8_t max_band = 0;
  for (int i = 0; i < AE_NUM_BARK_BANDS; ++i) {
    sum += loud.specific_loudness[i];
    if (loud.specific_loudness[i] > max_val) {
      max_val = loud.specific_loudness[i];
      max_band = (uint8_t)i;
    }
  }

  AE_ASSERT_FLOAT_EQ(sum, loud.total_loudness_sone, 1e-3f);
  AE_ASSERT_EQ(loud.peak_bark_band, max_band);
  AE_ASSERT_FLOAT_EQ(loud.peak_loudness_sone, max_val, 1e-3f);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_sharpness_frequency_order(void) {
  float low[2048];
  float high[2048];
  ae_test_generate_sine(low, 2048, 1000.0f, 48000.0f, 0.5f);
  ae_test_generate_sine(high, 2048, 8000.0f, 48000.0f, 0.5f);

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  ae_audio_buffer_t buf_low = {
      .samples = low, .frame_count = 2048, .channels = 1, .interleaved = true};
  ae_audio_buffer_t buf_high = {
      .samples = high, .frame_count = 2048, .channels = 1, .interleaved = true};

  float sharp_low = 0.0f;
  float sharp_high = 0.0f;
  AE_ASSERT_EQ(ae_compute_sharpness(engine, &buf_low, AE_SHARPNESS_DIN_45692,
                                    &sharp_low),
               AE_OK);
  AE_ASSERT_EQ(ae_compute_sharpness(engine, &buf_high, AE_SHARPNESS_DIN_45692,
                                    &sharp_high),
               AE_OK);

  AE_ASSERT(sharp_high > sharp_low);
  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_roughness_am_tone(void) {
  float plain[4096];
  float am[4096];
  ae_test_generate_sine(plain, 4096, 1000.0f, 48000.0f, 0.5f);
  ae_generate_am(am, 4096, 1000.0f, 70.0f, 48000.0f, 0.8f);

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  ae_audio_buffer_t buf_plain = {.samples = plain,
                                 .frame_count = 4096,
                                 .channels = 1,
                                 .interleaved = true};
  ae_audio_buffer_t buf_am = {
      .samples = am, .frame_count = 4096, .channels = 1, .interleaved = true};

  float rough_plain = 0.0f;
  float rough_am = 0.0f;
  AE_ASSERT_EQ(ae_compute_roughness(engine, &buf_plain, &rough_plain), AE_OK);
  AE_ASSERT_EQ(ae_compute_roughness(engine, &buf_am, &rough_am), AE_OK);

  AE_ASSERT(rough_am > rough_plain);
  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_fluctuation_am_tone(void) {
  float plain[24000];
  float am[24000];
  ae_test_generate_sine(plain, 24000, 1000.0f, 48000.0f, 0.5f);
  ae_generate_am(am, 24000, 1000.0f, 4.0f, 48000.0f, 0.8f);

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  ae_audio_buffer_t buf_plain = {.samples = plain,
                                 .frame_count = 24000,
                                 .channels = 1,
                                 .interleaved = true};
  ae_audio_buffer_t buf_am = {
      .samples = am, .frame_count = 24000, .channels = 1, .interleaved = true};

  float fluct_plain = 0.0f;
  float fluct_am = 0.0f;
  AE_ASSERT_EQ(
      ae_compute_fluctuation_strength(engine, &buf_plain, &fluct_plain), AE_OK);
  AE_ASSERT_EQ(ae_compute_fluctuation_strength(engine, &buf_am, &fluct_am),
               AE_OK);

  AE_ASSERT(fluct_am > fluct_plain);
  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_roughness_over_time(void) {
  float am[4096];
  ae_generate_am(am, 4096, 1000.0f, 70.0f, 48000.0f, 0.8f);

  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  ae_audio_buffer_t buf = {
      .samples = am, .frame_count = 4096, .channels = 1, .interleaved = true};

  float roughness[64];
  size_t n_frames = 64;
  AE_ASSERT_EQ(
      ae_compute_roughness_over_time(engine, &buf, 10.0f, roughness, &n_frames),
      AE_OK);
  AE_ASSERT(n_frames > 0);
  AE_ASSERT(roughness[0] >= 0.0f);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_drnl_compression(void) {
  ae_drnl_config_t config = {0};
  config.n_channels = 4;
  config.f_low = 500.0f;
  config.f_high = 4000.0f;
  config.compression_exp = 0.25f;
  config.lin_gain = 1.0f;
  config.nlin_a = 1.0f;
  config.nlin_b = 1.0f;
  config.sample_rate = 48000;

  ae_drnl_t *drnl = ae_drnl_create(&config);
  AE_ASSERT_NOT_NULL(drnl);

  /* Generate test signal */
  float input[1024];
  ae_test_generate_sine(input, 1024, 1000.0f, 48000.0f, 0.5f);

  /* Allocate output buffers */
  float *outputs[4];
  float *storage = (float *)calloc(4 * 1024, sizeof(float));
  AE_ASSERT_NOT_NULL(storage);
  for (int ch = 0; ch < 4; ++ch) {
    outputs[ch] = storage + ch * 1024;
  }

  AE_ASSERT_EQ(ae_drnl_process(drnl, input, 1024, outputs), AE_OK);

  /* Verify output has content */
  float energy = 0.0f;
  for (size_t i = 0; i < 1024; ++i) {
    energy += outputs[1][i] * outputs[1][i]; /* Channel near 1kHz */
  }
  AE_ASSERT(energy > 0.0f);

  /* Get center frequencies */
  float centers[4];
  size_t n_ch = 0;
  AE_ASSERT_EQ(ae_drnl_get_center_frequencies(drnl, centers, &n_ch), AE_OK);
  AE_ASSERT_EQ((int)n_ch, 4);

  free(storage);
  ae_drnl_destroy(drnl);
  AE_TEST_PASS();
}

void test_modfb_modulation_detection(void) {
  ae_modfb_config_t config = {0};
  config.n_channels = 10;
  config.f_low = 0.5f;
  config.f_high = 256.0f;
  config.sample_rate = 1000; /* Envelope rate (downsampled) */

  ae_modfb_t *mfb = ae_modfb_create(&config);
  AE_ASSERT_NOT_NULL(mfb);

  /* Generate 70Hz modulated envelope (roughness range) */
  float input[2000];
  for (size_t i = 0; i < 2000; ++i) {
    float t = (float)i / 1000.0f;
    input[i] = 0.5f + 0.5f * sinf(2.0f * 3.14159265f * 70.0f * t);
  }

  /* Allocate output buffers */
  float *outputs[10];
  float *storage = (float *)calloc(10 * 2000, sizeof(float));
  AE_ASSERT_NOT_NULL(storage);
  for (int ch = 0; ch < 10; ++ch) {
    outputs[ch] = storage + ch * 2000;
  }

  AE_ASSERT_EQ(ae_modfb_process(mfb, input, 2000, outputs), AE_OK);

  /* Find channel with most energy - should be near 70Hz */
  float max_energy = 0.0f;
  int peak_ch = 0;
  for (int ch = 0; ch < 10; ++ch) {
    float energy = 0.0f;
    for (size_t i = 0; i < 2000; ++i) {
      energy += outputs[ch][i] * outputs[ch][i];
    }
    if (energy > max_energy) {
      max_energy = energy;
      peak_ch = ch;
    }
  }

  /* Get center frequencies */
  float centers[10];
  size_t n_ch = 0;
  AE_ASSERT_EQ(ae_modfb_get_center_frequencies(mfb, centers, &n_ch), AE_OK);
  AE_ASSERT_EQ((int)n_ch, 10);

  /* Peak should be near 64Hz or 128Hz channel (closest to 70Hz) */
  AE_ASSERT(centers[peak_ch] >= 32.0f && centers[peak_ch] <= 128.0f);

  free(storage);
  ae_modfb_destroy(mfb);
  AE_TEST_PASS();
}

void test_bmld_spi_n0(void) {
  /* SπN0 configuration: signal anti-correlated, noise correlated */
  ae_bmld_params_t params = {
      .signal_frequency_hz = 500.0f,
      .noise_bandwidth_hz = 100.0f,
      .signal_correlation = -1.0f, /* Sπ */
      .noise_correlation = 1.0f    /* N0 */
  };

  float bmld = ae_compute_bmld(&params);
  /* Expected: ~15 dB at 500 Hz for SπN0 (typical range 12-18dB) */
  AE_ASSERT(bmld > 10.0f && bmld <= 20.0f);
  AE_TEST_PASS();
}

void test_bmld_s0_n0(void) {
  /* S0N0 configuration: both correlated -> no advantage */
  ae_bmld_params_t params = {
      .signal_frequency_hz = 500.0f,
      .noise_bandwidth_hz = 100.0f,
      .signal_correlation = 1.0f, /* S0 */
      .noise_correlation = 1.0f   /* N0 */
  };

  float bmld = ae_compute_bmld(&params);
  /* Expected: ~0 dB (no binaural advantage) */
  AE_ASSERT(bmld < 2.0f);
  AE_TEST_PASS();
}

void test_bmld_frequency_dependence(void) {
  ae_bmld_params_t low_freq = {.signal_frequency_hz = 500.0f,
                               .noise_bandwidth_hz = 100.0f,
                               .signal_correlation = -1.0f,
                               .noise_correlation = 1.0f};

  ae_bmld_params_t high_freq = {.signal_frequency_hz = 2000.0f,
                                .noise_bandwidth_hz = 100.0f,
                                .signal_correlation = -1.0f,
                                .noise_correlation = 1.0f};

  float bmld_low = ae_compute_bmld(&low_freq);
  float bmld_high = ae_compute_bmld(&high_freq);

  /* BMLD should be larger at low frequencies */
  AE_ASSERT(bmld_low > bmld_high);
  AE_TEST_PASS();
}

void test_sii_quiet_conditions(void) {
  ae_sii_params_t params = {.speech_level_db = 65.0f, /* Normal speech */
                            .noise_level_db = 30.0f,  /* Quiet room */
                            .rt60_seconds = 0.4f,
                            .use_extended_sii = false};

  float sii = 0.0f;
  AE_ASSERT_EQ(ae_compute_sii(&params, &sii), AE_OK);
  /* Expected: high intelligibility (>0.7) in quiet conditions */
  AE_ASSERT(sii > 0.7f);
  AE_TEST_PASS();
}

void test_sii_noisy_conditions(void) {
  ae_sii_params_t params = {.speech_level_db = 65.0f, /* Normal speech */
                            .noise_level_db =
                                70.0f, /* Noisy (louder than speech) */
                            .rt60_seconds = 0.4f,
                            .use_extended_sii = false};

  float sii = 0.0f;
  AE_ASSERT_EQ(ae_compute_sii(&params, &sii), AE_OK);
  /* Expected: low intelligibility (<0.3) in noisy conditions */
  AE_ASSERT(sii < 0.5f);
  AE_TEST_PASS();
}

void test_auditory_pipeline(void) {
  /* Create test signal */
  float samples[1024];
  ae_test_generate_sine(samples, 1024, 1000.0f, 48000.0f, 0.5f);

  ae_audio_buffer_t signal = {.samples = samples,
                              .frame_count = 1024,
                              .channels = 1,
                              .interleaved = true};

  ae_auditory_pipeline_config_t config = {
      .gammatone = {.n_channels = 8,
                    .f_low = 200.0f,
                    .f_high = 8000.0f,
                    .filter_order = 4,
                    .sample_rate = 48000},
      .ihc = {.compression_exponent = 0.3f, .lpf_cutoff_hz = 2000.0f},
      .adaptation = {.n_stages = 5, .min_output = 1e-5f, .sample_rate = 48000},
      .compute_gammatone = true,
      .compute_ihc = true,
      .compute_adaptation = true,
      .compute_modulation = false};

  ae_auditory_repr_t repr;
  AE_ASSERT_EQ(
      ae_compute_auditory_representation(NULL, &signal, &config, &repr), AE_OK);

  /* Verify outputs exist */
  AE_ASSERT_NOT_NULL(repr.gammatone_output);
  AE_ASSERT_NOT_NULL(repr.ihc_output);
  AE_ASSERT_NOT_NULL(repr.adaptation_output);
  AE_ASSERT_EQ((int)repr.n_audio_channels, 8);
  AE_ASSERT_EQ((int)repr.n_samples, 1024);

  /* Check gammatone has output */
  float energy = 0.0f;
  for (size_t i = 0; i < 1024; ++i) {
    energy += repr.gammatone_output[4][i] * repr.gammatone_output[4][i];
  }
  AE_ASSERT(energy > 0.0f);

  ae_free_auditory_representation(&repr);
  AE_TEST_PASS();
}

void test_bmld_extended_ec_model(void) {
  /* Test extended BMLD with full EC model */
  ae_bmld_extended_params_t params = {.signal_frequency_hz = 500.0f,
                                      .noise_bandwidth_hz = 100.0f,
                                      .signal_correlation = -1.0f, /* Sπ */
                                      .noise_correlation = 1.0f,   /* N0 */
                                      .signal_itd_us = 0.0f,
                                      .noise_itd_us = 0.0f,
                                      .equalization_error = 0.25f,
                                      .cancellation_error = 0.0001f};

  float bmld = ae_compute_bmld_extended(&params);
  /* Extended model should give similar result to simple model for basic case */
  AE_ASSERT(bmld > 10.0f && bmld <= 15.0f);

  /* Test with ITD */
  params.signal_itd_us = 300.0f; /* 300μs ITD */
  float bmld_itd = ae_compute_bmld_extended(&params);
  /* ITD should affect the result */
  AE_ASSERT(bmld_itd >= 0.0f);

  AE_TEST_PASS();
}

void test_sii_extended_21_band(void) {
  /* Test 21-band extended SII */
  ae_sii_extended_params_t params = {0};
  params.speech_level_db = 65.0f;
  params.rt60_seconds = 0.4f;
  params.use_21_band = true;
  params.model_hearing_loss = false;

  /* Set low noise level for good intelligibility */
  for (int i = 0; i < 21; ++i) {
    params.noise_spectrum_db[i] = 30.0f; /* Quiet room: 30dB */
  }

  ae_sii_result_t result;
  AE_ASSERT_EQ(ae_compute_sii_extended(&params, &result), AE_OK);

  /* Should have positive intelligibility (40dB noise vs 65dB speech) */
  AE_ASSERT(result.sii_value > 0.2f);
  AE_ASSERT_EQ((int)result.n_bands, 21);

  /* Per-band values should sum appropriately */
  float sum = 0.0f;
  for (int i = 0; i < 21; ++i) {
    AE_ASSERT(result.audibility[i] >= 0.0f && result.audibility[i] <= 1.0f);
    sum += result.band_sii[i];
  }
  AE_ASSERT(sum > 0.0f);

  AE_TEST_PASS();
}

void test_sii_hearing_loss(void) {
  /* Test SII with hearing loss modeling */
  ae_sii_extended_params_t normal = {0};
  normal.speech_level_db = 65.0f;
  normal.use_21_band = true;
  normal.model_hearing_loss = false;
  for (int i = 0; i < 21; ++i) {
    normal.noise_spectrum_db[i] = 30.0f;
  }

  ae_sii_extended_params_t hearing_loss = normal;
  hearing_loss.model_hearing_loss = true;
  /* Simulate high-frequency hearing loss (presbycusis pattern) */
  for (int i = 0; i < 21; ++i) {
    if (i >= 10) { /* Above 1600 Hz */
      hearing_loss.hearing_threshold_db[i] = 30.0f + (float)(i - 10) * 5.0f;
    }
  }

  ae_sii_result_t result_normal, result_hl;
  AE_ASSERT_EQ(ae_compute_sii_extended(&normal, &result_normal), AE_OK);
  AE_ASSERT_EQ(ae_compute_sii_extended(&hearing_loss, &result_hl), AE_OK);

  /* Hearing loss should reduce SII */
  AE_ASSERT(result_hl.sii_value < result_normal.sii_value);

  AE_TEST_PASS();
}

void test_binaural_sii(void) {
  /* Test binaural SII with BMLD advantage */
  ae_binaural_sii_params_t params = {
      .left = {.speech_level_db = 65.0f,
               .noise_level_db = 50.0f,
               .rt60_seconds = 0.4f,
               .use_extended_sii = false},
      .right = {.speech_level_db = 65.0f,
                .noise_level_db = 50.0f,
                .rt60_seconds = 0.4f,
                .use_extended_sii = false},
      .bmld_advantage_db = 6.0f /* 6 dB BMLD advantage */
  };

  float binaural_sii = 0.0f;
  AE_ASSERT_EQ(ae_compute_binaural_sii(&params, &binaural_sii), AE_OK);

  /* Compare to monaural */
  float mono_sii = 0.0f;
  AE_ASSERT_EQ(ae_compute_sii(&params.left, &mono_sii), AE_OK);

  /* Binaural should be better than monaural */
  AE_ASSERT(binaural_sii >= mono_sii);

  AE_TEST_PASS();
}

void test_pipeline_with_modulation(void) {
  /* Test full pipeline including modulation filterbank */
  float samples[4800]; /* 100ms at 48kHz */
  ae_test_generate_sine(samples, 4800, 1000.0f, 48000.0f, 0.5f);

  /* Add amplitude modulation at 70Hz for roughness-range signal */
  for (size_t i = 0; i < 4800; ++i) {
    float t = (float)i / 48000.0f;
    float mod = 1.0f + 0.5f * sinf(2.0f * 3.14159265f * 70.0f * t);
    samples[i] *= mod;
  }

  ae_audio_buffer_t signal = {.samples = samples,
                              .frame_count = 4800,
                              .channels = 1,
                              .interleaved = true};

  ae_auditory_pipeline_config_t config = {
      .gammatone = {.n_channels = 8,
                    .f_low = 200.0f,
                    .f_high = 8000.0f,
                    .filter_order = 4,
                    .sample_rate = 48000},
      .ihc = {.compression_exponent = 0.3f, .lpf_cutoff_hz = 2000.0f},
      .adaptation = {.n_stages = 5, .min_output = 1e-5f, .sample_rate = 48000},
      .modulation = {.n_channels = 10,
                     .f_low = 0.5f,
                     .f_high = 256.0f,
                     .sample_rate = 1000},
      .compute_gammatone = true,
      .compute_ihc = true,
      .compute_adaptation = true,
      .compute_modulation = true};

  ae_auditory_repr_t repr;
  AE_ASSERT_EQ(
      ae_compute_auditory_representation(NULL, &signal, &config, &repr), AE_OK);

  /* Verify modulation output exists */
  AE_ASSERT_NOT_NULL(repr.modulation_output);
  AE_ASSERT_EQ((int)repr.n_modulation_channels, 10);

  /* Check modulation output for each audio channel */
  for (uint32_t ch = 0; ch < repr.n_audio_channels; ++ch) {
    AE_ASSERT_NOT_NULL(repr.modulation_output[ch]);
    for (uint32_t m = 0; m < repr.n_modulation_channels; ++m) {
      AE_ASSERT_NOT_NULL(repr.modulation_output[ch][m]);
    }
  }

  ae_free_auditory_representation(&repr);
  AE_TEST_PASS();
}
/*============================================================================
 * Perceptual Profile Tests
 *============================================================================*/

void test_perceptual_profile_brightness(void) {
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  /* Low frequency tone should have low brightness */
  float low_freq[2048];
  ae_test_generate_sine(low_freq, 2048, 300.0f, 48000.0f, 0.5f);
  ae_audio_buffer_t buf_low = {.samples = low_freq,
                               .frame_count = 2048,
                               .channels = 1,
                               .interleaved = true};

  /* High frequency tone should have high brightness */
  float high_freq[2048];
  ae_test_generate_sine(high_freq, 2048, 6000.0f, 48000.0f, 0.5f);
  ae_audio_buffer_t buf_high = {.samples = high_freq,
                                .frame_count = 2048,
                                .channels = 1,
                                .interleaved = true};

  ae_perceptual_profile_t profile_low, profile_high;
  AE_ASSERT_EQ(ae_analyze_perceptual_profile(engine, &buf_low, &profile_low),
               AE_OK);
  AE_ASSERT_EQ(ae_analyze_perceptual_profile(engine, &buf_high, &profile_high),
               AE_OK);

  /* High frequency should have higher brightness */
  AE_ASSERT(profile_high.brightness > profile_low.brightness);

  /* Brightness should be normalized 0-1 */
  AE_ASSERT(profile_low.brightness >= 0.0f && profile_low.brightness <= 1.0f);
  AE_ASSERT(profile_high.brightness >= 0.0f && profile_high.brightness <= 1.0f);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_perceptual_profile_roughness(void) {
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  /* Plain tone should have low roughness */
  float plain[4096];
  ae_test_generate_sine(plain, 4096, 1000.0f, 48000.0f, 0.5f);
  ae_audio_buffer_t buf_plain = {.samples = plain,
                                 .frame_count = 4096,
                                 .channels = 1,
                                 .interleaved = true};

  /* AM tone at 70Hz should have high roughness */
  float am[4096];
  ae_generate_am(am, 4096, 1000.0f, 70.0f, 48000.0f, 0.8f);
  ae_audio_buffer_t buf_am = {
      .samples = am, .frame_count = 4096, .channels = 1, .interleaved = true};

  ae_perceptual_profile_t profile_plain, profile_am;
  AE_ASSERT_EQ(
      ae_analyze_perceptual_profile(engine, &buf_plain, &profile_plain), AE_OK);
  AE_ASSERT_EQ(ae_analyze_perceptual_profile(engine, &buf_am, &profile_am),
               AE_OK);

  /* AM tone should have higher roughness */
  AE_ASSERT(profile_am.roughness_norm > profile_plain.roughness_norm);

  /* Roughness_norm should be 0-1 */
  AE_ASSERT(profile_plain.roughness_norm >= 0.0f &&
            profile_plain.roughness_norm <= 1.0f);
  AE_ASSERT(profile_am.roughness_norm >= 0.0f &&
            profile_am.roughness_norm <= 1.0f);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_perceptual_profile_attack(void) {
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  /* Sharp attack: immediate onset */
  float sharp[2048];
  memset(sharp, 0, sizeof(sharp));
  for (size_t i = 0; i < 1000; ++i) {
    sharp[i] = 0.8f * sinf(2.0f * 3.14159265f * 1000.0f * (float)i / 48000.0f);
  }
  ae_audio_buffer_t buf_sharp = {.samples = sharp,
                                 .frame_count = 2048,
                                 .channels = 1,
                                 .interleaved = true};

  /* Soft attack: gradual fade in */
  float soft[2048];
  memset(soft, 0, sizeof(soft));
  for (size_t i = 0; i < 2000; ++i) {
    float env = (float)i / 2000.0f; /* Linear fade in */
    soft[i] =
        env * 0.8f * sinf(2.0f * 3.14159265f * 1000.0f * (float)i / 48000.0f);
  }
  ae_audio_buffer_t buf_soft = {
      .samples = soft, .frame_count = 2048, .channels = 1, .interleaved = true};

  ae_perceptual_profile_t profile_sharp, profile_soft;
  AE_ASSERT_EQ(
      ae_analyze_perceptual_profile(engine, &buf_sharp, &profile_sharp), AE_OK);
  AE_ASSERT_EQ(ae_analyze_perceptual_profile(engine, &buf_soft, &profile_soft),
               AE_OK);

  /* Sharp attack should have higher attack_sharpness */
  AE_ASSERT(profile_sharp.attack_sharpness > profile_soft.attack_sharpness);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_perceived_distance_mapping(void) {
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  /* Test near distance */
  AE_ASSERT_EQ(ae_set_perceived_distance(engine, 0.0f), AE_OK);
  ae_main_params_t near_params;
  AE_ASSERT_EQ(ae_get_main_params(engine, &near_params), AE_OK);

  /* Test far distance */
  AE_ASSERT_EQ(ae_set_perceived_distance(engine, 1.0f), AE_OK);
  ae_main_params_t far_params;
  AE_ASSERT_EQ(ae_get_main_params(engine, &far_params), AE_OK);

  /* Far should have more wet signal */
  AE_ASSERT(far_params.dry_wet > near_params.dry_wet);
  /* Far should be darker */
  AE_ASSERT(far_params.brightness < near_params.brightness);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_perceived_spaciousness_mapping(void) {
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  /* Test narrow */
  AE_ASSERT_EQ(ae_set_perceived_spaciousness(engine, 0.0f), AE_OK);
  ae_main_params_t narrow_params;
  AE_ASSERT_EQ(ae_get_main_params(engine, &narrow_params), AE_OK);

  /* Test wide */
  AE_ASSERT_EQ(ae_set_perceived_spaciousness(engine, 1.0f), AE_OK);
  ae_main_params_t wide_params;
  AE_ASSERT_EQ(ae_get_main_params(engine, &wide_params), AE_OK);

  /* Wide should have larger width */
  AE_ASSERT(wide_params.width > narrow_params.width);
  /* Wide should have larger room_size */
  AE_ASSERT(wide_params.room_size > narrow_params.room_size);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

void test_perceived_clarity_mapping(void) {
  ae_engine_t *engine = ae_create_engine(NULL);
  AE_ASSERT_NOT_NULL(engine);

  /* Test muddy (low clarity) */
  AE_ASSERT_EQ(ae_set_perceived_clarity(engine, 0.0f), AE_OK);
  ae_main_params_t muddy_params;
  AE_ASSERT_EQ(ae_get_main_params(engine, &muddy_params), AE_OK);

  /* Test clear (high clarity) */
  AE_ASSERT_EQ(ae_set_perceived_clarity(engine, 1.0f), AE_OK);
  ae_main_params_t clear_params;
  AE_ASSERT_EQ(ae_get_main_params(engine, &clear_params), AE_OK);

  /* Clear should have less wet signal */
  AE_ASSERT(clear_params.dry_wet < muddy_params.dry_wet);
  /* Clear should have smaller room_size */
  AE_ASSERT(clear_params.room_size < muddy_params.room_size);

  ae_destroy_engine(engine);
  AE_TEST_PASS();
}

int main(void) {
  printf("Acoustic Engine - Auditory Tests\n");

  AE_TEST_SUITE_BEGIN("Gammatone");
  AE_RUN_TEST(test_gammatone_impulse_response);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("IHC / Adaptation");
  AE_RUN_TEST(test_ihc_envelope_response);
  AE_RUN_TEST(test_adaptloop_forward_masking);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("DRNL / Modulation Filterbank");
  AE_RUN_TEST(test_drnl_compression);
  AE_RUN_TEST(test_modfb_modulation_detection);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Loudness / Timbral");
  AE_RUN_TEST(test_zwicker_loudness_consistency);
  AE_RUN_TEST(test_sharpness_frequency_order);
  AE_RUN_TEST(test_roughness_am_tone);
  AE_RUN_TEST(test_fluctuation_am_tone);
  AE_RUN_TEST(test_roughness_over_time);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("BMLD");
  AE_RUN_TEST(test_bmld_spi_n0);
  AE_RUN_TEST(test_bmld_s0_n0);
  AE_RUN_TEST(test_bmld_frequency_dependence);
  AE_RUN_TEST(test_bmld_extended_ec_model);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("SII");
  AE_RUN_TEST(test_sii_quiet_conditions);
  AE_RUN_TEST(test_sii_noisy_conditions);
  AE_RUN_TEST(test_sii_extended_21_band);
  AE_RUN_TEST(test_sii_hearing_loss);
  AE_RUN_TEST(test_binaural_sii);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Auditory Pipeline");
  AE_RUN_TEST(test_auditory_pipeline);
  AE_RUN_TEST(test_pipeline_with_modulation);
  AE_TEST_SUITE_END();

  AE_TEST_SUITE_BEGIN("Perceptual Profile");
  AE_RUN_TEST(test_perceptual_profile_brightness);
  AE_RUN_TEST(test_perceptual_profile_roughness);
  AE_RUN_TEST(test_perceptual_profile_attack);
  AE_RUN_TEST(test_perceived_distance_mapping);
  AE_RUN_TEST(test_perceived_spaciousness_mapping);
  AE_RUN_TEST(test_perceived_clarity_mapping);
  AE_TEST_SUITE_END();

  return ae_test_report();
}
