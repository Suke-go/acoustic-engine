#include "ae_internal.h"

void ae_set_error(ae_engine_t *engine, const char *message) {
  if (!engine || !message)
    return;
  strncpy(engine->last_error, message, sizeof(engine->last_error) - 1);
  engine->last_error[sizeof(engine->last_error) - 1] = '\0';
}

void ae_clear_error(ae_engine_t *engine) {
  if (engine)
    engine->last_error[0] = '\0';
}

AE_API ae_config_t ae_get_default_config(void) {
  ae_config_t config;
  config.sample_rate = AE_SAMPLE_RATE;
  config.max_buffer_size = AE_MAX_BUFFER_SIZE;
  config.data_path = NULL;
  config.hrtf_path = NULL;
  config.preload_hrtf = true;
  config.preload_all_presets = false;
  config.max_reverb_time_sec = 10;
  return config;
}

AE_API ae_engine_t *ae_create_engine(const ae_config_t *config) {
  ae_config_t cfg = config ? *config : ae_get_default_config();
  if (cfg.sample_rate != AE_SAMPLE_RATE) {
    return NULL;
  }

  ae_engine_t *engine = (ae_engine_t *)calloc(1, sizeof(ae_engine_t));
  if (!engine)
    return NULL;

  engine->config = cfg;
  engine->output_gain = 1.0f;
  engine->last_lufs = -120.0f;

  AE_ATOMIC_STORE(&engine->distance, 10.0f);
  AE_ATOMIC_STORE(&engine->room_size, 0.5f);
  AE_ATOMIC_STORE(&engine->brightness, 0.0f);
  AE_ATOMIC_STORE(&engine->width, 1.0f);
  AE_ATOMIC_STORE(&engine->dry_wet, 0.5f);
  AE_ATOMIC_STORE(&engine->intensity, 1.0f);

  AE_ATOMIC_STORE(&engine->decay_time, 0.0f);
  AE_ATOMIC_STORE(&engine->diffusion, 0.5f);
  AE_ATOMIC_STORE(&engine->lofi_amount, 0.0f);
  AE_ATOMIC_STORE(&engine->modulation, 0.0f);

  engine->doppler.enabled = false;
  engine->doppler_phase = 0.0f;
  engine->envelope.attack_ms = 0.0f;
  engine->envelope.decay_ms = 0.0f;
  engine->envelope.sustain_level = 1.0f;
  engine->envelope.release_ms = 0.0f;
  engine->env_state = AE_ENV_IDLE;
  engine->env_level = 1.0f;

  engine->precedence.delay_ms = 0.0f;
  engine->precedence.level_db = -6.0f;
  engine->precedence.pan = 0.0f;

  engine->scratch_size = cfg.max_buffer_size;
  engine->scratch_l = (float *)calloc(engine->scratch_size, sizeof(float));
  engine->scratch_r = (float *)calloc(engine->scratch_size, sizeof(float));
  engine->scratch_mono = (float *)calloc(engine->scratch_size, sizeof(float));
  engine->scratch_wet_l =
      (float *)calloc(engine->scratch_size, sizeof(float));
  engine->scratch_wet_r =
      (float *)calloc(engine->scratch_size, sizeof(float));

  if (!engine->scratch_l || !engine->scratch_r || !engine->scratch_mono ||
      !engine->scratch_wet_l || !engine->scratch_wet_r) {
    ae_destroy_engine(engine);
    return NULL;
  }

  engine->prev_mag_len = 0;
  engine->prev_mag = NULL;

  engine->precedence_size = (size_t)(cfg.sample_rate * 0.1f) + 1;
  engine->precedence_l =
      (float *)calloc(engine->precedence_size, sizeof(float));
  engine->precedence_r =
      (float *)calloc(engine->precedence_size, sizeof(float));
  if (!engine->precedence_l || !engine->precedence_r) {
    ae_destroy_engine(engine);
    return NULL;
  }

  ae_reverb_init(engine);
  ae_spatial_init(engine);

  return engine;
}

AE_API void ae_destroy_engine(ae_engine_t *engine) {
  if (!engine)
    return;
  free(engine->scratch_l);
  free(engine->scratch_r);
  free(engine->scratch_mono);
  free(engine->scratch_wet_l);
  free(engine->scratch_wet_r);
  free(engine->prev_mag);
  free(engine->precedence_l);
  free(engine->precedence_r);
  ae_reverb_cleanup(engine);
  ae_spatial_cleanup(engine);
  free(engine);
}

AE_API const char *ae_get_error_string(ae_result_t result) {
  switch (result) {
  case AE_OK:
    return "OK";
  case AE_ERROR_INVALID_PARAM:
    return "Invalid parameter";
  case AE_ERROR_OUT_OF_MEMORY:
    return "Out of memory";
  case AE_ERROR_FILE_NOT_FOUND:
    return "File not found";
  case AE_ERROR_INVALID_PRESET:
    return "Invalid preset";
  case AE_ERROR_HRTF_LOAD_FAILED:
    return "HRTF load failed";
  case AE_ERROR_JSON_PARSE:
    return "JSON parse error";
  case AE_ERROR_BUFFER_TOO_SMALL:
    return "Buffer too small";
  case AE_ERROR_NOT_INITIALIZED:
    return "Not initialized";
  default:
    return "Unknown error";
  }
}

AE_API const char *ae_get_last_error_detail(ae_engine_t *engine) {
  if (!engine || engine->last_error[0] == '\0')
    return NULL;
  return engine->last_error;
}

AE_API ae_result_t ae_process(ae_engine_t *engine,
                              const ae_audio_buffer_t *input,
                              ae_audio_buffer_t *output) {
  if (!engine || !output)
    return AE_ERROR_INVALID_PARAM;
  ae_clear_error(engine);

  size_t frames = output->frame_count;
  if (frames == 0 || frames > engine->scratch_size)
    return AE_ERROR_BUFFER_TOO_SMALL;

  if (input && input->frame_count != frames) {
    ae_set_error(engine, "Input and output frame counts mismatch");
    return AE_ERROR_INVALID_PARAM;
  }

  if (input && input->channels > 2)
    return AE_ERROR_INVALID_PARAM;
  if (output->channels > 2 || output->channels == 0)
    return AE_ERROR_INVALID_PARAM;
  if (!output->samples)
    return AE_ERROR_INVALID_PARAM;

  float *dry_l = engine->scratch_l;
  float *dry_r = engine->scratch_r;
  float *mono = engine->scratch_mono;
  float *wet_l = engine->scratch_wet_l;
  float *wet_r = engine->scratch_wet_r;

  if (!input || !input->samples) {
    ae_clear_buffer(dry_l, frames);
    ae_clear_buffer(dry_r, frames);
  } else if (input->channels == 1) {
    if (input->interleaved) {
      for (size_t i = 0; i < frames; ++i) {
        dry_l[i] = input->samples[i];
        dry_r[i] = input->samples[i];
      }
    } else {
      for (size_t i = 0; i < frames; ++i) {
        dry_l[i] = input->samples[i];
        dry_r[i] = input->samples[i];
      }
    }
  } else {
    if (input->interleaved) {
      for (size_t i = 0; i < frames; ++i) {
        dry_l[i] = input->samples[i * 2];
        dry_r[i] = input->samples[i * 2 + 1];
      }
    } else {
      for (size_t i = 0; i < frames; ++i) {
        dry_l[i] = input->samples[i];
        dry_r[i] = input->samples[i + frames];
      }
    }
  }

  if (engine->doppler.enabled) {
    ae_dsp_apply_doppler(&engine->doppler, dry_l, dry_r, wet_l, wet_r, frames,
                         &engine->doppler_phase);
    memcpy(dry_l, wet_l, frames * sizeof(float));
    memcpy(dry_r, wet_r, frames * sizeof(float));
  }

  float distance = AE_ATOMIC_LOAD(&engine->distance);
  float room_size = AE_ATOMIC_LOAD(&engine->room_size);
  float brightness = AE_ATOMIC_LOAD(&engine->brightness);
  float width = AE_ATOMIC_LOAD(&engine->width);
  float dry_wet = AE_ATOMIC_LOAD(&engine->dry_wet);
  float intensity = AE_ATOMIC_LOAD(&engine->intensity);
  float decay_time = AE_ATOMIC_LOAD(&engine->decay_time);
  float diffusion = AE_ATOMIC_LOAD(&engine->diffusion);
  float lofi_amount = AE_ATOMIC_LOAD(&engine->lofi_amount);
  float modulation = AE_ATOMIC_LOAD(&engine->modulation);

  distance = ae_clamp(distance, 0.1f, 1000.0f);
  room_size = ae_clamp(room_size, 0.0f, 1.0f);
  brightness = ae_clamp(brightness, -1.0f, 1.0f);
  width = ae_clamp(width, 0.0f, 2.0f);
  dry_wet = ae_clamp(dry_wet, 0.0f, 1.0f);
  intensity = ae_clamp(intensity, 0.0f, 1.0f);
  diffusion = ae_clamp(diffusion, 0.0f, 1.0f);
  lofi_amount = ae_clamp(lofi_amount, 0.0f, 1.0f);
  modulation = ae_clamp(modulation, 0.0f, 1.0f);

  float gain = 1.0f / (1.0f + 0.1f * distance);
  for (size_t i = 0; i < frames; ++i) {
    dry_l[i] *= gain;
    dry_r[i] *= gain;
  }

  ae_dsp_apply_brightness(dry_l, frames, brightness,
                          (float)engine->config.sample_rate,
                          &engine->lp_state_l, &engine->hp_state_l);
  ae_dsp_apply_brightness(dry_r, frames, brightness,
                          (float)engine->config.sample_rate,
                          &engine->lp_state_r, &engine->hp_state_r);

  for (size_t i = 0; i < frames; ++i) {
    mono[i] = 0.5f * (dry_l[i] + dry_r[i]);
  }

  float rt60 = decay_time > 0.0f ? decay_time : (0.3f + room_size * 9.7f);
  if (rt60 > (float)engine->config.max_reverb_time_sec)
    rt60 = (float)engine->config.max_reverb_time_sec;

  float damping = 0.6f - brightness * 0.3f;
  damping = ae_clamp(damping, 0.1f, 0.9f);
  AE_ATOMIC_STORE(&engine->modulation, modulation);

  ae_reverb_update_params(engine, room_size, rt60, diffusion, damping);
  ae_reverb_process_block(engine, mono, wet_l, wet_r, frames);
  ae_dsp_apply_lofi(wet_l, wet_r, frames, lofi_amount);

  ae_spatial_process(engine, dry_l, dry_r, frames);

  float wet_gain = dry_wet * intensity;
  float dry_gain = 1.0f - dry_wet;

  for (size_t i = 0; i < frames; ++i) {
    float out_l = dry_gain * dry_l[i] + wet_gain * wet_l[i];
    float out_r = dry_gain * dry_r[i] + wet_gain * wet_r[i];
    out_l = ae_dsp_apply_envelope(engine, out_l);
    out_r = ae_dsp_apply_envelope(engine, out_r);
    dry_l[i] = out_l * engine->output_gain;
    dry_r[i] = out_r * engine->output_gain;
  }

  ae_dsp_apply_precedence(engine, dry_l, dry_r, frames);
  ae_dsp_apply_width(dry_l, dry_r, frames, width);

  if (output->channels == 1) {
    for (size_t i = 0; i < frames; ++i) {
      output->samples[i] = 0.5f * (dry_l[i] + dry_r[i]);
    }
  } else if (output->interleaved) {
    for (size_t i = 0; i < frames; ++i) {
      output->samples[i * 2] = dry_l[i];
      output->samples[i * 2 + 1] = dry_r[i];
    }
  } else {
    for (size_t i = 0; i < frames; ++i) {
      output->samples[i] = dry_l[i];
      output->samples[i + frames] = dry_r[i];
    }
  }

  return AE_OK;
}

AE_API ae_result_t ae_load_preset(ae_engine_t *engine,
                                  const char *preset_name) {
  if (!engine || !preset_name)
    return AE_ERROR_INVALID_PARAM;
  ae_clear_error(engine);
  const ae_preset_entry_t *preset = ae_find_preset(preset_name);
  if (!preset) {
    ae_set_error(engine, "Preset not found");
    return AE_ERROR_INVALID_PRESET;
  }

  AE_ATOMIC_STORE(&engine->distance, preset->main_params.distance);
  AE_ATOMIC_STORE(&engine->room_size, preset->main_params.room_size);
  AE_ATOMIC_STORE(&engine->brightness, preset->main_params.brightness);
  AE_ATOMIC_STORE(&engine->width, preset->main_params.width);
  AE_ATOMIC_STORE(&engine->dry_wet, preset->main_params.dry_wet);
  AE_ATOMIC_STORE(&engine->intensity, preset->main_params.intensity);

  AE_ATOMIC_STORE(&engine->decay_time, preset->extended_params.decay_time);
  AE_ATOMIC_STORE(&engine->diffusion, preset->extended_params.diffusion);
  AE_ATOMIC_STORE(&engine->lofi_amount, preset->extended_params.lofi_amount);
  AE_ATOMIC_STORE(&engine->modulation, preset->extended_params.modulation);
  return AE_OK;
}

AE_API ae_result_t ae_get_scenario_defaults(const char *scenario_name,
                                            ae_main_params_t *main_out,
                                            ae_extended_params_t *extended_out) {
  if (!scenario_name || !main_out)
    return AE_ERROR_INVALID_PARAM;
  const ae_preset_entry_t *preset = ae_find_preset(scenario_name);
  if (!preset)
    return AE_ERROR_INVALID_PRESET;
  *main_out = preset->main_params;
  if (extended_out)
    *extended_out = preset->extended_params;
  return AE_OK;
}

AE_API ae_result_t ae_set_main_params(ae_engine_t *engine,
                                      const ae_main_params_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  ae_result_t valid = ae_validate_params(params);
  if (valid != AE_OK)
    return valid;
  AE_ATOMIC_STORE(&engine->distance, params->distance);
  AE_ATOMIC_STORE(&engine->room_size, params->room_size);
  AE_ATOMIC_STORE(&engine->brightness, params->brightness);
  AE_ATOMIC_STORE(&engine->width, params->width);
  AE_ATOMIC_STORE(&engine->dry_wet, params->dry_wet);
  AE_ATOMIC_STORE(&engine->intensity, params->intensity);
  return AE_OK;
}

AE_API ae_result_t ae_get_main_params(ae_engine_t *engine,
                                      ae_main_params_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  params->distance = AE_ATOMIC_LOAD(&engine->distance);
  params->room_size = AE_ATOMIC_LOAD(&engine->room_size);
  params->brightness = AE_ATOMIC_LOAD(&engine->brightness);
  params->width = AE_ATOMIC_LOAD(&engine->width);
  params->dry_wet = AE_ATOMIC_LOAD(&engine->dry_wet);
  params->intensity = AE_ATOMIC_LOAD(&engine->intensity);
  return AE_OK;
}

AE_API ae_result_t ae_set_extended_params(ae_engine_t *engine,
                                          const ae_extended_params_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  AE_ATOMIC_STORE(&engine->decay_time, params->decay_time);
  AE_ATOMIC_STORE(&engine->diffusion, params->diffusion);
  AE_ATOMIC_STORE(&engine->lofi_amount, params->lofi_amount);
  AE_ATOMIC_STORE(&engine->modulation, params->modulation);
  return AE_OK;
}

AE_API ae_result_t ae_set_distance(ae_engine_t *engine, float distance) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  if (distance <= 0.0f)
    distance = 0.1f;
  AE_ATOMIC_STORE(&engine->distance, distance);
  return AE_OK;
}

AE_API ae_result_t ae_set_room_size(ae_engine_t *engine, float room_size) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  AE_ATOMIC_STORE(&engine->room_size, ae_clamp(room_size, 0.0f, 1.0f));
  return AE_OK;
}

AE_API ae_result_t ae_set_brightness(ae_engine_t *engine, float brightness) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  AE_ATOMIC_STORE(&engine->brightness, ae_clamp(brightness, -1.0f, 1.0f));
  return AE_OK;
}

AE_API ae_result_t ae_set_width(ae_engine_t *engine, float width) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  AE_ATOMIC_STORE(&engine->width, ae_clamp(width, 0.0f, 2.0f));
  return AE_OK;
}

AE_API ae_result_t ae_set_dry_wet(ae_engine_t *engine, float dry_wet) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  AE_ATOMIC_STORE(&engine->dry_wet, ae_clamp(dry_wet, 0.0f, 1.0f));
  return AE_OK;
}

AE_API ae_result_t ae_set_intensity(ae_engine_t *engine, float intensity) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  AE_ATOMIC_STORE(&engine->intensity, ae_clamp(intensity, 0.0f, 1.0f));
  return AE_OK;
}

AE_API ae_result_t ae_apply_scenario(ae_engine_t *engine,
                                     const char *scenario_name,
                                     float intensity) {
  if (!engine || !scenario_name)
    return AE_ERROR_INVALID_PARAM;
  ae_main_params_t main_params;
  ae_extended_params_t ext_params;
  ae_result_t res =
      ae_get_scenario_defaults(scenario_name, &main_params, &ext_params);
  if (res != AE_OK)
    return res;
  main_params.intensity = ae_clamp(intensity, 0.0f, 1.0f);
  res = ae_set_main_params(engine, &main_params);
  if (res != AE_OK)
    return res;
  return ae_set_extended_params(engine, &ext_params);
}

AE_API ae_result_t ae_blend_scenarios(ae_engine_t *engine,
                                      const ae_scenario_blend_t *blends,
                                      size_t count) {
  if (!engine || !blends || count == 0)
    return AE_ERROR_INVALID_PARAM;
  ae_clear_error(engine);

  float weight_sum = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    if (!blends[i].name) {
      ae_set_error(engine, "Blend preset name is null");
      return AE_ERROR_INVALID_PARAM;
    }
    if (isnan(blends[i].weight) || blends[i].weight < 0.0f) {
      ae_set_error(engine, "Blend weight must be non-negative");
      return AE_ERROR_INVALID_PARAM;
    }
    if (!ae_find_preset(blends[i].name)) {
      ae_set_error(engine, "Blend preset not found");
      return AE_ERROR_INVALID_PRESET;
    }
    if (blends[i].weight > 0.0f)
      weight_sum += blends[i].weight;
  }
  if (weight_sum <= 0.0f)
    return AE_ERROR_INVALID_PARAM;

  ae_main_params_t main_acc = {0};
  ae_extended_params_t ext_acc = {0};

  for (size_t i = 0; i < count; ++i) {
    if (blends[i].weight <= 0.0f)
      continue;
    const ae_preset_entry_t *preset = ae_find_preset(blends[i].name);
    if (!preset)
      return AE_ERROR_INVALID_PRESET;
    float w = blends[i].weight / weight_sum;
    main_acc.distance += preset->main_params.distance * w;
    main_acc.room_size += preset->main_params.room_size * w;
    main_acc.brightness += preset->main_params.brightness * w;
    main_acc.width += preset->main_params.width * w;
    main_acc.dry_wet += preset->main_params.dry_wet * w;
    main_acc.intensity += preset->main_params.intensity * w;

    ext_acc.decay_time += preset->extended_params.decay_time * w;
    ext_acc.diffusion += preset->extended_params.diffusion * w;
    ext_acc.lofi_amount += preset->extended_params.lofi_amount * w;
    ext_acc.modulation += preset->extended_params.modulation * w;
  }

  ae_result_t res = ae_set_main_params(engine, &main_acc);
  if (res != AE_OK)
    return res;
  return ae_set_extended_params(engine, &ext_acc);
}

AE_API ae_result_t ae_apply_cave_model(ae_engine_t *engine,
                                       const ae_cave_params_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  float room_size = ae_clamp(params->cave_dimension_m / 50.0f, 0.0f, 1.0f);
  float decay_time = 3.0f + room_size * 5.0f;
  float brightness = -(params->alpha_high - params->alpha_low) * 5.0f;

  AE_ATOMIC_STORE(&engine->room_size, room_size);
  AE_ATOMIC_STORE(&engine->brightness, ae_clamp(brightness, -1.0f, 1.0f));
  AE_ATOMIC_STORE(&engine->decay_time, decay_time);
  AE_ATOMIC_STORE(&engine->diffusion,
                  ae_clamp(params->flutter_decay, 0.0f, 1.0f));
  return AE_OK;
}

AE_API ae_result_t ae_azimuth_to_binaural(float azimuth_deg,
                                         float elevation_deg,
                                         float frequency_hz,
                                         ae_binaural_params_t *out) {
  if (!out)
    return AE_ERROR_INVALID_PARAM;
  float az = ae_clamp(azimuth_deg, -180.0f, 180.0f);
  float el = ae_clamp(elevation_deg, -90.0f, 90.0f);
  float theta = az * (float)M_PI / 180.0f;
  float itd = (0.215f * sinf(theta)) / 343.0f;
  float itd_us = itd * 1e6f;
  float ild = 0.0f;
  if (frequency_hz > 500.0f) {
    float scale = ae_clamp((frequency_hz - 500.0f) / 1500.0f, 0.0f, 1.0f);
    ild = 20.0f * sinf(theta) * scale;
  }

  out->itd_us = ae_clamp(itd_us, -625.0f, 625.0f);
  out->ild_db = ae_clamp(ild, -20.0f, 20.0f);
  out->azimuth_deg = az;
  out->elevation_deg = el;
  return AE_OK;
}

AE_API ae_result_t ae_set_binaural_params(ae_engine_t *engine,
                                          const ae_binaural_params_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  ae_spatial_set_params(engine, params);
  return AE_OK;
}

AE_API ae_result_t ae_set_source_position(ae_engine_t *engine,
                                          float azimuth_deg,
                                          float elevation_deg) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  ae_binaural_params_t params;
  ae_result_t res =
      ae_azimuth_to_binaural(azimuth_deg, elevation_deg, 1000.0f, &params);
  if (res != AE_OK)
    return res;
  ae_spatial_set_params(engine, &params);
  return AE_OK;
}

AE_API ae_result_t ae_apply_precedence(ae_engine_t *engine,
                                       const ae_precedence_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  engine->precedence = *params;
  return AE_OK;
}

AE_API ae_result_t ae_set_doppler(ae_engine_t *engine,
                                  const ae_doppler_params_t *params) {
  if (!engine || !params)
    return AE_ERROR_INVALID_PARAM;
  bool was_enabled = engine->doppler.enabled;
  engine->doppler = *params;
  if (!was_enabled && engine->doppler.enabled)
    engine->doppler_phase = 0.0f;
  return AE_OK;
}

AE_API ae_result_t ae_set_envelope(ae_engine_t *engine,
                                   const ae_adsr_t *envelope) {
  if (!engine || !envelope)
    return AE_ERROR_INVALID_PARAM;
  engine->envelope = *envelope;
  engine->env_state = AE_ENV_ATTACK;
  engine->env_level = 0.0f;
  return AE_OK;
}

AE_API ae_result_t ae_apply_expression(ae_engine_t *engine,
                                       const char *expression) {
  if (!engine || !expression)
    return AE_ERROR_INVALID_PARAM;

  const char *cursor = expression;
  char key[64];
  float value = 0.0f;
  while (*cursor) {
    while (*cursor == ' ' || *cursor == ',')
      ++cursor;
    if (*cursor == '\0')
      break;
    size_t klen = 0;
    while (*cursor && *cursor != ':' && *cursor != '=' &&
           klen < sizeof(key) - 1) {
      key[klen++] = *cursor++;
    }
    key[klen] = '\0';
    if (*cursor == ':' || *cursor == '=')
      ++cursor;
    char *endptr = NULL;
    value = (float)strtod(cursor, &endptr);
    if (endptr == cursor)
      break;
    cursor = endptr;

    if (strcmp(key, "distance") == 0) {
      ae_set_distance(engine, 0.1f + value * 1000.0f);
    } else if (strcmp(key, "room_size") == 0) {
      ae_set_room_size(engine, value);
    } else if (strcmp(key, "brightness") == 0) {
      ae_set_brightness(engine, value * 2.0f - 1.0f);
    } else if (strcmp(key, "width") == 0) {
      ae_set_width(engine, value * 2.0f);
    } else if (strcmp(key, "dry_wet") == 0) {
      ae_set_dry_wet(engine, value);
    } else if (strcmp(key, "intensity") == 0) {
      ae_set_intensity(engine, value);
    } else if (strcmp(key, "warmth") == 0) {
      ae_set_brightness(engine, -value);
    } else if (strcmp(key, "tension") == 0) {
      ae_set_brightness(engine, value);
      AE_ATOMIC_STORE(&engine->modulation, value);
    } else if (strcmp(key, "intimacy") == 0) {
      ae_set_distance(engine, 0.1f + value * 5.0f);
      ae_set_dry_wet(engine, 0.3f);
      ae_set_width(engine, 0.6f);
    } else if (strcmp(key, "chaos") == 0) {
      AE_ATOMIC_STORE(&engine->lofi_amount, value);
      AE_ATOMIC_STORE(&engine->modulation, value);
    } else if (strcmp(key, "underwater") == 0) {
      ae_apply_scenario(engine, "deep_sea", 1.0f);
    }
  }
  return AE_OK;
}

AE_API ae_result_t ae_update_biosignal(ae_engine_t *engine,
                                       ae_biosignal_type_t type,
                                       float value) {
  if (!engine)
    return AE_ERROR_INVALID_PARAM;
  if (type == AE_BIOSIGNAL_HR) {
    float norm = ae_clamp((value - 40.0f) / 160.0f, 0.0f, 1.0f);
    ae_set_intensity(engine, 0.5f + 0.5f * norm);
    ae_set_room_size(engine, 0.3f + 0.6f * norm);
  } else if (type == AE_BIOSIGNAL_HRV) {
    float norm = ae_clamp((value - 10.0f) / 90.0f, 0.0f, 1.0f);
    AE_ATOMIC_STORE(&engine->modulation, 0.2f + 0.6f * (1.0f - norm));
    AE_ATOMIC_STORE(&engine->brightness, -0.2f + 0.4f * norm);
  }
  return AE_OK;
}

AE_API ae_hrtf_t *ae_get_hrtf(ae_engine_t *engine) {
  if (!engine)
    return NULL;
  return &engine->hrtf;
}

AE_API ae_reverb_t *ae_get_reverb(ae_engine_t *engine) {
  if (!engine)
    return NULL;
  return &engine->reverb;
}

AE_API ae_dynamics_t *ae_get_dynamics(ae_engine_t *engine) {
  if (!engine)
    return NULL;
  return &engine->dynamics;
}
