/**
 * @file ae_auditory.c
 * @brief Auditory modeling (Gammatone filterbank, IHC, adaptation loop,
 * timbral)
 */

#include "ae_internal.h"

/*============================================================================
 * ERB (Equivalent Rectangular Bandwidth) calculations
 *============================================================================*/

/**
 * Calculate ERB at given frequency (Glasberg & Moore 1990)
 */
static float erb_at_frequency(float f_hz) {
  return 24.7f * (4.37f * f_hz / 1000.0f + 1.0f);
}

/**
 * Convert Hz to ERB-rate scale
 */
static float hz_to_erb_rate(float f_hz) {
  return 21.4f * log10f(4.37f * f_hz / 1000.0f + 1.0f);
}

/**
 * Convert ERB-rate to Hz
 */
static float erb_rate_to_hz(float erb_rate) {
  return 1000.0f * (powf(10.0f, erb_rate / 21.4f) - 1.0f) / 4.37f;
}

/*============================================================================
 * Gammatone Filterbank
 *============================================================================*/

typedef struct {
  float a;       /* Amplitude */
  float b_coeff; /* Bandwidth coefficient */
  float cf_hz;   /* Center frequency */
  float phase;   /* Phase */

  /* IIR filter states (4th order = 4 stages) */
  float state_re[4];
  float state_im[4];
} ae_gammatone_channel_t;

struct ae_gammatone {
  ae_gammatone_config_t config;
  ae_gammatone_channel_t *channels;
  float *center_freqs;
};

/**
 * Calculate center frequencies for Gammatone filterbank
 */
static void gammatone_calculate_center_freqs(ae_gammatone_t *gt) {
  if (!gt || gt->config.n_channels == 0)
    return;

  float erb_low = hz_to_erb_rate(gt->config.f_low);
  float erb_high = hz_to_erb_rate(gt->config.f_high);

  for (uint32_t i = 0; i < gt->config.n_channels; ++i) {
    float erb = erb_low + (erb_high - erb_low) * (float)i /
                              (float)(gt->config.n_channels - 1);
    gt->center_freqs[i] = erb_rate_to_hz(erb);
  }
}

/**
 * Create Gammatone filterbank
 */
AE_API ae_gammatone_t *
ae_gammatone_create(const ae_gammatone_config_t *config) {
  if (!config || config->n_channels == 0 || config->sample_rate == 0) {
    return NULL;
  }

  ae_gammatone_t *gt = (ae_gammatone_t *)calloc(1, sizeof(ae_gammatone_t));
  if (!gt)
    return NULL;

  gt->config = *config;

  gt->channels = (ae_gammatone_channel_t *)calloc(
      config->n_channels, sizeof(ae_gammatone_channel_t));
  gt->center_freqs = (float *)calloc(config->n_channels, sizeof(float));

  if (!gt->channels || !gt->center_freqs) {
    ae_gammatone_destroy(gt);
    return NULL;
  }

  gammatone_calculate_center_freqs(gt);

  /* Initialize each channel */
  for (uint32_t i = 0; i < config->n_channels; ++i) {
    ae_gammatone_channel_t *ch = &gt->channels[i];
    ch->cf_hz = gt->center_freqs[i];
    ch->b_coeff = 1.019f * 2.0f * (float)M_PI * erb_at_frequency(ch->cf_hz);
    ch->a = 1.0f;
    ch->phase = 0.0f;

    memset(ch->state_re, 0, sizeof(ch->state_re));
    memset(ch->state_im, 0, sizeof(ch->state_im));
  }

  return gt;
}

/**
 * Destroy Gammatone filterbank
 */
AE_API void ae_gammatone_destroy(ae_gammatone_t *gt) {
  if (!gt)
    return;
  free(gt->channels);
  free(gt->center_freqs);
  free(gt);
}

/**
 * Process through Gammatone filterbank (complex baseband implementation)
 */
AE_API ae_result_t ae_gammatone_process(ae_gammatone_t *gt, const float *input,
                                        size_t n_samples, float **output) {
  if (!gt || !input || !output || n_samples == 0) {
    return AE_ERROR_INVALID_PARAM;
  }

  float dt = 1.0f / (float)gt->config.sample_rate;
  uint8_t order = gt->config.filter_order;

  for (uint32_t ch = 0; ch < gt->config.n_channels; ++ch) {
    ae_gammatone_channel_t *channel = &gt->channels[ch];
    float *out = output[ch];

    float b = channel->b_coeff;
    float cf = channel->cf_hz;
    float decay = expf(-b * dt);
    float omega = 2.0f * (float)M_PI * cf * dt;

    for (size_t i = 0; i < n_samples; ++i) {
      /* Input as complex (real only) */
      float in_re = input[i];
      float in_im = 0.0f;

      /* Apply 4 cascaded first-order filters */
      for (uint8_t stage = 0; stage < order; ++stage) {
        float cos_omega = cosf(omega);
        float sin_omega = sinf(omega);

        /* Complex multiplication for modulation */
        float state_re = channel->state_re[stage];
        float state_im = channel->state_im[stage];

        /* Lowpass filter in baseband */
        float new_re = decay * (cos_omega * state_re - sin_omega * state_im) +
                       (1.0f - decay) * in_re;
        float new_im = decay * (sin_omega * state_re + cos_omega * state_im) +
                       (1.0f - decay) * in_im;

        channel->state_re[stage] = new_re;
        channel->state_im[stage] = new_im;

        in_re = new_re;
        in_im = new_im;
      }

      /* Output magnitude */
      out[i] = sqrtf(in_re * in_re + in_im * in_im);
    }
  }

  return AE_OK;
}

/**
 * Get center frequencies
 */
AE_API ae_result_t ae_gammatone_get_center_frequencies(ae_gammatone_t *gt,
                                                       float *frequencies,
                                                       size_t *n_channels) {
  if (!gt || !frequencies || !n_channels) {
    return AE_ERROR_INVALID_PARAM;
  }

  *n_channels = gt->config.n_channels;
  memcpy(frequencies, gt->center_freqs, gt->config.n_channels * sizeof(float));

  return AE_OK;
}

/*============================================================================
 * IHC (Inner Hair Cell) Envelope
 *============================================================================*/

/**
 * Compute IHC envelope (half-wave rectification + compression + lowpass)
 */
AE_API ae_result_t ae_ihc_envelope(const float *basilar_membrane_output,
                                   size_t n_samples,
                                   const ae_ihc_config_t *config,
                                   float *ihc_output) {
  if (!basilar_membrane_output || !config || !ihc_output || n_samples == 0) {
    return AE_ERROR_INVALID_PARAM;
  }

  float theta = config->compression_exponent;
  float lpf_cutoff = config->lpf_cutoff_hz;
  float sample_rate = AE_SAMPLE_RATE;

  /* Simple 1-pole lowpass coefficient */
  float rc = 1.0f / (2.0f * (float)M_PI * lpf_cutoff);
  float dt = 1.0f / sample_rate;
  float alpha = dt / (rc + dt);

  float state = 0.0f;

  for (size_t i = 0; i < n_samples; ++i) {
    /* Half-wave rectification */
    float rectified = fmaxf(0.0f, basilar_membrane_output[i]);

    /* Compression */
    float compressed = powf(rectified + AE_LOG_EPSILON, theta);

    /* Lowpass filter */
    state = state + alpha * (compressed - state);
    ihc_output[i] = state;
  }

  return AE_OK;
}

/*============================================================================
 * Adaptation Loop (Dau et al. 1996)
 *============================================================================*/

struct ae_adaptloop {
  ae_adaptloop_config_t config;
  float *stage_states;
};

/**
 * Create adaptation loop
 */
AE_API ae_adaptloop_t *
ae_adaptloop_create(const ae_adaptloop_config_t *config) {
  if (!config || config->n_stages == 0 || config->n_stages > 5) {
    return NULL;
  }

  ae_adaptloop_t *al = (ae_adaptloop_t *)calloc(1, sizeof(ae_adaptloop_t));
  if (!al)
    return NULL;

  al->config = *config;
  {
    float defaults[5] = {5.0f, 50.0f, 129.0f, 253.0f, 500.0f};
    for (uint8_t i = 0; i < al->config.n_stages; ++i) {
      if (al->config.time_constants[i] <= 0.0f)
        al->config.time_constants[i] = defaults[i];
    }
  }
  if (al->config.min_output <= 0.0f)
    al->config.min_output = 1e-5f;
  al->stage_states = (float *)calloc(config->n_stages, sizeof(float));

  if (!al->stage_states) {
    ae_adaptloop_destroy(al);
    return NULL;
  }

  /* Initialize states to min_output */
  for (uint8_t i = 0; i < config->n_stages; ++i) {
    al->stage_states[i] = config->min_output;
  }

  return al;
}

/**
 * Destroy adaptation loop
 */
AE_API void ae_adaptloop_destroy(ae_adaptloop_t *al) {
  if (!al)
    return;
  free(al->stage_states);
  free(al);
}

/**
 * Process through adaptation loop (Dau et al. 1996)
 *
 * Forward masking effect: Each stage divides the input by an adaptive state
 * that tracks stimulus history. When input drops, the high state persists,
 * causing suppressed output. The state then slowly decays towards A_min.
 */
AE_API ae_result_t ae_adaptloop_process(ae_adaptloop_t *al, const float *input,
                                        size_t n_samples, float *output) {
  if (!al || !input || !output || n_samples == 0) {
    return AE_ERROR_INVALID_PARAM;
  }

  float dt = 1.0f / (float)al->config.sample_rate;
  float A_min = al->config.min_output;

  /* Process each sample through all stages */
  for (size_t i = 0; i < n_samples; ++i) {
    float x = fmaxf(input[i], A_min);

    for (uint8_t stage = 0; stage < al->config.n_stages; ++stage) {
      float tau = al->config.time_constants[stage] * 0.001f; /* ms to s */
      float A = al->stage_states[stage];

      /* Divisive adaptation: output = input / state
       * State tracks input but decays slowly towards A_min */
      float alpha = expf(-dt / tau);

      /* State update: tracks input, decays towards A_min when input is low */
      A = alpha * A + (1.0f - alpha) * fmaxf(x, A_min);
      A = fmaxf(A, A_min);
      al->stage_states[stage] = A;

      /* Divisive normalization: creates forward masking */
      x = x / A;
    }

    output[i] = x;
  }

  return AE_OK;
}

/**
 * Reset adaptation loop
 */
AE_API void ae_adaptloop_reset(ae_adaptloop_t *al) {
  if (!al)
    return;

  for (uint8_t i = 0; i < al->config.n_stages; ++i) {
    al->stage_states[i] = al->config.min_output;
  }
}

/*============================================================================
 * Timbral Calculations
 *============================================================================*/

/**
 * Calculate Sharpness (DIN 45692)
 * S = 0.11 × ∫ N'(z) × g(z) × z dz / N
 */
AE_API ae_result_t ae_compute_sharpness(ae_engine_t *engine,
                                        const ae_audio_buffer_t *signal,
                                        ae_sharpness_method_t method,
                                        float *sharpness_acum) {
  (void)engine; /* May be used for sample rate */

  if (!signal || !sharpness_acum) {
    return AE_ERROR_INVALID_PARAM;
  }

  /* Simplified sharpness estimation from spectral centroid */
  /* Full implementation would require Bark-band specific loudness */

  float *samples = signal->samples;
  size_t n = signal->frame_count;
  if (!samples || n == 0) {
    *sharpness_acum = 0.0f;
    return AE_OK;
  }

  /* Calculate spectral centroid as proxy for sharpness */
  float energy_sum = 0.0f;

  /* Simple time-domain estimation using zero-crossing rate and energy */
  float prev = 0.0f;
  float zcr = 0.0f;

  for (size_t i = 0; i < n; ++i) {
    float s = samples[i];
    energy_sum += s * s;

    if ((s >= 0.0f && prev < 0.0f) || (s < 0.0f && prev >= 0.0f)) {
      zcr += 1.0f;
    }
    prev = s;
  }

  zcr /= (float)n;
  float rms = sqrtf(energy_sum / (float)n);

  /* Map ZCR and RMS to sharpness estimate */
  float normalized_zcr = zcr * AE_SAMPLE_RATE / 2.0f;

  float sharpness = 0.0f;

  if (method == AE_SHARPNESS_DIN_45692) {
    sharpness = ae_clamp(normalized_zcr / 1000.0f, 0.0f, 5.0f);
  } else if (method == AE_SHARPNESS_AURES) {
    sharpness = ae_clamp(normalized_zcr / 800.0f * rms * 10.0f, 0.0f, 5.0f);
  } else {
    sharpness = ae_clamp(normalized_zcr / 1200.0f, 0.0f, 5.0f);
  }

  *sharpness_acum = sharpness;
  return AE_OK;
}

/**
 * Calculate Roughness (Daniel & Weber 1997)
 */
AE_API ae_result_t ae_compute_roughness(ae_engine_t *engine,
                                        const ae_audio_buffer_t *signal,
                                        float *roughness_asper) {
  (void)engine;

  if (!signal || !roughness_asper) {
    return AE_ERROR_INVALID_PARAM;
  }

  float *samples = signal->samples;
  size_t n = signal->frame_count;
  if (!samples || n == 0) {
    *roughness_asper = 0.0f;
    return AE_OK;
  }

  size_t window = (size_t)(AE_SAMPLE_RATE * 0.01f);
  if (window > n)
    window = n;

  float prev_env = 0.0f;
  float mod_sum = 0.0f;
  size_t mod_count = 0;

  for (size_t i = 0; i < n - window; i += window / 2) {
    float env = 0.0f;
    for (size_t j = 0; j < window; ++j) {
      env += fabsf(samples[i + j]);
    }
    env /= (float)window;

    if (prev_env > AE_LOG_EPSILON) {
      float mod = fabsf(env - prev_env) / prev_env;
      mod_sum += mod;
      mod_count++;
    }
    prev_env = env;
  }

  float avg_mod = (mod_count > 0) ? mod_sum / (float)mod_count : 0.0f;
  *roughness_asper = ae_clamp(avg_mod * 3.0f, 0.0f, 5.0f);

  return AE_OK;
}

/**
 * Calculate Fluctuation Strength (Fastl & Zwicker)
 */
AE_API ae_result_t ae_compute_fluctuation_strength(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    float *fluctuation_vacil) {
  (void)engine;

  if (!signal || !fluctuation_vacil) {
    return AE_ERROR_INVALID_PARAM;
  }

  float *samples = signal->samples;
  size_t n = signal->frame_count;
  if (!samples || n == 0) {
    *fluctuation_vacil = 0.0f;
    return AE_OK;
  }

  size_t window = (size_t)(AE_SAMPLE_RATE * 0.125f);
  if (window > n / 2)
    window = n / 2;
  if (window < 1)
    window = 1;

  float prev_env = 0.0f;
  float mod_sum = 0.0f;
  size_t count = 0;

  for (size_t i = 0; i < n - window; i += window) {
    float env = 0.0f;
    for (size_t j = 0; j < window; ++j) {
      env += fabsf(samples[i + j]);
    }
    env /= (float)window;

    if (prev_env > AE_LOG_EPSILON && count > 0) {
      float mod = fabsf(env - prev_env) / ((env + prev_env) * 0.5f);
      mod_sum += mod * mod;
    }
    prev_env = env;
    count++;
  }

  float rms_mod = (count > 0) ? sqrtf(mod_sum / (float)count) : 0.0f;
  *fluctuation_vacil = ae_clamp(rms_mod * 2.0f, 0.0f, 5.0f);

  return AE_OK;
}

static void ae_fill_mono_segment(const ae_audio_buffer_t *signal, size_t start,
                                 size_t length, float *mono) {
  if (!signal || !mono || length == 0)
    return;
  size_t frames = signal->frame_count;
  if (signal->channels == 1) {
    for (size_t i = 0; i < length; ++i) {
      mono[i] = signal->samples[start + i];
    }
    return;
  }

  if (signal->interleaved) {
    for (size_t i = 0; i < length; ++i) {
      size_t idx = (start + i) * 2;
      mono[i] = 0.5f * (signal->samples[idx] + signal->samples[idx + 1]);
    }
  } else {
    for (size_t i = 0; i < length; ++i) {
      mono[i] = 0.5f * (signal->samples[start + i] +
                        signal->samples[start + i + frames]);
    }
  }
}

AE_API ae_result_t ae_compute_roughness_over_time(
    ae_engine_t *engine, const ae_audio_buffer_t *signal, float hop_size_ms,
    float *roughness_array, size_t *n_frames) {
  (void)engine;

  if (!signal || !roughness_array || !n_frames || !signal->samples)
    return AE_ERROR_INVALID_PARAM;
  if (signal->frame_count == 0)
    return AE_ERROR_INVALID_PARAM;

  size_t hop = (size_t)(hop_size_ms * 0.001f * (float)AE_SAMPLE_RATE);
  if (hop < 1)
    hop = 1;

  size_t total = signal->frame_count;
  size_t frames = (total + hop - 1) / hop;
  if (*n_frames < frames) {
    *n_frames = frames;
    return AE_ERROR_BUFFER_TOO_SMALL;
  }

  float *mono = (float *)calloc(hop, sizeof(float));
  if (!mono)
    return AE_ERROR_OUT_OF_MEMORY;

  for (size_t f = 0; f < frames; ++f) {
    size_t start = f * hop;
    size_t len = hop;
    if (start + len > total)
      len = total - start;
    ae_fill_mono_segment(signal, start, len, mono);

    ae_audio_buffer_t segment = {.samples = mono,
                                 .frame_count = len,
                                 .channels = 1,
                                 .interleaved = true};
    float rough = 0.0f;
    ae_result_t res = ae_compute_roughness(engine, &segment, &rough);
    if (res != AE_OK) {
      free(mono);
      return res;
    }
    roughness_array[f] = rough;
  }

  free(mono);
  *n_frames = frames;
  return AE_OK;
}

AE_API ae_result_t ae_analyze_zwicker_loudness(ae_engine_t *engine,
                                               const ae_audio_buffer_t *signal,
                                               ae_loudness_method_t method,
                                               ae_zwicker_loudness_t *out) {
  (void)method;
  if (!engine || !signal || !signal->samples || !out)
    return AE_ERROR_INVALID_PARAM;

  ae_spectral_features_t features;
  ae_loudness_t loudness;
  ae_result_t res = ae_analyze_spectrum(engine, signal, &features);
  if (res != AE_OK)
    return res;
  res = ae_analyze_loudness(engine, signal, AE_WEIGHTING_NONE, &loudness);
  if (res != AE_OK)
    return res;

  float total_energy = 0.0f;
  for (int i = 0; i < AE_NUM_BARK_BANDS; ++i)
    total_energy += features.bark_spectrum[i];

  memset(out->specific_loudness, 0, sizeof(out->specific_loudness));
  out->total_loudness_sone = 0.0f;
  out->loudness_level_phon = loudness.loudness_phon;
  out->peak_loudness_sone = 0.0f;
  out->peak_bark_band = 0;

  if (total_energy < AE_LOG_EPSILON)
    return AE_OK;

  float max_val = 0.0f;
  uint8_t max_band = 0;
  for (int i = 0; i < AE_NUM_BARK_BANDS; ++i) {
    float share = features.bark_spectrum[i] / total_energy;
    float spec = loudness.loudness_sone * share;
    out->specific_loudness[i] = spec;
    out->total_loudness_sone += spec;
    if (spec > max_val) {
      max_val = spec;
      max_band = (uint8_t)i;
    }
  }

  out->peak_loudness_sone = max_val;
  out->peak_bark_band = max_band;
  return AE_OK;
}

/*============================================================================
 * BMLD (Binaural Masking Level Difference)
 * Based on Durlach (1963) Equalization-Cancellation model
 *============================================================================*/

/**
 * Compute BMLD for various signal/noise configurations
 *
 * Common configurations:
 *   S0N0: signal_corr=+1, noise_corr=+1 → BMLD ≈ 0 dB
 *   SπN0: signal_corr=-1, noise_corr=+1 → BMLD ≈ 15 dB (low freq)
 *   S0Nπ: signal_corr=+1, noise_corr=-1 → BMLD ≈ 15 dB (low freq)
 */
AE_API float ae_compute_bmld(const ae_bmld_params_t *params) {
  if (!params)
    return 0.0f;

  float f = params->signal_frequency_hz;
  float rho_S = params->signal_correlation;
  float rho_N = params->noise_correlation;

  /* Clamp correlations to valid range */
  rho_S = ae_clamp(rho_S, -1.0f, 1.0f);
  rho_N = ae_clamp(rho_N, -1.0f, 1.0f);

  /* Frequency-dependent interaural processing limit
   * BMLD decreases at high frequencies due to phase ambiguity */
  float freq_factor = 1.0f;
  if (f > 500.0f) {
    freq_factor = 500.0f / f;
    freq_factor = ae_clamp(freq_factor, 0.1f, 1.0f);
  }

  /* Core EC model: BMLD depends on correlation difference
   * Maximum BMLD when signal and noise have opposite correlations */
  float correlation_diff = fabsf(rho_S - rho_N);

  /* Normalize to 0-1 range (max diff is 2 when rho_S=-1 and rho_N=+1) */
  correlation_diff = correlation_diff / 2.0f;

  /* Maximum theoretical BMLD (approximately 15 dB for SπN0 at 500 Hz) */
  float max_bmld = 15.0f;

  /* Simple model: BMLD proportional to correlation difference */
  float bmld = max_bmld * correlation_diff * freq_factor;

  /* Additional penalty for same-polarity correlations */
  if (rho_S * rho_N > 0) {
    bmld *= 0.5f;
  }

  return bmld;
}

/*============================================================================
 * SII (Speech Intelligibility Index) - ANSI S3.5
 *============================================================================*/

/* SII band importance values (simplified 7-band version) */
static const float SII_BAND_IMPORTANCE[7] = {0.0617f, 0.0671f, 0.0781f, 0.0997f,
                                             0.1104f, 0.1111f, 0.0987f};
/* Band center frequencies (Hz) */
static const float SII_BAND_CENTERS[7] = {250.0f,  500.0f,  1000.0f, 2000.0f,
                                          3000.0f, 4000.0f, 6000.0f};
/* Standard speech spectrum level (dB SPL) per band */
static const float SII_SPEECH_SPECTRUM[7] = {62.0f, 55.0f, 50.0f, 47.0f,
                                             45.0f, 44.0f, 42.0f};

/**
 * Compute Speech Intelligibility Index
 * Simplified ANSI S3.5 implementation
 */
AE_API ae_result_t ae_compute_sii(const ae_sii_params_t *params,
                                  float *sii_value) {
  if (!params || !sii_value) {
    return AE_ERROR_INVALID_PARAM;
  }

  float speech_db = params->speech_level_db;
  float noise_db = params->noise_level_db;
  float rt60 = params->rt60_seconds;

  float sii = 0.0f;

  for (int i = 0; i < 7; ++i) {
    /* Effective speech level at this band */
    float speech_band = SII_SPEECH_SPECTRUM[i] + (speech_db - 65.0f);

    /* Effective noise level at this band */
    float noise_band = noise_db;

    /* Reverberant speech energy (adds to effective noise) */
    float reverb_contrib = 0.0f;
    if (rt60 > 0.0f) {
      /* Simplified reverb masking: longer RT60 = more self-masking */
      reverb_contrib = 10.0f * log10f(1.0f + rt60 * 2.0f);
    }
    noise_band += reverb_contrib;

    /* Signal-to-noise ratio in this band */
    float snr = speech_band - noise_band;

    /* Audibility function (0-1) */
    float audibility;
    if (snr >= 15.0f) {
      audibility = 1.0f;
    } else if (snr <= -15.0f) {
      audibility = 0.0f;
    } else {
      audibility = (snr + 15.0f) / 30.0f;
    }

    /* Weight by band importance */
    sii += SII_BAND_IMPORTANCE[i] * audibility;
  }

  /* Normalize to approximate total importance (~0.57 for these 7 bands) */
  float total_importance = 0.0f;
  for (int i = 0; i < 7; ++i) {
    total_importance += SII_BAND_IMPORTANCE[i];
  }
  sii /= total_importance;

  *sii_value = ae_clamp(sii, 0.0f, 1.0f);
  return AE_OK;
}

/*============================================================================
 * Extended BMLD (Full EC Model - Durlach 1963)
 *============================================================================*/

/**
 * Compute extended BMLD with full Equalization-Cancellation model
 * Includes ITD effects and configurable processing errors
 */
AE_API float ae_compute_bmld_extended(const ae_bmld_extended_params_t *params) {
  if (!params)
    return 0.0f;

  float f = params->signal_frequency_hz;
  float rho_S = ae_clamp(params->signal_correlation, -1.0f, 1.0f);
  float rho_N = ae_clamp(params->noise_correlation, -1.0f, 1.0f);

  /* EC model errors (Durlach 1963 typical values) */
  float sigma_eps = params->equalization_error > 0.0f
                        ? params->equalization_error
                        : 0.25f; /* Amplitude jitter */
  float sigma_delta = params->cancellation_error > 0.0f
                          ? params->cancellation_error
                          : 0.0001f; /* Time jitter (s) */

  /* ITD phase difference at signal frequency */
  float signal_phase = 2.0f * (float)M_PI * f * (params->signal_itd_us * 1e-6f);
  float noise_phase = 2.0f * (float)M_PI * f * (params->noise_itd_us * 1e-6f);

  /* Effective correlation including ITD */
  float eff_signal_corr = rho_S * cosf(signal_phase);
  float eff_noise_corr = rho_N * cosf(noise_phase);

  /* Processing efficiency decreases with frequency (phase ambiguity) */
  float freq_efficiency = 1.0f;
  if (f > 500.0f) {
    freq_efficiency = expf(-0.001f * (f - 500.0f));
    freq_efficiency = ae_clamp(freq_efficiency, 0.1f, 1.0f);
  }

  /* EC model: internal noise due to processing imperfection */
  float internal_noise =
      sigma_eps * sigma_eps + (2.0f * (float)M_PI * f * sigma_delta) *
                                  (2.0f * (float)M_PI * f * sigma_delta);

  /* BMLD computation following EC model */
  float correlation_diff = fabsf(eff_signal_corr - eff_noise_corr);

  /* Maximum BMLD (approximately 15 dB for optimal conditions) */
  float max_bmld = 15.0f;

  /* EC model advantage */
  float ec_factor = correlation_diff / (1.0f + internal_noise);

  float bmld = max_bmld * ec_factor * freq_efficiency;

  /* Reduce advantage for same-polarity correlations */
  if (rho_S * rho_N > 0) {
    bmld *= 0.5f;
  }

  return ae_clamp(bmld, 0.0f, max_bmld);
}

/*============================================================================
 * Extended SII (21-band ANSI S3.5 with hearing loss)
 *============================================================================*/

/* ANSI S3.5 21-band center frequencies (Hz) */
static const float SII_21_CENTERS[21] = {
    160.0f,  200.0f,  250.0f,  315.0f,  400.0f,   500.0f,   630.0f,
    800.0f,  1000.0f, 1250.0f, 1600.0f, 2000.0f,  2500.0f,  3150.0f,
    4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f};

/* ANSI S3.5 Table 1 band importance function (critical band procedure) */
static const float SII_21_IMPORTANCE[21] = {
    0.0083f, 0.0095f, 0.0150f, 0.0289f, 0.0440f, 0.0578f, 0.0653f,
    0.0711f, 0.0818f, 0.0844f, 0.0873f, 0.0802f, 0.0706f, 0.0617f,
    0.0532f, 0.0401f, 0.0268f, 0.0184f, 0.0083f, 0.0049f, 0.0025f};

/* Standard speech spectrum at normal vocal effort (dB) */
static const float SII_21_SPEECH[21] = {
    32.41f, 34.48f, 34.75f, 33.98f, 34.59f, 34.27f, 32.06f,
    28.30f, 25.01f, 23.00f, 20.15f, 17.32f, 13.18f, 11.55f,
    9.33f,  5.31f,  2.59f,  1.13f,  0.00f,  0.00f,  0.00f};

/* Reference internal noise floor (dB) */
static const float SII_21_INTERNAL_NOISE[21] = {
    0.60f,   -1.70f,  -3.90f,  -6.10f,  -8.20f,  -9.70f,  -10.80f,
    -11.90f, -12.50f, -13.50f, -15.40f, -17.70f, -21.20f, -24.20f,
    -25.90f, -23.60f, -15.80f, -7.10f,  -6.20f,  -12.00f, -17.50f};

/**
 * Compute extended SII with full 21-band ANSI S3.5 and hearing loss modeling
 */
AE_API ae_result_t ae_compute_sii_extended(
    const ae_sii_extended_params_t *params, ae_sii_result_t *result) {
  if (!params || !result) {
    return AE_ERROR_INVALID_PARAM;
  }

  memset(result, 0, sizeof(ae_sii_result_t));
  int n_bands = params->use_21_band ? 21 : 7;
  result->n_bands = (uint8_t)n_bands;

  const float *importance =
      params->use_21_band ? SII_21_IMPORTANCE : SII_BAND_IMPORTANCE;
  const float *centers =
      params->use_21_band ? SII_21_CENTERS : SII_BAND_CENTERS;
  const float *speech_ref =
      params->use_21_band ? SII_21_SPEECH : SII_SPEECH_SPECTRUM;
  const float *internal = params->use_21_band ? SII_21_INTERNAL_NOISE : NULL;

  float sii_total = 0.0f;
  float importance_total = 0.0f;

  for (int i = 0; i < n_bands; ++i) {
    /* Effective speech level */
    float speech_band = speech_ref[i] + (params->speech_level_db - 65.0f);

    /* Noise level (per-band or flat) */
    float noise_band = params->noise_spectrum_db[i];
    if (noise_band == 0.0f && i == 0) {
      /* If no per-band noise, use flat spectrum assumption */
      noise_band = params->speech_level_db - 15.0f; /* Default 15 dB SNR */
    }

    /* Add internal noise floor */
    float internal_noise =
        (internal && params->use_21_band) ? internal[i] : 0.0f;
    float effective_noise = 10.0f * log10f(powf(10.0f, noise_band / 10.0f) +
                                           powf(10.0f, internal_noise / 10.0f));

    /* Hearing loss effect: raise effective threshold */
    if (params->model_hearing_loss) {
      float hl_threshold = params->hearing_threshold_db[i];
      if (hl_threshold > 0.0f) {
        /* Hearing loss reduces effective speech level */
        speech_band -= hl_threshold * 0.5f; /* 50% rule approximation */
        /* Add recruitment effect for severe losses */
        if (hl_threshold > 40.0f) {
          float recruitment = (hl_threshold - 40.0f) * 0.1f;
          result->distortion_factor[i] =
              ae_clamp(recruitment / 10.0f, 0.0f, 0.5f);
        }
      }
    }

    /* Reverb contribution */
    if (params->rt60_seconds > 0.0f) {
      float reverb = 10.0f * log10f(1.0f + params->rt60_seconds * 2.0f);
      effective_noise += reverb;
    }

    /* SNR and audibility */
    float snr = speech_band - effective_noise;
    float audibility;
    if (snr >= 15.0f) {
      audibility = 1.0f;
    } else if (snr <= -15.0f) {
      audibility = 0.0f;
    } else {
      audibility = (snr + 15.0f) / 30.0f;
    }

    /* Apply distortion factor */
    audibility *= (1.0f - result->distortion_factor[i]);

    result->audibility[i] = audibility;
    result->band_sii[i] = importance[i] * audibility;
    sii_total += result->band_sii[i];
    importance_total += importance[i];
  }

  /* Normalize */
  if (importance_total > 0.0f) {
    result->sii_value = ae_clamp(sii_total / importance_total, 0.0f, 1.0f);
  } else {
    result->sii_value = 0.0f;
  }

  return AE_OK;
}

/**
 * Compute binaural SII with BMLD advantage
 * Combines better-ear listening with binaural masking release
 */
AE_API ae_result_t ae_compute_binaural_sii(
    const ae_binaural_sii_params_t *params, float *sii_value) {
  if (!params || !sii_value) {
    return AE_ERROR_INVALID_PARAM;
  }

  /* Compute monaural SII for each ear */
  float sii_left = 0.0f;
  float sii_right = 0.0f;
  ae_compute_sii(&params->left, &sii_left);
  ae_compute_sii(&params->right, &sii_right);

  /* Better-ear effect: use the better ear as baseline */
  float better_ear_sii = fmaxf(sii_left, sii_right);

  /* BMLD advantage improves effective SNR */
  float bmld_db = params->bmld_advantage_db;
  if (bmld_db > 0.0f) {
    /* Convert BMLD to SII improvement (approximately 0.02 per dB) */
    float sii_improvement = bmld_db * 0.02f;
    better_ear_sii += sii_improvement;
  }

  /* Binaural redundancy gain (small additional benefit from two ears) */
  float redundancy_gain = 0.0f;
  if (sii_left > 0.0f && sii_right > 0.0f) {
    redundancy_gain = 0.05f * fminf(sii_left, sii_right);
  }

  *sii_value = ae_clamp(better_ear_sii + redundancy_gain, 0.0f, 1.0f);
  return AE_OK;
}

/*============================================================================
 * Auditory Representation Pipeline
 *============================================================================*/

/**
 * Compute full auditory representation pipeline
 * Gammatone → IHC → Adaptation → Modulation
 */
AE_API ae_result_t ae_compute_auditory_representation(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    const ae_auditory_pipeline_config_t *config, ae_auditory_repr_t *out) {
  (void)engine; /* May be used for sample rate config */

  if (!signal || !config || !out || !signal->samples) {
    return AE_ERROR_INVALID_PARAM;
  }

  size_t n_samples = signal->frame_count;
  uint32_t n_audio_ch = config->gammatone.n_channels;

  /* Initialize output structure */
  memset(out, 0, sizeof(ae_auditory_repr_t));
  out->n_samples = n_samples;
  out->n_audio_channels = n_audio_ch;
  out->n_modulation_channels = config->modulation.n_channels;

  ae_gammatone_t *gt = NULL;
  ae_adaptloop_t **adaptloops = NULL;

  /* Step 1: Gammatone filterbank */
  if (config->compute_gammatone) {
    gt = ae_gammatone_create(&config->gammatone);
    if (!gt) {
      return AE_ERROR_OUT_OF_MEMORY;
    }

    out->gammatone_output = (float **)calloc(n_audio_ch, sizeof(float *));
    if (!out->gammatone_output) {
      ae_gammatone_destroy(gt);
      return AE_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t ch = 0; ch < n_audio_ch; ++ch) {
      out->gammatone_output[ch] = (float *)calloc(n_samples, sizeof(float));
      if (!out->gammatone_output[ch]) {
        ae_free_auditory_representation(out);
        ae_gammatone_destroy(gt);
        return AE_ERROR_OUT_OF_MEMORY;
      }
    }

    ae_result_t res = ae_gammatone_process(gt, signal->samples, n_samples,
                                           out->gammatone_output);
    if (res != AE_OK) {
      ae_free_auditory_representation(out);
      ae_gammatone_destroy(gt);
      return res;
    }
  }

  /* Step 2: IHC envelope */
  if (config->compute_ihc && out->gammatone_output) {
    out->ihc_output = (float **)calloc(n_audio_ch, sizeof(float *));
    if (!out->ihc_output) {
      ae_free_auditory_representation(out);
      ae_gammatone_destroy(gt);
      return AE_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t ch = 0; ch < n_audio_ch; ++ch) {
      out->ihc_output[ch] = (float *)calloc(n_samples, sizeof(float));
      if (!out->ihc_output[ch]) {
        ae_free_auditory_representation(out);
        ae_gammatone_destroy(gt);
        return AE_ERROR_OUT_OF_MEMORY;
      }

      ae_result_t res = ae_ihc_envelope(out->gammatone_output[ch], n_samples,
                                        &config->ihc, out->ihc_output[ch]);
      if (res != AE_OK) {
        ae_free_auditory_representation(out);
        ae_gammatone_destroy(gt);
        return res;
      }
    }
  }

  /* Step 3: Adaptation loop */
  if (config->compute_adaptation && out->ihc_output) {
    out->adaptation_output = (float **)calloc(n_audio_ch, sizeof(float *));
    adaptloops =
        (ae_adaptloop_t **)calloc(n_audio_ch, sizeof(ae_adaptloop_t *));
    if (!out->adaptation_output || !adaptloops) {
      ae_free_auditory_representation(out);
      ae_gammatone_destroy(gt);
      free(adaptloops);
      return AE_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t ch = 0; ch < n_audio_ch; ++ch) {
      out->adaptation_output[ch] = (float *)calloc(n_samples, sizeof(float));
      adaptloops[ch] = ae_adaptloop_create(&config->adaptation);
      if (!out->adaptation_output[ch] || !adaptloops[ch]) {
        for (uint32_t j = 0; j <= ch; ++j) {
          ae_adaptloop_destroy(adaptloops[j]);
        }
        free(adaptloops);
        ae_free_auditory_representation(out);
        ae_gammatone_destroy(gt);
        return AE_ERROR_OUT_OF_MEMORY;
      }

      ae_result_t res =
          ae_adaptloop_process(adaptloops[ch], out->ihc_output[ch], n_samples,
                               out->adaptation_output[ch]);
      ae_adaptloop_destroy(adaptloops[ch]);
      if (res != AE_OK) {
        for (uint32_t j = ch + 1; j < n_audio_ch; ++j) {
          ae_adaptloop_destroy(adaptloops[j]);
        }
        free(adaptloops);
        ae_free_auditory_representation(out);
        ae_gammatone_destroy(gt);
        return res;
      }
    }
    free(adaptloops);
  }

  /* Step 4: Modulation filterbank */
  if (config->compute_modulation && out->adaptation_output) {
    uint32_t n_mod_ch = config->modulation.n_channels;
    if (n_mod_ch == 0)
      n_mod_ch = 10; /* Default to 10 modulation channels */

    /* Calculate envelope sample rate (downsampled for modulation analysis) */
    uint32_t env_sample_rate = config->modulation.sample_rate;
    if (env_sample_rate == 0)
      env_sample_rate = 1000; /* Default 1kHz envelope rate */

    /* Downsample ratio */
    uint32_t audio_sr = config->gammatone.sample_rate;
    if (audio_sr == 0)
      audio_sr = 48000;
    size_t downsample_factor = audio_sr / env_sample_rate;
    if (downsample_factor < 1)
      downsample_factor = 1;
    size_t n_env_samples = n_samples / downsample_factor;
    if (n_env_samples < 1)
      n_env_samples = 1;

    /* Create modulation filterbank */
    ae_modfb_config_t mfb_config = config->modulation;
    mfb_config.n_channels = n_mod_ch;
    mfb_config.sample_rate = env_sample_rate;
    ae_modfb_t *mfb = ae_modfb_create(&mfb_config);
    if (!mfb) {
      ae_gammatone_destroy(gt);
      ae_free_auditory_representation(out);
      return AE_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate modulation output [n_audio_ch][n_mod_ch][n_env_samples] */
    out->modulation_output = (float ***)calloc(n_audio_ch, sizeof(float **));
    if (!out->modulation_output) {
      ae_modfb_destroy(mfb);
      ae_gammatone_destroy(gt);
      ae_free_auditory_representation(out);
      return AE_ERROR_OUT_OF_MEMORY;
    }

    /* Temporary buffer for downsampled envelope */
    float *env_buffer = (float *)calloc(n_env_samples, sizeof(float));
    if (!env_buffer) {
      ae_modfb_destroy(mfb);
      ae_gammatone_destroy(gt);
      ae_free_auditory_representation(out);
      return AE_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t ch = 0; ch < n_audio_ch; ++ch) {
      out->modulation_output[ch] = (float **)calloc(n_mod_ch, sizeof(float *));
      if (!out->modulation_output[ch]) {
        free(env_buffer);
        ae_modfb_destroy(mfb);
        ae_gammatone_destroy(gt);
        ae_free_auditory_representation(out);
        return AE_ERROR_OUT_OF_MEMORY;
      }

      /* Allocate output for each modulation channel */
      for (uint32_t m = 0; m < n_mod_ch; ++m) {
        out->modulation_output[ch][m] =
            (float *)calloc(n_env_samples, sizeof(float));
        if (!out->modulation_output[ch][m]) {
          free(env_buffer);
          ae_modfb_destroy(mfb);
          ae_gammatone_destroy(gt);
          ae_free_auditory_representation(out);
          return AE_ERROR_OUT_OF_MEMORY;
        }
      }

      /* Downsample adaptation output to envelope rate */
      for (size_t i = 0; i < n_env_samples; ++i) {
        size_t src_idx = i * downsample_factor;
        if (src_idx < n_samples) {
          env_buffer[i] = out->adaptation_output[ch][src_idx];
        }
      }

      /* Apply modulation filterbank */
      ae_result_t res = ae_modfb_process(mfb, env_buffer, n_env_samples,
                                         out->modulation_output[ch]);
      if (res != AE_OK) {
        free(env_buffer);
        ae_modfb_destroy(mfb);
        ae_gammatone_destroy(gt);
        ae_free_auditory_representation(out);
        return res;
      }
    }

    free(env_buffer);
    ae_modfb_destroy(mfb);
    out->n_modulation_channels = n_mod_ch;
  }

  ae_gammatone_destroy(gt);
  return AE_OK;
}

/**
 * Free auditory representation memory
 */
AE_API void ae_free_auditory_representation(ae_auditory_repr_t *repr) {
  if (!repr)
    return;

  if (repr->gammatone_output) {
    for (uint32_t i = 0; i < repr->n_audio_channels; ++i) {
      free(repr->gammatone_output[i]);
    }
    free(repr->gammatone_output);
  }

  if (repr->ihc_output) {
    for (uint32_t i = 0; i < repr->n_audio_channels; ++i) {
      free(repr->ihc_output[i]);
    }
    free(repr->ihc_output);
  }

  if (repr->adaptation_output) {
    for (uint32_t i = 0; i < repr->n_audio_channels; ++i) {
      free(repr->adaptation_output[i]);
    }
    free(repr->adaptation_output);
  }

  if (repr->modulation_output) {
    for (uint32_t i = 0; i < repr->n_audio_channels; ++i) {
      if (repr->modulation_output[i]) {
        for (uint32_t j = 0; j < repr->n_modulation_channels; ++j) {
          free(repr->modulation_output[i][j]);
        }
        free(repr->modulation_output[i]);
      }
    }
    free(repr->modulation_output);
  }

  memset(repr, 0, sizeof(ae_auditory_repr_t));
}
