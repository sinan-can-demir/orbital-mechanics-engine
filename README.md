# 🌍 Orbital Dynamics Simulation Engine

*A high-accuracy C++17 N‑Body gravitational simulator with RK4 integration, OpenGL visualization, JPL HORIZONS ephemeris support, and eclipse detection.*

---

## 🚀 Overview

This project is a full‑featured **orbital mechanics simulation engine** written in modern **C++17**.  
It numerically propagates arbitrary **N‑body systems** using Newtonian gravity with a custom **Runge–Kutta 4 (RK4)** integrator and outputs high‑precision trajectories.

A built‑in **OpenGL 3D viewer** renders the simulated orbits, and Python tools provide plotting, analysis, and animation.

The simulator can ingest real ephemeris data from **NASA JPL HORIZONS**, enabling direct comparison between RK4 propagation and real‑world ephemerides.

This system is engineered to be **modular, accurate, and extensible** — suitable for research, visualization, and educational use.

---

## ✨ Features

### 🧮 Physics & Numerical Integration
- Full **N‑body Newtonian gravity**
- Custom **RK4** integrator
- **Energy**, **linear momentum**, and **angular momentum** drift tracking
- Barycentric transformation utilities
- **Eclipse detection** using umbra/penumbra geometry

---

### 🗂 Data & Configuration
- JSON‑defined systems (planets, moons, binary systems, custom bodies)
- Output CSV includes:
  - Positions & velocities
  - Energies & momenta
  - Eclipse flags
  - Timestamps
- Supports Solar System data

---

### 🛰 HORIZONS Integration
- Fetch real ephemerides from **NASA JPL HORIZONS**
- Configure:
  - Target body
  - Center body
  - Time span
  - Step size
  - Frame (barycentric, heliocentric)
- Useful for:
  - model validation  
  - drift measurement  
  - comparing simulated vs. observed orbits  

---

### 🎥 3D Visualization
- OpenGL 3.3 orbit viewer
- Physically‑scaled planets (optional exaggeration modes)
- Reverse‑Z infinite‑distance projection
- Real‑time sphere-mesh rendering
- Dynamic camera:
  - Rotate, zoom, pan
  - Focus on any planet (Sun → Neptune)
- Visual legend
- Smooth lighting + rim‑light shading
- Ideal for Solar System playback

---

## 📁 Project Structure

```
earth-and-moon-orbits/
│
├── include/                 # Header files
│   ├── body.h
│   ├── simulation.h
│   ├── json_loader.h
│   ├── barycenter.h
│   ├── eclipse.h
│   ├── conservations.h
│   └── viewer/
│       ├── csv_loader.h
│       └── sphere_mesh.h
│
├── src/
│   ├── core/               # Physics engine
│   ├── io/                 # JSON + HORIZONS
│   ├── cli/                # Command-line interface
│   ├── viewer/             # OpenGL renderer
│   └── …
│
├── external/glad          # glad library
├── systems/               # JSON orbital systems
├── shaders/               # GLSL shaders
├── docs/                  # Technical documentation
├── plotting_scripts/      # Python analysis tools
├── orbit_sim_cli_reference.md  # CLI commands
└── README.md
```

---

## 🛠 Building

### Requirements
- C++17 compiler
- CMake ≥ 3.14
- OpenGL 3.3
- GLAD
- GLFW3
- GLM (OpenGL Mathematics)
- libcurl
- Python 3 (optional, for plotting)

### Build Steps

```bash
git clone https://github.com/eisensenpou/earth-and-moon-orbits.git
cd earth-and-moon-orbits
mkdir build && cd build
cmake ..
make -j
```

Executables appear in `build/bin/`:

- `orbit-sim` — main simulation engine  
- `orbit-viewer` — real‑time 3D visualization  

---

## ▶️ Running Simulations

### Simulate an orbital system

```bash
./orbit-sim run \
    --system ../systems/earth_moon.json \
    --steps 20000 \
    --dt 60 \
    --out ../results/out.csv
```

### Fetch HORIZONS ephemerides

```bash
./orbit-sim fetch \
    --target 399 \
    --center 0 \
    --start "2025-01-01" \
    --stop  "2025-01-02" \
    --step  "360m" \
    --out earth_ephem.txt
```

### Validate a system file

```bash
./orbit-sim info --system ../systems/earth_moon.json
```

---

## 🎥 3D Orbit Viewer

Launch with:

```bash
./orbit-viewer ../results/out.csv
```

### Controls
| Action | Input |
|-------|--------|
| Rotate | Right mouse drag |
| Zoom | Scroll wheel |
| Pan | Middle drag |
| Change focus | Click legend (Sun/Earth/Moon) |
| Hotkeys | `1`..`0` select Sun→Neptune |
| Reset camera | `R` |

Uses real planetary radii and optional distance‑compression scaling for visibility.

---

## 📊 Python Visualization Tools

From `plotting_scripts/`:

```bash
python3 plot_orbits.py ../results/out.csv
python3 animate_orbits.py
```

Includes:
- 2D orbit plots
- 3D matplotlib playback
- Animated GIF/MP4 generation
- Energy/momentum drift graphs

---

## 📐 Physical Model Summary

### Newtonian gravity
$$
\vec{F} = -G \frac{m_1 m_2}{r^3} \vec{r}
$$

### RK4 integration
$$
y_{n+1} = y_n + \frac{1}{6}(k_1 + 2k_2 + 2k_3 + k_4)
$$

### Conservation tracking
- Kinetic energy  
- Potential energy  
- Total energy drift  
- Linear & angular momentum  

### Eclipse detection
- Umbra / penumbra cones  
- Angular geometry tests  
- Line‑of‑sight visibility  

---

## 📐 Numerical Methods Summary

The simulation employs a fourth-order Runge-Kutta (RK4) integrator for state propagation:

- **State vector**: $\mathbf{y} = [\mathbf{r}, \mathbf{v}]^T$ for each body
- **Time step**: User-configurable, typically 60-3600 seconds
- **Accuracy**: $O(\Delta t^4)$ local truncation error
- **Stability**: Suitable for orbital timescales with appropriate step sizing
- **Conservation monitoring**: Energy, linear momentum, angular momentum tracked at each step

---

## 🧪 Validation Summary

### Conservation Law Monitoring
- Total energy drift: $|E(t) - E(0)|/E(0)$
- Linear momentum conservation: $|\mathbf{P}(t) - \mathbf{P}(0)|$
- Angular momentum conservation: $|\mathbf{L}(t) - \mathbf{L}(0)|$

### Ephemeris Comparison
- Direct comparison with NASA JPL HORIZONS data
- Position and velocity residuals
- Long-term orbital element drift analysis

### Barycenter Stability
- System barycenter tracking
- Numerical drift assessment
- Reference frame consistency

---

## 🛤 Limitations

### Numerical
- Fixed time step integration (no adaptive stepping)
- Long-term energy drift in multi-body systems
- Round-off error accumulation in extended simulations

### Physical Model
- Newtonian gravity only (no relativistic corrections)
- Point mass approximation (no oblateness or tidal effects)
- No perturbations from non-gravitational forces

### System Constraints
- Limited to modest N-body counts (performance considerations)
- No collision detection or handling
- Simplified eclipse geometry (no atmospheric effects)

---

## 🚀 Future Work

- Adaptive RK45 integrator implementation
- Barnes-Hut tree acceleration for large N
- GPU acceleration (CUDA/OpenCL)
- Relativistic corrections for high-precision applications
- Planetary oblateness and tidal effects
- Collision detection and handling
- In-viewer time controls and playback
- Planetary textures and enhanced rendering
- GUI overlay (ImGui integration)
- Ephemeris interpolation utilities

---

## 📜 License

MIT License

---

## 👨‍🚀 Author

**Sinan Can Demir**  
Aspiring Aerospace / Simulation Engineer  

GitHub: https://github.com/eisensenpou  
LinkedIn: https://linkedin.com/in/sinan-can-demir  

---