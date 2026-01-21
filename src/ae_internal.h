#ifndef AE_INTERNAL_H
#define AE_INTERNAL_H

#include "../include/acoustic_engine.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__STDC_NO_ATOMICS__)
typedef volatile float ae_atomic_float;
#define AE_ATOMIC_STORE(ptr, val) (*(ptr) = (val))
#define AE_ATOMIC_LOAD(ptr) (*(ptr))
#else
#include <stdatomic.h>
typedef _Atomic float ae_atomic_float;
#define AE_ATOMIC_STORE(ptr, val)                                              \
  atomic_store_explicit((ptr), (val), memory_order_relaxed)
#define AE_ATOMIC_LOAD(ptr) atomic_load_explicit((ptr), memory_order_relaxed)
#endif

#define AE_FDN_CHANNELS 8
#define AE_ER_TAPS 12

#ifdef AE_USE_LIBMYSOFA
#include "mysofa.h"
#endif

typedef struct {
  float *buffer;
  size_t size;
  size_t delay;
  size_t index;
  float feedback;
  float damping;
  float filter_state;
} ae_fdn_delay_t;

typedef struct {
  float *buffer;
  size_t size;
  size_t delay;
  size_t index;
  float feedback;
} ae_allpass_t;

typedef struct {
  float *buffer;
  size_t size;
  size_t index;
  size_t tap_count;
  size_t delay_samples[AE_ER_TAPS];
  float gains[AE_ER_TAPS];
  float pans[AE_ER_TAPS];
} ae_early_reflections_t;

struct ae_reverb {
  ae_fdn_delay_t lines[AE_FDN_CHANNELS];
  ae_allpass_t diffusion[2];
  ae_early_reflections_t early;
  float *pre_delay;
  size_t pre_delay_size;
  size_t pre_delay_delay;
  size_t pre_delay_index;
  float room_size;
  float rt60;
  float diffusion_amount;
  float damping;
  float lfo_phase;
  float sample_rate;
};

struct ae_hrtf {
  bool enabled;
  ae_binaural_params_t params;
  int itd_samples;
  float ild_gain_l;
  float ild_gain_r;
  float shadow_alpha;
  float shadow_state_l;
  float shadow_state_r;
  float *delay_l;
  float *delay_r;
  size_t delay_size;
  size_t delay_index;
#ifdef AE_USE_LIBMYSOFA
  struct MYSOFA_EASY *sofa;
  float *hrir_l;
  float *hrir_r;
  size_t hrir_len;
  float delay_l_samples;
  float delay_r_samples;
  float last_azimuth;
  float last_elevation;
  float *history;
  size_t history_size;
  size_t history_index;
  bool loaded;
#endif
};

struct ae_dynamics {
  float threshold_db;      /* Compression threshold in dB */
  float ratio;             /* Compression ratio (e.g., 4.0 = 4:1) */
  float attack_ms;         /* Attack time in ms */
  float release_ms;        /* Release time in ms */
  float knee_db;           /* Soft knee width in dB */
  float makeup_db;         /* Makeup gain in dB */
  float envelope;          /* Current envelope level */
  float gain_reduction_db; /* Current gain reduction in dB */
};

typedef enum {
  AE_ENV_IDLE = 0,
  AE_ENV_ATTACK,
  AE_ENV_DECAY,
  AE_ENV_SUSTAIN,
  AE_ENV_RELEASE
} ae_env_state_t;

typedef struct {
  const char *name;
  ae_main_params_t main_params;
  ae_extended_params_t extended_params;
} ae_preset_entry_t;

struct ae_engine {
  ae_config_t config;
  char last_error[256];

  ae_atomic_float distance;
  ae_atomic_float room_size;
  ae_atomic_float brightness;
  ae_atomic_float width;
  ae_atomic_float dry_wet;
  ae_atomic_float intensity;

  ae_atomic_float decay_time;
  ae_atomic_float diffusion;
  ae_atomic_float lofi_amount;
  ae_atomic_float modulation;

  ae_doppler_params_t doppler;
  float doppler_phase;
  ae_adsr_t envelope;
  ae_env_state_t env_state;
  float env_level;

  ae_precedence_t precedence;
  float *precedence_l;
  float *precedence_r;
  size_t precedence_size;
  size_t precedence_index;

  struct ae_reverb reverb;
  struct ae_hrtf hrtf;
  struct ae_dynamics dynamics;

  float lp_state_l;
  float lp_state_r;
  float hp_state_l;
  float hp_state_r;

  float *scratch_l;
  float *scratch_r;
  float *scratch_mono;
  float *scratch_wet_l;
  float *scratch_wet_r;
  size_t scratch_size;

  float *prev_mag;
  size_t prev_mag_len;

  float last_lufs;
  float output_gain;
};

static inline float ae_clamp(float value, float min_val, float max_val) {
  if (value < min_val)
    return min_val;
  if (value > max_val)
    return max_val;
  return value;
}

static inline float ae_db_to_linear(float db) {
  return powf(10.0f, db / 20.0f);
}

static inline size_t ae_next_pow2(size_t value) {
  size_t power = 1;
  while (power < value)
    power <<= 1;
  return power;
}

static inline void ae_clear_buffer(float *buffer, size_t n) {
  if (buffer && n > 0)
    memset(buffer, 0, n * sizeof(float));
}

void ae_set_error(ae_engine_t *engine, const char *message);
void ae_clear_error(ae_engine_t *engine);

const ae_preset_entry_t *ae_find_preset(const char *name);

void ae_reverb_init(ae_engine_t *engine);
void ae_reverb_reset(ae_engine_t *engine);
void ae_reverb_update_params(ae_engine_t *engine, float room_size, float rt60,
                             float diffusion, float damping);
void ae_reverb_process_block(ae_engine_t *engine, const float *input,
                             float *out_l, float *out_r, size_t frames);
void ae_reverb_cleanup(ae_engine_t *engine);

void ae_spatial_init(ae_engine_t *engine);
void ae_spatial_cleanup(ae_engine_t *engine);
void ae_spatial_set_params(ae_engine_t *engine,
                           const ae_binaural_params_t *params);
void ae_spatial_process(ae_engine_t *engine, float *left, float *right,
                        size_t frames);

void ae_dsp_apply_brightness(float *samples, size_t n, float brightness,
                             float sample_rate, float *lp_state,
                             float *hp_state);
void ae_dsp_apply_lofi(float *left, float *right, size_t n, float amount);
void ae_dsp_apply_width(float *left, float *right, size_t n, float width);
void ae_dsp_apply_precedence(ae_engine_t *engine, float *left, float *right,
                             size_t frames);
void ae_dsp_apply_doppler(const ae_doppler_params_t *doppler, const float *in_l,
                          const float *in_r, float *out_l, float *out_r,
                          size_t frames, float *phase);
float ae_dsp_apply_envelope(ae_engine_t *engine, float sample);

/* Propagation models */
float ae_francois_garrison_absorption(float f_khz, float T, float S, float D);
float ae_iso9613_absorption(float f_hz, float T_celsius, float humidity_pct,
                            float pressure_kpa);
float ae_cave_modal_frequency(float dimension_m, int mode_number,
                              float temperature_c);
void ae_calculate_flutter(float wall_distance_m, float temperature_c,
                          float *delay_ms_out, float *flutter_freq_out);
float ae_eyring_rt60(float volume_m3, float surface_m2, float avg_alpha);
float ae_rock_wall_absorption(float f_hz);
void ae_apply_distance_absorption(float *samples, size_t frames,
                                  float distance_m, float absorption_db_per_km,
                                  float sample_rate, float *filter_state);

/* Dynamics processing */
void ae_dynamics_init(ae_dynamics_t *dyn);
float ae_compressor_process_sample(ae_dynamics_t *dyn, float sample,
                                   float sample_rate);
void ae_compressor_process(ae_dynamics_t *dyn, float *samples, size_t frames,
                           float sample_rate);
void ae_compressor_process_stereo(ae_dynamics_t *dyn, float *left, float *right,
                                  size_t frames, float sample_rate);
void ae_limiter_process(float *samples, size_t frames, float ceiling_db,
                        size_t lookahead_samples, float release_ms,
                        float sample_rate, float *delay_buffer,
                        size_t *delay_index, float *envelope);
float ae_soft_clip(float sample, float threshold);

/* Extended LoFi effects */
void ae_dsp_apply_wow_flutter(float *samples, size_t n, float depth,
                              float rate_hz, float sample_rate, float *phase);
void ae_dsp_apply_tape_saturation(float *samples, size_t n, float drive);

/* SIMD helpers */
void ae_simd_copy_gain(float *dst, const float *src, float gain, size_t n);
void ae_simd_mix_gain(float *dst, const float *src, float gain, size_t n);
void ae_simd_interleave_stereo(float *dst, const float *left,
                               const float *right, size_t frames);
void ae_simd_deinterleave_stereo(float *left, float *right, const float *src,
                                 size_t frames);
float ae_simd_max_abs(const float *src, size_t n);

#endif /* AE_INTERNAL_H */
