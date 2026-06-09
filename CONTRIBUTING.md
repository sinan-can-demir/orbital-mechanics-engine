# Contributing to Orbital Mechanics Engine

Contributions of all kinds are welcome — new integrators, system files, Jupyter notebooks, bug fixes, and documentation improvements.

---

## Getting started

```bash
git clone https://github.com/sinan-can-demir/orbital-mechanics-engine.git
cd orbital-mechanics-engine
git checkout -b feature/your-feature-name
```

### Build

```bash
make                              # build everything
make BUILD_VIEWER=0 build         # headless build (no OpenGL required)
make python-check                 # install Python bindings + run pytest
cd build && ctest --output-on-failure  # run C++ tests
```

### Dependencies

| Platform | Command |
|----------|---------|
| Ubuntu/Debian | `sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libglfw3-dev libglm-dev` |
| macOS | `brew install cmake curl glfw glm` |

Python bindings require Python ≥ 3.10. pybind11 is fetched automatically by CMake.

---

## Project structure

```
src/core/       physics engine (simulation, integrators, conservation)
src/cli/        orbit-sim command-line interface
src/viewer/     OpenGL 3D viewer
src/io/         JSON loader, HORIZONS fetch, validation
include/        public headers
orbit_py/       pybind11 Python bindings (bindings.cpp)
tests/          C++ tests (CTest) + Python tests (pytest)
examples/       Jupyter notebooks
systems/        JSON orbital system definitions
```

---

## Adding a new integrator

1. Add the integrator name to the `Integrator` enum in `include/simulation.h`
2. Implement the step function in `src/core/simulation.cpp`
3. Add a `case` for it in the `runSimulationCore()` loop
4. Expose it in `orbit_py/bindings.cpp` via `py::enum_<Integrator>`
5. Add a test in `tests/` verifying energy conservation

---

## Code style

- C++17, `clang-format` enforced — run `make format` before committing
- SI units throughout (metres, kilograms, seconds)
- No raw pointers — use `std::vector`, automatic storage
- Functions should have a single responsibility

---

## Testing requirements

All contributions must pass CI:

```bash
make format-check          # lint
make BUILD_VIEWER=0 build  # C++ build
cd build && ctest          # C++ tests
make python-check          # Python bindings + pytest
```

Physics contributions must maintain:
- Energy conservation: relative drift < 10⁻⁵ over 10k steps
- Momentum conservation: relative drift < 10⁻⁸

---

## Commit message format

```
type(scope): description
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `style`

Examples:
```
feat(core): add Barnes-Hut tree for large N-body systems
fix(viewer): resolve crash on window resize
docs(readme): add macOS build instructions
```

---

## Pull request checklist

- [ ] `make format-check` passes
- [ ] `ctest` passes
- [ ] `make python-check` passes
- [ ] New features include a test
- [ ] README updated if behaviour changed

---

## Bug reports

Open an issue and include: OS, compiler, CMake version, exact commands to reproduce, and expected vs actual behaviour.

## Questions

Open a GitHub issue — happy to help you get started.
