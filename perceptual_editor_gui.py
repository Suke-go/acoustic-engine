"""
Acoustic Engine - GUI Perceptual Parameter Editor with Scenario Presets
Uses the ae_wrapper module for clean DLL access.

Usage:
  python perceptual_editor_gui.py

Requirements:
  pip install sounddevice numpy pydub matplotlib
"""

import os
import sys
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Add parent directory to path for bindings import
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bindings import (
    AcousticEngine, Scenario, MainParams,
    SCENARIO_INFO, get_scenarios_by_category
)

# Try to import audio/visualization libraries
try:
    import numpy as np
    import sounddevice as sd
    AUDIO_AVAILABLE = True
except ImportError:
    AUDIO_AVAILABLE = False
    print("Warning: sounddevice/numpy not available.")

try:
    from pydub import AudioSegment
    PYDUB_AVAILABLE = True
except ImportError:
    PYDUB_AVAILABLE = False

try:
    import matplotlib
    matplotlib.use('TkAgg')
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ============================================================================
# GUI Application
# ============================================================================

class PerceptualEditorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Acoustic Engine - Perceptual Editor")
        self.root.geometry("750x800")
        self.root.resizable(True, True)
        
        # Initialize engine using wrapper
        self.engine = AcousticEngine()
        print(f"Acoustic Engine {AcousticEngine.get_version()} loaded")
        
        # Audio state
        self.audio_data = None
        self.processed_audio = None
        self.sample_rate = 48000
        self.is_playing = False
        self.process_pending = False
        
        # Scenario state
        self.current_scenario = None
        self.scenario_intensity = tk.DoubleVar(value=1.0)
        
        # Variables for perceptual sliders
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
    
    def create_widgets(self):
        # Title
        title_frame = ttk.Frame(self.root)
        title_frame.pack(fill='x', pady=5)
        
        ttk.Label(title_frame, text="Perceptual Parameter Editor", 
                 font=('Helvetica', 14, 'bold')).pack()
        ttk.Label(title_frame, text=f"Acoustic Engine {AcousticEngine.get_version()}").pack()
        
        # Audio file controls
        audio_frame = ttk.LabelFrame(self.root, text="Audio File", padding=5)
        audio_frame.pack(fill='x', padx=10, pady=5)
        
        file_row = ttk.Frame(audio_frame)
        file_row.pack(fill='x')
        
        self.file_label = ttk.Label(file_row, text="No file loaded", width=35)
        self.file_label.pack(side='left', padx=5)
        
        ttk.Button(file_row, text="Load", command=self.browse_audio, width=8).pack(side='left', padx=2)
        ttk.Button(file_row, text="▶ Play", command=self.play_audio, width=8).pack(side='left', padx=2)
        ttk.Button(file_row, text="■ Stop", command=self.stop_audio, width=8).pack(side='left', padx=2)
        ttk.Button(file_row, text="Export", command=self.export_audio, width=8).pack(side='left', padx=2)
        
        # Scenario presets
        scenario_frame = ttk.LabelFrame(self.root, text="Scenario Presets", padding=5)
        scenario_frame.pack(fill='x', padx=10, pady=5)
        
        # Category tabs using notebook
        self.scenario_notebook = ttk.Notebook(scenario_frame)
        self.scenario_notebook.pack(fill='x', pady=5)
        
        categories = get_scenarios_by_category()
        category_names = {"natural": "🌍 Natural", "artificial": "🏛️ Artificial", "emotional": "💭 Emotional"}
        
        for cat_key, cat_name in category_names.items():
            tab = ttk.Frame(self.scenario_notebook)
            self.scenario_notebook.add(tab, text=cat_name)
            
            btn_frame = ttk.Frame(tab)
            btn_frame.pack(fill='x', pady=5)
            
            for i, (scenario, info) in enumerate(categories[cat_key]):
                btn = ttk.Button(
                    btn_frame, 
                    text=f"{info['icon']} {info['name']}", 
                    width=12,
                    command=lambda s=scenario: self.apply_scenario(s)
                )
                btn.grid(row=i // 4, column=i % 4, padx=2, pady=2)
        
        # Intensity slider
        intensity_row = ttk.Frame(scenario_frame)
        intensity_row.pack(fill='x', pady=5)
        
        ttk.Label(intensity_row, text="Intensity:").pack(side='left', padx=5)
        intensity_slider = ttk.Scale(intensity_row, from_=0.0, to=1.0, orient='horizontal',
                 variable=self.scenario_intensity, command=lambda _: self.on_intensity_change())
        intensity_slider.pack(side='left', fill='x', expand=True, padx=5)
        self.intensity_label = ttk.Label(intensity_row, text="1.00", width=5)
        self.intensity_label.pack(side='left')
        self.scenario_label = ttk.Label(intensity_row, text="(No scenario)", width=15)
        self.scenario_label.pack(side='left')
        
        # Waveform visualization
        if MATPLOTLIB_AVAILABLE:
            wave_frame = ttk.LabelFrame(self.root, text="Waveform (Blue: Original, Orange: Processed)", padding=5)
            wave_frame.pack(fill='both', expand=True, padx=10, pady=5)
            
            self.fig = Figure(figsize=(6, 2), dpi=100)
            self.fig.patch.set_facecolor('#f0f0f0')
            self.ax = self.fig.add_subplot(111)
            self.ax.set_facecolor('#ffffff')
            self.fig.tight_layout()
            
            self.canvas = FigureCanvasTkAgg(self.fig, master=wave_frame)
            self.canvas.get_tk_widget().pack(fill='both', expand=True)
        
        # Perceptual parameters
        perc_frame = ttk.LabelFrame(self.root, text="Perceptual Parameters (override scenario)", padding=10)
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
        ttk.Button(btn_frame, text="Reset All", command=self.reset_all).pack()
        
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
    
    def apply_scenario(self, scenario: Scenario):
        """Apply a scenario preset"""
        try:
            intensity = self.scenario_intensity.get()
            self.engine.apply_scenario(scenario, intensity)
            self.current_scenario = scenario
            
            info = SCENARIO_INFO[scenario]
            self.scenario_label.config(text=f"{info['icon']} {info['name']}")
            self.intensity_label.config(text=f"{intensity:.2f}")
            self.status_var.set(f"Applied: {info['name']} (intensity={intensity:.2f})")
            
            # Update physical params display
            self.update_physical_display()
            
            # Auto-process
            if self.audio_data is not None:
                self.process_audio()
                self.update_waveform()
                
        except Exception as e:
            self.status_var.set(f"Error: {e}")
    
    def on_intensity_change(self):
        """Re-apply current scenario when intensity changes"""
        intensity = self.scenario_intensity.get()
        self.intensity_label.config(text=f"{intensity:.2f}")
        
        if self.current_scenario is not None:
            self.apply_scenario(self.current_scenario)
    
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
        """Apply perceptual parameters via wrapper"""
        try:
            self.engine.set_perceived_distance(self.distance_var.get())
            self.engine.set_perceived_spaciousness(self.spaciousness_var.get())
            self.engine.set_perceived_clarity(self.clarity_var.get())
            self.update_physical_display()
        except Exception as e:
            self.status_var.set(f"Error: {e}")
    
    def update_physical_display(self):
        """Update physical parameters display"""
        try:
            params = self.engine.get_main_params()
            self.phys_labels['distance'].config(text=f"{params.distance:.1f}m")
            self.phys_labels['room_size'].config(text=f"{params.room_size:.2f}")
            self.phys_labels['brightness'].config(text=f"{params.brightness:.2f}")
            self.phys_labels['width'].config(text=f"{params.width:.2f}")
            self.phys_labels['dry_wet'].config(text=f"{params.dry_wet:.2f}")
            self.phys_labels['intensity'].config(text=f"{params.intensity:.2f}")
        except:
            pass
    
    def reset_all(self):
        self.distance_var.set(0.5)
        self.spaciousness_var.set(0.5)
        self.clarity_var.set(0.5)
        self.scenario_intensity.set(1.0)
        self.current_scenario = None
        self.scenario_label.config(text="(No scenario)")
        self.on_slider_change()
    
    def update_waveform(self):
        if not MATPLOTLIB_AVAILABLE or self.audio_data is None:
            return
        
        self.ax.clear()
        
        n_samples = len(self.audio_data)
        step = max(1, n_samples // 2000)
        times = np.arange(0, n_samples, step) / self.sample_rate
        
        original_mono = (self.audio_data[::step, 0] + self.audio_data[::step, 1]) / 2
        self.ax.plot(times, original_mono, color='#3498db', alpha=0.7, linewidth=0.8, label='Original')
        
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
            
            self.file_label.config(text=os.path.basename(filepath))
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
        
        try:
            params = self.engine.get_main_params()
        except:
            return
        
        processed = self.audio_data.copy()
        
        # 1. Distance attenuation (stronger exponential falloff)
        # distance: 1m = 1.0, 50m = 0.17, 100m = 0.10
        distance_factor = 1.0 / (1.0 + params.distance * 0.05) ** 1.5
        processed *= distance_factor
        
        # 2. Brightness filter (IIR low-pass and high-pass)
        # brightness < 0: low-pass (darken)
        # brightness > 0: high-pass emphasis (brighten)
        if abs(params.brightness) > 0.05:
            if params.brightness < 0:
                # Stronger low-pass: alpha 0.1 (dark) to 1.0 (neutral)
                alpha = 0.1 + 0.9 * (1.0 + params.brightness)
                alpha = max(0.1, min(1.0, alpha))
                for ch in range(2):
                    for i in range(1, len(processed)):
                        processed[i, ch] = alpha * processed[i, ch] + (1 - alpha) * processed[i-1, ch]
            else:
                # High-frequency emphasis (simple high-pass + original)
                hp_alpha = 0.85 - params.brightness * 0.3
                hp_alpha = max(0.4, min(0.95, hp_alpha))
                for ch in range(2):
                    prev = 0.0
                    for i in range(len(processed)):
                        hp = hp_alpha * (prev + processed[i, ch] - (processed[i-1, ch] if i > 0 else 0))
                        processed[i, ch] = processed[i, ch] + hp * params.brightness * 0.5
                        prev = hp
        
        # 3. Stereo width (M/S processing)
        if abs(params.width - 1.0) > 0.01:
            mid = (processed[:, 0] + processed[:, 1]) * 0.5
            side = (processed[:, 0] - processed[:, 1]) * 0.5 * params.width
            processed[:, 0] = mid + side
            processed[:, 1] = mid - side
        
        # 4. Reverb simulation (multi-tap delay with room_size)
        if params.dry_wet > 0.05:
            # Base delay from room_size (small room: 20ms, large: 100ms)
            base_delay_sec = 0.02 + params.room_size * 0.08
            
            reverb = np.zeros_like(processed)
            taps = [
                (base_delay_sec * 1.0, 0.35),
                (base_delay_sec * 1.7, 0.25),
                (base_delay_sec * 2.8, 0.18),
                (base_delay_sec * 4.3, 0.12),
                (base_delay_sec * 6.0, 0.08),
            ]
            
            for delay_sec, gain in taps:
                delay_samples = int(delay_sec * self.sample_rate)
                if delay_samples < len(processed):
                    reverb[delay_samples:] += processed[:-delay_samples] * gain
            
            # Apply room_size to reverb diffusion (blur)
            if params.room_size > 0.3:
                blur_len = int(params.room_size * 100)
                kernel = np.ones(blur_len) / blur_len
                for ch in range(2):
                    reverb[:, ch] = np.convolve(reverb[:, ch], kernel, mode='same')
            
            # Mix dry/wet
            wet_gain = params.dry_wet * params.intensity
            dry_gain = 1.0 - params.dry_wet * 0.7
            processed = dry_gain * processed + wet_gain * reverb
        
        # 5. Soft clipping and normalization
        processed = np.tanh(processed * 1.2) / 1.2  # Soft clip
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
        self.engine.close()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = PerceptualEditorGUI(root)
    root.mainloop()
