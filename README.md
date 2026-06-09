# Orbital Mechanics Engine

A high-performance **C++17 N-body gravitational simulator** with adaptive integration, real ephemeris support, and a Python API.

[![CI](https://github.com/sinan-can-demir/orbital-mechanics-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/sinan-can-demir/orbital-mechanics-engine/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## Features

- **Three integrators** — RK4 (fixed-step), Leapfrog (symplectic), and RK45 Dormand-Prince (adaptive timestep)
- **Full N-body Newtonian gravity** — arbitrary number of bodies, pairwise forces, Newton's third law
- **Conservation monitoring** — energy, linear momentum, and angular momentum tracked at every step
- **NASA JPL HORIZONS integration** — fetch real ephemeris data and build system files automatically
- **Eclipse detection** — umbra, penumbra, and antumbra cone geometry
- **Python API** — run simulations and access results as NumPy arrays with `pip install -e .`
- **OpenGL viewer** — real-time 3D orbit visualization with camera controls
- **CLI** — `orbit-sim run`, `validate`, `fetch`, `build-system`, and more

---

## Quick start

### Python API

```bash
git clone https://github.com/sinan-can-demir/orbital-mechanics-engine.git
cd orbital-mechanics-engine
mkdir build && cmake -S . -B build && cmake --build build --parallel
pip install -e . --no-build-isolation
```

```python
import orbit
import numpy as np

result = orbit.simulate('systems/solar_system.json', steps=8760, dt=3600.0)

pos = result.positions_numpy()   # shape: (n_steps, n_bodies, 3)  metres
E   = result.energies_numpy()    # shape: (n_steps,)               Joules

print(result.body_names)         # ['Sun', 'Mercury', 'Venus', ...]
print('Energy drift:', (E[-1] - E[0]) / abs(E[0]))  # ~1e-10
```

### CLI

```bash
./build/orbit-sim run \
    --system systems/solar_system.json \
    --integrator rk4 \
    --steps 8760 --dt 3600 \
    --output results/solar_system.csv
```

---

## Requirements

| Dependency | Version | Purpose |
|------------|---------|---------|
| C++ compiler | C++17 | Core simulation |
| CMake | ≥ 3.14 | Build system |
| Python | ≥ 3.10 | Python bindings |
| pybind11 | 2.13 | Fetched automatically via CMake |
| libcurl | any | HORIZONS ephemeris fetch |
| OpenGL / GLFW / GLM | 3.3 | Viewer (optional) |

pybind11 is downloaded automatically at configure time — no manual install needed.

---

## Build

### With Make (recommended)

```bash
make              # build everything (simulator + viewer + Python bindings)
make BUILD_VIEWER=0 build   # headless build (CI / no OpenGL)
make help         # list all available targets
```

### With CMake directly

```bash
mkdir build
cmake -S . -B build
cmake --build build --parallel
```

### Python bindings

```bash
pip install -e . --no-build-isolation
```

### Run the test suite

```bash
cd build && ctest --output-on-failure
```

---

## Python API

After `pip install -e .`, the `orbit` module is available system-wide.

```python
import orbit

# Run a simulation
result = orbit.simulate(path, steps, dt)

# Access results
result.body_names          # list of body names
result.body_masses         # list of masses in kg
result.snapshots           # list of SimulationSnapshot objects
result.positions_numpy()   # NumPy array (n_steps, n_bodies, 3)
result.energies_numpy()    # NumPy array (n_steps,)
```

See [examples/01_solar_system.ipynb](examples/01_solar_system.ipynb) for a full walkthrough.

---

## Examples

Jupyter notebooks are in `examples/`. Execute all cells with:

```bash
jupyter nbconvert --to notebook --execute examples/01_solar_system.ipynb \
    --output 01_solar_system.ipynb --output-dir examples/
```

| Notebook | Description |
|----------|-------------|
| [01_solar_system.ipynb](examples/01_solar_system.ipynb) | Full solar system — orbital trajectories and energy conservation |

---

## Repository layout

```
include/          public headers
src/
  core/           simulation, integrators, conservation
  cli/            orbit-sim command-line interface
  viewer/         OpenGL 3D viewer
orbit_py/         pybind11 Python bindings
systems/          JSON orbital system definitions
examples/         Jupyter notebook examples
tests/            C++ and Python test suites
docs/             architecture, validation, and roadmap
```

---

## HORIZONS system builder

Fetch real initial conditions from NASA JPL HORIZONS and build a system file:

```bash
make build-earth-moon       # fetch Earth + Moon from HORIZONS → systems/earth_moon_horizons.json
make build-solar-system     # fetch all 8 planets + Moon      → systems/solar_system_horizons.json
make pipeline-earth-moon    # fetch → simulate → open viewer in one step
```

---

## License

MIT — see [LICENSE](LICENSE).

---

## Author

**Sinan Can Demir**  
GitHub: [sinan-can-demir](https://github.com/sinan-can-demir)
