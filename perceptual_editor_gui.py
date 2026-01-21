"""
Acoustic Engine - GUI Perceptual Parameter Editor with Waveform Visualization
Uses tkinter for GUI, matplotlib for waveform display, and acoustic_engine.dll for processing.

Usage:
  python perceptual_editor_gui.py

Requirements:
  pip install sounddevice numpy pydub matplotlib
"""

import ctypes
import os
import sys
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from ctypes import (
    c_float, c_int, c_uint32, c_uint8, c_size_t, c_char_p, c_void_p, c_bool,
    Structure, POINTER, byref
)

# Try to import audio/visualization libraries
try:
    import numpy as np
    import sounddevice as sd
    AUDIO_AVAILABLE = True
except ImportError:
    AUDIO_AVAILABLE = False
    print("Warning: sounddevice/numpy not available. Audio playback disabled.")

try:
    from pydub import AudioSegment
    PYDUB_AVAILABLE = True
except ImportError:
    PYDUB_AVAILABLE = False
    print("Warning: pydub not available. MP3 loading disabled.")

try:
    import matplotlib
    matplotlib.use('TkAgg')
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not available. Waveform display disabled.")

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

ae.ae_get_version_string.restype = c_char_p
ae.ae_get_version_string.argtypes = []

ae.ae_get_default_config.restype = ae_config_t
ae.ae_get_default_config.argtypes = []

ae.ae_create_engine.restype = c_void_p
ae.ae_create_engine.argtypes = [POINTER(ae_config_t)]

ae.ae_destroy_engine.restype = None
ae.ae_destroy_engine.argtypes = [c_void_p]

ae.ae_get_main_params.restype = c_int
ae.ae_get_main_params.argtypes = [c_void_p, POINTER(ae_main_params_t)]

ae.ae_set_perceived_distance.restype = c_int
ae.ae_set_perceived_distance.argtypes = [c_void_p, c_float]

ae.ae_set_perceived_spaciousness.restype = c_int
ae.ae_set_perceived_spaciousness.argtypes = [c_void_p, c_float]

ae.ae_set_perceived_clarity.restype = c_int
ae.ae_set_perceived_clarity.argtypes = [c_void_p, c_float]

# ============================================================================
# GUI Application
# ============================================================================

class PerceptualEditorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Acoustic Engine - Perceptual Editor")
        self.root.geometry("700x750")
        self.root.resizable(True, True)
        
        # Initialize engine
        self.engine = None
        self.init_engine()
        
        # Audio state
        self.audio_data = None
        self.processed_audio = None
        self.sample_rate = 48000
        self.audio_file_path = None
        self.is_playing = False
        self.process_pending = False
        
        # Variables for sliders
        self.distance_var = tk.DoubleVar(value=0.5)
        self.spaciousness_var = tk.DoubleVar(value=0.5)
        self.clarity_var = tk.DoubleVar(value=0.5)
        
        # Build UI
        self.create_widgets()
        
        # Initial update
        self.update_parameters()
        
        # Cleanup on close
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        
        # Auto-load default file
        default_file = os.path.join(SCRIPT_DIR, "sample_sounds", "test_heartbeat.mp3")
        if os.path.exists(default_file):
            self.load_audio_file(default_file)
    
    def init_engine(self):
        config = ae.ae_get_default_config()
        self.engine = ae.ae_create_engine(byref(config))
        if not self.engine:
            print("Error: Failed to create engine")
            sys.exit(1)
        version = ae.ae_get_version_string()
        print(f"Acoustic Engine {version.decode('utf-8')} loaded")
    
    def create_widgets(self):
        # Title
        title_frame = ttk.Frame(self.root)
        title_frame.pack(fill='x', pady=5)
        
        ttk.Label(title_frame, text="Perceptual Parameter Editor", 
                 font=('Helvetica', 14, 'bold')).pack()
        version = ae.ae_get_version_string().decode('utf-8')
        ttk.Label(title_frame, text=f"Acoustic Engine {version}").pack()
        
        # Audio file controls
        audio_frame = ttk.LabelFrame(self.root, text="Audio File", padding=5)
        audio_frame.pack(fill='x', padx=10, pady=5)
        
        file_row = ttk.Frame(audio_frame)
        file_row.pack(fill='x')
        
        self.file_label = ttk.Label(file_row, text="No file loaded", width=35)
        self.file_label.pack(side='left', padx=5)
        
        ttk.Button(file_row, text="Load", command=self.browse_audio, width=8).pack(side='left', padx=2)
        ttk.Button(file_row, text="Play", command=self.play_audio, width=8).pack(side='left', padx=2)
        ttk.Button(file_row, text="Stop", command=self.stop_audio, width=8).pack(side='left', padx=2)
        ttk.Button(file_row, text="Export", command=self.export_audio, width=8).pack(side='left', padx=2)
        
        # Waveform visualization
        if MATPLOTLIB_AVAILABLE:
            wave_frame = ttk.LabelFrame(self.root, text="Waveform (Original: Blue, Processed: Orange)", padding=5)
            wave_frame.pack(fill='both', expand=True, padx=10, pady=5)
            
            self.fig = Figure(figsize=(6, 2.5), dpi=100)
            self.fig.patch.set_facecolor('#f0f0f0')
            self.ax = self.fig.add_subplot(111)
            self.ax.set_facecolor('#ffffff')
            self.ax.set_xlabel('Time (s)', fontsize=8)
            self.ax.set_ylabel('Amplitude', fontsize=8)
            self.ax.tick_params(labelsize=7)
            self.fig.tight_layout()
            
            self.canvas = FigureCanvasTkAgg(self.fig, master=wave_frame)
            self.canvas.get_tk_widget().pack(fill='both', expand=True)
        
        # Perceptual parameters
        perc_frame = ttk.LabelFrame(self.root, text="Perceptual Parameters", padding=10)
        perc_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(perc_frame, "Distance (near → far)", self.distance_var)
        self.create_slider(perc_frame, "Spaciousness (narrow → wide)", self.spaciousness_var)
        self.create_slider(perc_frame, "Clarity (muddy → clear)", self.clarity_var)
        
        # Physical parameters display
        phys_frame = ttk.LabelFrame(self.root, text="Physical Parameters (mapped)", padding=5)
        phys_frame.pack(fill='x', padx=10, pady=5)
        
        self.phys_labels = {}
        params = ['distance', 'room_size', 'brightness', 'width', 'dry_wet', 'intensity']
        
        for i, param in enumerate(params):
            row = i // 3
            col = i % 3
            frame = ttk.Frame(phys_frame)
            frame.grid(row=row, column=col, padx=8, pady=2, sticky='w')
            ttk.Label(frame, text=f"{param}:", width=10).pack(side='left')
            self.phys_labels[param] = ttk.Label(frame, text="0.00", width=8)
            self.phys_labels[param].pack(side='left')
        
        # Reset button
        btn_frame = ttk.Frame(self.root)
        btn_frame.pack(pady=5)
        ttk.Button(btn_frame, text="Reset Parameters", command=self.reset_parameters).pack()
        
        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        ttk.Label(self.root, textvariable=self.status_var, relief='sunken').pack(fill='x', padx=5, pady=5)
    
    def create_slider(self, parent, label, variable):
        frame = ttk.Frame(parent)
        frame.pack(fill='x', pady=3)
        
        ttk.Label(frame, text=label, width=28).pack(side='left')
        
        slider = ttk.Scale(frame, from_=0.0, to=1.0, orient='horizontal',
                          variable=variable, command=lambda _: self.on_slider_change())
        slider.pack(side='left', fill='x', expand=True, padx=5)
        
        value_label = ttk.Label(frame, text="0.50", width=5)
        value_label.pack(side='left')
        variable.label = value_label
    
    def on_slider_change(self):
        # Update labels
        self.distance_var.label.config(text=f"{self.distance_var.get():.2f}")
        self.spaciousness_var.label.config(text=f"{self.spaciousness_var.get():.2f}")
        self.clarity_var.label.config(text=f"{self.clarity_var.get():.2f}")
        
        self.update_parameters()
        
        # Auto-process with debounce
        if self.audio_data is not None and not self.process_pending:
            self.process_pending = True
            self.root.after(100, self.delayed_process)
    
    def delayed_process(self):
        self.process_pending = False
        self.process_audio()
        self.update_waveform()
    
    def update_parameters(self):
        if not self.engine:
            return
        
        ae.ae_set_perceived_distance(self.engine, c_float(self.distance_var.get()))
        ae.ae_set_perceived_spaciousness(self.engine, c_float(self.spaciousness_var.get()))
        ae.ae_set_perceived_clarity(self.engine, c_float(self.clarity_var.get()))
        
        params = ae_main_params_t()
        if ae.ae_get_main_params(self.engine, byref(params)) == 0:
            self.phys_labels['distance'].config(text=f"{params.distance:.1f}m")
            self.phys_labels['room_size'].config(text=f"{params.room_size:.2f}")
            self.phys_labels['brightness'].config(text=f"{params.brightness:.2f}")
            self.phys_labels['width'].config(text=f"{params.width:.2f}")
            self.phys_labels['dry_wet'].config(text=f"{params.dry_wet:.2f}")
            self.phys_labels['intensity'].config(text=f"{params.intensity:.2f}")
    
    def reset_parameters(self):
        self.distance_var.set(0.5)
        self.spaciousness_var.set(0.5)
        self.clarity_var.set(0.5)
        self.on_slider_change()
    
    def update_waveform(self):
        if not MATPLOTLIB_AVAILABLE or self.audio_data is None:
            return
        
        self.ax.clear()
        
        # Downsample for display (max 2000 points)
        n_samples = len(self.audio_data)
        step = max(1, n_samples // 2000)
        
        # Time axis
        times = np.arange(0, n_samples, step) / self.sample_rate
        
        # Original waveform (mono mix)
        original_mono = (self.audio_data[::step, 0] + self.audio_data[::step, 1]) / 2
        self.ax.plot(times, original_mono, color='#3498db', alpha=0.7, linewidth=0.8, label='Original')
        
        # Processed waveform
        if self.processed_audio is not None:
            processed_mono = (self.processed_audio[::step, 0] + self.processed_audio[::step, 1]) / 2
            self.ax.plot(times, processed_mono, color='#e67e22', alpha=0.8, linewidth=0.8, label='Processed')
        
        self.ax.set_xlabel('Time (s)', fontsize=8)
        self.ax.set_ylabel('Amplitude', fontsize=8)
        self.ax.set_ylim(-1.1, 1.1)
        self.ax.legend(loc='upper right', fontsize=7)
        self.ax.tick_params(labelsize=7)
        self.fig.tight_layout()
        
        self.canvas.draw()
    
    def browse_audio(self):
        filetypes = [("Audio files", "*.wav *.mp3"), ("All files", "*.*")]
        filename = filedialog.askopenfilename(
            title="Select Audio File",
            initialdir=os.path.join(SCRIPT_DIR, "sample_sounds"),
            filetypes=filetypes
        )
        if filename:
            self.load_audio_file(filename)
    
    def load_audio_file(self, filepath):
        if not AUDIO_AVAILABLE:
            messagebox.showerror("Error", "Audio libraries not available")
            return
        
        self.status_var.set(f"Loading: {os.path.basename(filepath)}...")
        self.root.update()
        
        try:
            ext = os.path.splitext(filepath)[1].lower()
            
            if ext == '.mp3' and PYDUB_AVAILABLE:
                audio = AudioSegment.from_mp3(filepath)
                audio = audio.set_frame_rate(48000).set_channels(2)
                samples = np.array(audio.get_array_of_samples(), dtype=np.float32) / 32768.0
                samples = samples.reshape(-1, 2)
                self.audio_data = samples
                self.sample_rate = audio.frame_rate
            elif ext == '.wav':
                import wave
                with wave.open(filepath, 'rb') as wf:
                    n_ch = wf.getnchannels()
                    raw = wf.readframes(wf.getnframes())
                    samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
                    if n_ch == 1:
                        samples = np.column_stack([samples, samples])
                    else:
                        samples = samples.reshape(-1, n_ch)
                    self.audio_data = samples
                    self.sample_rate = wf.getframerate()
            else:
                raise ValueError(f"Unsupported format: {ext}")
            
            self.audio_file_path = filepath
            self.file_label.config(text=os.path.basename(filepath))
            
            # Initial process and display
            self.process_audio()
            self.update_waveform()
            
            duration = len(self.audio_data) / self.sample_rate
            self.status_var.set(f"Loaded: {os.path.basename(filepath)} ({duration:.1f}s)")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load: {e}")
            self.status_var.set("Load error")
    
    def process_audio(self):
        if self.audio_data is None:
            return
        
        params = ae_main_params_t()
        ae.ae_get_main_params(self.engine, byref(params))
        
        processed = self.audio_data.copy()
        
        # Distance -> attenuation
        distance_factor = 1.0 / (1.0 + params.distance * 0.1)
        processed *= distance_factor
        
        # Brightness -> simple filter
        if params.brightness < 0:
            alpha = 0.3 + 0.7 * (1.0 + params.brightness)
            for i in range(1, len(processed)):
                processed[i] = alpha * processed[i] + (1 - alpha) * processed[i-1]
        
        # Width -> stereo adjustment
        if params.width != 1.0:
            mid = (processed[:, 0] + processed[:, 1]) / 2
            side = (processed[:, 0] - processed[:, 1]) / 2 * params.width
            processed[:, 0] = mid + side
            processed[:, 1] = mid - side
        
        # Dry/wet -> reverb simulation
        if params.dry_wet > 0.1:
            delay = int(0.05 * self.sample_rate)
            if delay < len(processed):
                reverb = np.zeros_like(processed)
                reverb[delay:] = processed[:-delay] * 0.3 * params.dry_wet
                processed = processed * (1.0 - params.dry_wet * 0.5) + reverb
        
        # Normalize
        max_val = np.max(np.abs(processed))
        if max_val > 0.95:
            processed *= 0.95 / max_val
        
        self.processed_audio = processed
    
    def play_audio(self):
        if not AUDIO_AVAILABLE:
            return
        
        audio = self.processed_audio if self.processed_audio is not None else self.audio_data
        if audio is None:
            return
        
        if self.is_playing:
            self.stop_audio()
        
        self.is_playing = True
        self.status_var.set("Playing...")
        
        def play_thread():
            try:
                sd.play(audio, self.sample_rate)
                sd.wait()
            finally:
                self.is_playing = False
                self.root.after(0, lambda: self.status_var.set("Ready"))
        
        threading.Thread(target=play_thread, daemon=True).start()
    
    def stop_audio(self):
        if AUDIO_AVAILABLE:
            sd.stop()
        self.is_playing = False
        self.status_var.set("Stopped")
    
    def export_audio(self):
        audio = self.processed_audio if self.processed_audio is not None else self.audio_data
        if audio is None:
            messagebox.showwarning("Warning", "No audio to export")
            return
        
        filepath = filedialog.asksaveasfilename(
            defaultextension=".wav",
            filetypes=[("WAV files", "*.wav")],
            initialfile="processed_output.wav"
        )
        if filepath:
            try:
                import wave
                audio_int16 = (audio * 32767).astype(np.int16)
                with wave.open(filepath, 'wb') as wf:
                    wf.setnchannels(2)
                    wf.setsampwidth(2)
                    wf.setframerate(self.sample_rate)
                    wf.writeframes(audio_int16.tobytes())
                self.status_var.set(f"Exported: {os.path.basename(filepath)}")
            except Exception as e:
                messagebox.showerror("Error", f"Export failed: {e}")
    
    def on_close(self):
        self.stop_audio()
        if self.engine:
            ae.ae_destroy_engine(self.engine)
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = PerceptualEditorGUI(root)
    root.mainloop()
