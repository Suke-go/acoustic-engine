/**
 * @file acoustic_engine.h
 * @brief Acoustic Engine - Main Public API Header
 * @version 0.1.0
 */

#ifndef ACOUSTIC_ENGINE_H
#define ACOUSTIC_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
#define AE_VERSION_MINOR 2
#define AE_VERSION_PATCH 0
#define AE_VERSION                                                             \
  ((AE_VERSION_MAJOR << 16) | (AE_VERSION_MINOR << 8) | AE_VERSION_PATCH)

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
#define AE_MAX_EQ_BANDS 8

/*============================================================================
 * Version API
 *============================================================================*/
AE_API uint32_t ae_get_version(void);
AE_API const char *ae_get_version_string(void);
AE_API bool ae_check_abi_compatibility(uint32_t expected_version);

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
  float *samples;     /* Sample data (16-byte aligned recommended) */
  size_t frame_count; /* Number of frames (64 - 4096) */
  uint8_t channels;   /* Channel count (1 = mono, 2 = stereo) */
  bool interleaved;   /* true: LRLRLR, false: LLL...RRR */
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
  float distance;   /* 0.1 - 1000 m */
  float room_size;  /* 0.0 - 1.0 */
  float brightness; /* -1.0 - 1.0 */
  float width;      /* 0.0 - 2.0 */
  float dry_wet;    /* 0.0 - 1.0 */
  float intensity;  /* 0.0 - 1.0 */
} ae_main_params_t;

/*============================================================================
 * Extended parameters (Tier 2)
 *============================================================================*/
typedef struct {
  float decay_time;  /* 0.1 - 30.0 seconds */
  float diffusion;   /* 0.0 - 1.0 */
  float lofi_amount; /* 0.0 - 1.0 */
  float modulation;  /* 0.0 - 1.0 */
} ae_extended_params_t;

/*============================================================================
 * Scenario blending
 *============================================================================*/
typedef struct {
  const char *name; /* Scenario name */
  float weight;     /* Blend weight (0.0 - 1.0) */
} ae_scenario_blend_t;

/*============================================================================
 * Timbral parameters (extended perceptual controls)
 *============================================================================*/
typedef struct {
  float roughness;   /* 0.0 - 5.0 asper */
  float sharpness;   /* 0.0 - 4.0 acum */
  float fluctuation; /* 0.0 - 5.0 vacil */
  float tonality;    /* 0.0 - 1.0 */

  float warmth;   /* 0.0 - 1.0 */
  float presence; /* 0.0 - 1.0 */
  float air;      /* 0.0 - 1.0 */
  float body;     /* 0.0 - 1.0 */
  float clarity;  /* 0.0 - 1.0 */
  float punch;    /* 0.0 - 1.0 */
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
  float itd_us;        /* Interaural time difference (us) */
  float ild_db;        /* Interaural level difference (dB) */
  float azimuth_deg;   /* Azimuth (-180 to 180) */
  float elevation_deg; /* Elevation (-90 to 90) */
} ae_binaural_params_t;

typedef struct {
  float delay_ms; /* Delay time (ms) */
  float level_db; /* Level difference (dB) */
  float pan;      /* Pan (-1 to 1) */
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
  float attack_ms;     /* Attack time (ms) */
  float decay_ms;      /* Decay time (ms) */
  float sustain_level; /* Sustain level (0-1) */
  float release_ms;    /* Release time (ms) */
} ae_adsr_t;

/*============================================================================
 * Biosignal mapping
 *============================================================================*/
typedef enum { AE_BIOSIGNAL_HR = 0, AE_BIOSIGNAL_HRV = 1 } ae_biosignal_type_t;

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
  const char *model_path;
  size_t max_tokens;
  float temperature;
} ae_slm_config_t;

/*============================================================================
 * Engine configuration
 *============================================================================*/
typedef struct {
  uint32_t sample_rate;       /* 48000 (fixed) */
  uint32_t max_buffer_size;   /* Max buffer size (default: 4096) */
  const char *data_path;      /* Data directory (NULL = exe path) */
  const char *hrtf_path;      /* HRTF file path (NULL = builtin) */
  bool preload_hrtf;          /* true = load at startup */
  bool preload_all_presets;   /* true = load all presets */
  size_t max_reverb_time_sec; /* Max reverb time (default: 10s) */
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
 * Perceptual Profile - Scientifically grounded perceptual dimensions
 *
 * References:
 * - Brightness: Grey (1977), McAdams et al. (1995) - spectral centroid
 * - Roughness: Zwicker & Fastl (2007) - 70Hz modulation depth
 * - Fluctuation: Fastl (1982) - 4Hz modulation depth
 * - Distance: Zahorik (2002), Bronkhorst & Houtgast (1999) - DRR correlation
 * - Spaciousness: Bradley & Soulodre (1995) - IACC, LEV
 * - Clarity: ISO 3382 - C50/C80
 *============================================================================*/
typedef struct {
  /* Timbral dimensions (MDS literature: Grey 1977, McAdams 1995) */
  float brightness;       /* 0-1, normalized spectral centroid */
  float attack_sharpness; /* 0-1, from rise time/attack slope */
  float spectral_flux;    /* 0-1, spectral change rate */

  /* Psychoacoustic dimensions (Zwicker & Fastl 2007) */
  float roughness_norm;   /* 0-1, normalized from asper */
  float fluctuation_norm; /* 0-1, normalized from vacil */
  float sharpness_norm;   /* 0-1, normalized from acum */

  /* Spatial dimensions (room acoustics: Bradley 1995, ISO 3382) */
  float perceived_distance; /* 0-1, from DRR (Zahorik 2002) */
  float spaciousness;       /* 0-1, from 1-IACC (ASW/LEV) */
  float clarity_norm;       /* 0-1, from C50/C80 */
  float envelopment;        /* 0-1, from late IACC (LEV) */

  /* Raw values for advanced users */
  float spectral_centroid_hz; /* Raw centroid (Hz) */
  float roughness_asper;      /* Raw roughness (asper) */
  float fluctuation_vacil;    /* Raw fluctuation (vacil) */
  float sharpness_acum;       /* Raw sharpness (acum) */
  float drr_db;               /* Direct-to-reverberant ratio (dB) */
  float iacc_early;           /* Early IACC (0-80ms) */
  float iacc_late;            /* Late IACC (80ms+) */
  float c50_db;               /* Clarity C50 (dB) */
  float c80_db;               /* Clarity C80 (dB) */
} ae_perceptual_profile_t;

/*============================================================================
 * Room metrics (ISO 3382)
 *============================================================================*/
typedef struct {
  float edt;         /* Early Decay Time (seconds) */
  float edt_band[6]; /* Band-specific EDT (125Hz-4kHz) */
  float c50;         /* Clarity for speech (dB) */
  float c80;         /* Clarity for music (dB) */
  float d50;         /* Definition (0-1) */
  float ts_ms;       /* Center time (ms) */
  float strength_g;  /* Sound strength (dB) */
  float sti;         /* Speech Transmission Index (0-1) */
} ae_room_metrics_t;

/*============================================================================
 * Spectral features
 *============================================================================*/
typedef struct {
  float centroid_hz;                      /* Spectral centroid (Hz) */
  float spread_hz;                        /* Spectral spread (Hz) */
  float flux;                             /* Spectral flux */
  float hnr_db;                           /* Harmonic-to-noise ratio (dB) */
  float flatness;                         /* Spectral flatness (0-1) */
  float rolloff_hz;                       /* Rolloff frequency (Hz) */
  float bark_spectrum[AE_NUM_BARK_BANDS]; /* Bark spectrum */
} ae_spectral_features_t;

/*============================================================================
 * Loudness metrics
 *============================================================================*/
typedef enum {
  AE_WEIGHTING_NONE,     /* Z-weighting (flat) */
  AE_WEIGHTING_A,        /* A-weighting */
  AE_WEIGHTING_C,        /* C-weighting */
  AE_WEIGHTING_ITU_R_468 /* ITU-R 468 */
} ae_weighting_t;

typedef struct {
  float loudness_sone; /* Loudness (sone) */
  float loudness_phon; /* Loudness level (phon) */
  float peak_db;       /* Peak level (dBFS) */
  float rms_db;        /* RMS level (dBFS) */
  float lufs;          /* Integrated LUFS (EBU R128) */
  float lra;           /* Loudness Range (LU) */
} ae_loudness_t;

/*============================================================================
 * Auditory modeling (AMT)
 *============================================================================*/
typedef struct ae_gammatone ae_gammatone_t;

typedef struct {
  uint32_t n_channels;
  float f_low;
  float f_high;
  uint8_t filter_order;
  uint32_t sample_rate;
} ae_gammatone_config_t;

typedef struct {
  float compression_exponent;
  float lpf_cutoff_hz;
} ae_ihc_config_t;

typedef struct ae_adaptloop ae_adaptloop_t;

typedef struct {
  uint8_t n_stages;
  float time_constants[5];
  float min_output;
  uint32_t sample_rate;
} ae_adaptloop_config_t;

typedef struct {
  float specific_loudness[AE_NUM_BARK_BANDS];
  float total_loudness_sone;
  float loudness_level_phon;
  float peak_loudness_sone;
  uint8_t peak_bark_band;
} ae_zwicker_loudness_t;

typedef enum {
  AE_LOUDNESS_METHOD_ISO_532_1,
  AE_LOUDNESS_METHOD_ISO_532_2,
  AE_LOUDNESS_METHOD_MOORE
} ae_loudness_method_t;

typedef enum {
  AE_SHARPNESS_DIN_45692,
  AE_SHARPNESS_AURES,
  AE_SHARPNESS_BISMARCK
} ae_sharpness_method_t;

/*============================================================================
 * DRNL (Dual-Resonance Nonlinear) Filterbank
 *============================================================================*/
typedef struct ae_drnl ae_drnl_t;

typedef struct {
  uint32_t n_channels;   /* Number of frequency channels (4-64) */
  float f_low;           /* Lowest center frequency (Hz) */
  float f_high;          /* Highest center frequency (Hz) */
  float compression_exp; /* Compression exponent (~0.25) */
  float lin_gain;        /* Linear path gain */
  float nlin_a;          /* Nonlinear path 'a' coefficient */
  float nlin_b;          /* Nonlinear path 'b' coefficient */
  uint32_t sample_rate;  /* Sample rate (Hz) */
} ae_drnl_config_t;

/*============================================================================
 * Modulation Filterbank
 *============================================================================*/
typedef struct ae_modfb ae_modfb_t;

typedef struct {
  uint32_t n_channels;  /* Number of modulation channels (8-16) */
  float f_low;          /* Lowest modulation frequency (0.5 Hz) */
  float f_high;         /* Highest modulation frequency (256 Hz) */
  uint32_t sample_rate; /* Sample rate of envelope input (Hz) */
} ae_modfb_config_t;

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
AE_API ae_engine_t *ae_create_engine(const ae_config_t *config);
AE_API void ae_destroy_engine(ae_engine_t *engine);

/* Error handling */
AE_API const char *ae_get_error_string(ae_result_t result);
AE_API const char *ae_get_last_error_detail(ae_engine_t *engine);

/* Audio processing */
AE_API ae_result_t ae_process(ae_engine_t *engine,
                              const ae_audio_buffer_t *input,
                              ae_audio_buffer_t *output);

/*============================================================================
 * Parameter API
 *============================================================================*/

/* Preset management */
AE_API ae_result_t ae_load_preset(ae_engine_t *engine, const char *preset_name);
AE_API ae_result_t ae_get_scenario_defaults(const char *scenario_name,
                                            ae_main_params_t *main_out,
                                            ae_extended_params_t *extended_out);

/* Main parameters */
AE_API ae_result_t ae_set_main_params(ae_engine_t *engine,
                                      const ae_main_params_t *params);
AE_API ae_result_t ae_get_main_params(ae_engine_t *engine,
                                      ae_main_params_t *params);

/* Extended parameters */
AE_API ae_result_t ae_set_extended_params(ae_engine_t *engine,
                                          const ae_extended_params_t *params);

/* Individual setters (lock-free) */
AE_API ae_result_t ae_set_distance(ae_engine_t *engine, float distance);
AE_API ae_result_t ae_set_room_size(ae_engine_t *engine, float room_size);
AE_API ae_result_t ae_set_brightness(ae_engine_t *engine, float brightness);
AE_API ae_result_t ae_set_width(ae_engine_t *engine, float width);
AE_API ae_result_t ae_set_dry_wet(ae_engine_t *engine, float dry_wet);
AE_API ae_result_t ae_set_intensity(ae_engine_t *engine, float intensity);

/* Scenario application */
AE_API ae_result_t ae_apply_scenario(ae_engine_t *engine,
                                     const char *scenario_name,
                                     float intensity);
AE_API ae_result_t ae_blend_scenarios(ae_engine_t *engine,
                                      const ae_scenario_blend_t *blends,
                                      size_t count);

/* Cave model */
AE_API ae_result_t ae_apply_cave_model(ae_engine_t *engine,
                                       const ae_cave_params_t *params);

/* Binaural and precedence */
AE_API ae_result_t ae_azimuth_to_binaural(float azimuth_deg,
                                          float elevation_deg,
                                          float frequency_hz,
                                          ae_binaural_params_t *out);
AE_API ae_result_t ae_set_binaural_params(ae_engine_t *engine,
                                          const ae_binaural_params_t *params);
AE_API ae_result_t ae_set_source_position(ae_engine_t *engine,
                                          float azimuth_deg,
                                          float elevation_deg);
AE_API ae_result_t ae_apply_precedence(ae_engine_t *engine,
                                       const ae_precedence_t *params);

/* Dynamic parameters */
AE_API ae_result_t ae_set_doppler(ae_engine_t *engine,
                                  const ae_doppler_params_t *params);
AE_API ae_result_t ae_set_envelope(ae_engine_t *engine,
                                   const ae_adsr_t *envelope);

/* Semantic expression and biosignal update */
AE_API ae_result_t ae_apply_expression(ae_engine_t *engine,
                                       const char *expression);
AE_API ae_result_t ae_update_biosignal(ae_engine_t *engine,
                                       ae_biosignal_type_t type, float value);

/*============================================================================
 * Analysis API
 *============================================================================*/

/* Auditory filterbank */
AE_API ae_gammatone_t *ae_gammatone_create(const ae_gammatone_config_t *config);
AE_API void ae_gammatone_destroy(ae_gammatone_t *gt);
AE_API ae_result_t ae_gammatone_process(ae_gammatone_t *gt, const float *input,
                                        size_t n_samples, float **output);
AE_API ae_result_t ae_gammatone_get_center_frequencies(ae_gammatone_t *gt,
                                                       float *frequencies,
                                                       size_t *n_channels);

/* IHC envelope */
AE_API ae_result_t ae_ihc_envelope(const float *basilar_membrane_output,
                                   size_t n_samples,
                                   const ae_ihc_config_t *config,
                                   float *ihc_output);

/* Adaptation loop */
AE_API ae_adaptloop_t *ae_adaptloop_create(const ae_adaptloop_config_t *config);
AE_API void ae_adaptloop_destroy(ae_adaptloop_t *al);
AE_API ae_result_t ae_adaptloop_process(ae_adaptloop_t *al, const float *input,
                                        size_t n_samples, float *output);
AE_API void ae_adaptloop_reset(ae_adaptloop_t *al);

/* DRNL Filterbank */
AE_API ae_drnl_t *ae_drnl_create(const ae_drnl_config_t *config);
AE_API void ae_drnl_destroy(ae_drnl_t *drnl);
AE_API ae_result_t ae_drnl_process(ae_drnl_t *drnl, const float *input,
                                   size_t n_samples, float **output);
AE_API ae_result_t ae_drnl_get_center_frequencies(ae_drnl_t *drnl,
                                                  float *frequencies,
                                                  size_t *n_channels);

/* Modulation Filterbank */
AE_API ae_modfb_t *ae_modfb_create(const ae_modfb_config_t *config);
AE_API void ae_modfb_destroy(ae_modfb_t *mfb);
AE_API ae_result_t ae_modfb_process(ae_modfb_t *mfb, const float *input,
                                    size_t n_samples, float **output);
AE_API ae_result_t ae_modfb_get_center_frequencies(ae_modfb_t *mfb,
                                                   float *frequencies,
                                                   size_t *n_channels);

/* Perceptual metrics */
AE_API ae_result_t ae_compute_perceptual_metrics(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    ae_perceptual_metrics_t *out);

/*============================================================================
 * Perceptual Profile Analysis API
 *============================================================================*/

/**
 * Analyze perceptual profile of audio signal
 *
 * Computes scientifically-grounded perceptual dimensions.
 * Requires stereo input for spatial metrics (IACC).
 *
 * @param engine Engine handle
 * @param signal Audio buffer (stereo recommended)
 * @param out Output perceptual profile
 * @return AE_OK on success
 */
AE_API ae_result_t ae_analyze_perceptual_profile(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    ae_perceptual_profile_t *out);

/*============================================================================
 * Perceptual Control API - Scientifically grounded mappings
 *============================================================================*/

/**
 * Set perceived distance (Zahorik 2002, Bronkhorst 1999)
 *
 * Maps perceptual distance to physical parameters:
 * - DRR (Direct-to-Reverberant Ratio)
 * - High-frequency attenuation (air absorption)
 * - Pre-delay adjustment
 *
 * @param engine Engine handle
 * @param distance_perception 0.0 (near) to 1.0 (far)
 * @return AE_OK on success
 */
AE_API ae_result_t ae_set_perceived_distance(ae_engine_t *engine,
                                             float distance_perception);

/**
 * Set perceived spaciousness (Bradley & Soulodre 1995)
 *
 * Maps perceptual spaciousness to:
 * - M/S width (stereo width)
 * - Early reflection density
 * - Decorrelation amount
 *
 * Based on ASW (Apparent Source Width) and LEV (Listener Envelopment).
 *
 * @param engine Engine handle
 * @param spaciousness 0.0 (narrow) to 1.0 (wide/enveloping)
 * @return AE_OK on success
 */
AE_API ae_result_t ae_set_perceived_spaciousness(ae_engine_t *engine,
                                                 float spaciousness);

/**
 * Set perceived clarity (ISO 3382 C50/C80)
 *
 * Maps perceptual clarity to:
 * - DRR (Direct-to-Reverberant Ratio)
 * - RT60 (Reverberation Time)
 * - Early-to-late energy ratio
 *
 * @param engine Engine handle
 * @param clarity 0.0 (muddy) to 1.0 (clear)
 * @return AE_OK on success
 */
AE_API ae_result_t ae_set_perceived_clarity(ae_engine_t *engine, float clarity);

/* Room metrics (from impulse response) */
AE_API ae_result_t ae_compute_room_metrics(const float *impulse_response,
                                           size_t ir_length,
                                           uint32_t sample_rate,
                                           ae_room_metrics_t *out);

/* Spectral analysis */
AE_API ae_result_t ae_analyze_spectrum(ae_engine_t *engine,
                                       const ae_audio_buffer_t *signal,
                                       ae_spectral_features_t *out);

/* Loudness analysis */
AE_API ae_result_t ae_analyze_loudness(ae_engine_t *engine,
                                       const ae_audio_buffer_t *signal,
                                       ae_weighting_t weighting,
                                       ae_loudness_t *out);

AE_API ae_result_t ae_normalize_loudness(ae_engine_t *engine,
                                         float target_lufs);

/* Zwicker loudness */
AE_API ae_result_t ae_analyze_zwicker_loudness(ae_engine_t *engine,
                                               const ae_audio_buffer_t *signal,
                                               ae_loudness_method_t method,
                                               ae_zwicker_loudness_t *out);

/* Timbral analysis */
AE_API ae_result_t ae_timbral_to_processing(const ae_timbral_params_t *timbral,
                                            ae_main_params_t *main_out,
                                            ae_extended_params_t *extended_out);
AE_API ae_result_t ae_analyze_timbral(ae_engine_t *engine,
                                      const ae_audio_buffer_t *signal,
                                      ae_timbral_params_t *out);

/* Sharpness, roughness, fluctuation */
AE_API ae_result_t ae_compute_sharpness(ae_engine_t *engine,
                                        const ae_audio_buffer_t *signal,
                                        ae_sharpness_method_t method,
                                        float *sharpness_acum);
AE_API ae_result_t ae_compute_roughness(ae_engine_t *engine,
                                        const ae_audio_buffer_t *signal,
                                        float *roughness_asper);
AE_API ae_result_t ae_compute_roughness_over_time(
    ae_engine_t *engine, const ae_audio_buffer_t *signal, float hop_size_ms,
    float *roughness_array, size_t *n_frames);
AE_API ae_result_t ae_compute_fluctuation_strength(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    float *fluctuation_vacil);

/*============================================================================
 * BMLD (Binaural Masking Level Difference)
 *============================================================================*/
typedef struct {
  float signal_frequency_hz; /* Signal frequency (Hz) */
  float noise_bandwidth_hz;  /* Noise bandwidth (Hz) */
  float signal_correlation;  /* Signal L/R correlation: -1 (Sπ) to +1 (S0) */
  float noise_correlation;   /* Noise L/R correlation: -1 to +1 */
} ae_bmld_params_t;

/* Extended BMLD parameters with full EC model */
typedef struct {
  float signal_frequency_hz; /* Signal frequency (Hz) */
  float noise_bandwidth_hz;  /* Noise bandwidth (Hz) */
  float signal_correlation;  /* Signal L/R correlation: -1 (Sπ) to +1 (S0) */
  float noise_correlation;   /* Noise L/R correlation: -1 to +1 */
  float signal_itd_us;       /* Signal interaural time difference (μs) */
  float noise_itd_us;        /* Noise interaural time difference (μs) */
  float
      equalization_error; /* EC model equalization error σ_ε (default: 0.25) */
  float cancellation_error; /* EC model cancellation error σ_δ (default: 0.0001)
                             */
} ae_bmld_extended_params_t;

AE_API float ae_compute_bmld(const ae_bmld_params_t *params);
AE_API float ae_compute_bmld_extended(const ae_bmld_extended_params_t *params);

/*============================================================================
 * SII (Speech Intelligibility Index) - ANSI S3.5
 *============================================================================*/
#define AE_SII_BANDS_7 7
#define AE_SII_BANDS_21 21

typedef struct {
  float speech_level_db; /* Speech level (dB SPL) */
  float noise_level_db;  /* Noise level (dB SPL) */
  float rt60_seconds;    /* Reverberation time (seconds) */
  bool use_extended_sii; /* Use extended SII */
} ae_sii_params_t;

/* Extended SII with 21-band ANSI S3.5 and hearing loss modeling */
typedef struct {
  float speech_level_db;                    /* Overall speech level (dB SPL) */
  float noise_spectrum_db[AE_SII_BANDS_21]; /* Per-band noise (dB SPL) */
  float hearing_threshold_db[AE_SII_BANDS_21]; /* Hearing loss thresholds (dB
                                                  HL) */
  float rt60_seconds;                          /* Reverberation time (s) */
  bool use_21_band;                            /* true=21-band, false=7-band */
  bool model_hearing_loss;                     /* Apply hearing loss model */
} ae_sii_extended_params_t;

/* Detailed SII result */
typedef struct {
  float sii_value;                          /* Total SII (0-1) */
  float band_sii[AE_SII_BANDS_21];          /* Per-band SII contribution */
  float audibility[AE_SII_BANDS_21];        /* Per-band audibility (0-1) */
  float distortion_factor[AE_SII_BANDS_21]; /* Per-band distortion */
  uint8_t n_bands;                          /* Number of bands used */
} ae_sii_result_t;

/* Binaural SII with BMLD advantage */
typedef struct {
  ae_sii_params_t left;    /* Left ear parameters */
  ae_sii_params_t right;   /* Right ear parameters */
  float bmld_advantage_db; /* BMLD threshold improvement (dB) */
} ae_binaural_sii_params_t;

AE_API ae_result_t ae_compute_sii(const ae_sii_params_t *params,
                                  float *sii_value);
AE_API ae_result_t ae_compute_sii_extended(
    const ae_sii_extended_params_t *params, ae_sii_result_t *result);
AE_API ae_result_t ae_compute_binaural_sii(
    const ae_binaural_sii_params_t *params, float *sii_value);

/*============================================================================
 * Auditory Representation Pipeline
 *============================================================================*/
typedef struct {
  float **gammatone_output;   /* [n_audio_ch][n_samples] */
  float **ihc_output;         /* [n_audio_ch][n_samples] */
  float **adaptation_output;  /* [n_audio_ch][n_samples] */
  float ***modulation_output; /* [n_audio_ch][n_mod_ch][n_samples] */
  uint32_t n_audio_channels;
  uint32_t n_modulation_channels;
  size_t n_samples;
} ae_auditory_repr_t;

typedef struct {
  ae_gammatone_config_t gammatone;
  ae_ihc_config_t ihc;
  ae_adaptloop_config_t adaptation;
  ae_modfb_config_t modulation;
  bool compute_gammatone;
  bool compute_ihc;
  bool compute_adaptation;
  bool compute_modulation;
} ae_auditory_pipeline_config_t;

AE_API ae_result_t ae_compute_auditory_representation(
    ae_engine_t *engine, const ae_audio_buffer_t *signal,
    const ae_auditory_pipeline_config_t *config, ae_auditory_repr_t *out);
AE_API void ae_free_auditory_representation(ae_auditory_repr_t *repr);

/* SLM interface */
AE_API ae_slm_t *ae_slm_create(const ae_slm_config_t *config);
AE_API void ae_slm_destroy(ae_slm_t *slm);
AE_API ae_result_t ae_slm_interpret(ae_slm_t *slm,
                                    const char *natural_language_input,
                                    ae_main_params_t *params_out,
                                    char *preset_name_out,
                                    size_t preset_name_size);

/*============================================================================
 * Utility functions
 *============================================================================*/

/* Offline audio import (non-real-time) */
AE_API ae_result_t ae_import_audio_file(ae_engine_t *engine, const char *path,
                                        ae_audio_data_t *out);
AE_API void ae_free_audio_data(ae_audio_data_t *data);

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
AE_API ae_result_t ae_validate_params(const ae_main_params_t *params);

/*============================================================================
 * Physical Propagation Models
 *============================================================================*/

/* Francois-Garrison ocean absorption model (dB/km) */
AE_API float ae_francois_garrison_absorption(float f_khz, float T_celsius,
                                             float salinity_ppt, float depth_m);

/* ISO 9613-1 atmospheric absorption model (dB/m) */
AE_API float ae_iso9613_absorption(float f_hz, float T_celsius,
                                   float humidity_pct, float pressure_kpa);

/* Cave modal resonance frequency (Hz) */
AE_API float ae_cave_modal_frequency(float dimension_m, int mode_number,
                                     float temperature_c);

/* Calculate flutter echo parameters */
AE_API void ae_calculate_flutter(float wall_distance_m, float temperature_c,
                                 float *delay_ms_out, float *flutter_freq_out);

/* Eyring RT60 for low-absorption spaces (seconds) */
AE_API float ae_eyring_rt60(float volume_m3, float surface_m2, float avg_alpha);

/* Frequency-dependent rock wall absorption coefficient (0-1) */
AE_API float ae_rock_wall_absorption(float f_hz);

/* SIMD helper functions */
AE_API void ae_simd_add(float *dst, const float *a, const float *b, size_t n);
AE_API void ae_simd_mul(float *dst, const float *a, const float *b, size_t n);
AE_API void ae_simd_scale(float *dst, const float *src, float scale, size_t n);
AE_API void ae_simd_mac(float *dst, const float *a, const float *b, size_t n);

/* Module accessors */
AE_API ae_hrtf_t *ae_get_hrtf(ae_engine_t *engine);
AE_API ae_reverb_t *ae_get_reverb(ae_engine_t *engine);
AE_API ae_dynamics_t *ae_get_dynamics(ae_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* ACOUSTIC_ENGINE_H */
