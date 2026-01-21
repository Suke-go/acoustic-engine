/**
 * @file acoustic_engine.h
 * @brief Acoustic Engine - Main Public API Header
 * @version 0.1.0
 */

#ifndef ACOUSTIC_ENGINE_H
#define ACOUSTIC_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Export/Import macros */
#ifdef _WIN32
    #ifdef AE_BUILD_DLL
        #define AE_API __declspec(dllexport)
    #else
        #define AE_API __declspec(dllimport)
    #endif
#else
    #define AE_API __attribute__((visibility("default")))
#endif

/* Version info */
#define AE_VERSION_MAJOR 0
#define AE_VERSION_MINOR 1
#define AE_VERSION_PATCH 0

/* Alignment macro for SIMD */
#ifdef _MSC_VER
    #define AE_ALIGN(n) __declspec(align(n))
#else
    #define AE_ALIGN(n) __attribute__((aligned(n)))
#endif

/* Constants */
#define AE_SAMPLE_RATE 48000
#define AE_MAX_BUFFER_SIZE 4096
#define AE_NUM_BARK_BANDS 24
#define AE_LOG_EPSILON 1e-10f

/*============================================================================
 * Result codes
 *============================================================================*/
typedef enum {
    AE_OK = 0,
    AE_ERROR_INVALID_PARAM = -1,
    AE_ERROR_OUT_OF_MEMORY = -2,
    AE_ERROR_FILE_NOT_FOUND = -3,
    AE_ERROR_INVALID_PRESET = -4,
    AE_ERROR_HRTF_LOAD_FAILED = -5,
    AE_ERROR_JSON_PARSE = -6,
    AE_ERROR_BUFFER_TOO_SMALL = -7,
    AE_ERROR_NOT_INITIALIZED = -8,
} ae_result_t;

/*============================================================================
 * Audio buffer
 *============================================================================*/
typedef struct {
    float* samples;          /* Sample data (16-byte aligned recommended) */
    size_t frame_count;      /* Number of frames (64 - 4096) */
    uint8_t channels;        /* Channel count (1 = mono, 2 = stereo) */
    bool interleaved;        /* true: LRLRLR, false: LLL...RRR */
} ae_audio_buffer_t;

/*============================================================================
 * Offline audio import (non-real-time)
 *============================================================================*/
typedef struct {
    ae_audio_buffer_t buffer; /* Interleaved float32 samples */
    uint32_t sample_rate;     /* Sample rate after import */
} ae_audio_data_t;

/*============================================================================
 * Main parameters (Tier 1)
 *============================================================================*/
typedef struct {
    float distance;          /* 0.1 - 1000 m */
    float room_size;         /* 0.0 - 1.0 */
    float brightness;        /* -1.0 - 1.0 */
    float width;             /* 0.0 - 2.0 */
    float dry_wet;           /* 0.0 - 1.0 */
    float intensity;         /* 0.0 - 1.0 */
} ae_main_params_t;

/*============================================================================
 * Extended parameters (Tier 2)
 *============================================================================*/
typedef struct {
    float decay_time;        /* 0.1 - 30.0 seconds */
    float diffusion;         /* 0.0 - 1.0 */
    float lofi_amount;       /* 0.0 - 1.0 */
    float modulation;        /* 0.0 - 1.0 */
} ae_extended_params_t;

/*============================================================================
 * Scenario blending
 *============================================================================*/
typedef struct {
    const char* name;        /* Scenario name */
    float weight;            /* Blend weight (0.0 - 1.0) */
} ae_scenario_blend_t;

/*============================================================================
 * Timbral parameters (extended perceptual controls)
 *============================================================================*/
typedef struct {
    float roughness;         /* 0.0 - 5.0 asper */
    float sharpness;         /* 0.0 - 4.0 acum */
    float fluctuation;       /* 0.0 - 5.0 vacil */
    float tonality;          /* 0.0 - 1.0 */

    float warmth;            /* 0.0 - 1.0 */
    float presence;          /* 0.0 - 1.0 */
    float air;               /* 0.0 - 1.0 */
    float body;              /* 0.0 - 1.0 */
    float clarity;           /* 0.0 - 1.0 */
    float punch;             /* 0.0 - 1.0 */
} ae_timbral_params_t;

/*============================================================================
 * Cave model parameters
 *============================================================================*/
typedef struct {
    float cave_dimension_m;  /* Major dimension */
    float wall_distance_m;   /* Wall spacing (flutter) */
    uint8_t flutter_repeats; /* Flutter echo repeats */
    float flutter_decay;     /* Flutter decay */
    float alpha_low;         /* Low frequency absorption */
    float alpha_high;        /* High frequency absorption */
} ae_cave_params_t;

/*============================================================================
 * Binaural parameters
 *============================================================================*/
typedef struct {
    float itd_us;            /* Interaural time difference (us) */
    float ild_db;            /* Interaural level difference (dB) */
    float azimuth_deg;       /* Azimuth (-180 to 180) */
    float elevation_deg;     /* Elevation (-90 to 90) */
} ae_binaural_params_t;

typedef struct {
    float delay_ms;          /* Delay time (ms) */
    float level_db;          /* Level difference (dB) */
    float pan;               /* Pan (-1 to 1) */
} ae_precedence_t;

/*============================================================================
 * Dynamic parameters
 *============================================================================*/
typedef struct {
    float source_velocity;   /* m/s (positive = approaching) */
    float listener_velocity; /* m/s (positive = approaching) */
    bool enabled;            /* Enable doppler */
} ae_doppler_params_t;

typedef struct {
    float attack_ms;         /* Attack time (ms) */
    float decay_ms;          /* Decay time (ms) */
    float sustain_level;     /* Sustain level (0-1) */
    float release_ms;        /* Release time (ms) */
} ae_adsr_t;

/*============================================================================
 * Biosignal mapping
 *============================================================================*/
typedef enum {
    AE_BIOSIGNAL_HR = 0,
    AE_BIOSIGNAL_HRV = 1
} ae_biosignal_type_t;

typedef enum {
    AE_PARAM_DISTANCE = 0,
    AE_PARAM_ROOM_SIZE,
    AE_PARAM_BRIGHTNESS,
    AE_PARAM_WIDTH,
    AE_PARAM_DRY_WET,
    AE_PARAM_INTENSITY
} ae_param_target_t;

typedef enum {
    AE_CURVE_LINEAR = 0,
    AE_CURVE_EXPONENTIAL,
    AE_CURVE_LOGARITHMIC,
    AE_CURVE_SIGMOID,
    AE_CURVE_STEPPED,
    AE_CURVE_CUSTOM
} ae_curve_type_t;

typedef struct {
    ae_biosignal_type_t input;
    ae_param_target_t target;
    ae_curve_type_t curve;
    float in_min;
    float in_max;
    float out_min;
    float out_max;
    float smoothing;
} ae_mapping_t;

/*============================================================================
 * SLM (optional semantic module)
 *============================================================================*/
typedef struct ae_slm ae_slm_t;

typedef struct {
    const char* model_path;
    size_t max_tokens;
    float temperature;
} ae_slm_config_t;

/*============================================================================
 * Engine configuration
 *============================================================================*/
typedef struct {
    uint32_t sample_rate;           /* 48000 (fixed) */
    uint32_t max_buffer_size;       /* Max buffer size (default: 4096) */
    const char* data_path;          /* Data directory (NULL = exe path) */
    const char* hrtf_path;          /* HRTF file path (NULL = builtin) */
    bool preload_hrtf;              /* true = load at startup */
    bool preload_all_presets;       /* true = load all presets */
    size_t max_reverb_time_sec;     /* Max reverb time (default: 10s) */
} ae_config_t;

/*============================================================================
 * Perceptual metrics
 *============================================================================*/
typedef struct {
    float iacc_early;        /* Early IACC (0-80ms) -> ASW */
    float iacc_late;         /* Late IACC (80ms+) -> LEV */
    float drr_db;            /* Direct-to-Reverberant Ratio (dB) */
    float spectral_centroid; /* Spectral centroid (Hz) */
    float loudness_sone;     /* Loudness (sone) */
    float clarity_c50;       /* Clarity C50 (dB) */
    float clarity_c80;       /* Clarity C80 (dB) */
} ae_perceptual_metrics_t;

/*============================================================================
 * Room metrics (ISO 3382)
 *============================================================================*/
typedef struct {
    float edt;               /* Early Decay Time (seconds) */
    float edt_band[6];       /* Band-specific EDT (125Hz-4kHz) */
    float c50;               /* Clarity for speech (dB) */
    float c80;               /* Clarity for music (dB) */
    float d50;               /* Definition (0-1) */
    float ts_ms;             /* Center time (ms) */
    float strength_g;        /* Sound strength (dB) */
    float sti;               /* Speech Transmission Index (0-1) */
} ae_room_metrics_t;

/*============================================================================
 * Spectral features
 *============================================================================*/
typedef struct {
    float centroid_hz;       /* Spectral centroid (Hz) */
    float spread_hz;         /* Spectral spread (Hz) */
    float flux;              /* Spectral flux */
    float hnr_db;            /* Harmonic-to-noise ratio (dB) */
    float flatness;          /* Spectral flatness (0-1) */
    float rolloff_hz;        /* Rolloff frequency (Hz) */
    float bark_spectrum[AE_NUM_BARK_BANDS]; /* Bark spectrum */
} ae_spectral_features_t;

/*============================================================================
 * Loudness metrics
 *============================================================================*/
typedef enum {
    AE_WEIGHTING_NONE,       /* Z-weighting (flat) */
    AE_WEIGHTING_A,          /* A-weighting */
    AE_WEIGHTING_C,          /* C-weighting */
    AE_WEIGHTING_ITU_R_468   /* ITU-R 468 */
} ae_weighting_t;

typedef struct {
    float loudness_sone;     /* Loudness (sone) */
    float loudness_phon;     /* Loudness level (phon) */
    float peak_db;           /* Peak level (dBFS) */
    float rms_db;            /* RMS level (dBFS) */
    float lufs;              /* Integrated LUFS (EBU R128) */
    float lra;               /* Loudness Range (LU) */
} ae_loudness_t;

/*============================================================================
 * Opaque engine handle
 *============================================================================*/
typedef struct ae_engine ae_engine_t;
typedef struct ae_hrtf ae_hrtf_t;
typedef struct ae_reverb ae_reverb_t;
typedef struct ae_dynamics ae_dynamics_t;

/*============================================================================
 * Core API
 *============================================================================*/

/* Get default configuration */
AE_API ae_config_t ae_get_default_config(void);

/* Create/destroy engine */
AE_API ae_engine_t* ae_create_engine(const ae_config_t* config);
AE_API void ae_destroy_engine(ae_engine_t* engine);

/* Error handling */
AE_API const char* ae_get_error_string(ae_result_t result);
AE_API const char* ae_get_last_error_detail(ae_engine_t* engine);

/* Audio processing */
AE_API ae_result_t ae_process(
    ae_engine_t* engine,
    const ae_audio_buffer_t* input,
    ae_audio_buffer_t* output
);

/*============================================================================
 * Parameter API
 *============================================================================*/

/* Preset management */
AE_API ae_result_t ae_load_preset(ae_engine_t* engine, const char* preset_name);
AE_API ae_result_t ae_get_scenario_defaults(const char* scenario_name,
                                            ae_main_params_t* main_out,
                                            ae_extended_params_t* extended_out);

/* Main parameters */
AE_API ae_result_t ae_set_main_params(ae_engine_t* engine, const ae_main_params_t* params);
AE_API ae_result_t ae_get_main_params(ae_engine_t* engine, ae_main_params_t* params);

/* Extended parameters */
AE_API ae_result_t ae_set_extended_params(ae_engine_t* engine, const ae_extended_params_t* params);

/* Individual setters (lock-free) */
AE_API ae_result_t ae_set_distance(ae_engine_t* engine, float distance);
AE_API ae_result_t ae_set_room_size(ae_engine_t* engine, float room_size);
AE_API ae_result_t ae_set_brightness(ae_engine_t* engine, float brightness);
AE_API ae_result_t ae_set_width(ae_engine_t* engine, float width);
AE_API ae_result_t ae_set_dry_wet(ae_engine_t* engine, float dry_wet);
AE_API ae_result_t ae_set_intensity(ae_engine_t* engine, float intensity);

/* Scenario application */
AE_API ae_result_t ae_apply_scenario(ae_engine_t* engine,
                                     const char* scenario_name,
                                     float intensity);
AE_API ae_result_t ae_blend_scenarios(ae_engine_t* engine,
                                     const ae_scenario_blend_t* blends,
                                     size_t count);

/* Cave model */
AE_API ae_result_t ae_apply_cave_model(ae_engine_t* engine,
                                       const ae_cave_params_t* params);

/* Binaural and precedence */
AE_API ae_result_t ae_azimuth_to_binaural(float azimuth_deg,
                                         float elevation_deg,
                                         float frequency_hz,
                                         ae_binaural_params_t* out);
AE_API ae_result_t ae_set_binaural_params(ae_engine_t* engine,
                                          const ae_binaural_params_t* params);
AE_API ae_result_t ae_set_source_position(ae_engine_t* engine,
                                          float azimuth_deg,
                                          float elevation_deg);
AE_API ae_result_t ae_apply_precedence(ae_engine_t* engine,
                                       const ae_precedence_t* params);

/* Dynamic parameters */
AE_API ae_result_t ae_set_doppler(ae_engine_t* engine,
                                  const ae_doppler_params_t* params);
AE_API ae_result_t ae_set_envelope(ae_engine_t* engine,
                                   const ae_adsr_t* envelope);

/* Semantic expression and biosignal update */
AE_API ae_result_t ae_apply_expression(ae_engine_t* engine,
                                       const char* expression);
AE_API ae_result_t ae_update_biosignal(ae_engine_t* engine,
                                       ae_biosignal_type_t type,
                                       float value);

/*============================================================================
 * Analysis API
 *============================================================================*/

/* Perceptual metrics */
AE_API ae_result_t ae_compute_perceptual_metrics(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_perceptual_metrics_t* out
);

/* Room metrics (from impulse response) */
AE_API ae_result_t ae_compute_room_metrics(
    const float* impulse_response,
    size_t ir_length,
    uint32_t sample_rate,
    ae_room_metrics_t* out
);

/* Spectral analysis */
AE_API ae_result_t ae_analyze_spectrum(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_spectral_features_t* out
);

/* Loudness analysis */
AE_API ae_result_t ae_analyze_loudness(
    ae_engine_t* engine,
    const ae_audio_buffer_t* signal,
    ae_weighting_t weighting,
    ae_loudness_t* out
);

AE_API ae_result_t ae_normalize_loudness(ae_engine_t* engine, float target_lufs);

/* Timbral analysis */
AE_API ae_result_t ae_timbral_to_processing(const ae_timbral_params_t* timbral,
                                            ae_main_params_t* main_out,
                                            ae_extended_params_t* extended_out);
AE_API ae_result_t ae_analyze_timbral(ae_engine_t* engine,
                                      const ae_audio_buffer_t* signal,
                                      ae_timbral_params_t* out);

/* SLM interface */
AE_API ae_slm_t* ae_slm_create(const ae_slm_config_t* config);
AE_API void ae_slm_destroy(ae_slm_t* slm);
AE_API ae_result_t ae_slm_interpret(ae_slm_t* slm,
                                    const char* natural_language_input,
                                    ae_main_params_t* params_out,
                                    char* preset_name_out,
                                    size_t preset_name_size);

/*============================================================================
 * Utility functions
 *============================================================================*/

/* Offline audio import (non-real-time) */
AE_API ae_result_t ae_import_audio_file(ae_engine_t* engine,
                                        const char* path,
                                        ae_audio_data_t* out);
AE_API void ae_free_audio_data(ae_audio_data_t* data);

/* Phon/Sone conversion */
AE_API float ae_phon_to_sone(float phon);
AE_API float ae_sone_to_phon(float sone);

/* Bark conversion */
AE_API float ae_hz_to_bark(float hz);
AE_API float ae_bark_to_hz(float bark);

/* Safe math utilities */
AE_API float ae_safe_log10(float x);
AE_API float ae_to_db(float linear);
AE_API float ae_from_db(float db);

/* Parameter validation */
AE_API ae_result_t ae_validate_params(const ae_main_params_t* params);

/* SIMD helper functions */
AE_API void ae_simd_add(float* dst, const float* a, const float* b, size_t n);
AE_API void ae_simd_mul(float* dst, const float* a, const float* b, size_t n);
AE_API void ae_simd_scale(float* dst, const float* src, float scale, size_t n);
AE_API void ae_simd_mac(float* dst, const float* a, const float* b, size_t n);

/* Module accessors */
AE_API ae_hrtf_t* ae_get_hrtf(ae_engine_t* engine);
AE_API ae_reverb_t* ae_get_reverb(ae_engine_t* engine);
AE_API ae_dynamics_t* ae_get_dynamics(ae_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* ACOUSTIC_ENGINE_H */
