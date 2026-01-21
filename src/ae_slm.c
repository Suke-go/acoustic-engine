#include "ae_internal.h"

struct ae_slm {
  ae_slm_config_t config;
};

static int ae_str_contains(const char *haystack, const char *needle) {
  if (!haystack || !needle)
    return 0;
  size_t hlen = strlen(haystack);
  size_t nlen = strlen(needle);
  if (nlen == 0 || hlen < nlen)
    return 0;
  for (size_t i = 0; i <= hlen - nlen; ++i) {
    size_t j = 0;
    while (j < nlen) {
      char a = haystack[i + j];
      char b = needle[j];
      if (a >= 'A' && a <= 'Z')
        a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z')
        b = (char)(b - 'A' + 'a');
      if (a != b)
        break;
      ++j;
    }
    if (j == nlen)
      return 1;
  }
  return 0;
}

AE_API ae_slm_t *ae_slm_create(const ae_slm_config_t *config) {
  ae_slm_t *slm = (ae_slm_t *)calloc(1, sizeof(ae_slm_t));
  if (!slm)
    return NULL;
  if (config)
    slm->config = *config;
  return slm;
}

AE_API void ae_slm_destroy(ae_slm_t *slm) { free(slm); }

AE_API ae_result_t ae_slm_interpret(ae_slm_t *slm,
                                    const char *natural_language_input,
                                    ae_main_params_t *params_out,
                                    char *preset_name_out,
                                    size_t preset_name_size) {
  if (!slm || !natural_language_input || !params_out)
    return AE_ERROR_INVALID_PARAM;

  ae_main_params_t params = {10.0f, 0.5f, 0.0f, 1.0f, 0.5f, 1.0f};
  const char *preset = NULL;

  if (ae_str_contains(natural_language_input, "deep") ||
      ae_str_contains(natural_language_input, "sea") ||
      ae_str_contains(natural_language_input, "underwater")) {
    preset = "deep_sea";
    params.room_size = 0.85f;
    params.brightness = -0.6f;
    params.distance = 80.0f;
  } else if (ae_str_contains(natural_language_input, "cave")) {
    preset = "cave";
    params.room_size = 0.7f;
    params.brightness = -0.1f;
  } else if (ae_str_contains(natural_language_input, "forest")) {
    preset = "forest";
    params.room_size = 0.3f;
    params.brightness = -0.2f;
  } else if (ae_str_contains(natural_language_input, "cathedral")) {
    preset = "cathedral";
    params.room_size = 0.9f;
    params.brightness = -0.1f;
  } else if (ae_str_contains(natural_language_input, "tension")) {
    preset = "tension";
    params.brightness = 0.4f;
  } else if (ae_str_contains(natural_language_input, "nostalgia")) {
    preset = "nostalgia";
    params.brightness = -0.3f;
  } else if (ae_str_contains(natural_language_input, "intimate")) {
    preset = "intimate";
    params.distance = 1.0f;
    params.width = 0.4f;
  } else if (ae_str_contains(natural_language_input, "dream")) {
    preset = "dream";
  } else if (ae_str_contains(natural_language_input, "chaos")) {
    preset = "chaos";
  } else if (ae_str_contains(natural_language_input, "ethereal")) {
    preset = "ethereal";
  }

  if (ae_str_contains(natural_language_input, "dark")) {
    params.brightness -= 0.3f;
  }
  if (ae_str_contains(natural_language_input, "bright")) {
    params.brightness += 0.3f;
  }
  if (ae_str_contains(natural_language_input, "warm")) {
    params.brightness -= 0.2f;
  }
  if (ae_str_contains(natural_language_input, "cold")) {
    params.brightness -= 0.2f;
    params.distance += 10.0f;
  }
  if (ae_str_contains(natural_language_input, "close")) {
    params.distance = 1.0f;
    params.dry_wet = 0.3f;
  }
  if (ae_str_contains(natural_language_input, "wide")) {
    params.width = 1.6f;
  }

  params.brightness = ae_clamp(params.brightness, -1.0f, 1.0f);
  params.width = ae_clamp(params.width, 0.0f, 2.0f);
  params.dry_wet = ae_clamp(params.dry_wet, 0.0f, 1.0f);
  params.distance = ae_clamp(params.distance, 0.1f, 1000.0f);

  *params_out = params;
  if (preset_name_out && preset_name_size > 0) {
    if (preset) {
      strncpy(preset_name_out, preset, preset_name_size - 1);
      preset_name_out[preset_name_size - 1] = '\0';
    } else {
      preset_name_out[0] = '\0';
    }
  }
  return AE_OK;
}
