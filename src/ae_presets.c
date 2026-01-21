#include "ae_internal.h"

static const ae_preset_entry_t k_presets[] = {
    {"deep_sea", {50.0f, 0.85f, -0.5f, 1.6f, 0.7f, 1.0f},
     {8.0f, 0.8f, 0.1f, 0.2f}},
    {"cave", {20.0f, 0.7f, 0.0f, 1.5f, 0.65f, 1.0f},
     {6.0f, 0.7f, 0.0f, 0.1f}},
    {"forest", {15.0f, 0.3f, -0.2f, 1.2f, 0.4f, 1.0f},
     {1.5f, 0.5f, 0.0f, 0.0f}},
    {"cathedral", {30.0f, 0.9f, -0.1f, 1.6f, 0.75f, 1.0f},
     {10.0f, 0.9f, 0.0f, 0.2f}},
    {"tension", {5.0f, 0.35f, 0.4f, 0.6f, 0.5f, 1.0f},
     {1.2f, 0.4f, 0.1f, 0.4f}},
    {"nostalgia", {8.0f, 0.45f, -0.3f, 1.0f, 0.55f, 1.0f},
     {2.2f, 0.5f, 0.5f, 0.2f}},
    {"intimate", {1.0f, 0.15f, 0.1f, 0.4f, 0.25f, 1.0f},
     {0.6f, 0.3f, 0.0f, 0.0f}},
    {"open_field", {100.0f, 0.0f, -0.2f, 1.0f, 0.1f, 1.0f},
     {0.2f, 0.1f, 0.0f, 0.0f}},
    {"space", {200.0f, 0.0f, -0.8f, 1.2f, 0.2f, 1.0f},
     {0.2f, 0.2f, 0.0f, 0.0f}},
    {"office", {10.0f, 0.2f, -0.1f, 0.9f, 0.3f, 1.0f},
     {0.8f, 0.3f, 0.0f, 0.0f}},
    {"tunnel", {12.0f, 0.4f, 0.1f, 1.1f, 0.45f, 1.0f},
     {2.5f, 0.6f, 0.0f, 0.2f}},
    {"small_room", {4.0f, 0.2f, 0.0f, 0.8f, 0.4f, 1.0f},
     {0.7f, 0.4f, 0.0f, 0.0f}},
    {"radio", {8.0f, 0.2f, -0.4f, 0.7f, 0.6f, 1.0f},
     {1.0f, 0.2f, 0.7f, 0.0f}},
    {"telephone", {12.0f, 0.2f, -0.3f, 0.7f, 0.6f, 1.0f},
     {1.0f, 0.2f, 0.6f, 0.0f}},
    {"dream", {20.0f, 0.7f, -0.1f, 1.4f, 0.7f, 1.0f},
     {6.0f, 0.7f, 0.2f, 0.5f}},
    {"chaos", {10.0f, 0.5f, 0.2f, 1.4f, 0.8f, 1.0f},
     {3.0f, 0.3f, 0.9f, 0.8f}},
    {"ethereal", {25.0f, 0.8f, 0.1f, 1.8f, 0.75f, 1.0f},
     {7.0f, 0.9f, 0.1f, 0.6f}}};

const ae_preset_entry_t *ae_find_preset(const char *name) {
  if (!name)
    return NULL;
  for (size_t i = 0; i < sizeof(k_presets) / sizeof(k_presets[0]); ++i) {
    if (strcmp(k_presets[i].name, name) == 0)
      return &k_presets[i];
  }
  return NULL;
}
