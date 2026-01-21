"""
Acoustic Engine Python Wrapper

Clean Python bindings for acoustic_engine.dll

Usage:
    from bindings.ae_wrapper import AcousticEngine, Scenario
    
    engine = AcousticEngine()
    engine.apply_scenario(Scenario.DEEP_SEA, intensity=0.8)
    engine.set_perceived_distance(0.7)
"""

import ctypes
import os
from ctypes import (
    c_float, c_int, c_uint32, c_uint8, c_size_t, c_char_p, c_void_p, c_bool,
    Structure, POINTER, byref
)
from enum import Enum
from dataclasses import dataclass
from typing import Optional, Tuple

# ============================================================================
# Find and load DLL
# ============================================================================

def _find_dll():
    """Find acoustic_engine.dll in common locations"""
    search_paths = [
        os.path.join(os.path.dirname(__file__), "..", "build", "Release", "acoustic_engine.dll"),
        os.path.join(os.path.dirname(__file__), "..", "build", "Debug", "acoustic_engine.dll"),
        os.path.join(os.path.dirname(__file__), "acoustic_engine.dll"),
        "acoustic_engine.dll",
    ]
    
    for path in search_paths:
        abs_path = os.path.abspath(path)
        if os.path.exists(abs_path):
            return abs_path
    
    raise FileNotFoundError(
        "acoustic_engine.dll not found. Build the project first:\n"
        "  cmake --build build --config Release"
    )

_dll = ctypes.CDLL(_find_dll())

# ============================================================================
# C Structure Definitions
# ============================================================================

class _ae_config_t(Structure):
    _fields_ = [
        ("sample_rate", c_uint32),
        ("max_buffer_size", c_uint32),
        ("data_path", c_char_p),
        ("hrtf_path", c_char_p),
        ("preload_hrtf", c_bool),
        ("preload_all_presets", c_bool),
        ("max_reverb_time_sec", c_size_t),
    ]

class _ae_main_params_t(Structure):
    _fields_ = [
        ("distance", c_float),
        ("room_size", c_float),
        ("brightness", c_float),
        ("width", c_float),
        ("dry_wet", c_float),
        ("intensity", c_float),
    ]

class _ae_extended_params_t(Structure):
    _fields_ = [
        ("decay_time", c_float),
        ("diffusion", c_float),
        ("lofi_amount", c_float),
        ("modulation", c_float),
    ]

class _ae_audio_buffer_t(Structure):
    _fields_ = [
        ("samples", POINTER(c_float)),
        ("frame_count", c_size_t),
        ("channels", c_uint8),
        ("interleaved", c_bool),
    ]

class _ae_scenario_blend_t(Structure):
    _fields_ = [
        ("name", c_char_p),
        ("weight", c_float),
    ]

# ============================================================================
# Function Prototypes
# ============================================================================

# Version
_dll.ae_get_version.restype = c_uint32
_dll.ae_get_version.argtypes = []

_dll.ae_get_version_string.restype = c_char_p
_dll.ae_get_version_string.argtypes = []

# Config
_dll.ae_get_default_config.restype = _ae_config_t
_dll.ae_get_default_config.argtypes = []

# Engine lifecycle
_dll.ae_create_engine.restype = c_void_p
_dll.ae_create_engine.argtypes = [POINTER(_ae_config_t)]

_dll.ae_destroy_engine.restype = None
_dll.ae_destroy_engine.argtypes = [c_void_p]

# Error handling
_dll.ae_get_error_string.restype = c_char_p
_dll.ae_get_error_string.argtypes = [c_int]

# Main parameters
_dll.ae_get_main_params.restype = c_int
_dll.ae_get_main_params.argtypes = [c_void_p, POINTER(_ae_main_params_t)]

_dll.ae_set_main_params.restype = c_int
_dll.ae_set_main_params.argtypes = [c_void_p, POINTER(_ae_main_params_t)]

# Individual setters
_dll.ae_set_distance.restype = c_int
_dll.ae_set_distance.argtypes = [c_void_p, c_float]

_dll.ae_set_room_size.restype = c_int
_dll.ae_set_room_size.argtypes = [c_void_p, c_float]

_dll.ae_set_brightness.restype = c_int
_dll.ae_set_brightness.argtypes = [c_void_p, c_float]

_dll.ae_set_width.restype = c_int
_dll.ae_set_width.argtypes = [c_void_p, c_float]

_dll.ae_set_dry_wet.restype = c_int
_dll.ae_set_dry_wet.argtypes = [c_void_p, c_float]

_dll.ae_set_intensity.restype = c_int
_dll.ae_set_intensity.argtypes = [c_void_p, c_float]

# Scenario
_dll.ae_apply_scenario.restype = c_int
_dll.ae_apply_scenario.argtypes = [c_void_p, c_char_p, c_float]

_dll.ae_blend_scenarios.restype = c_int
_dll.ae_blend_scenarios.argtypes = [c_void_p, POINTER(_ae_scenario_blend_t), c_size_t]

# Perceptual controls
_dll.ae_set_perceived_distance.restype = c_int
_dll.ae_set_perceived_distance.argtypes = [c_void_p, c_float]

_dll.ae_set_perceived_spaciousness.restype = c_int
_dll.ae_set_perceived_spaciousness.argtypes = [c_void_p, c_float]

_dll.ae_set_perceived_clarity.restype = c_int
_dll.ae_set_perceived_clarity.argtypes = [c_void_p, c_float]

# Audio processing
_dll.ae_process.restype = c_int
_dll.ae_process.argtypes = [c_void_p, POINTER(_ae_audio_buffer_t), POINTER(_ae_audio_buffer_t)]

# ============================================================================
# Enums and Data Classes
# ============================================================================

class Scenario(Enum):
    """Available acoustic scenarios"""
    # Natural environments
    DEEP_SEA = "deep_sea"
    FOREST = "forest"
    CAVE = "cave"
    OPEN_FIELD = "open_field"
    SPACE = "space"
    
    # Artificial environments
    CATHEDRAL = "cathedral"
    OFFICE = "office"
    TUNNEL = "tunnel"
    SMALL_ROOM = "small_room"
    RADIO = "radio"
    TELEPHONE = "telephone"
    
    # Emotional/artistic
    TENSION = "tension"
    NOSTALGIA = "nostalgia"
    DREAM = "dream"
    INTIMATE = "intimate"
    CHAOS = "chaos"
    ETHEREAL = "ethereal"


@dataclass
class MainParams:
    """Main audio processing parameters"""
    distance: float = 10.0      # 0.1 - 1000 m
    room_size: float = 0.5      # 0.0 - 1.0
    brightness: float = 0.0     # -1.0 - 1.0
    width: float = 1.0          # 0.0 - 2.0
    dry_wet: float = 0.5        # 0.0 - 1.0
    intensity: float = 0.5      # 0.0 - 1.0


@dataclass  
class ExtendedParams:
    """Extended processing parameters"""
    decay_time: float = 1.0     # 0.1 - 30.0 seconds
    diffusion: float = 0.5      # 0.0 - 1.0
    lofi_amount: float = 0.0    # 0.0 - 1.0
    modulation: float = 0.0     # 0.0 - 1.0


class AcousticEngineError(Exception):
    """Exception raised when DLL function returns an error"""
    pass

# ============================================================================
# Main Wrapper Class
# ============================================================================

class AcousticEngine:
    """
    Python wrapper for Acoustic Engine DLL
    
    Example:
        engine = AcousticEngine()
        engine.apply_scenario(Scenario.DEEP_SEA, intensity=0.8)
        engine.set_perceived_distance(0.7)
        params = engine.get_main_params()
        print(f"Distance: {params.distance}m")
    """
    
    def __init__(self, sample_rate: int = 48000, max_buffer_size: int = 4096):
        """
        Create a new Acoustic Engine instance
        
        Args:
            sample_rate: Audio sample rate (default: 48000)
            max_buffer_size: Maximum buffer size (default: 4096)
        """
        config = _dll.ae_get_default_config()
        config.sample_rate = sample_rate
        config.max_buffer_size = max_buffer_size
        
        self._handle = _dll.ae_create_engine(byref(config))
        if not self._handle:
            raise AcousticEngineError("Failed to create engine")
        
        self._sample_rate = sample_rate
    
    def __del__(self):
        self.close()
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Release engine resources"""
        if hasattr(self, '_handle') and self._handle:
            _dll.ae_destroy_engine(self._handle)
            self._handle = None
    
    @staticmethod
    def get_version() -> str:
        """Get library version string"""
        return _dll.ae_get_version_string().decode('utf-8')
    
    def _check_result(self, result: int, operation: str = "operation"):
        """Check DLL return code and raise exception if error"""
        if result != 0:
            error_msg = _dll.ae_get_error_string(result)
            msg = error_msg.decode('utf-8') if error_msg else f"Error code {result}"
            raise AcousticEngineError(f"{operation} failed: {msg}")
    
    # ========================================================================
    # Main Parameters
    # ========================================================================
    
    def get_main_params(self) -> MainParams:
        """Get current main parameters"""
        params = _ae_main_params_t()
        result = _dll.ae_get_main_params(self._handle, byref(params))
        self._check_result(result, "get_main_params")
        
        return MainParams(
            distance=params.distance,
            room_size=params.room_size,
            brightness=params.brightness,
            width=params.width,
            dry_wet=params.dry_wet,
            intensity=params.intensity,
        )
    
    def set_main_params(self, params: MainParams):
        """Set all main parameters at once"""
        c_params = _ae_main_params_t(
            distance=params.distance,
            room_size=params.room_size,
            brightness=params.brightness,
            width=params.width,
            dry_wet=params.dry_wet,
            intensity=params.intensity,
        )
        result = _dll.ae_set_main_params(self._handle, byref(c_params))
        self._check_result(result, "set_main_params")
    
    def set_distance(self, distance: float):
        """Set source distance (0.1 - 1000 m)"""
        result = _dll.ae_set_distance(self._handle, c_float(distance))
        self._check_result(result, "set_distance")
    
    def set_room_size(self, room_size: float):
        """Set room size (0.0 - 1.0)"""
        result = _dll.ae_set_room_size(self._handle, c_float(room_size))
        self._check_result(result, "set_room_size")
    
    def set_brightness(self, brightness: float):
        """Set brightness/tonal quality (-1.0 to 1.0)"""
        result = _dll.ae_set_brightness(self._handle, c_float(brightness))
        self._check_result(result, "set_brightness")
    
    def set_width(self, width: float):
        """Set stereo width (0.0 - 2.0)"""
        result = _dll.ae_set_width(self._handle, c_float(width))
        self._check_result(result, "set_width")
    
    def set_dry_wet(self, dry_wet: float):
        """Set dry/wet mix (0.0 - 1.0)"""
        result = _dll.ae_set_dry_wet(self._handle, c_float(dry_wet))
        self._check_result(result, "set_dry_wet")
    
    def set_intensity(self, intensity: float):
        """Set processing intensity (0.0 - 1.0)"""
        result = _dll.ae_set_intensity(self._handle, c_float(intensity))
        self._check_result(result, "set_intensity")
    
    # ========================================================================
    # Perceptual Controls
    # ========================================================================
    
    def set_perceived_distance(self, distance: float):
        """
        Set perceived distance (0.0 - 1.0)
        
        Based on Zahorik (2002) DRR correlation.
        0.0 = near, 1.0 = far
        """
        result = _dll.ae_set_perceived_distance(self._handle, c_float(distance))
        self._check_result(result, "set_perceived_distance")
    
    def set_perceived_spaciousness(self, spaciousness: float):
        """
        Set perceived spaciousness (0.0 - 1.0)
        
        Based on Bradley & Soulodre (1995) ASW/LEV research.
        0.0 = narrow, 1.0 = wide/enveloping
        """
        result = _dll.ae_set_perceived_spaciousness(self._handle, c_float(spaciousness))
        self._check_result(result, "set_perceived_spaciousness")
    
    def set_perceived_clarity(self, clarity: float):
        """
        Set perceived clarity (0.0 - 1.0)
        
        Based on ISO 3382 C50/C80 metrics.
        0.0 = muddy, 1.0 = clear
        """
        result = _dll.ae_set_perceived_clarity(self._handle, c_float(clarity))
        self._check_result(result, "set_perceived_clarity")
    
    # ========================================================================
    # Scenarios
    # ========================================================================
    
    def apply_scenario(self, scenario: Scenario, intensity: float = 1.0):
        """
        Apply an acoustic scenario preset
        
        Args:
            scenario: Scenario enum value
            intensity: Effect intensity (0.0 - 1.0)
        """
        name = scenario.value.encode('utf-8')
        result = _dll.ae_apply_scenario(self._handle, name, c_float(intensity))
        self._check_result(result, f"apply_scenario({scenario.name})")
    
    def blend_scenarios(self, scenarios: dict):
        """
        Blend multiple scenarios
        
        Args:
            scenarios: Dict of {Scenario: weight} pairs
            
        Example:
            engine.blend_scenarios({
                Scenario.DEEP_SEA: 0.6,
                Scenario.CAVE: 0.3,
                Scenario.TENSION: 0.4,
            })
        """
        count = len(scenarios)
        if count == 0:
            return
        
        BlendArray = _ae_scenario_blend_t * count
        blends = BlendArray()
        
        for i, (scenario, weight) in enumerate(scenarios.items()):
            blends[i].name = scenario.value.encode('utf-8')
            blends[i].weight = weight
        
        result = _dll.ae_blend_scenarios(self._handle, blends, count)
        self._check_result(result, "blend_scenarios")
    
    # ========================================================================
    # Audio Processing
    # ========================================================================
    
    @property
    def sample_rate(self) -> int:
        """Get the engine's sample rate"""
        return self._sample_rate


# ============================================================================
# Scenario Presets Data
# ============================================================================

SCENARIO_INFO = {
    Scenario.DEEP_SEA: {
        "name": "Deep Sea",
        "description": "High-frequency absorption, long reverb",
        "category": "natural",
        "icon": "🌊",
    },
    Scenario.FOREST: {
        "name": "Forest", 
        "description": "Scattered reflections, short decay",
        "category": "natural",
        "icon": "🌲",
    },
    Scenario.CAVE: {
        "name": "Cave",
        "description": "Long reverb, flutter echo",
        "category": "natural", 
        "icon": "🏔️",
    },
    Scenario.OPEN_FIELD: {
        "name": "Open Field",
        "description": "Distance attenuation only",
        "category": "natural",
        "icon": "🏜️",
    },
    Scenario.SPACE: {
        "name": "Space",
        "description": "Extreme high-frequency reduction",
        "category": "natural",
        "icon": "🌌",
    },
    Scenario.CATHEDRAL: {
        "name": "Cathedral",
        "description": "Very long reverb (5-12s)",
        "category": "artificial",
        "icon": "🏛️",
    },
    Scenario.OFFICE: {
        "name": "Office",
        "description": "Short reverb, absorbent",
        "category": "artificial",
        "icon": "🏢",
    },
    Scenario.TUNNEL: {
        "name": "Tunnel",
        "description": "Metallic resonance",
        "category": "artificial",
        "icon": "🚇",
    },
    Scenario.SMALL_ROOM: {
        "name": "Small Room",
        "description": "Intimate reverb (0.3-0.8s)",
        "category": "artificial",
        "icon": "🏠",
    },
    Scenario.RADIO: {
        "name": "Radio",
        "description": "Band-limited, noise",
        "category": "artificial",
        "icon": "📻",
    },
    Scenario.TELEPHONE: {
        "name": "Telephone",
        "description": "Narrow band, distortion",
        "category": "artificial",
        "icon": "📞",
    },
    Scenario.TENSION: {
        "name": "Tension",
        "description": "Anxiety, pressure",
        "category": "emotional",
        "icon": "😰",
    },
    Scenario.NOSTALGIA: {
        "name": "Nostalgia",
        "description": "Warm, lo-fi",
        "category": "emotional",
        "icon": "🌅",
    },
    Scenario.DREAM: {
        "name": "Dream",
        "description": "Surreal, floating",
        "category": "emotional",
        "icon": "💭",
    },
    Scenario.INTIMATE: {
        "name": "Intimate",
        "description": "Close, ASMR-like",
        "category": "emotional",
        "icon": "💓",
    },
    Scenario.CHAOS: {
        "name": "Chaos",
        "description": "Glitchy, destructive",
        "category": "emotional",
        "icon": "⚡",
    },
    Scenario.ETHEREAL: {
        "name": "Ethereal",
        "description": "Mystical, transcendent",
        "category": "emotional",
        "icon": "✨",
    },
}


def get_scenarios_by_category() -> dict:
    """Get scenarios organized by category"""
    categories = {"natural": [], "artificial": [], "emotional": []}
    for scenario, info in SCENARIO_INFO.items():
        categories[info["category"]].append((scenario, info))
    return categories
