#include "ae_internal.h"

#include <errno.h>
#include <stdio.h>

#ifndef AE_ENABLE_EXTERNAL_DECODER
#define AE_ENABLE_EXTERNAL_DECODER 0
#endif

#if AE_ENABLE_EXTERNAL_DECODER
#if defined(_WIN32)
#define AE_POPEN _popen
#define AE_PCLOSE _pclose
#else
#define AE_POPEN popen
#define AE_PCLOSE pclose
#endif
#endif

typedef struct {
  float *samples;
  size_t frame_count;
  uint8_t channels;
  uint32_t sample_rate;
} ae_decoded_audio_t;

static void ae_audio_io_set_error(ae_engine_t *engine, const char *message) {
  if (engine && message)
    ae_set_error(engine, message);
}

#if AE_ENABLE_EXTERNAL_DECODER
static const char *ae_audio_io_get_ffmpeg_path(void) {
  const char *path = getenv("AE_FFMPEG_PATH");
  if (path && path[0] != '\0')
    return path;
  return "ffmpeg";
}
#endif

static uint16_t ae_read_u16_le(const uint8_t *data) {
  return (uint16_t)(data[0] | (uint16_t)data[1] << 8);
}

static uint32_t ae_read_u32_le(const uint8_t *data) {
  return (uint32_t)(data[0] | (uint32_t)data[1] << 8 |
                    (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24);
}

static int ae_audio_io_is_wav(FILE *file) {
  uint8_t header[12];
  if (!file)
    return 0;
  if (fread(header, 1, sizeof(header), file) != sizeof(header))
    return 0;
  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  return 1;
}

static int ae_audio_io_is_subformat_pcm(uint32_t subformat) {
  return subformat == 0x00000001u;
}

static int ae_audio_io_is_subformat_float(uint32_t subformat) {
  return subformat == 0x00000003u;
}

static ae_result_t ae_audio_io_decode_wav(FILE *file,
                                          ae_decoded_audio_t *decoded,
                                          ae_engine_t *engine) {
  uint8_t header[12];
  uint8_t chunk_header[8];
  uint32_t fmt_size = 0;
  uint16_t audio_format = 0;
  uint16_t channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits_per_sample = 0;
  uint16_t block_align = 0;
  uint32_t data_size = 0;
  long data_offset = 0;
  int fmt_found = 0;
  int data_found = 0;

  if (!file || !decoded)
    return AE_ERROR_INVALID_PARAM;

  if (fread(header, 1, sizeof(header), file) != sizeof(header))
    return AE_ERROR_INVALID_PARAM;
  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0)
    return AE_ERROR_INVALID_PARAM;

  while (fread(chunk_header, 1, sizeof(chunk_header), file) ==
         sizeof(chunk_header)) {
    uint32_t chunk_size = ae_read_u32_le(chunk_header + 4);
    if (memcmp(chunk_header, "fmt ", 4) == 0) {
      uint8_t fmt[64];
      if (chunk_size < 16 || chunk_size > sizeof(fmt)) {
        ae_audio_io_set_error(engine, "Unsupported WAV fmt chunk size");
        return AE_ERROR_INVALID_PARAM;
      }
      if (fread(fmt, 1, chunk_size, file) != chunk_size)
        return AE_ERROR_INVALID_PARAM;
      fmt_size = chunk_size;
      audio_format = ae_read_u16_le(fmt);
      channels = ae_read_u16_le(fmt + 2);
      sample_rate = ae_read_u32_le(fmt + 4);
      block_align = ae_read_u16_le(fmt + 12);
      bits_per_sample = ae_read_u16_le(fmt + 14);
      if (audio_format == 0xFFFE && fmt_size >= 40) {
        uint32_t subformat = ae_read_u32_le(fmt + 24);
        if (ae_audio_io_is_subformat_pcm(subformat)) {
          audio_format = 1;
        } else if (ae_audio_io_is_subformat_float(subformat)) {
          audio_format = 3;
        } else {
          ae_audio_io_set_error(engine, "Unsupported WAV subformat");
          return AE_ERROR_INVALID_PARAM;
        }
      }
      fmt_found = 1;
    } else if (memcmp(chunk_header, "data", 4) == 0) {
      data_offset = ftell(file);
      data_size = chunk_size;
      data_found = 1;
      fseek(file, (long)chunk_size, SEEK_CUR);
    } else {
      fseek(file, (long)chunk_size, SEEK_CUR);
    }
    if (chunk_size & 1)
      fseek(file, 1, SEEK_CUR);
    if (fmt_found && data_found)
      break;
  }

  if (!fmt_found || !data_found) {
    ae_audio_io_set_error(engine, "WAV fmt/data chunk not found");
    return AE_ERROR_INVALID_PARAM;
  }
  if (channels == 0 || sample_rate == 0) {
    ae_audio_io_set_error(engine, "Invalid WAV metadata");
    return AE_ERROR_INVALID_PARAM;
  }

  if (block_align == 0) {
    block_align =
        (uint16_t)(channels * (uint16_t)((bits_per_sample + 7) / 8));
  }
  if (block_align == 0) {
    ae_audio_io_set_error(engine, "Invalid WAV block align");
    return AE_ERROR_INVALID_PARAM;
  }

  size_t frame_count = (size_t)(data_size / block_align);
  if (frame_count == 0) {
    ae_audio_io_set_error(engine, "WAV data is empty");
    return AE_ERROR_INVALID_PARAM;
  }

  if (fseek(file, data_offset, SEEK_SET) != 0)
    return AE_ERROR_INVALID_PARAM;

  uint8_t *raw = (uint8_t *)malloc(data_size);
  if (!raw)
    return AE_ERROR_OUT_OF_MEMORY;
  if (fread(raw, 1, data_size, file) != data_size) {
    free(raw);
    return AE_ERROR_INVALID_PARAM;
  }

  float *samples =
      (float *)malloc(frame_count * channels * sizeof(float));
  if (!samples) {
    free(raw);
    return AE_ERROR_OUT_OF_MEMORY;
  }

  size_t sample_count = frame_count * channels;
  size_t bytes_per_sample = (size_t)((bits_per_sample + 7) / 8);
  if (bytes_per_sample == 0) {
    free(raw);
    free(samples);
    ae_audio_io_set_error(engine, "Invalid WAV bit depth");
    return AE_ERROR_INVALID_PARAM;
  }
  size_t offset = 0;
  for (size_t i = 0; i < sample_count; ++i) {
    float value = 0.0f;
    if (audio_format == 1) {
      if (bits_per_sample == 8) {
        uint8_t v = raw[offset];
        value = ((float)v - 128.0f) / 128.0f;
      } else if (bits_per_sample == 16) {
        int16_t v = (int16_t)(raw[offset] | (raw[offset + 1] << 8));
        value = (float)v / 32768.0f;
      } else if (bits_per_sample == 24) {
        int32_t v =
            (int32_t)(raw[offset] | (raw[offset + 1] << 8) |
                      (raw[offset + 2] << 16));
        if (v & 0x800000)
          v |= ~0xFFFFFF;
        value = (float)v / 8388608.0f;
      } else if (bits_per_sample == 32) {
        int32_t v = (int32_t)(raw[offset] | (raw[offset + 1] << 8) |
                              (raw[offset + 2] << 16) |
                              (raw[offset + 3] << 24));
        value = (float)v / 2147483648.0f;
      } else {
        free(raw);
        free(samples);
        ae_audio_io_set_error(engine, "Unsupported WAV PCM bit depth");
        return AE_ERROR_INVALID_PARAM;
      }
    } else if (audio_format == 3) {
      if (bits_per_sample == 32) {
        float v = 0.0f;
        memcpy(&v, raw + offset, sizeof(float));
        value = v;
      } else if (bits_per_sample == 64) {
        double v = 0.0;
        memcpy(&v, raw + offset, sizeof(double));
        value = (float)v;
      } else {
        free(raw);
        free(samples);
        ae_audio_io_set_error(engine, "Unsupported WAV float bit depth");
        return AE_ERROR_INVALID_PARAM;
      }
    } else {
      free(raw);
      free(samples);
      ae_audio_io_set_error(engine, "Unsupported WAV format");
      return AE_ERROR_INVALID_PARAM;
    }
    samples[i] = ae_clamp(value, -1.0f, 1.0f);
    offset += bytes_per_sample;
  }

  free(raw);
  decoded->samples = samples;
  decoded->frame_count = frame_count;
  decoded->channels = (uint8_t)channels;
  decoded->sample_rate = sample_rate;
  return AE_OK;
}

static ae_result_t ae_audio_io_downmix(const float *input,
                                       size_t frames,
                                       uint8_t in_channels,
                                       uint8_t out_channels,
                                       float **out_samples) {
  if (!input || !out_samples || frames == 0 || in_channels == 0)
    return AE_ERROR_INVALID_PARAM;

  if (out_channels != 1 && out_channels != 2)
    return AE_ERROR_INVALID_PARAM;

  float *output =
      (float *)malloc(frames * out_channels * sizeof(float));
  if (!output)
    return AE_ERROR_OUT_OF_MEMORY;

  if (out_channels == 1) {
    for (size_t i = 0; i < frames; ++i) {
      float sum = 0.0f;
      for (uint8_t c = 0; c < in_channels; ++c) {
        sum += input[i * in_channels + c];
      }
      output[i] = sum / (float)in_channels;
    }
  } else if (out_channels == 2) {
    if (in_channels == 1) {
      for (size_t i = 0; i < frames; ++i) {
        float mono = input[i];
        output[i * 2] = mono;
        output[i * 2 + 1] = mono;
      }
    } else if (in_channels == 2) {
      memcpy(output, input, frames * 2 * sizeof(float));
    } else {
      for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (uint8_t c = 0; c < in_channels; ++c) {
          sum += input[i * in_channels + c];
        }
        float mono = sum / (float)in_channels;
        output[i * 2] = mono;
        output[i * 2 + 1] = mono;
      }
    }
  }

  *out_samples = output;
  return AE_OK;
}

static ae_result_t ae_audio_io_resample(const float *input,
                                        size_t in_frames,
                                        uint32_t in_rate,
                                        uint32_t out_rate,
                                        uint8_t channels,
                                        float **out_samples,
                                        size_t *out_frames) {
  if (!input || !out_samples || !out_frames || in_frames == 0 ||
      in_rate == 0 || out_rate == 0 || channels == 0)
    return AE_ERROR_INVALID_PARAM;

  if (in_rate == out_rate) {
    size_t count = in_frames * channels;
    float *copy = (float *)malloc(count * sizeof(float));
    if (!copy)
      return AE_ERROR_OUT_OF_MEMORY;
    memcpy(copy, input, count * sizeof(float));
    *out_samples = copy;
    *out_frames = in_frames;
    return AE_OK;
  }

  double ratio = (double)out_rate / (double)in_rate;
  size_t frames = (size_t)((double)in_frames * ratio + 0.5);
  if (frames == 0)
    frames = 1;

  float *output = (float *)malloc(frames * channels * sizeof(float));
  if (!output)
    return AE_ERROR_OUT_OF_MEMORY;

  for (size_t i = 0; i < frames; ++i) {
    double src_pos = (double)i / ratio;
    size_t idx = (size_t)src_pos;
    double frac = src_pos - (double)idx;
    if (idx >= in_frames - 1) {
      idx = in_frames - 1;
      frac = 0.0;
    }
    size_t idx_next = (idx + 1 < in_frames) ? idx + 1 : idx;
    for (uint8_t c = 0; c < channels; ++c) {
      float s0 = input[idx * channels + c];
      float s1 = input[idx_next * channels + c];
      output[i * channels + c] = s0 + (float)((s1 - s0) * frac);
    }
  }

  *out_samples = output;
  *out_frames = frames;
  return AE_OK;
}

static ae_result_t ae_audio_io_convert_decoded(const ae_decoded_audio_t *decoded,
                                               ae_audio_data_t *out,
                                               ae_engine_t *engine) {
  if (!decoded || !out || !decoded->samples)
    return AE_ERROR_INVALID_PARAM;

  uint8_t out_channels = decoded->channels == 1 ? 1 : 2;
  float *downmixed = NULL;
  ae_result_t res = ae_audio_io_downmix(
      decoded->samples, decoded->frame_count, decoded->channels, out_channels,
      &downmixed);
  if (res != AE_OK)
    return res;

  float *resampled = NULL;
  size_t resampled_frames = 0;
  res = ae_audio_io_resample(downmixed, decoded->frame_count,
                             decoded->sample_rate, AE_SAMPLE_RATE,
                             out_channels, &resampled, &resampled_frames);
  free(downmixed);
  if (res != AE_OK)
    return res;

  size_t count = resampled_frames * out_channels;
  for (size_t i = 0; i < count; ++i)
    resampled[i] = ae_clamp(resampled[i], -1.0f, 1.0f);

  out->buffer.samples = resampled;
  out->buffer.frame_count = resampled_frames;
  out->buffer.channels = out_channels;
  out->buffer.interleaved = true;
  out->sample_rate = AE_SAMPLE_RATE;
  return AE_OK;
}

#if AE_ENABLE_EXTERNAL_DECODER
static ae_result_t ae_audio_io_decode_ffmpeg(const char *path,
                                             ae_audio_data_t *out,
                                             ae_engine_t *engine) {
  char command[1024];
  FILE *pipe = NULL;
  uint8_t *buffer = NULL;
  size_t buffer_size = 0;
  size_t buffer_capacity = 0;

  if (!path || !out)
    return AE_ERROR_INVALID_PARAM;

  const char *ffmpeg = ae_audio_io_get_ffmpeg_path();
  snprintf(command, sizeof(command),
           "\"%s\" -v error -i \"%s\" -f f32le -ac 2 -ar %d pipe:1",
           ffmpeg, path, AE_SAMPLE_RATE);

  pipe = AE_POPEN(command, "rb");
  if (!pipe) {
    ae_audio_io_set_error(engine, "Failed to launch ffmpeg");
    return AE_ERROR_INVALID_PARAM;
  }

  for (;;) {
    uint8_t chunk[4096];
    size_t read = fread(chunk, 1, sizeof(chunk), pipe);
    if (read == 0)
      break;
    if (buffer_size + read > buffer_capacity) {
      size_t new_capacity = buffer_capacity == 0 ? 16384 : buffer_capacity;
      while (new_capacity < buffer_size + read)
        new_capacity *= 2;
      uint8_t *new_buffer = (uint8_t *)realloc(buffer, new_capacity);
      if (!new_buffer) {
        AE_PCLOSE(pipe);
        free(buffer);
        return AE_ERROR_OUT_OF_MEMORY;
      }
      buffer = new_buffer;
      buffer_capacity = new_capacity;
    }
    memcpy(buffer + buffer_size, chunk, read);
    buffer_size += read;
  }

  int status = AE_PCLOSE(pipe);
  if (buffer_size == 0) {
    free(buffer);
    if (status != 0)
      ae_audio_io_set_error(engine, "External decoder failed");
    else
      ae_audio_io_set_error(engine, "External decoder produced no audio data");
    return AE_ERROR_INVALID_PARAM;
  }

  size_t frame_size = sizeof(float) * 2;
  size_t frames = buffer_size / frame_size;
  if (frames == 0) {
    free(buffer);
    ae_audio_io_set_error(engine, "ffmpeg output size is invalid");
    return AE_ERROR_INVALID_PARAM;
  }

  out->buffer.samples = (float *)buffer;
  out->buffer.frame_count = frames;
  out->buffer.channels = 2;
  out->buffer.interleaved = true;
  out->sample_rate = AE_SAMPLE_RATE;
  for (size_t i = 0; i < frames * 2; ++i) {
    out->buffer.samples[i] = ae_clamp(out->buffer.samples[i], -1.0f, 1.0f);
  }
  return AE_OK;
}
#endif

AE_API ae_result_t ae_import_audio_file(ae_engine_t *engine, const char *path,
                                        ae_audio_data_t *out) {
  if (!path || !out)
    return AE_ERROR_INVALID_PARAM;
  if (engine)
    ae_clear_error(engine);
  memset(out, 0, sizeof(*out));

  FILE *file = fopen(path, "rb");
  if (!file) {
    ae_audio_io_set_error(engine, strerror(errno));
    return AE_ERROR_FILE_NOT_FOUND;
  }

  ae_decoded_audio_t decoded = {0};
  ae_result_t res = AE_ERROR_INVALID_PARAM;
  int is_wav = ae_audio_io_is_wav(file);
  if (is_wav) {
    res = ae_audio_io_decode_wav(file, &decoded, engine);
  }
  fclose(file);

  if (res == AE_OK) {
    res = ae_audio_io_convert_decoded(&decoded, out, engine);
    free(decoded.samples);
    return res;
  }
  if (decoded.samples)
    free(decoded.samples);

  return ae_audio_io_decode_ffmpeg(path, out, engine);
}

AE_API void ae_free_audio_data(ae_audio_data_t *data) {
  if (!data)
    return;
  free(data->buffer.samples);
  data->buffer.samples = NULL;
  data->buffer.frame_count = 0;
  data->buffer.channels = 0;
  data->buffer.interleaved = true;
  data->sample_rate = 0;
}
