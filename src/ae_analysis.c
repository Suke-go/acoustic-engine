#include "ae_internal.h"

typedef struct {
  float re;
  float im;
} ae_complex_t;

static void ae_fft(ae_complex_t *data, size_t n) {
  size_t j = 0;
  for (size_t i = 1; i < n; ++i) {
    size_t bit = n >> 1;
    while (j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;
    if (i < j) {
      ae_complex_t tmp = data[i];
      data[i] = data[j];
      data[j] = tmp;
    }
  }

  for (size_t len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * (float)M_PI / (float)len;
    float wlen_re = cosf(ang);
    float wlen_im = sinf(ang);
    for (size_t i = 0; i < n; i += len) {
      float w_re = 1.0f;
      float w_im = 0.0f;
      for (size_t j2 = 0; j2 < len / 2; ++j2) {
        ae_complex_t u = data[i + j2];
        ae_complex_t v = data[i + j2 + len / 2];
        float vr = v.re * w_re - v.im * w_im;
        float vi = v.re * w_im + v.im * w_re;
        data[i + j2].re = u.re + vr;
        data[i + j2].im = u.im + vi;
        data[i + j2 + len / 2].re = u.re - vr;
        data[i + j2 + len / 2].im = u.im - vi;
        float next_re = w_re * wlen_re - w_im * wlen_im;
        float next_im = w_re * wlen_im + w_im * wlen_re;
        w_re = next_re;
        w_im = next_im;
      }
    }
  }
}

static float ae_energy_sum(const float *samples, size_t n) {
  float sum = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    sum += samples[i] * samples[i];
  }
  return sum;
}

static float ae_calculate_spectral_centroid_lr(const float *left,
                                               const float *right,
                                               size_t frames,
                                               uint32_t sample_rate) {
  if (!left || !right || frames < 2 || sample_rate == 0)
    return 0.0f;

  size_t nfft = ae_next_pow2(frames);
  if (nfft < 2)
    return 0.0f;

  ae_complex_t *data = (ae_complex_t *)calloc(nfft, sizeof(ae_complex_t));
  if (!data)
    return 0.0f;

  for (size_t i = 0; i < frames; ++i) {
    float mono = 0.5f * (left[i] + right[i]);
    float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i /
                                 (float)(frames - 1)));
    data[i].re = mono * w;
    data[i].im = 0.0f;
  }

  ae_fft(data, nfft);

  float bin_hz = (float)sample_rate / (float)nfft;
  float sum_fm = 0.0f;
  float sum_m = 0.0f;
  for (size_t i = 1; i < nfft / 2; ++i) {
    float re = data[i].re;
    float im = data[i].im;
    float m2 = re * re + im * im;
    sum_fm += ((float)i * bin_hz) * m2;
    sum_m += m2;
  }

  free(data);
  if (sum_m < AE_LOG_EPSILON)
    return 0.0f;
  return sum_fm / sum_m;
}

static float ae_calculate_c50(const float *ir, size_t length,
                              uint32_t sample_rate) {
  size_t n50 = (size_t)(0.050f * sample_rate);
  if (n50 >= length)
    n50 = length - 1;
  float e_early = 0.0f;
  float e_late = 0.0f;
  for (size_t i = 0; i < n50; ++i)
    e_early += ir[i] * ir[i];
  for (size_t i = n50; i < length; ++i)
    e_late += ir[i] * ir[i];
  if (e_late < AE_LOG_EPSILON)
    return 100.0f;
  return 10.0f * log10f(e_early / e_late);
}

static float ae_calculate_c80(const float *ir, size_t length,
                              uint32_t sample_rate) {
  size_t n80 = (size_t)(0.080f * sample_rate);
  if (n80 >= length)
    n80 = length - 1;
  float e_early = 0.0f;
  float e_late = 0.0f;
  for (size_t i = 0; i < n80; ++i)
    e_early += ir[i] * ir[i];
  for (size_t i = n80; i < length; ++i)
    e_late += ir[i] * ir[i];
  if (e_late < AE_LOG_EPSILON)
    return 100.0f;
  return 10.0f * log10f(e_early / e_late);
}

static float ae_calculate_d50(const float *ir, size_t length,
                              uint32_t sample_rate) {
  size_t n50 = (size_t)(0.050f * sample_rate);
  if (n50 >= length)
    n50 = length - 1;
  float e_early = 0.0f;
  float e_total = 0.0f;
  for (size_t i = 0; i < n50; ++i)
    e_early += ir[i] * ir[i];
  for (size_t i = 0; i < length; ++i)
    e_total += ir[i] * ir[i];
  if (e_total < AE_LOG_EPSILON)
    return 0.0f;
  return e_early / e_total;
}

static float ae_calculate_ts(const float *ir, size_t length,
                             uint32_t sample_rate) {
  float sum_te = 0.0f;
  float sum_e = 0.0f;
  for (size_t i = 0; i < length; ++i) {
    float t = (float)i / (float)sample_rate;
    float e = ir[i] * ir[i];
    sum_te += t * e;
    sum_e += e;
  }
  if (sum_e < AE_LOG_EPSILON)
    return 0.0f;
  return (sum_te / sum_e) * 1000.0f;
}

static float ae_calculate_edt(const float *ir, size_t length,
                              uint32_t sample_rate) {
  float *schroeder = (float *)malloc(length * sizeof(float));
  if (!schroeder)
    return 0.0f;
  schroeder[length - 1] = ir[length - 1] * ir[length - 1];
  for (size_t i = length - 1; i > 0; --i) {
    schroeder[i - 1] = schroeder[i] + ir[i - 1] * ir[i - 1];
  }
  float max_val = schroeder[0];
  if (max_val < AE_LOG_EPSILON) {
    free(schroeder);
    return 0.0f;
  }
  for (size_t i = 0; i < length; ++i) {
    schroeder[i] = 10.0f * log10f(schroeder[i] / max_val + AE_LOG_EPSILON);
  }
  size_t t10 = length - 1;
  for (size_t i = 0; i < length; ++i) {
    if (schroeder[i] <= -10.0f) {
      t10 = i;
      break;
    }
  }
  free(schroeder);
  return 6.0f * (float)t10 / (float)sample_rate;
}

static float ae_calculate_iacc(const float *left, const float *right,
                               size_t length, size_t max_lag) {
  size_t start = max_lag;
  size_t end = length > max_lag ? length - max_lag : 0;
  if (end <= start)
    return 1.0f;

  float el = 0.0f;
  float er = 0.0f;
  for (size_t i = start; i < end; ++i) {
    el += left[i] * left[i];
    er += right[i] * right[i];
  }
  float denom = sqrtf(el * er);
  if (denom < AE_LOG_EPSILON)
    return 1.0f;

  float max_corr = 0.0f;
  for (int lag = -(int)max_lag; lag <= (int)max_lag; ++lag) {
    float corr = 0.0f;
    for (size_t i = start; i < end; ++i) {
      size_t idx = (size_t)((int)i + lag);
      corr += left[i] * right[idx];
    }
    float abs_corr = fabsf(corr);
    if (abs_corr > max_corr)
      max_corr = abs_corr;
  }
  return max_corr / denom;
}

AE_API ae_result_t ae_compute_perceptual_metrics(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    ae_perceptual_metrics_t *out) {
  if (!engine || !signal || !signal->samples || !out)
    return AE_ERROR_INVALID_PARAM;
  if (signal->channels < 1 || signal->channels > 2)
    return AE_ERROR_INVALID_PARAM;

  size_t frames = signal->frame_count;
  if (frames == 0)
    return AE_ERROR_INVALID_PARAM;

  float *left = (float *)malloc(frames * sizeof(float));
  float *right = (float *)malloc(frames * sizeof(float));
  if (!left || !right) {
    free(left);
    free(right);
    return AE_ERROR_OUT_OF_MEMORY;
  }

  if (signal->channels == 1) {
    if (signal->interleaved) {
      for (size_t i = 0; i < frames; ++i) {
        left[i] = signal->samples[i];
        right[i] = signal->samples[i];
      }
    } else {
      for (size_t i = 0; i < frames; ++i) {
        left[i] = signal->samples[i];
        right[i] = signal->samples[i];
      }
    }
  } else {
    if (signal->interleaved) {
      for (size_t i = 0; i < frames; ++i) {
        left[i] = signal->samples[i * 2];
        right[i] = signal->samples[i * 2 + 1];
      }
    } else {
      for (size_t i = 0; i < frames; ++i) {
        left[i] = signal->samples[i];
        right[i] = signal->samples[i + frames];
      }
    }
  }

  size_t early_samples = (size_t)(0.080f * engine->config.sample_rate);
  if (early_samples > frames)
    early_samples = frames;
  size_t late_samples = frames > early_samples ? frames - early_samples : 0;

  size_t max_lag = (size_t)(0.001f * engine->config.sample_rate);
  if (max_lag < 1)
    max_lag = 1;
  if (max_lag * 2 >= frames)
    max_lag = frames / 2;

  out->iacc_early = ae_calculate_iacc(left, right, early_samples, max_lag);
  if (late_samples > max_lag * 2) {
    out->iacc_late =
        ae_calculate_iacc(left + early_samples, right + early_samples,
                          late_samples, max_lag);
  } else {
    out->iacc_late = out->iacc_early;
  }

  float direct_energy = ae_energy_sum(left, early_samples) +
                        ae_energy_sum(right, early_samples);
  float late_energy =
      ae_energy_sum(left + early_samples, late_samples) +
      ae_energy_sum(right + early_samples, late_samples);
  if (late_energy < AE_LOG_EPSILON)
    out->drr_db = 60.0f;
  else
    out->drr_db = 10.0f * log10f(direct_energy / late_energy);

  float sum_m = 0.0f;
  for (size_t i = 0; i < frames; ++i) {
    float sample = 0.5f * (left[i] + right[i]);
    sum_m += sample * sample;
  }
  out->spectral_centroid = ae_calculate_spectral_centroid_lr(
      left, right, frames, engine->config.sample_rate);

  float rms = sqrtf(sum_m / (float)frames);
  float rms_db = ae_to_db(rms);
  float phon = rms_db + 100.0f;
  out->loudness_sone = ae_phon_to_sone(phon);

  out->clarity_c50 = ae_calculate_c50(left, frames, engine->config.sample_rate);
  out->clarity_c80 = ae_calculate_c80(left, frames, engine->config.sample_rate);

  free(left);
  free(right);
  return AE_OK;
}

AE_API ae_result_t ae_compute_room_metrics(const float *impulse_response,
                                           size_t ir_length,
                                           uint32_t sample_rate,
                                           ae_room_metrics_t *out) {
  if (!impulse_response || !out || ir_length == 0 || sample_rate == 0)
    return AE_ERROR_INVALID_PARAM;

  out->edt = ae_calculate_edt(impulse_response, ir_length, sample_rate);
  for (size_t i = 0; i < 6; ++i) {
    out->edt_band[i] = out->edt;
  }
  out->c50 = ae_calculate_c50(impulse_response, ir_length, sample_rate);
  out->c80 = ae_calculate_c80(impulse_response, ir_length, sample_rate);
  out->d50 = ae_calculate_d50(impulse_response, ir_length, sample_rate);
  out->ts_ms = ae_calculate_ts(impulse_response, ir_length, sample_rate);

  float energy = ae_energy_sum(impulse_response, ir_length);
  out->strength_g = 10.0f * log10f(fmaxf(energy, AE_LOG_EPSILON));
  out->sti = ae_clamp(0.05f * (out->c50 + 10.0f), 0.0f, 1.0f);

  return AE_OK;
}

AE_API ae_result_t ae_analyze_spectrum(ae_engine_t *engine,
                                       const ae_audio_buffer_t *signal,
                                       ae_spectral_features_t *out) {
  if (!engine || !signal || !signal->samples || !out)
    return AE_ERROR_INVALID_PARAM;
  if (signal->channels < 1 || signal->channels > 2)
    return AE_ERROR_INVALID_PARAM;

  size_t frames = signal->frame_count;
  if (frames < 2)
    return AE_ERROR_INVALID_PARAM;

  size_t nfft = ae_next_pow2(frames);
  if (nfft < 2)
    return AE_ERROR_INVALID_PARAM;

  ae_complex_t *data = (ae_complex_t *)calloc(nfft, sizeof(ae_complex_t));
  float *mag = (float *)calloc(nfft / 2, sizeof(float));
  float *mono = (float *)calloc(frames, sizeof(float));
  if (!data || !mag || !mono) {
    free(data);
    free(mag);
    free(mono);
    return AE_ERROR_OUT_OF_MEMORY;
  }

  if (signal->channels == 1) {
    if (signal->interleaved) {
      for (size_t i = 0; i < frames; ++i)
        mono[i] = signal->samples[i];
    } else {
      for (size_t i = 0; i < frames; ++i)
        mono[i] = signal->samples[i];
    }
  } else {
    if (signal->interleaved) {
      for (size_t i = 0; i < frames; ++i) {
        mono[i] = 0.5f * (signal->samples[i * 2] + signal->samples[i * 2 + 1]);
      }
    } else {
      for (size_t i = 0; i < frames; ++i) {
        mono[i] =
            0.5f * (signal->samples[i] + signal->samples[i + frames]);
      }
    }
  }

  for (size_t i = 0; i < frames; ++i) {
    float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i /
                                 (float)(frames - 1)));
    data[i].re = mono[i] * w;
    data[i].im = 0.0f;
  }

  ae_fft(data, nfft);

  for (size_t i = 0; i < nfft / 2; ++i) {
    float re = data[i].re;
    float im = data[i].im;
    mag[i] = sqrtf(re * re + im * im);
  }

  float bin_hz = (float)engine->config.sample_rate / (float)nfft;
  float sum_fm = 0.0f;
  float sum_m = 0.0f;
  float sum_dev = 0.0f;
  float geom_sum = 0.0f;
  float energy = 0.0f;

  memset(out->bark_spectrum, 0, sizeof(out->bark_spectrum));

  for (size_t i = 1; i < nfft / 2; ++i) {
    float f = (float)i * bin_hz;
    float m2 = mag[i] * mag[i];
    sum_fm += f * m2;
    sum_m += m2;
    energy += m2;
    geom_sum += logf(m2 + AE_LOG_EPSILON);

    float bark = ae_hz_to_bark(f);
    int band = (int)floorf(bark);
    if (band < 0)
      band = 0;
    if (band >= AE_NUM_BARK_BANDS)
      band = AE_NUM_BARK_BANDS - 1;
    out->bark_spectrum[band] += m2;
  }

  if (sum_m < AE_LOG_EPSILON) {
    out->centroid_hz = 0.0f;
    out->spread_hz = 0.0f;
  } else {
    out->centroid_hz = sum_fm / sum_m;
    for (size_t i = 1; i < nfft / 2; ++i) {
      float f = (float)i * bin_hz;
      float m2 = mag[i] * mag[i];
      float diff = f - out->centroid_hz;
      sum_dev += diff * diff * m2;
    }
    out->spread_hz = sqrtf(sum_dev / sum_m);
  }

  float arith_mean = sum_m / (float)(nfft / 2);
  float geom_mean = expf(geom_sum / (float)(nfft / 2));
  out->flatness =
      (arith_mean > AE_LOG_EPSILON) ? (geom_mean / arith_mean) : 0.0f;

  float rolloff_energy = energy * 0.85f;
  float cumulative = 0.0f;
  out->rolloff_hz = 0.0f;
  for (size_t i = 1; i < nfft / 2; ++i) {
    float m2 = mag[i] * mag[i];
    cumulative += m2;
    if (cumulative >= rolloff_energy) {
      out->rolloff_hz = (float)i * bin_hz;
      break;
    }
  }

  float max_mag = 0.0f;
  float noise_sum = 0.0f;
  size_t noise_count = 0;
  for (size_t i = 1; i < nfft / 2; ++i) {
    if (mag[i] > max_mag)
      max_mag = mag[i];
  }
  for (size_t i = 1; i < nfft / 2; ++i) {
    if (mag[i] < max_mag * 0.5f) {
      noise_sum += mag[i] * mag[i];
      noise_count++;
    }
  }
  float noise_avg = noise_count > 0 ? noise_sum / noise_count : AE_LOG_EPSILON;
  out->hnr_db = 10.0f * log10f((max_mag * max_mag) / noise_avg);

  if (!engine->prev_mag || engine->prev_mag_len != nfft / 2) {
    free(engine->prev_mag);
    engine->prev_mag = (float *)calloc(nfft / 2, sizeof(float));
    engine->prev_mag_len = nfft / 2;
    out->flux = 0.0f;
  } else {
    float flux = 0.0f;
    for (size_t i = 0; i < nfft / 2; ++i) {
      float diff = mag[i] - engine->prev_mag[i];
      flux += diff * diff;
    }
    out->flux = flux / (float)(nfft / 2);
  }

  if (engine->prev_mag && engine->prev_mag_len == nfft / 2) {
    memcpy(engine->prev_mag, mag, sizeof(float) * (nfft / 2));
  }

  free(data);
  free(mag);
  free(mono);
  return AE_OK;
}

static void ae_apply_iir(const float *b, const float *a, float *samples,
                         size_t n) {
  float z[6] = {0};
  for (size_t i = 0; i < n; ++i) {
    float in = samples[i];
    float out = b[0] * in + z[0];
    z[0] = b[1] * in - a[1] * out + z[1];
    z[1] = b[2] * in - a[2] * out + z[2];
    z[2] = b[3] * in - a[3] * out + z[3];
    z[3] = b[4] * in - a[4] * out + z[4];
    z[4] = b[5] * in - a[5] * out + z[5];
    z[5] = b[6] * in - a[6] * out;
    samples[i] = out;
  }
}

AE_API ae_result_t ae_analyze_loudness(ae_engine_t *engine,
                                       const ae_audio_buffer_t *signal,
                                       ae_weighting_t weighting,
                                       ae_loudness_t *out) {
  if (!engine || !signal || !signal->samples || !out)
    return AE_ERROR_INVALID_PARAM;
  if (signal->channels < 1 || signal->channels > 2)
    return AE_ERROR_INVALID_PARAM;

  size_t frames = signal->frame_count;
  if (frames == 0)
    return AE_ERROR_INVALID_PARAM;

  float *mono = (float *)calloc(frames, sizeof(float));
  if (!mono)
    return AE_ERROR_OUT_OF_MEMORY;

  if (signal->channels == 1) {
    if (signal->interleaved) {
      for (size_t i = 0; i < frames; ++i)
        mono[i] = signal->samples[i];
    } else {
      for (size_t i = 0; i < frames; ++i)
        mono[i] = signal->samples[i];
    }
  } else {
    if (signal->interleaved) {
      for (size_t i = 0; i < frames; ++i) {
        mono[i] = 0.5f * (signal->samples[i * 2] + signal->samples[i * 2 + 1]);
      }
    } else {
      for (size_t i = 0; i < frames; ++i) {
        mono[i] =
            0.5f * (signal->samples[i] + signal->samples[i + frames]);
      }
    }
  }

  if (weighting == AE_WEIGHTING_A || weighting == AE_WEIGHTING_ITU_R_468) {
    static const float b[7] = {0.2343f, -0.4686f, -0.2343f, 0.9372f,
                               -0.2343f, -0.4686f, 0.2343f};
    static const float a[7] = {1.0000f, -4.0195f, 6.1894f, -4.4532f,
                               1.4208f, -0.1418f, 0.0043f};
    ae_apply_iir(b, a, mono, frames);
  }

  float peak = 0.0f;
  float sum = 0.0f;
  for (size_t i = 0; i < frames; ++i) {
    float val = fabsf(mono[i]);
    if (val > peak)
      peak = val;
    sum += mono[i] * mono[i];
  }

  float mean_square = sum / (float)frames;
  float rms = sqrtf(mean_square);
  out->peak_db = ae_to_db(peak);
  out->rms_db = ae_to_db(rms);
  out->lufs = -0.691f + 10.0f * log10f(fmaxf(mean_square, AE_LOG_EPSILON));
  out->loudness_phon = out->rms_db + 100.0f;
  out->loudness_sone = ae_phon_to_sone(out->loudness_phon);

  size_t segment = (size_t)(0.1f * engine->config.sample_rate);
  if (segment < 1)
    segment = 1;
  float min_db = 1000.0f;
  float max_db = -1000.0f;
  for (size_t start = 0; start < frames; start += segment) {
    size_t end = start + segment;
    if (end > frames)
      end = frames;
    float seg_sum = 0.0f;
    for (size_t i = start; i < end; ++i)
      seg_sum += mono[i] * mono[i];
    float seg_rms = sqrtf(seg_sum / (float)(end - start));
    float seg_db = ae_to_db(seg_rms);
    if (seg_db < min_db)
      min_db = seg_db;
    if (seg_db > max_db)
      max_db = seg_db;
  }
  out->lra = max_db - min_db;

  engine->last_lufs = out->lufs;
  free(mono);
  return AE_OK;
}

AE_API ae_result_t ae_normalize_loudness(ae_engine_t *engine,
                                         float target_lufs) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  if (engine->last_lufs <= -100.0f)
    return AE_ERROR_NOT_INITIALIZED;
  float gain_db = target_lufs - engine->last_lufs;
  engine->output_gain = ae_db_to_linear(gain_db);
  return AE_OK;
}

AE_API ae_result_t ae_timbral_to_processing(const ae_timbral_params_t *timbral,
                                            ae_main_params_t *main_out,
                                            ae_extended_params_t *extended_out) {
  if (!timbral || !main_out)
    return AE_ERROR_INVALID_PARAM;
  ae_main_params_t main_params = {10.0f, 0.5f, 0.0f, 1.0f, 0.5f, 1.0f};
  ae_extended_params_t ext_params = {3.0f, 0.5f, 0.0f, 0.0f};

  float brightness =
      (timbral->presence * 0.6f + timbral->air * 0.8f - timbral->warmth * 0.7f);
  main_params.brightness = ae_clamp(brightness, -1.0f, 1.0f);
  main_params.width = ae_clamp(1.0f + timbral->air * 0.5f, 0.0f, 2.0f);
  main_params.dry_wet =
      ae_clamp(0.5f - timbral->clarity * 0.3f + timbral->warmth * 0.1f, 0.0f,
               1.0f);

  ext_params.lofi_amount = ae_clamp(timbral->roughness / 5.0f, 0.0f, 1.0f);
  ext_params.modulation = ae_clamp(timbral->fluctuation / 5.0f, 0.0f, 1.0f);
  ext_params.diffusion = ae_clamp(0.4f + timbral->air * 0.4f, 0.0f, 1.0f);

  *main_out = main_params;
  if (extended_out)
    *extended_out = ext_params;
  return AE_OK;
}

AE_API ae_result_t ae_analyze_timbral(ae_engine_t *engine,
                                      const ae_audio_buffer_t *signal,
                                      ae_timbral_params_t *out) {
  if (!engine || !signal || !out)
    return AE_ERROR_INVALID_PARAM;
  ae_spectral_features_t features;
  ae_loudness_t loudness;

  ae_result_t res = ae_analyze_spectrum(engine, signal, &features);
  if (res != AE_OK)
    return res;
  res = ae_analyze_loudness(engine, signal, AE_WEIGHTING_NONE, &loudness);
  if (res != AE_OK)
    return res;

  float sharpness = features.centroid_hz / 6000.0f;
  out->sharpness = ae_clamp(sharpness * 4.0f, 0.0f, 4.0f);
  out->roughness = ae_clamp(features.flux * 10.0f, 0.0f, 5.0f);
  out->fluctuation = ae_clamp(features.flux * 6.0f, 0.0f, 5.0f);
  out->tonality = ae_clamp(1.0f - features.flatness, 0.0f, 1.0f);

  float low_energy = 0.0f;
  float mid_energy = 0.0f;
  float high_energy = 0.0f;
  for (int i = 0; i < AE_NUM_BARK_BANDS; ++i) {
    if (i < 6)
      low_energy += features.bark_spectrum[i];
    else if (i < 14)
      mid_energy += features.bark_spectrum[i];
    else
      high_energy += features.bark_spectrum[i];
  }
  float total = low_energy + mid_energy + high_energy + AE_LOG_EPSILON;
  out->warmth = ae_clamp(low_energy / total, 0.0f, 1.0f);
  out->presence = ae_clamp(mid_energy / total, 0.0f, 1.0f);
  out->air = ae_clamp(high_energy / total, 0.0f, 1.0f);
  out->body = ae_clamp(low_energy / total, 0.0f, 1.0f);
  out->clarity = ae_clamp(1.0f - features.flatness, 0.0f, 1.0f);
  out->punch = ae_clamp((loudness.rms_db + 60.0f) / 60.0f, 0.0f, 1.0f);
  return AE_OK;
}
