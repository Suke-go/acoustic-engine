"""
Acoustic Engine - Interactive Perceptual Parameter Editor
Uses the acoustic_engine.dll to dynamically adjust perceptual parameters.

Usage:
  python perceptual_editor.py

Controls:
  d/D - Decrease/Increase perceived distance
  s/S - Decrease/Increase perceived spaciousness  
  c/C - Decrease/Increase perceived clarity
  r   - Reset all to default
  q   - Quit
"""

import ctypes
import os
import sys
from ctypes import (
    c_float, c_int, c_uint32, c_uint8, c_size_t, c_char_p, c_void_p, c_bool,
    Structure, POINTER, byref
)

# Find the DLL
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DLL_PATH = os.path.join(SCRIPT_DIR, "build", "Release", "acoustic_engine.dll")

if not os.path.exists(DLL_PATH):
    print(f"Error: DLL not found at {DLL_PATH}")
    sys.exit(1)

# Load DLL
ae = ctypes.CDLL(DLL_PATH)

# ============================================================================
# Structure definitions
# ============================================================================

class ae_config_t(Structure):
    _fields_ = [
        ("sample_rate", c_uint32),
        ("max_buffer_size", c_uint32),
        ("data_path", c_char_p),
        ("hrtf_path", c_char_p),
        ("preload_hrtf", c_bool),
        ("preload_all_presets", c_bool),
        ("max_reverb_time_sec", c_size_t),
    ]

class ae_main_params_t(Structure):
    _fields_ = [
        ("distance", c_float),
        ("room_size", c_float),
        ("brightness", c_float),
        ("width", c_float),
        ("dry_wet", c_float),
        ("intensity", c_float),
    ]

# ============================================================================
# Function prototypes
# ============================================================================

# Version
ae.ae_get_version_string.restype = c_char_p
ae.ae_get_version_string.argtypes = []

# Config
ae.ae_get_default_config.restype = ae_config_t
ae.ae_get_default_config.argtypes = []

# Engine lifecycle
ae.ae_create_engine.restype = c_void_p
ae.ae_create_engine.argtypes = [POINTER(ae_config_t)]

ae.ae_destroy_engine.restype = None
ae.ae_destroy_engine.argtypes = [c_void_p]

# Parameter getters/setters
ae.ae_get_main_params.restype = c_int
ae.ae_get_main_params.argtypes = [c_void_p, POINTER(ae_main_params_t)]

ae.ae_set_main_params.restype = c_int
ae.ae_set_main_params.argtypes = [c_void_p, POINTER(ae_main_params_t)]

# Perceptual controls
ae.ae_set_perceived_distance.restype = c_int
ae.ae_set_perceived_distance.argtypes = [c_void_p, c_float]

ae.ae_set_perceived_spaciousness.restype = c_int
ae.ae_set_perceived_spaciousness.argtypes = [c_void_p, c_float]

ae.ae_set_perceived_clarity.restype = c_int
ae.ae_set_perceived_clarity.argtypes = [c_void_p, c_float]

# ============================================================================
# Interactive Editor
# ============================================================================

class PerceptualEditor:
    def __init__(self):
        self.engine = None
        self.distance = 0.5  # 0.0 (near) - 1.0 (far)
        self.spaciousness = 0.5  # 0.0 (narrow) - 1.0 (wide)
        self.clarity = 0.5  # 0.0 (muddy) - 1.0 (clear)
        
    def init_engine(self):
        """Initialize the acoustic engine"""
        config = ae.ae_get_default_config()
        self.engine = ae.ae_create_engine(byref(config))
        if not self.engine:
            print("Error: Failed to create engine")
            return False
        
        version = ae.ae_get_version_string()
        print(f"Acoustic Engine {version.decode('utf-8')}")
        return True
    
    def cleanup(self):
        """Clean up resources"""
        if self.engine:
            ae.ae_destroy_engine(self.engine)
            self.engine = None
    
    def update_parameters(self):
        """Apply perceptual parameters to the engine"""
        ae.ae_set_perceived_distance(self.engine, c_float(self.distance))
        ae.ae_set_perceived_spaciousness(self.engine, c_float(self.spaciousness))
        ae.ae_set_perceived_clarity(self.engine, c_float(self.clarity))
    
    def get_physical_params(self):
        """Get the resulting physical parameters"""
        params = ae_main_params_t()
        result = ae.ae_get_main_params(self.engine, byref(params))
        if result == 0:
            return params
        return None
    
    def print_status(self):
        """Print current parameter status"""
        os.system('cls' if os.name == 'nt' else 'clear')
        
        print("=" * 60)
        print("   Acoustic Engine - Interactive Perceptual Editor")
        print("=" * 60)
        print()
        
        # Perceptual parameters
        print("+-- PERCEPTUAL PARAMETERS (0.0 - 1.0) ----------------------+")
        print(f"|  Distance    [d/D]: {self._bar(self.distance)} {self.distance:.2f}  |")
        print(f"|  Spaciousness[s/S]: {self._bar(self.spaciousness)} {self.spaciousness:.2f}  |")
        print(f"|  Clarity     [c/C]: {self._bar(self.clarity)} {self.clarity:.2f}  |")
        print("+------------------------------------------------------------+")
        print()
        
        # Physical parameters (result of mapping)
        params = self.get_physical_params()
        if params:
            print("+-- PHYSICAL PARAMETERS (mapped) ---------------------------+")
            print(f"|  distance:   {params.distance:8.2f} m                        |")
            print(f"|  room_size:  {params.room_size:8.2f}                          |")
            print(f"|  brightness: {params.brightness:8.2f}                          |")
            print(f"|  width:      {params.width:8.2f}                          |")
            print(f"|  dry_wet:    {params.dry_wet:8.2f}                          |")
            print(f"|  intensity:  {params.intensity:8.2f}                          |")
            print("+------------------------------------------------------------+")
        
        print()
        print("Controls: d/D=distance, s/S=spaciousness, c/C=clarity")
        print("          r=reset, q=quit")
        print()
    
    def _bar(self, value, width=30):
        """Create a visual progress bar (ASCII only)"""
        filled = int(value * width)
        return "#" * filled + "-" * (width - filled)
    
    def run(self):
        """Main interactive loop"""
        if not self.init_engine():
            return
        
        try:
            self.update_parameters()
            self.print_status()
            
            while True:
                key = self._getch()
                
                if key == 'q':
                    break
                elif key == 'd':
                    self.distance = max(0.0, self.distance - 0.05)
                elif key == 'D':
                    self.distance = min(1.0, self.distance + 0.05)
                elif key == 's':
                    self.spaciousness = max(0.0, self.spaciousness - 0.05)
                elif key == 'S':
                    self.spaciousness = min(1.0, self.spaciousness + 0.05)
                elif key == 'c':
                    self.clarity = max(0.0, self.clarity - 0.05)
                elif key == 'C':
                    self.clarity = min(1.0, self.clarity + 0.05)
                elif key == 'r':
                    self.distance = 0.5
                    self.spaciousness = 0.5
                    self.clarity = 0.5
                else:
                    continue
                
                self.update_parameters()
                self.print_status()
                
        finally:
            self.cleanup()
            print("Goodbye!")
    
    def _getch(self):
        """Get a single character from stdin"""
        if os.name == 'nt':  # Windows
            import msvcrt
            return msvcrt.getch().decode('utf-8', errors='ignore')
        else:  # Unix/Linux/Mac
            import tty
            import termios
            fd = sys.stdin.fileno()
            old_settings = termios.tcgetattr(fd)
            try:
                tty.setraw(sys.stdin.fileno())
                ch = sys.stdin.read(1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
            return ch


if __name__ == "__main__":
    editor = PerceptualEditor()
    editor.run()
