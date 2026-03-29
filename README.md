# 🌍 Orbital Mechanics Engine

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
orbital-mechanics-engine/
│
├── include/                 # Header files
│   ├── body.h
│   ├── simulation.h
│   ├── json_loader.h
│   ├── barycenter.h
│   ├── eclipse.h
│   ├── conservations.h
│   ├── nlohmann/            # JSON library
│   └── viewer/
│       ├── csv_loader.h
│       └── sphere_mesh.h
│
├── src/
│   ├── core/                # Physics engine (simulation, conservations, eclipse)
│   ├── io/                  # JSON loading + HORIZONS integration
│   ├── cli/                 # Command-line interface
│   └── viewer/              # OpenGL renderer
│
├── python/                  # Python visualization tools
│   ├── 3Dplot.py
│   ├── 3Dplot_earth_moon.py
│   ├── 3Dexaggerated_plot.py
│   ├── 3Draytracing.py
│   ├── angular_momentum_plot.py
│   └── energy_conservation.py
│
├── systems/                 # JSON orbital system definitions
│   ├── earth_moon.json
│   └── solar_system.json
│
├── docs/                    # Technical documentation
│   ├── architecture.md
│   ├── design-notes.md
│   ├── physics-and-methods.md
│   ├── validation.md
│   ├── ROADMAP.md
│   ├── orbital_mechanics_engine_cli_reference.md
│   └── debug/
│       └── debug_list.md
│
├── tests/                   # Test files
├── examples/                # Example scripts
│
├── Makefile                 # Build orchestration
├── CMakeLists.txt
├── viewer_config.json
│
├── .github/
│   └── workflows/
│       └── ci.yml           # CI pipeline
│
├── CONTRIBUTING.md
└── README.md
```

---

## 🛠 Building

### Requirements
- C++17 compiler
- CMake ≥ 3.14
- OpenGL 3.3 (optional, for viewer)
- GLAD
- GLFW3
- GLM (OpenGL Mathematics)
- libcurl
- Python 3 (optional, for plotting)

### Quick Start with Makefile

```bash
git clone https://github.com/eisensenpou/orbital-mechanics-engine.git
cd orbital-mechanics-engine
make
```

### Makefile Targets

| Target | Description |
|--------|-------------|
| `make build` | Build all executables |
| `make build-sim` | Build CLI only |
| `make build-viewer` | Build viewer only |
| `make clean` | Remove build directory |
| `make reconfigure` | Clean and reconfigure CMake |

**Simulation:**
| Target | Description |
|--------|-------------|
| `make run` | Run default simulation |
| `make run-earth-moon` | Run Earth-Moon simulation |
| `make run-solar-system` | Run full Solar System (100k steps) |

**Viewer:**
| Target | Description |
|--------|-------------|
| `make view` | Launch viewer with default output |
| `make view-last` | Launch viewer with Earth-Moon results |

**Utilities:**
| Target | Description |
|--------|-------------|
| `make validate` | Validate system file |
| `make test` | Quick validation test |
| `make fetch` | Fetch HORIZONS ephemeris |

**Python Plotting:**
| Target | Description |
|--------|-------------|
| `make plot` | Generate all plots |
| `make plot-energy` | Energy conservation plot |
| `make plot-momentum` | Angular momentum plot |
| `make plot-3d` | 3D orbit visualization |
| `make plot-3d-exaggerated` | 3D with exaggerated scale |

### Configuration Options

```bash
# Build without OpenGL viewer (headless/CI)
make BUILD_VIEWER=0 build

# Pass custom CMake flags
make CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Debug" build
```

### Manual Build Steps

```bash
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

From `python/`:

```bash
python3 python/3Dplot.py ../results/out.csv
python3 python/3Draytracing.py
```

Includes:
- 3D orbit plots with matplotlib
- 3D raytracing visualization
- Angular momentum plotting
- Energy conservation analysis
- Exaggerated scale 3D views

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

## 🔄 Continuous Integration

The project uses GitHub Actions for CI on every push and pull request.

**CI Pipeline:**
1. **Lint** — Code formatting check (clang-format on changed files)
2. **Build** — Compile release binary
3. **Test** — Validate system files and run test simulations

**Workflow:** `.github/workflows/ci.yml`

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