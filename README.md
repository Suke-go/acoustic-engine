# Acoustic Engine

**Perception-based audio processing library with intent-driven API**

[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-orange.svg)](https://en.cppreference.com/w/c/11)

## Overview

Acoustic Engine is an audio processing library designed around perceptual psychoacoustics. Instead of requiring users to manually tweak dozens of parameters like RT60, pre-delay, and damping, it provides a **semantic, intent-based API** for achieving desired acoustic effects.

```c
// Traditional approach:
//   "Set RT60 to 2.3s, pre-delay to 45ms, damping to 0.7..."

// Acoustic Engine approach:
ae_apply_scenario(engine, "deep_sea", 0.8f);  // "Make it sound like deep underwater"
ae_set_distance(engine, 50.0f);               // Simple perceptual controls
```

## Features

- **Perception-based Processing**: Built on IACC, loudness perception, and masking effect models
- **Physical Modeling**: Francois-Garrison seawater absorption, ISO 9613-1 atmospheric absorption
- **Intent-based API**: Semantic expressions like `distance: 0.7` abstract complex DSP chains
- **Biosignal Integration**: Real-time mapping of HR/HRV to acoustic parameters
- **SIMD Optimized**: SSE2/AVX2 acceleration for low-latency processing
- **Scenario System**: Pre-defined acoustic environments (deep_sea, cave, cathedral, etc.)

## Requirements

- **CMake** 3.16 or later
- **C11** compatible compiler (MSVC 2019+, GCC 7+, Clang 5+)

### Optional Dependencies

- **libmysofa**: SOFA HRTF support (auto-fetched via CMake)

## Build

### Windows (MSVC)

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Linux / macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `AE_USE_LIBMYSOFA` | ON | Enable SOFA HRTF support |
| `CMAKE_BUILD_TYPE` | Release | Build configuration |

## Quick Start

```c
#include <acoustic_engine.h>

int main() {
    // 1. Create engine with default config
    ae_config_t config = ae_get_default_config();
    ae_engine_t* engine = ae_create_engine(&config);
    
    // 2. Apply a scenario
    ae_apply_scenario(engine, "deep_sea", 0.8f);
    
    // 3. Process audio
    ae_audio_buffer_t input = { .samples = my_samples, .frame_count = 1024, .channels = 2 };
    ae_audio_buffer_t output = { .samples = out_samples, .frame_count = 1024, .channels = 2 };
    ae_process(engine, &input, &output);
    
    // 4. Cleanup
    ae_destroy_engine(engine);
    return 0;
}
```

## Available Scenarios

### Natural Environments
| Scenario | Description | Use Cases |
|----------|-------------|-----------|
| `deep_sea` | High-frequency absorption, long reverb | Underwater scenes, meditation |
| `forest` | Scattered reflections, short decay | Nature ambience, ASMR |
| `cave` | Long reverb, flutter echo | Horror, exploration |
| `open_field` | Distance attenuation only | Outdoor, isolation |

### Artificial Environments
| Scenario | Description | Use Cases |
|----------|-------------|-----------|
| `cathedral` | Very long reverb (5-12s) | Sacred, grand |
| `small_room` | Intimate reverb (0.3-0.8s) | Dialog, interview |
| `tunnel` | Metallic resonance | Industrial, eerie |
| `radio` | Band-limited, noise | Retro, communication |

### Emotional/Artistic
| Scenario | Description | Use Cases |
|----------|-------------|-----------|
| `tension` | Heightened anxiety | Horror, suspense |
| `nostalgia` | Warm, lo-fi | Flashback, memory |
| `dream` | Surreal, floating | Dreams, subconscious |
| `ethereal` | Mystical, transcendent | Spiritual, ambient |

## Running Tests

```bash
cd build
ctest --output-on-failure
```

Or use the convenience script (Windows):

```powershell
.\run_tests.ps1
```

## Project Structure

```
acoustic_engine/
├── include/
│   └── acoustic_engine.h    # Public API header
├── src/
│   ├── acoustic_engine.c    # Core engine implementation
│   ├── ae_reverb.c          # FDN reverb module
│   ├── ae_spatial.c         # HRTF & spatial processing
│   ├── ae_propagation.c     # Physical propagation models
│   ├── ae_dynamics.c        # Compressor, limiter, de-esser
│   ├── ae_analysis.c        # Perceptual metrics analysis
│   ├── ae_dsp.c             # DSP primitives
│   ├── ae_simd.c            # SIMD optimizations
│   ├── ae_slm.c             # Natural language interface
│   └── ...
├── tests/
│   ├── test_math.c          # Math utility tests
│   ├── test_dsp.c           # DSP primitive tests
│   └── test_analysis.c      # Analysis function tests
├── docs/
│   ├── requirements_ja.md   # Requirements (Japanese)
│   └── requirements_en.md   # Requirements (English)
├── CMakeLists.txt
└── README.md
```

## API Reference

See [`include/acoustic_engine.h`](include/acoustic_engine.h) for the complete API documentation.

### Core Functions

| Function | Description |
|----------|-------------|
| `ae_create_engine()` | Create engine instance |
| `ae_destroy_engine()` | Destroy engine instance |
| `ae_process()` | Process audio buffer |
| `ae_apply_scenario()` | Apply acoustic scenario |
| `ae_blend_scenarios()` | Blend multiple scenarios |

### Parameter Control

| Function | Description |
|----------|-------------|
| `ae_set_distance()` | Set perceived distance |
| `ae_set_room_size()` | Set room size |
| `ae_set_brightness()` | Set tonal brightness |
| `ae_set_width()` | Set stereo width |

### Analysis

| Function | Description |
|----------|-------------|
| `ae_compute_perceptual_metrics()` | Analyze IACC, DRR, loudness |
| `ae_compute_room_metrics()` | Compute ISO 3382 room metrics |
| `ae_analyze_spectrum()` | Spectral feature extraction |
| `ae_analyze_loudness()` | EBU R128 loudness analysis |

## Documentation

- [Requirements Specification (日本語)](docs/requirements_ja.md)
- [Requirements Specification (English)](docs/requirements_en.md)
- [Development Progress](DEVELOPMENT.md)

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Francois-Garrison seawater absorption model
- ISO 9613 atmospheric absorption standard
- ISO 3382 room acoustic parameters
- libmysofa for SOFA HRTF support
