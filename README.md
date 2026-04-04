# 🌍 Orbital Mechanics Engine

A modern **C++17 N-body orbital mechanics simulator** with:
- **RK4 integration** for Newtonian gravity
- **OpenGL viewer** for 3D orbit visualization
- **NASA JPL HORIZONS** ephemeris import
- **Eclipse detection** and conservation monitoring

---

## 🚀 What it does

This repository simulates gravitational systems of planets, moons, and custom bodies using a fixed-step Runge-Kutta 4 integrator.
It supports JSON-defined system configurations, exports CSV trajectories, and provides tools for real-world ephemeris comparison and Python plotting.

The engine is built to be:
- accurate enough for short-term orbital propagation
- modular for new system definitions and validation
- easy to run from the provided `Makefile`

---

## ✨ Key features

- **N-body Newtonian gravity**
- **Fixed-step RK4 integration**
- **Energy, linear momentum, and angular momentum tracking**
- **Barycenter utilities**
- **Umbra / penumbra eclipse detection**
- **JSON system files** for solar system, Earth-Moon, and custom scenarios
- **HORIZONS fetch / system-builder** support
- **OpenGL orbit viewer** with camera controls
- **Python plotting scripts** for analysis and visualization

---

## 📁 Repository layout

- `include/` — public headers and utilities
- `src/` — core simulation, I/O, CLI, and viewer code
- `systems/` — JSON orbital system definitions
- `results/` — generated simulation outputs and data
- `python/` — plotting and analysis scripts
- `tests/` — test suites
- `Makefile` — project automation and build targets
- `CMakeLists.txt` — CMake build configuration

---

## 🛠 Requirements

- C++17 compiler
- CMake ≥ 3.14
- OpenGL 3.3 (for viewer)
- `glad`, `glfw`, `glm`
- `libcurl`
- Python 3 (optional, for plotting)

---

## 🚧 Build

### Standard build

```bash
git clone https://github.com/sinan-can-demir/orbital-mechanics-engine.git
cd orbital-mechanics-engine
make
```

### Build only the simulator

```bash
make build-sim
```

### Build only the viewer

```bash
make build-viewer
```

### Build without the OpenGL viewer

```bash
make BUILD_VIEWER=0 build
```

### Manual CMake build

```bash
mkdir -p build && cd build
cmake -S .. -B . -DBUILD_VIEWER=ON
cmake --build . --parallel
```

---

## ▶️ Run simulations

### Default simulation

```bash
make run
```

### Earth-Moon simulation

```bash
make run-earth-moon
```

### Solar System simulation

```bash
make run-solar-system
```

### Validate a system file

```bash
make validate
```

### Quick test

```bash
make test
```

---

## 🎥 Viewer

Launch the viewer with default output:

```bash
make view
```

View the most recent simulation output:

```bash
make view-last
```

You can also run the viewer script directly:

```bash
./utils/view.sh
```

---

## 🌌 HORIZONS and system generation

Fetch ephemeris data from JPL HORIZONS using:

```bash
make fetch
```

Generate JSON system files from HORIZONS data:

```bash
make build-earth-moon
make build-solar-system
```

Run a full Earth-Moon pipeline and open the viewer:

```bash
make pipeline-earth-moon
```

---

## 📊 Python visualization

The `python/` folder contains plotting scripts for analysis and visualization.

```bash
make plot
make plot-energy
make plot-momentum
make plot-3d
make plot-3d-exaggerated
make plot-3d-earth-moon
```

---

## ✅ Helpful make targets

- `make clean` — remove the build directory
- `make reconfigure` — recreate build files
- `make format` — run `clang-format` on source files
- `make format-check` — verify formatting

---

## 📜 License

This project is licensed under the MIT License.

---

## 👨‍🚀 Author

**Sinan Can Demir**

GitHub: https://github.com/sinan-can-demir
