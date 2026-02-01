# LombardEar 🎧

**Real-time 3-Microphone Hearing Aid Audio Processing System**

[![Language](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Windows%20%7C%20Linux-green.svg)](#platform-support)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

LombardEar is a low-latency, research-grade hearing aid audio processing system implementing a **Time-domain 3-channel Generalized Sidelobe Canceller (GSC)** with leakage-aware dual-loop adaptation. Designed for both Windows and Ubuntu Linux, it provides a real-time audio processing pipeline with a web-based control interface.

---

## ✨ Features

- **3-Microphone Beamforming**: Time-domain GSC algorithm with adaptive interference cancellation
- **Dual-Loop Adaptation**: Leakage-aware NLMS with soft rate control
- **Ultra-Low Latency**: Target < 10ms (initial), optimized for < 1ms with WASAPI Exclusive mode
- **Real-time DSP Chain**: GSC → AEC → AGC → Noise Gate pipeline
- **Web-based UI**: Real-time parameter control and monitoring via WebSocket
- **Cross-Platform**: Windows 11 (WASAPI) and Ubuntu 24.04 LTS (ALSA/PulseAudio)
- **AVX2 SIMD**: Optional vectorized processing for improved performance

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Audio Input (3ch)                        │
│                    [Mic L] [Mic R] [Mic Back]                   │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    GSC Beamformer Core                          │
│  ┌─────────┐    ┌─────────┐    ┌──────────────────────┐         │
│  │   FBF   │───▶│   BM    │───▶│  Leaky NLMS (AIC)    │         │
│  │ (Fixed) │    │(Blocking│    │  + β Adaptation Loop │         │
│  └─────────┘    │ Matrix) │    └──────────────────────┘         │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                      DSP Processing Chain                       │
│     [AEC Echo Canceller]  →  [AGC]  →  [Noise Gate]             │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Audio Output (Stereo)                        │
│                      [Speaker L] [Speaker R]                    │
└─────────────────────────────────────────────────────────────────┘
```

### GSC Algorithm Overview

The **Generalized Sidelobe Canceller (GSC)** implements adaptive beamforming:

1. **Fixed Beamformer (FBF)**: Computes `mid = 0.5 * (xL + xR)` as the target signal
2. **Blocking Matrix (BM)**: Creates noise references:
   - `u1 = xL - xR` (left-right difference)
   - `u2 = mid - β * xB` (adaptive back-mic subtraction)
3. **Adaptive Interference Canceller (AIC)**: Leaky NLMS filters on u1, u2
4. **Dual-Loop Adaptation**:
   - Fast loop: AIC weight update with leakage detection
   - Slow loop: β (back-mic coupling) adaptation via 1-tap NLMS

---

## 📁 Project Structure

```
LombardEar/
├── CMakeLists.txt          # Build configuration
├── LICENSE                 # MIT License
├── README.md               # Project documentation
├── docs/
│   └── plan.md             # Development plan
├── config/
│   └── default.json        # Runtime configuration
├── src/
│   ├── main.c              # Application entry point
│   ├── audio/
│   │   ├── audio_io.c      # PortAudio wrapper (WASAPI/ALSA)
│   │   └── audio_io.h
│   ├── dsp/
│   │   ├── gsc.c           # GSC beamformer core (AVX2 optimized)
│   │   ├── gsc.h
│   │   ├── aec.c           # Acoustic Echo Canceller (NLMS)
│   │   ├── aec.h
│   │   ├── agc.c           # Automatic Gain Control
│   │   ├── agc.h
│   │   ├── noise_gate.c    # Noise Gate
│   │   ├── noise_gate.h
│   │   └── math_fast.h     # Fast math utilities
│   ├── server/
│   │   ├── web_server.c    # Mongoose-based WebSocket server
│   │   └── web_server.h
│   ├── platform/
│   │   ├── platform.h      # OS abstraction layer
│   │   ├── platform_win32.c
│   │   └── platform_unix.c
│   ├── utils/
│   │   ├── config.c        # JSON configuration loader
│   │   └── config.h
│   └── third_party/
│       └── mongoose/       # Embedded HTTP/WebSocket server
├── web/
│   ├── css/
│   │   └── style.css       # Dashboard styles
│   ├── js/
│   │   └── script.js       # Dashboard logic
│   └── index.html          # Web UI dashboard
└── tests/
    ├── test_gsc_offline.c  # GSC unit tests
    ├── test_aec_offline.c  # AEC unit tests
    └── verify_ws.py        # WebSocket verification script
```

---

## 🔧 Technology Stack

| Component | Technology |
|-----------|------------|
| **Language** | C99 |
| **Build System** | CMake 3.20+ |
| **Audio I/O** | PortAudio (WASAPI on Windows, ALSA on Linux) |
| **WebSocket** | Mongoose (embedded) |
| **JSON Parsing** | cJSON (FetchContent) |
| **Optimization** | AVX2 SIMD intrinsics |

---

## 🚀 Getting Started

### Prerequisites

**macOS (recommended for Lark A1):**
```bash
# Install Homebrew and PortAudio
brew install portaudio cmake
```

**Windows:**
```powershell
# Install via vcpkg
vcpkg install portaudio:x64-windows
```

**Ubuntu/Linux:**
```bash
sudo apt install portaudio19-dev cmake build-essential
```

### Build

**macOS (Quick Start):**
```bash
# Clone and build
git clone https://github.com/yourusername/LombardEar.git
cd LombardEar

# Use the Mac build script
chmod +x scripts/build_mac.sh
./scripts/build_mac.sh

# Or run tests too
./scripts/build_mac.sh test
```

**Manual Build (All Platforms):**
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

### Hollyland Lark A1 Setup (macOS)

For 4-channel wireless microphone input with 2x Rx2 dongles:

```bash
# Interactive setup guide
chmod +x scripts/setup_audio_mac.sh
./scripts/setup_audio_mac.sh
```

**Manual Setup:**
1. Connect both Lark A1 Rx2 USB-C dongles
2. Open **Audio MIDI Setup** (Cmd+Space → "Audio MIDI Setup")
3. Click **+** → **Create Aggregate Device**
4. Check both Lark A1 devices
5. Rename to "LombardEar 4ch"

### Run

```bash
# List audio devices
./build/lombardear --list-devices

# Run with default config
./build/lombardear
```

Open `http://localhost:8000` in your browser to access the Web UI.

### WebSocket Telemetry (New)

The WebSocket now includes jitter and phase statistics:

```json
{
  "l": 0.1, "r": 0.1, "b": 0.05, "e": 0.02,
  "beta": 0.5, "mu": 0.001,
  "jitter": {"delay": 45.2, "mean": 2.1, "std": 0.8, "fill": 0.75},
  "phase": [0.0, -1.2, 0.5, 0.3]
}
```

---

## ⚙️ Configuration

Edit `config/default.json` to customize:

```json
{
  "audio": {
    "sample_rate": 48000,
    "frames_per_buffer": 480,
    "input_channels": 3,
    "output_channels": 2,
    "input_device_id": -1,
    "output_device_id": -1
  },
  "gsc": {
    "M": 96,
    "alpha": 0.005,
    "mu_max": 0.01,
    "leak_lambda": 1e-5
  },
  "ws": {
    "enable": true,
    "port": 8080
  }
}
```

### Key GSC Parameters

| Parameter | Description | Typical Range |
|-----------|-------------|---------------|
| `M` | FIR filter length | 64-128 |
| `alpha` | EWMA smoothing factor | 0.001-0.01 |
| `mu_max` | Maximum AIC step size | 0.001-0.05 |
| `eta_max` | Maximum β adaptation rate | 0.001-0.01 |
| `leak_lambda` | Leaky NLMS regularization | 1e-6 to 1e-4 |
| `g_lo`, `g_hi` | Soft control thresholds | 0.1, 0.3 |

---

## 🎛️ Web UI

The web interface provides real-time control and monitoring:

- **Signal Levels**: RMS meters for L/R/Back/Enhanced signals
- **GSC Parameters**: Real-time adjustment of α, λ, μ_max
- **DSP Controls**: Toggle AEC, AGC, Noise Gate
- **Device Selection**: Switch output devices on-the-fly

---

## 🔬 Use Cases

- **Hearing Aid Research**: Prototype and evaluate beamforming algorithms
- **Assistive Listening Devices**: Real-time noise suppression and speech enhancement
- **Audio Processing Education**: Learn adaptive filtering and beamforming
- **Acoustic Echo Cancellation**: Test AEC algorithms in real-time

---

## 📊 Performance

| Metric | Value |
|--------|-------|
| **Target Latency** | < 10ms (initial), < 1ms (optimized) |
| **Sample Rate** | 16-48 kHz |
| **Buffer Size** | 64-480 samples |
| **DSP Load** | < 1ms per block (typical) |

---

## 📚 Background

The Lombard effect refers to the involuntary tendency of speakers to increase vocal effort in noisy environments. This project aims to help users with hearing impairments by:

1. **Beamforming**: Focus on sounds from the front while suppressing interference
2. **Leakage Compensation**: Detect and compensate for acoustic leakage in open-fit hearing aids
3. **Adaptive Processing**: Dynamically adjust to changing acoustic environments

---

## 🛠️ Development

### Build Options

```bash
# Enable/disable features
cmake -B build \
  -DLE_BUILD_APP=ON \
  -DLE_BUILD_TESTS=ON \
  -DLE_WITH_WEBSOCKETS=ON \
  -DLE_WITH_SERIAL=OFF
```

### Running Tests

```bash
ctest --test-dir build --output-on-failure
```

---

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- [PortAudio](http://www.portaudio.com/) - Cross-platform audio I/O
- [Mongoose](https://mongoose.ws/) - Embedded web server
- [cJSON](https://github.com/DaveGamble/cJSON) - Lightweight JSON parser

---

## 📬 Contact

For questions or contributions, please open an issue or submit a pull request.
