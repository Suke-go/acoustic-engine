#include "ae_internal.h"

#ifdef AE_USE_LIBMYSOFA
static void ae_spatial_az_el_to_xyz(float az_deg, float el_deg, float *x,
                                    float *y, float *z) {
  float az = az_deg * (float)M_PI / 180.0f;
  float el = el_deg * (float)M_PI / 180.0f;
  float cos_el = cosf(el);
  *x = cos_el * cosf(az);
  *y = cos_el * sinf(az);
  *z = sinf(el);
}

static void ae_spatial_unload_sofa(ae_engine_t *engine) {
  if (!engine)
    return;
  if (engine->hrtf.sofa) {
    mysofa_close(engine->hrtf.sofa);
    engine->hrtf.sofa = NULL;
  }
  free(engine->hrtf.hrir_l);
  free(engine->hrtf.hrir_r);
  free(engine->hrtf.history);
  engine->hrtf.hrir_l = NULL;
  engine->hrtf.hrir_r = NULL;
  engine->hrtf.history = NULL;
  engine->hrtf.hrir_len = 0;
  engine->hrtf.history_size = 0;
  engine->hrtf.history_index = 0;
  engine->hrtf.loaded = false;
}

static bool ae_spatial_alloc_hrtf_buffers(ae_engine_t *engine, size_t hrir_len) {
  if (!engine || hrir_len == 0)
    return false;
  engine->hrtf.hrir_l = (float *)calloc(hrir_len, sizeof(float));
  engine->hrtf.hrir_r = (float *)calloc(hrir_len, sizeof(float));
  engine->hrtf.history = (float *)calloc(hrir_len, sizeof(float));
  if (!engine->hrtf.hrir_l || !engine->hrtf.hrir_r ||
      !engine->hrtf.history) {
    free(engine->hrtf.hrir_l);
    free(engine->hrtf.hrir_r);
    free(engine->hrtf.history);
    engine->hrtf.hrir_l = NULL;
    engine->hrtf.hrir_r = NULL;
    engine->hrtf.history = NULL;
    return false;
  }
  engine->hrtf.hrir_len = hrir_len;
  engine->hrtf.history_size = hrir_len;
  engine->hrtf.history_index = 0;
  return true;
}

static bool ae_spatial_update_hrir(ae_engine_t *engine, float az_deg,
                                   float el_deg) {
  if (!engine || !engine->hrtf.sofa || !engine->hrtf.hrir_l ||
      !engine->hrtf.hrir_r)
    return false;
  if (fabsf(az_deg - engine->hrtf.last_azimuth) < 0.01f &&
      fabsf(el_deg - engine->hrtf.last_elevation) < 0.01f)
    return true;

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  ae_spatial_az_el_to_xyz(az_deg, el_deg, &x, &y, &z);

  float delay_l = 0.0f;
  float delay_r = 0.0f;
  int filter_len =
      mysofa_getfilter_float(engine->hrtf.sofa, x, y, z, engine->hrtf.hrir_l,
                             engine->hrtf.hrir_r, &delay_l, &delay_r);
  if (filter_len <= 0)
    return false;

  engine->hrtf.delay_l_samples = delay_l;
  engine->hrtf.delay_r_samples = delay_r;
  engine->hrtf.last_azimuth = az_deg;
  engine->hrtf.last_elevation = el_deg;
  return true;
}

static bool ae_spatial_load_sofa(ae_engine_t *engine, const char *path) {
  if (!engine || !path || path[0] == '\0')
    return false;

  ae_spatial_unload_sofa(engine);

  int err = 0;
  struct MYSOFA_EASY *sofa =
      mysofa_open(path, (float)engine->config.sample_rate, &err);
  if (!sofa || err != MYSOFA_OK) {
    if (sofa)
      mysofa_close(sofa);
    return false;
  }

  size_t hrir_len = (size_t)sofa->hrtf->N;
  if (hrir_len == 0) {
    mysofa_close(sofa);
    return false;
  }

  engine->hrtf.sofa = sofa;
  if (!ae_spatial_alloc_hrtf_buffers(engine, hrir_len)) {
    mysofa_close(sofa);
    engine->hrtf.sofa = NULL;
    return false;
  }

  engine->hrtf.delay_l_samples = 0.0f;
  engine->hrtf.delay_r_samples = 0.0f;
  engine->hrtf.last_azimuth = 9999.0f;
  engine->hrtf.last_elevation = 9999.0f;
  engine->hrtf.loaded = true;

  ae_clear_buffer(engine->hrtf.delay_l, engine->hrtf.delay_size);
  ae_clear_buffer(engine->hrtf.delay_r, engine->hrtf.delay_size);
  ae_clear_buffer(engine->hrtf.history, engine->hrtf.history_size);
  if (!ae_spatial_update_hrir(engine, 0.0f, 0.0f)) {
    ae_spatial_unload_sofa(engine);
    return false;
  }

  return true;
}
#endif

void ae_spatial_init(ae_engine_t *engine) {
  if (!engine)
    return;
  engine->hrtf.enabled = false;
  engine->hrtf.params.azimuth_deg = 0.0f;
  engine->hrtf.params.elevation_deg = 0.0f;
  engine->hrtf.params.itd_us = 0.0f;
  engine->hrtf.params.ild_db = 0.0f;
  engine->hrtf.itd_samples = 0;
  engine->hrtf.ild_gain_l = 1.0f;
  engine->hrtf.ild_gain_r = 1.0f;
  engine->hrtf.shadow_alpha = 0.0f;
  engine->hrtf.shadow_state_l = 0.0f;
  engine->hrtf.shadow_state_r = 0.0f;

  engine->hrtf.delay_size =
      (size_t)(engine->config.sample_rate * 0.01f) + 1;
  engine->hrtf.delay_l =
      (float *)calloc(engine->hrtf.delay_size, sizeof(float));
  engine->hrtf.delay_r =
      (float *)calloc(engine->hrtf.delay_size, sizeof(float));
  engine->hrtf.delay_index = 0;

#ifdef AE_USE_LIBMYSOFA
  engine->hrtf.sofa = NULL;
  engine->hrtf.hrir_l = NULL;
  engine->hrtf.hrir_r = NULL;
  engine->hrtf.hrir_len = 0;
  engine->hrtf.delay_l_samples = 0.0f;
  engine->hrtf.delay_r_samples = 0.0f;
  engine->hrtf.last_azimuth = 9999.0f;
  engine->hrtf.last_elevation = 9999.0f;
  engine->hrtf.history = NULL;
  engine->hrtf.history_size = 0;
  engine->hrtf.history_index = 0;
  engine->hrtf.loaded = false;

  if (engine->config.preload_hrtf && engine->config.hrtf_path) {
    if (!ae_spatial_load_sofa(engine, engine->config.hrtf_path)) {
      ae_set_error(engine, "HRTF load failed");
    }
  }
#endif
}

void ae_spatial_cleanup(ae_engine_t *engine) {
  if (!engine)
    return;
#ifdef AE_USE_LIBMYSOFA
  ae_spatial_unload_sofa(engine);
#endif
  free(engine->hrtf.delay_l);
  free(engine->hrtf.delay_r);
  engine->hrtf.delay_l = NULL;
  engine->hrtf.delay_r = NULL;
  engine->hrtf.delay_size = 0;
  engine->hrtf.delay_index = 0;
}

void ae_spatial_set_params(ae_engine_t *engine,
                           const ae_binaural_params_t *params) {
  if (!engine || !params)
    return;
  engine->hrtf.params = *params;
  engine->hrtf.enabled = true;

#ifdef AE_USE_LIBMYSOFA
  if (!engine->hrtf.loaded && engine->config.hrtf_path) {
    if (!ae_spatial_load_sofa(engine, engine->config.hrtf_path)) {
      ae_set_error(engine, "HRTF load failed");
    }
  }
  if (engine->hrtf.loaded) {
    if (!ae_spatial_update_hrir(engine, params->azimuth_deg,
                                params->elevation_deg)) {
      ae_set_error(engine, "HRTF update failed");
    }
    return;
  }
#endif

  float itd_samples = params->itd_us * 1e-6f * engine->config.sample_rate;
  int itd = (int)lrintf(itd_samples);
  int max_itd = (int)(engine->hrtf.delay_size - 1);
  if (itd > max_itd)
    itd = max_itd;
  if (itd < -max_itd)
    itd = -max_itd;
  engine->hrtf.itd_samples = itd;

  float ild = ae_clamp(params->ild_db, -20.0f, 20.0f);
  engine->hrtf.ild_gain_l = ae_db_to_linear(-0.5f * ild);
  engine->hrtf.ild_gain_r = ae_db_to_linear(0.5f * ild);

  float az = fabsf(params->azimuth_deg);
  float shadow = ae_clamp(az / 90.0f, 0.0f, 1.0f);
  float cutoff = 2000.0f + (1.0f - shadow) * 8000.0f;
  float rc = 1.0f / (2.0f * (float)M_PI * cutoff);
  float dt = 1.0f / engine->config.sample_rate;
  engine->hrtf.shadow_alpha = dt / (rc + dt);
}

void ae_spatial_process(ae_engine_t *engine, float *left, float *right,
                        size_t frames) {
  if (!engine || !left || !right || frames == 0)
    return;
  if (!engine->hrtf.enabled)
    return;

#ifdef AE_USE_LIBMYSOFA
  if (engine->hrtf.loaded && engine->hrtf.hrir_l && engine->hrtf.hrir_r &&
      engine->hrtf.history && engine->hrtf.hrir_len > 0) {
    if (!engine->hrtf.delay_l || !engine->hrtf.delay_r)
      return;

    size_t hrir_len = engine->hrtf.hrir_len;
    size_t history_size = engine->hrtf.history_size;
    size_t delay_size = engine->hrtf.delay_size;
    size_t delay_index = engine->hrtf.delay_index;
    size_t history_index = engine->hrtf.history_index;

    size_t delay_l = 0;
    size_t delay_r = 0;
    if (delay_size > 1) {
      int max_delay = (int)delay_size - 1;
      delay_l = (size_t)ae_clamp(lrintf(engine->hrtf.delay_l_samples), 0,
                                 max_delay);
      delay_r = (size_t)ae_clamp(lrintf(engine->hrtf.delay_r_samples), 0,
                                 max_delay);
    }

    for (size_t i = 0; i < frames; ++i) {
      float input = 0.5f * (left[i] + right[i]);
      engine->hrtf.history[history_index] = input;

      float out_l = 0.0f;
      float out_r = 0.0f;
      size_t idx = history_index;
      for (size_t k = 0; k < hrir_len; ++k) {
        float sample = engine->hrtf.history[idx];
        out_l += engine->hrtf.hrir_l[k] * sample;
        out_r += engine->hrtf.hrir_r[k] * sample;
        if (idx == 0)
          idx = history_size - 1;
        else
          --idx;
      }

      engine->hrtf.delay_l[delay_index] = out_l;
      engine->hrtf.delay_r[delay_index] = out_r;

      size_t read_l = (delay_index + delay_size - delay_l) % delay_size;
      size_t read_r = (delay_index + delay_size - delay_r) % delay_size;

      left[i] = engine->hrtf.delay_l[read_l];
      right[i] = engine->hrtf.delay_r[read_r];

      delay_index = (delay_index + 1) % delay_size;
      history_index = (history_index + 1) % history_size;
    }

    engine->hrtf.delay_index = delay_index;
    engine->hrtf.history_index = history_index;
    return;
  }
#endif

  if (!engine->hrtf.delay_l || !engine->hrtf.delay_r)
    return;

  int itd = engine->hrtf.itd_samples;
  size_t delay_size = engine->hrtf.delay_size;
  float gain_l = engine->hrtf.ild_gain_l;
  float gain_r = engine->hrtf.ild_gain_r;

  bool shadow_left = engine->hrtf.params.azimuth_deg > 0.0f;
  float alpha = engine->hrtf.shadow_alpha;
  float state_l = engine->hrtf.shadow_state_l;
  float state_r = engine->hrtf.shadow_state_r;

  for (size_t i = 0; i < frames; ++i) {
    engine->hrtf.delay_l[engine->hrtf.delay_index] = left[i];
    engine->hrtf.delay_r[engine->hrtf.delay_index] = right[i];

    size_t read_l = engine->hrtf.delay_index;
    size_t read_r = engine->hrtf.delay_index;
    if (itd > 0) {
      read_l = (engine->hrtf.delay_index + delay_size - (size_t)itd) %
               delay_size;
    } else if (itd < 0) {
      read_r =
          (engine->hrtf.delay_index + delay_size - (size_t)(-itd)) %
          delay_size;
    }

    float out_l = engine->hrtf.delay_l[read_l] * gain_l;
    float out_r = engine->hrtf.delay_r[read_r] * gain_r;

    if (alpha > 0.0f) {
      if (shadow_left) {
        state_l = state_l + alpha * (out_l - state_l);
        out_l = state_l;
      } else if (engine->hrtf.params.azimuth_deg < 0.0f) {
        state_r = state_r + alpha * (out_r - state_r);
        out_r = state_r;
      }
    }

    left[i] = out_l;
    right[i] = out_r;

    engine->hrtf.delay_index =
        (engine->hrtf.delay_index + 1) % delay_size;
  }

  engine->hrtf.shadow_state_l = state_l;
  engine->hrtf.shadow_state_r = state_r;
}
