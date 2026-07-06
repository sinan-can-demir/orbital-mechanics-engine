# Orbital Mechanics Engine

A high-performance **C++17 N-body gravitational simulator** with adaptive integration, real ephemeris support, and a Python API.

[![CI](https://github.com/sinan-can-demir/orbital-mechanics-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/sinan-can-demir/orbital-mechanics-engine/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.21215316.svg)](https://doi.org/10.5281/zenodo.21215316)

---

## Features

- **Five integrators** — RK4 (fixed-step), Leapfrog (symplectic), Yoshida4 (4th-order symplectic), RK45 Dormand-Prince (adaptive timestep), and Hermite (4th-order predictor-corrector)
- **Full N-body Newtonian gravity** — arbitrary number of bodies, pairwise forces, Newton's third law
- **Conservation monitoring** — energy, linear momentum, and angular momentum tracked at every step
- **NASA JPL HORIZONS integration** — fetch real ephemeris data and build system files automatically
- **Eclipse detection** — umbra, penumbra, and antumbra cone geometry
- **Python API** — run simulations and access results as NumPy arrays with `pip install -e .`
- **OpenGL viewer** — real-time 3D orbit visualization with camera controls
- **CLI** — `orbit-sim run`, `validate`, `fetch`, `build-system`, and more

---

## Why this project?

Orbital Mechanics Engine is designed for students, educators, and researchers who want to simulate gravitational systems and experiment with different integration methods. Unlike black-box solvers, swapping integrators requires changing a single argument — making it easy to compare accuracy, performance, and stability across RK4, Leapfrog, Yoshida4, RK45, and Hermite without rewriting any simulation code.

## Who is this for?

| If you want to... | Use this |
|---|---|
| Run a simulation and plot results in Python | `orbit.simulate()` + Jupyter notebooks in `examples/` |
| Compare integrator accuracy side by side | `orbit.Integrator.RK4` / `Leapfrog` / `Yoshida4` / `RK45` / `Hermite` — one argument |
| Simulate with real planetary positions from NASA | `make build-solar-system` then `orbit.simulate()` |
| Watch the simulation in real time | `make build` + `make view` (OpenGL viewer) |
| Run from the command line without Python | `./build/bin/orbit-sim run --system systems/solar_system.json` |
| Validate a custom system file | `./build/bin/orbit-sim validate --system your_system.json` |

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
./build/bin/orbit-sim run \
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
make help         # list all available targets with descriptions
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
cmake --build build --parallel   # ensure all targets including tests are built
cd build && ctest --output-on-failure
```

---

## Python API

After `pip install -e .`, the `orbit` module is available system-wide.

```python
import orbit

# Fixed-step simulation (RK4, Leapfrog, or RK45)
result = orbit.simulate(path, steps, dt, integrator=orbit.Integrator.RK4)

# Adaptive RK45 simulation
result = orbit.simulate_adaptive(path, duration_s, dt_initial)

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

# or run all notebooks at once
for nb in examples/*.ipynb; do
    jupyter nbconvert --to notebook --execute "$nb" \
        --output "$(basename $nb)" --output-dir examples/
done
```

| Notebook | Description |
|----------|-------------|
| [01_solar_system.ipynb](examples/01_solar_system.ipynb) | Full solar system — orbital trajectories and energy conservation |
| [02_earth_moon.ipynb](examples/02_earth_moon.ipynb) | Earth-Moon system with real NASA HORIZONS data — lunar orbit and perigee/apogee |
| [03_integrator_comparison.ipynb](examples/03_integrator_comparison.ipynb) | RK4 vs Leapfrog vs Yoshida4 vs RK45 vs Hermite — energy drift side-by-side |
| [04_rk45_adaptive.ipynb](examples/04_rk45_adaptive.ipynb) | RK45 adaptive timestep history — how the integrator responds to orbital dynamics |
| [05_rebound_earth_moon.ipynb](examples/05_rebound_earth_moon.ipynb) | Earth-Moon (3-body) vs REBOUND's IAS15 and WHFast — wall-clock time and energy drift |
| [06_rebound_solar_system.ipynb](examples/06_rebound_solar_system.ipynb) | Full solar system (10-body) vs REBOUND — same benchmarks at larger N |
| [07_spice_validation.ipynb](examples/07_spice_validation.ipynb) | Validation against DE440 SPICE ephemerides |
| [08_yoshida4.ipynb](examples/08_yoshida4.ipynb) | Yoshida4 symplectic integrator — long-term energy conservation |
| [09_mercury_gr_precession.ipynb](examples/09_mercury_gr_precession.ipynb) | Mercury perihelion precession — Schwarzschild 1PN GR correction validation |

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

## OpenGL Viewer

The engine includes a real-time 3D orbital viewer built with OpenGL. After running a simulation, launch it with:

```bash
make view          # view default simulation output
make view-last     # view most recent simulation output
```

The viewer renders all bodies with mass-proportional sizes and hash-based colors. Camera controls:

| Key | Action |
|-----|--------|
| Mouse drag | Rotate camera |
| Scroll | Zoom in/out |
| `1`–`9` | Focus on body by index |
| `Space` | Pause / resume |
| `+` / `-` | Speed up / slow down |
| `R` | Reset to frame 0 |

> The viewer requires OpenGL 3.3. Build without it on headless systems with `make BUILD_VIEWER=0 build`.

---

## HORIZONS system builder

Fetch real initial conditions from NASA JPL HORIZONS and build a system file:

```bash
make build-earth-moon       # fetch Earth + Moon from HORIZONS → systems/earth_moon_horizons.json
make build-solar-system     # fetch all 8 planets + Moon      → systems/solar_system_horizons.json
make pipeline-earth-moon    # fetch → simulate → open viewer in one step
```

---

## Contributing

Contributions are welcome — new integrators, system files, notebooks, bug fixes, or documentation improvements. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on setup, coding style, and physics accuracy requirements.

For planned and potential future directions, see [docs/FUTURE.md](docs/FUTURE.md).

---

## License

MIT — see [LICENSE](LICENSE).

---

## Physics references

- Vallado, D.A. *Fundamentals of Astrodynamics and Applications*, 4th ed.
- Dormand, J.R. & Prince, P.J. (1980). *A family of embedded Runge-Kutta formulae*. Journal of Computational and Applied Mathematics.
- Hairer, E., Nørsett, S.P. & Wanner, G. *Solving Ordinary Differential Equations I*, 2nd ed.
- Murray, C.D. & Dermott, S.F. *Solar System Dynamics*. Cambridge University Press.
- NASA JPL HORIZONS system: https://ssd.jpl.nasa.gov/horizons/

## AI assistance

This project was developed with AI assistance (Claude, Anthropic) for implementation details, debugging, and architecture discussions. All physics decisions, architectural choices, scientific validation, and the paper's Statement of Need are the author's own work.

---

## Author

**Sinan Can Demir**  
GitHub: [sinan-can-demir](https://github.com/sinan-can-demir)
