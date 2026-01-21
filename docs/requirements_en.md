# Acoustic Engine Requirements Specification

> **Version**: 0.1.0-draft  
> **Updated**: 2026-01-20  
> **Status**: Initial Draft

---

## 1. Project Overview

### 1.1 Vision

**Acoustic Engine** is an audio processing library based on perceptual psychology. It solves the "parameter overload" problem of traditional DAWs (Audition, Pro Tools, etc.) by providing a simple **intent-based** API for high-quality audio processing.

```
Traditional approach:
  "Set reverb RT60 to 2.3 seconds, pre-delay to 45ms, damping to..."

Acoustic Engine approach:
  "Make it sound like deep underwater" or "Modulate reverb in sync with heartbeat"
```

### 1.2 Target Users

| User Type | Use Case |
|-----------|----------|
| Media Artists | Interactive installations, biosignal-linked artworks |
| VR/AR Designers | Spatial audio asset creation, real-time 3D audio |
| Sound Designers | High-speed batch processing for video/game assets |
| Researchers | Psychoacoustic experiments, acoustic simulation |

### 1.3 Key Features

1. **Perceptual Psychology-Based**: Processing guided by scientific metrics like IACC, loudness perception, and masking effects
2. **Physical Modeling**: Physically accurate acoustic propagation simulation using Francois-Garrison equation
3. **Intent-Based API**: Abstract complex processing with semantic expressions like "distance: 0.7"
4. **Biosignal Integration**: Real-time mapping of heart rate (HR/HRV) to audio parameters
5. **High Performance**: Low-latency processing with SIMD optimization (SSE/AVX)

---

## 2. Reference Scenarios

Acoustic Engine supports multiple acoustic scenarios. Each scenario is defined as a physically and perceptually accurate parameter set, and scenarios can be blended together.

### 2.1 Scenario List

#### 2.1.1 Natural Environment Scenarios

| Scenario | Physical Characteristics | Main Processing | Use Cases |
|----------|-------------------------|-----------------|-----------|
| 🌊 **deep_sea** | HF absorption, long reverb, pressure | Francois-Garrison, FDN (RT60: 5-10s) | Underwater scenes, meditation |
| 🌲 **forest** | Scattering, short decay | ER-focused, mild HF attenuation | Nature, ASMR |
| 🏔️ **cave** | Very long reverb, flutter echo | FDN (RT60: 3-8s), metallic ER | Horror, exploration |
| 🏜️ **open_field** | No reflections, distance only | Atmospheric absorption (ISO 9613), no reverb | Outdoors, isolation |
| 🌌 **space** | Silent / ultra-low freq only | Extreme HF attenuation, drone addition | Sci-fi, floating |

#### 2.1.2 Artificial Environment Scenarios

| Scenario | Physical Characteristics | Main Processing | Use Cases |
|----------|-------------------------|-----------------|-----------|
| 🏛️ **cathedral** | Ultra-long reverb (5-12s) | FDN high diffusion, low damping | Solemn, religious |
| 🏢 **office** | Short reverb, high absorption | Short ER, high damping | Modern, everyday |
| 🚇 **tunnel** | Metallic resonance, flutter | Short delay repetition, resonance | Industrial, ominous |
| 🏠 **small_room** | Intimate reverb (0.3-0.8s) | Close ER, moderate diffusion | Dialogue, interview |
| 📻 **radio** | Bandlimited, noise | 300-5000Hz BP, bit reduction | Retro, communication |
| 📞 **telephone** | Narrow band, distortion | 300-3400Hz BP, light distortion | Conversation, distance |

#### 2.1.3 Emotional/Artistic Expression Scenarios

| Scenario | Perceptual Effect | Main Processing | Use Cases |
|----------|------------------|-----------------|-----------|
| 😰 **tension** | Anxiety, oppression | HF emphasis, dissonant resonance, strong compression | Horror, suspense |
| 🌅 **nostalgia** | Warmth, longing | Lo-fi, wow/flutter, HF rolloff | Flashback, nostalgic |
| 💭 **dream** | Unreality, floating | Slow pitch modulation, long reverb, decorrelation | Dreams, memory |
| 💓 **intimate** | Closeness, ASMR-like | Proximity effect (LF boost), narrow stereo, dry-dominant | Whisper, intimacy |
| ⚡ **chaos** | Confusion, destruction | Buffer glitch, bit crush, async modulation | Glitch art |
| ✨ **ethereal** | Mystical, transcendent | Shimmer reverb, pitch-shifted layers, high diffusion | Spiritual |

---

### 2.2 Scenario Composer Architecture

Blend multiple scenarios in real-time to construct dynamic acoustic spaces.

```
┌─────────────────────────────────────────────────────────────────┐
│                    Scenario Composer Architecture               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Scenario Presets                       │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────┐   │   │
│  │  │ deep_sea│ │  cave   │ │ forest  │ │ cathedral   │   │   │
│  │  └────┬────┘ └────┬────┘ └────┬────┘ └──────┬──────┘   │   │
│  │       └───────────┴───────────┴─────────────┘           │   │
│  │                         │                                │   │
│  │                         ▼                                │   │
│  │              ┌───────────────────┐                       │   │
│  │              │ Scenario Blender  │ ◀── Blend ratios      │   │
│  │              │ deep_sea: 0.6     │                       │   │
│  │              │ cave: 0.3         │                       │   │
│  │              │ tension: 0.4      │ ◀── Emotion layer     │   │
│  │              └─────────┬─────────┘                       │   │
│  │                        │                                 │   │
│  │                        ▼                                 │   │
│  │              ┌───────────────────┐                       │   │
│  │              │ Parameter Resolver│ ◀── Parameter merge   │   │
│  │              │ RT60, absorption, │                       │   │
│  │              │ EQ, width, etc.   │                       │   │
│  │              └─────────┬─────────┘                       │   │
│  └────────────────────────┼─────────────────────────────────┘   │
│                           ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Processing Chain                       │   │
│  │  Propagation → Spatial → Reverb → Dynamics → EQ → Out   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Biosignal Input                        │   │
│  │  Heart Rate ──▶ Dynamically modulate blend ratios        │   │
│  │  HRV ──────────▶ chaos/tension layer intensity           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 Scenario API

```c
// Apply single scenario
ae_result_t ae_apply_scenario(ae_engine_t* engine,
                              const char* scenario_name,
                              float intensity);  // 0.0 - 1.0

// Blend multiple scenarios
ae_result_t ae_blend_scenarios(ae_engine_t* engine,
                               const ae_scenario_blend_t* blends,
                               size_t count);

// Blend definition
typedef struct {
    const char* name;       // Scenario name
    float       weight;     // Blend weight (0.0 - 1.0)
} ae_scenario_blend_t;

// Usage example
ae_scenario_blend_t blends[] = {
    { "deep_sea", 0.6f },
    { "cave", 0.3f },
    { "tension", 0.4f }
};
ae_blend_scenarios(engine, blends, 3);
```

---

### 2.4 Reference Implementation: Deep Sea Scenario (deep_sea)

As a concrete design guideline, we define the processing chain for creating "deep sea sound" in detail.

#### 2.4.1 Processing Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    Deep Sea Processing Chain                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [Input: Mono] ──▶ ┌─────────────────────────────────────────┐  │
│                    │ 1. HRTF Spatializer                     │  │
│                    │    - Direction interpolation + crossfade│  │
│                    │    - Horizontal sweep motion            │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 2. Physical Absorption                  │  │
│                    │    - Francois-Garrison equation         │  │
│                    │    - Distance-dependent HF attenuation  │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 3. Reverb (FDN)                         │  │
│                    │    - Early Reflections (~tens of ms)    │  │
│                    │    - Late Reverb (RT60: ~8 sec)         │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 4. M/S Width Control                    │  │
│                    │    - Dry signal: Mid-focused (narrow)   │  │
│                    │    - Reverb: Side-enhanced (wide)       │  │
│                    │    - Envelopment via IACC reduction     │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 5. Sidechain Ducking                    │  │
│                    │    - Suppress reverb when dry is active │  │
│                    │    - Multiband support                  │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                    ┌─────────────────────────────────────────┐  │
│                    │ 6. Mastering Chain                      │  │
│                    │    - De-esser (sibilance control)       │  │
│                    │    - Tilt EQ (HF gradient)              │  │
│                    │    - Lookahead Limiter                  │  │
│                    └──────────────┬──────────────────────────┘  │
│                                   ▼                             │
│                             [Output: Stereo]                    │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.4.2 deep_sea Scenario Parameters

| Module | Parameter | Value | Rationale |
|--------|-----------|-------|-----------|
| Propagation | absorption_model | francois_garrison | Seawater physics |
| | distance | 50m | Moderate distance perception |
| | temperature | 5°C | Deep sea cold |
| | depth | 200m | Deep sea assumption |
| Reverb | rt60 | 8.0s | Large underwater space |
| | pre_delay | 80ms | Distance to reflections |
| | damping | 0.7 | Rapid HF decay |
| | diffusion | 0.85 | High diffusion |
| M/S | dry_width | 0.3 | Narrow dry signal |
| | wet_width | 1.8 | Wide reverb |
| EQ | tilt | -3dB | Dark tone |

---

### 2.5 Scenario Detailed Definitions

#### 2.5.1 forest

```yaml
forest:
  description: "Forest environment surrounded by trees"
  propagation:
    model: iso_9613_simplified
    humidity: 70%
    absorption_coefficient: 0.15  # Leaf scattering
  reverb:
    rt60: 0.8s
    pre_delay: 15ms
    early_reflections:
      density: high
      pattern: scattered  # Irregular reflections
    damping: 0.5
  eq:
    high_shelf: -2dB @ 8kHz  # Leaf HF absorption
    low_shelf: +1dB @ 200Hz  # Ground reflection
  spatial:
    width: 1.2
    decorrelation: 0.4
```

#### 2.5.2 cave

```yaml
cave:
  description: "Limestone cave with long flutter echo"
  propagation:
    model: geometric
    reflection_coefficient: 0.85  # Hard walls
  reverb:
    rt60: 4.0s
    pre_delay: 30ms
    early_reflections:
      density: medium
      pattern: flutter  # Parallel wall flutter
      flutter_rate: 15Hz
    damping: 0.3  # Low damping (bright reverb)
    modulation: 0.2  # Reduce metallic character
  eq:
    resonance: [250Hz, 500Hz, 1kHz]  # Cave resonances
  spatial:
    width: 1.5
```

#### 2.5.3 cathedral

```yaml
cathedral:
  description: "Cathedral with high ceilings and stone walls"
  reverb:
    rt60: 6.0s
    pre_delay: 50ms
    early_reflections:
      density: medium
      pattern: geometric  # Regular reflections
    damping: 0.4
    diffusion: 0.9  # Very high diffusion
  eq:
    tilt: -1dB  # Slightly dark
    low_cut: 40Hz  # Ultra-low cut
  spatial:
    width: 1.6
    elevation_spread: 0.5  # Upward spread
```

#### 2.5.4 tension

```yaml
tension:
  description: "Audio processing that induces anxiety and tension"
  eq:
    high_shelf: +3dB @ 4kHz  # HF emphasis
    peak: +4dB @ 2.5kHz, Q=3  # Uncomfortable band boost
  dynamics:
    compression_ratio: 4:1
    attack: 5ms  # Fast attack
    release: 50ms
  modulation:
    pitch_wobble: 0.5%  # Subtle pitch drift
    rate: 0.3Hz  # Very slow
  reverb:
    rt60: 1.2s
    damping: 0.2  # Bright, sharp reverb
  spatial:
    width: 0.6  # Narrow, claustrophobic
```

#### 2.5.5 nostalgia

```yaml
nostalgia:
  description: "Warm, nostalgic audio processing"
  eq:
    high_cut: 12kHz  # HF rolloff
    low_shelf: +2dB @ 300Hz  # Warmth
    tilt: -2dB
  lofi:
    bit_depth: 12  # Bit reduction
    sample_rate_reduction: 0.9  # Mild aliasing
    wow_flutter: 0.3%  # Tape-like wobble
    noise: -40dB  # Hiss noise
  saturation:
    type: tape
    amount: 0.3
  reverb:
    rt60: 1.5s
    damping: 0.6
```

---

### 2.6 Required Modules from This Architecture

1. **HRTF Spatializer** - 3D positioning + interpolation
2. **Physical Propagation** - Multiple absorption models (seawater, atmosphere, geometric)
3. **FDN Reverb** - Variable RT60, flutter echo support
4. **Early Reflections** - Selectable patterns (scattered, flutter, geometric)
5. **M/S Processor** - Spatial width control
6. **Multiband Compressor** - Sidechain support
7. **Parametric EQ** - Resonance/peak support
8. **Lo-Fi Processor** - Bit reduction, wow/flutter
9. **Saturation** - Tape/tube-style
10. **De-esser, Tilt EQ, Limiter** - Mastering
11. **Scenario Blender** - Dynamic scenario blending

---

## 3. Functional Requirements

### 3.1 Physical Propagation Module

#### 3.1.1 Frequency-Dependent Absorption

Simulate sound wave absorption in seawater based on the **Francois-Garrison equation**.

```
α(f) = A₁·f₁·f² / (f₁² + f²) + A₂·f₂·f² / (f₂² + f²) + A₃·f²

where:
  α: Absorption coefficient (dB/km)
  f: Frequency (kHz)
  f₁, f₂: Relaxation frequencies (temperature/pressure dependent)
  A₁, A₂, A₃: Constants (salinity/temperature/pressure dependent)
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `distance` | Distance from source (m) | 10.0 |
| `temperature` | Water temperature (°C) | 10.0 |
| `salinity` | Salinity (ppt) | 35.0 |
| `depth` | Water depth (m) | 100.0 |

#### 3.1.2 Distance Attenuation

```
L(r) = L₀ - 20·log₁₀(r/r₀) - α·(r - r₀)

where:
  L: Sound pressure level (dB)
  r: Distance (m)
  r₀: Reference distance (typically 1m)
  α: Absorption coefficient
```

#### 3.1.3 Atmospheric Propagation (ISO 9613 Simplified)

Atmospheric absorption for outdoor environments (simplified implementation):

```
α_air(f) ≈ 0.01 × (f / 1000)^1.5 × (1 + 0.01 × (20 - T))

where:
  α_air: Absorption coefficient (dB/m)
  f: Frequency (Hz)
  T: Temperature (°C)

Practical approximation:
  1kHz @ 20°C, 50%RH → ~0.005 dB/m
  8kHz @ 20°C, 50%RH → ~0.04 dB/m
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `distance` | Distance from source (m) | 10.0 |
| `temperature` | Air temperature (°C) | 20.0 |
| `humidity` | Relative humidity (%) | 50.0 |

---

#### 3.1.4 Cave Acoustics Model

Physics-based simulation of acoustic phenomena specific to caves and tunnels.

##### 3.1.4.1 Modal Resonance

Low-frequency resonance based on cave geometry:

```
f_n = n × c / (2 × L)

where:
  f_n: nth mode resonance frequency (Hz)
  c: Speed of sound (343 m/s @ 20°C)
  L: Primary cave dimension (m)
  n: Mode number (1, 2, 3...)

Example: 15m wide cave
  f₁ = 343 / (2 × 15) = 11.4 Hz
  f₂ = 22.9 Hz
  f₃ = 34.3 Hz
```

**Implementation**: Add EQ peaks (+3-6dB, Q=2-4) at calculated resonance frequencies

| Parameter | Description | Default |
|-----------|-------------|---------|
| `cave_dimension` | Primary dimension (m) | 15.0 |
| `resonance_boost_db` | Resonance boost amount (dB) | 4.0 |
| `resonance_modes` | Number of modes to generate | 3 |

##### 3.1.4.2 Flutter Echo

Repeated reflections from parallel walls:

```
T_flutter = 2 × d / c
f_flutter = c / (2 × d)

where:
  T_flutter: Flutter period (s)
  f_flutter: Flutter frequency (Hz)
  d: Wall distance (m)
  c: Speed of sound (343 m/s)

Example: 8m passage
  T_flutter = 2 × 8 / 343 = 46.6 ms
  f_flutter = 343 / (2 × 8) = 21.4 Hz (repetition rate)
```

**Implementation**: Multi-tap delay (repeating with decay at 46.6ms intervals)

| Parameter | Description | Default |
|-----------|-------------|---------|
| `wall_distance` | Distance between walls (m) | 8.0 |
| `flutter_repeats` | Number of repetitions | 6 |
| `flutter_decay` | Decay per repetition (0-1) | 0.7 |

##### 3.1.4.3 Eyring Reverberation Time

RT60 calculation suited for low-absorption spaces (caves):

```
RT60 = 0.161 × V / (-S × ln(1 - ᾱ))

where:
  V: Volume (m³)
  S: Surface area (m²)
  ᾱ: Average absorption coefficient

Comparison with Sabine (important for high-reflection spaces):
  Sabine: RT60 = 0.161 × V / (S × ᾱ)
  → Overestimates when absorption is low (ᾱ < 0.2)
```

**Usage**: Used for room_size to RT60 conversion

##### 3.1.4.4 Rock Wall Frequency-Dependent Absorption

Absorption characteristics of limestone, granite, etc.:

```
α(f) = α_low + (α_high - α_low) × (log₁₀(f / f_low) / log₁₀(f_high / f_low))

Typical rock wall (limestone):
  125 Hz: α = 0.02
  500 Hz: α = 0.04
  2kHz:   α = 0.06
  8kHz:   α = 0.10

→ Low frequencies are almost fully reflected, high frequencies slightly absorbed
→ This causes the bright, long reverb characteristic of caves
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `wall_material` | Material ("limestone", "granite", "concrete") | "limestone" |
| `alpha_low` | Low frequency absorption | 0.02 |
| `alpha_high` | High frequency absorption | 0.10 |

##### 3.1.4.5 Cave Model Integration

```c
// Cave acoustics parameters
typedef struct {
    float cave_dimension_m;      // Primary dimension
    float wall_distance_m;       // Wall distance (for flutter)
    uint8_t flutter_repeats;     // Flutter repetition count
    float flutter_decay;         // Flutter decay
    float alpha_low;             // Low frequency absorption
    float alpha_high;            // High frequency absorption
} ae_cave_params_t;

// Apply cave model
ae_result_t ae_apply_cave_model(
    ae_engine_t* engine,
    const ae_cave_params_t* params
);
```

---

### 3.2 Spatial Audio Module

#### 3.2.1 HRTF Spatializer

| Feature | Details |
|---------|---------|
| Supported format | SOFA (Spatially Oriented Format for Acoustics) |
| Default HRTF | MIT KEMAR (anechoic) |
| Direction interpolation | Spherical linear interpolation (SLERP) |
| Switching | Crossfade (10-50ms, configurable) |
| Convolution | Partitioned convolution (low latency) |

#### 3.2.2 M/S Processor

```
Mid   = (L + R) / 2
Side  = (L - R) / 2

L_out = Mid + Side·width
R_out = Mid - Side·width
```

| Parameter | Range | Description |
|-----------|-------|-------------|
| `width` | 0.0 - 2.0 | 0=mono, 1=stereo, 2=super-wide |
| `mid_gain` | -∞ to +12dB | Mid component gain |
| `side_gain` | -∞ to +12dB | Side component gain |

#### 3.2.3 Decorrelation

Reduce IACC (Interaural Cross-Correlation) to increase spatial envelopment.

- Phase dispersion via all-pass filters
- Micro delay differences
- Stereo widening without pitch shift

---

### 3.3 Reverb Module

#### 3.3.1 FDN (Feedback Delay Network)

| Parameter | Range | Description |
|-----------|-------|-------------|
| `rt60` | 0.1 - 30.0 sec | Reverberation time |
| `pre_delay` | 0 - 500 ms | Delay before early reflections |
| `damping` | 0.0 - 1.0 | High-frequency decay rate |
| `size` | 0.0 - 1.0 | Room size perception |
| `diffusion` | 0.0 - 1.0 | Diffusion amount |
| `modulation` | 0.0 - 1.0 | Internal modulation (reduces metallic character) |

**FDN Configuration**:
- 8 channels (standard) / 16 channels (high quality)
- Hadamard matrix feedback
- Per-band RT60 for frequency-dependent decay

#### 3.3.2 Early Reflections

- Up to 16 tap multi-tap delay
- Room shape presets
- Ray-traced custom settings (future)

---

### 3.4 Dynamics Module

#### 3.4.1 Compressor

| Parameter | Range | Description |
|-----------|-------|-------------|
| `threshold` | -60 to 0 dB | Threshold level |
| `ratio` | 1:1 - 20:1 | Compression ratio |
| `attack` | 0.1 - 100 ms | Attack time |
| `release` | 10 - 1000 ms | Release time |
| `knee` | 0 - 20 dB | Soft knee width |
| `makeup` | 0 - 24 dB | Makeup gain |

**Additional Features**:
- Sidechain input
- External/internal key switching
- Multiband (3-5 bands)

#### 3.4.2 Lookahead Limiter

- Lookahead: 1-10ms (configurable)
- True peak detection
- Soft clip / hard clip selection

#### 3.4.3 De-esser

- Frequency range: 4-10kHz (auto-detect or manual)
- Wideband / split-band modes

---

### 3.5 Tone/EQ Module

#### 3.5.1 Tilt EQ

```
      ↑ Gain
      │      ___________
      │     /
      │    /
------│---●-------------- Pivot (1kHz default)
      │  /
      │ /
      │/___________
      └─────────────────▶ Frequency
```

| Parameter | Range | Description |
|-----------|-------|-------------|
| `tilt` | -6 to +6 dB | Tilt amount |
| `pivot` | 200 - 4000 Hz | Pivot frequency |

#### 3.5.2 Parametric EQ

- Up to 8 bands
- Peak / Low Shelf / High Shelf / Notch
- Q: 0.1 - 30

---

### 3.6 Biosignal Mapping

#### 3.6.1 Supported Signals

| Signal | Unit | Typical Range |
|--------|------|---------------|
| Heart Rate (HR) | BPM | 40 - 200 |
| Heart Rate Variability (HRV) | ms (RMSSD) | 10 - 100 |

#### 3.6.2 Mapping Definition

```c
typedef struct {
    ae_biosignal_type_t input;      // Input signal type
    ae_param_target_t   target;     // Target parameter
    ae_curve_type_t     curve;      // Mapping curve
    float               in_min;     // Input range minimum
    float               in_max;     // Input range maximum  
    float               out_min;    // Output range minimum
    float               out_max;    // Output range maximum
    float               smoothing;  // Smoothing time constant (ms)
} ae_mapping_t;
```

#### 3.6.3 Mapping Curves

| Curve | Use Case |
|-------|----------|
| `LINEAR` | Proportional relationship |
| `EXPONENTIAL` | Perceptual (frequency, gain) |
| `LOGARITHMIC` | Compressed relationship |
| `SIGMOID` | Soft transition with saturation |
| `STEPPED` | Discrete changes |
| `CUSTOM` | User-defined lookup table |

---

### 3.7 Semantic Expressions

#### 3.7.1 Expression Grammar

```
expression := param_name ":" value ("," param_name ":" value)*
value      := float (0.0 - 1.0)
```

#### 3.7.2 Supported Expressions

| Expression | Internal Mapping |
|------------|------------------|
| `distance` | Dry/Wet ratio, HPF, absorption |
| `depth` | Francois-Garrison depth, reverb |
| `room_size` | RT60, ER density |
| `tension` | HPF, light distortion, compression |
| `warmth` | Low shelf, saturation |
| `intimacy` | Proximity effect, dry-dominant |
| `chaos` | Modulation depth, async delays |
| `underwater` | Francois-Garrison + LPF + modulation |

---

### 3.8 Unified Parameter Design

A design that minimizes user-facing parameters while performing complex processing internally.

#### 3.8.1 Design Philosophy

```
User Operation: 10 simple parameters
      ↓
Internal Expansion: Auto-converted to 35+ detailed parameters
      ↓
Processing: Each module uses detailed parameters
```

#### 3.8.2 Tier 1: Main Parameters (6)

Core parameters always displayed, used for basic sound design.

| Parameter | Range | Description | Internal Mapping Example |
|-----------|-------|-------------|-------------------------|
| **distance** | 0.1 - 1000 m | Distance from source | Propagation absorption, dry/wet ratio, EQ |
| **room_size** | 0.0 - 1.0 | Perceived room size | RT60, ER density, pre-delay |
| **brightness** | -1.0 - 1.0 | Tone brightness (neg=dark) | Tilt EQ, damping, high-cut |
| **width** | 0.0 - 2.0 | Spatial spread | M/S balance, decorrelation |
| **dry_wet** | 0.0 - 1.0 | Dry/effect ratio | Wet amount for each effect |
| **intensity** | 0.0 - 1.0 | Scenario effect strength | Scaling of all parameters |

#### 3.8.3 Tier 2: Extended Parameters (4)

Optional parameters for detailed adjustments when needed.

| Parameter | Range | Description | Internal Mapping Example |
|-----------|-------|-------------|-------------------------|
| **decay_time** | 0.1 - 30.0 s | Reverb time (direct) | RT60 (overrides room_size) |
| **diffusion** | 0.0 - 1.0 | Reverb diffusion | FDN diffusion coefficient |
| **lofi_amount** | 0.0 - 1.0 | Lo-fi effect amount | Bit depth, noise, flutter |
| **modulation** | 0.0 - 1.0 | Modulation strength | Pitch wobble, chorus depth |

#### 3.8.4 C API

```c
// Main parameters structure
typedef struct {
    float distance;      // 0.1 - 1000 m
    float room_size;     // 0.0 - 1.0
    float brightness;    // -1.0 - 1.0
    float width;         // 0.0 - 2.0
    float dry_wet;       // 0.0 - 1.0
    float intensity;     // 0.0 - 1.0
} ae_main_params_t;

// Extended parameters structure
typedef struct {
    float decay_time;    // 0.1 - 30.0 s (0 = auto from room_size)
    float diffusion;     // 0.0 - 1.0
    float lofi_amount;   // 0.0 - 1.0
    float modulation;    // 0.0 - 1.0
} ae_extended_params_t;

// Get default parameters from scenario
ae_result_t ae_get_scenario_defaults(
    const char* scenario_name,
    ae_main_params_t* main_out,
    ae_extended_params_t* extended_out  // Can be NULL
);

// Apply parameters
ae_result_t ae_set_main_params(
    ae_engine_t* engine,
    const ae_main_params_t* params
);

ae_result_t ae_set_extended_params(
    ae_engine_t* engine,
    const ae_extended_params_t* params
);

// Individual parameter setters (convenience functions)
ae_result_t ae_set_distance(ae_engine_t* engine, float value);
ae_result_t ae_set_room_size(ae_engine_t* engine, float value);
ae_result_t ae_set_brightness(ae_engine_t* engine, float value);
ae_result_t ae_set_width(ae_engine_t* engine, float value);
ae_result_t ae_set_dry_wet(ae_engine_t* engine, float value);
ae_result_t ae_set_intensity(ae_engine_t* engine, float value);
```

#### 3.8.5 Default Values by Scenario

| Scenario | distance | room_size | brightness | width | dry_wet | intensity |
|----------|----------|-----------|------------|-------|---------|-----------|
| deep_sea | 50.0 | 0.85 | -0.50 | 1.60 | 0.70 | 1.00 |
| cave | 20.0 | 0.70 | 0.00 | 1.50 | 0.65 | 1.00 |
| forest | 15.0 | 0.30 | -0.20 | 1.20 | 0.40 | 1.00 |
| cathedral | 30.0 | 0.90 | -0.10 | 1.60 | 0.75 | 1.00 |
| tension | 5.0 | 0.35 | 0.40 | 0.60 | 0.50 | 1.00 |
| nostalgia | 8.0 | 0.45 | -0.30 | 1.00 | 0.55 | 1.00 |
| intimate | 1.0 | 0.15 | 0.10 | 0.40 | 0.25 | 1.00 |

#### 3.8.6 Internal Parameter Conversion Examples

For `brightness = -0.5`:

```c
// Internally expanded as:
internal.tilt_db = -3.0f;           // Tilt EQ
internal.damping = 0.70f;           // Reverb damping
internal.high_cut_hz = 8000.0f;     // High-cut filter
internal.high_shelf_db = -2.0f;     // High shelf
```

For `room_size = 0.85`:

```c
// Internally expanded as:
internal.rt60 = 8.0f;               // Reverb time
internal.pre_delay_ms = 80.0f;      // Pre-delay
internal.er_density = 0.7f;         // ER density
internal.er_pattern = "scattered";   // ER pattern
```

---

### 4.1 Performance

| Item | Requirement |
|------|-------------|
| **Latency** | ≤50ms (real-time processing) |
| **Buffer size** | 64 - 4096 samples supported |
| **Sample rate** | 44.1kHz, 48kHz, 96kHz supported |
| **SIMD** | SSE2 (required), AVX2 (recommended), ARM NEON (future) |
| **Batch processing** | 100 parallel file processing supported |

### 4.2 Platforms

| Platform | Priority | Format |
|----------|----------|--------|
| Windows x64 | **Phase 1** | DLL |
| macOS (Intel/ARM) | Phase 2 | dylib |
| Linux x64 | Phase 2 | so |
| WebAssembly | Phase 3 | WASM |

### 4.3 Language Bindings

| Language | Priority | Use Case |
|----------|----------|----------|
| C | Required | Native API |
| Python (ctypes) | **Phase 1** | GUI, prototyping |
| C# (P/Invoke) | Phase 2 | Unity integration |
| JavaScript | Phase 3 | WebAudio, TouchDesigner |

---

## 5. Architecture

### 5.1 Component Structure

```
acoustic_engine/
├── include/
│   └── acoustic_engine/
│       ├── ae_core.h           # Basic types, buffers
│       ├── ae_propagation.h    # Physical propagation
│       ├── ae_spatial.h        # Spatial audio
│       ├── ae_reverb.h         # Reverb
│       ├── ae_dynamics.h       # Dynamics
│       ├── ae_eq.h             # EQ
│       ├── ae_biosignal.h      # Biosignal
│       ├── ae_expression.h     # Semantic
│       └── acoustic_engine.h   # Unified header
│
├── src/
│   ├── core/                   # SIMD, memory, FFT
│   ├── propagation/            # Francois-Garrison, etc.
│   ├── spatial/                # HRTF, M/S
│   ├── reverb/                 # FDN, ER
│   ├── dynamics/               # Comp, Limiter
│   ├── eq/                     # Tilt, Parametric
│   ├── biosignal/              # Mapping
│   └── expression/             # Parser, presets
│
├── data/
│   └── hrtf/                   # HRTF data (SOFA)
│
├── bindings/
│   ├── python/
│   └── csharp/
│
├── docs/
│   ├── requirements_ja.md      # Japanese version
│   └── requirements_en.md      # This document
│
└── tests/
```

### 5.2 C API Design Principles

```c
// 1. Engine creation/destruction
ae_engine_t* ae_create_engine(const ae_config_t* config);
void         ae_destroy_engine(ae_engine_t* engine);

// 2. Module access
ae_hrtf_t*      ae_get_hrtf(ae_engine_t* engine);
ae_reverb_t*    ae_get_reverb(ae_engine_t* engine);
ae_dynamics_t*  ae_get_dynamics(ae_engine_t* engine);

// 3. Processing
ae_result_t ae_process(ae_engine_t* engine, 
                       float* input, 
                       float* output, 
                       size_t frames);

// 4. Semantic expressions
ae_result_t ae_apply_expression(ae_engine_t* engine,
                                const char* expression);

// 5. Biosignal update
ae_result_t ae_update_biosignal(ae_engine_t* engine,
                                ae_biosignal_type_t type,
                                float value);
```

---

## 6. External Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| **pffft** | FFT/IFFT | BSD-like |
| **SOFA (libmysofa)** | HRTF loading | BSD-3-Clause |

---

## 7. Development Plan

### Phase 1: Core + Deep Sea (8 weeks)

- [ ] DSP core (SIMD, FFT, filters, delay)
- [ ] Francois-Garrison absorption
- [ ] HRTF (SOFA loading, convolution, interpolation)
- [ ] FDN reverb
- [ ] M/S processing
- [ ] Python bindings

### Phase 2: Semantic + Biosignal (4 weeks)

- [ ] Expression parser
- [ ] Preset system
- [ ] Heartbeat mapping
- [ ] Sidechain compressor

### Phase 3: Mastering + Polish (4 weeks)

- [ ] Tilt EQ, de-esser
- [ ] Lookahead limiter
- [ ] C# bindings
- [ ] Documentation

### Phase 4: Expansion (Future)

- [ ] WebAssembly support
- [ ] Convolution reverb (IR)
- [ ] Atmospheric propagation (ISO 9613)
- [ ] Personalized HRTF

---

## 8. Glossary

| Term | Description |
|------|-------------|
| **HRTF** | Head-Related Transfer Function |
| **HRIR** | Head-Related Impulse Response - time domain representation of HRTF |
| **IACC** | Interaural Cross-Correlation - metric for spatial envelopment |
| **FDN** | Feedback Delay Network - reverberation synthesis algorithm |
| **M/S** | Mid/Side - stereo signal processing method |
| **SOFA** | Spatially Oriented Format for Acoustics - HRTF standard format |
| **RT60** | Reverberation time - time for 60dB decay |

---

## Appendix A: Francois-Garrison Equation Details

Sound wave absorption coefficient in seawater is calculated as:

```
α(f) = (A₁·P₁·f₁·f²)/(f₁² + f²) + (A₂·P₂·f₂·f²)/(f₂² + f²) + A₃·P₃·f²

where:
  f   : Frequency [kHz]
  A₁  : Boric acid contribution coefficient
  A₂  : Magnesium sulfate contribution coefficient
  A₃  : Pure water contribution coefficient
  f₁  : Boric acid relaxation frequency [kHz]
  f₂  : Magnesium sulfate relaxation frequency [kHz]
  P₁,P₂,P₃ : Pressure correction factors

Implementation derives each coefficient from temperature T[°C], 
salinity S[ppt], and depth D[m].
```

---

## Appendix B: References

1. Francois, R.E. & Garrison, G.R. (1982). "Sound absorption based on ocean measurements". *J. Acoust. Soc. Am.*, 72(6), 1879-1890.
2. Algazi, V.R. et al. (2001). "The CIPIC HRTF Database". *IEEE ASSP Workshop*.
3. Jot, J.M. (1992). "An analysis/synthesis approach to real-time artificial reverberation". *ICASSP*.
4. Farina, A. (2000). "Simultaneous measurement of impulse response and distortion with a swept-sine technique". *AES Convention*.
