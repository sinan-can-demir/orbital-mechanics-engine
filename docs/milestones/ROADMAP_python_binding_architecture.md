# Roadmap — Python Binding Architecture and Design

*Turning the current C++ simulation engine into a clean Python-facing library
with pybind11, NumPy-backed results, packaging, notebooks, and CI coverage.*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026  
**GitHub**: https://github.com/sinan-can-demir/orbital-mechanics-engine

---

## Motivation

Phase 3 is not just "add Python bindings." It is the point where the project
must start behaving like a reusable scientific library instead of only a CLI
tool that writes CSV files.

The target audience for JOSS is not primarily people who want to shell out to
`orbit-sim`. It is researchers, students, and notebook users who want:

1. `import orbit`
2. Run a simulation from Python
3. Get trajectories and diagnostics directly as arrays
4. Plot, compare, and validate results inside a notebook

That means the architecture has to support:

1. In-memory simulation results
2. Stable ownership and lifetimes across the C++/Python boundary
3. Packaging with `pip install -e .`
4. Reproducible notebooks and Python tests

If this phase is done well, the Python API becomes the public face of the
project. If it is done poorly, the bindings will become a thin layer over CLI
behavior and will be hard to maintain.

---

## Current State (Honest Baseline)

| Component | Status | Notes |
|-----------|--------|-------|
| `orbit_core` static library | ✅ Working | Core physics code already isolated in CMake |
| CLI executable | ✅ Working | `orbit-sim` drives most workflows |
| JSON system loading | ✅ Working | Reusable entry point for Python |
| Conservation calculations | ✅ Working | Already present in core |
| Python plotting scripts | ✅ Partial | Useful examples, but not an importable API |
| Python package | ❌ Missing | No `pyproject.toml`, no wheel/editable install |
| pybind11 integration | ❌ Missing | No extension module target |
| In-memory result object | ❌ Missing | Core simulation currently writes files |
| NumPy zero-copy exposure | ❌ Missing | No array view layer exists yet |
| Python tests in CI | ❌ Missing | CI is C++-centric today |
| Executable notebooks in CI | ❌ Missing | Required for research-facing polish |

### What the codebase currently implies

The biggest architectural constraint is that `runSimulation()` is currently
file-oriented:

```cpp
void runSimulation(std::vector<CelestialBody>& bodies, int steps, double dt,
                   const std::string& outputPath,
                   Integrator integrator = Integrator::RK4,
                   int stride = 1);
```

This is good for the CLI and viewer pipeline, but not good enough for Python.
Python needs a result object that owns trajectory and diagnostic data in memory,
and optional exporters that can write those results to disk afterward.

### Core risks going into this phase

1. Binding the current file-writing API directly instead of refactoring first
2. Letting Python-visible objects expose mutable internal simulation state
3. Mixing CLI concerns, export concerns, and simulation concerns in one function
4. Spending too much time on notebook polish before the C++ API is stable
5. Treating packaging as an afterthought instead of part of the design

---

## Phase 3 Design Goals

The Python API should satisfy these design goals:

### 1. Python feels native

This should feel like a Python library, not a subprocess wrapper:

```python
import orbit

sim = orbit.Simulation(
    config="systems/earth_moon.json",
    integrator="rk4",
    dt=60.0,
)

result = sim.run(duration_days=30)

positions = result.positions_numpy()
energies = result.energies_numpy()
names = result.body_names()
```

### 2. C++ remains the source of truth

The simulation engine stays in C++. Python is a binding layer, not a re-write.

### 3. File export becomes optional

The engine should be able to:

1. simulate and keep results in memory
2. optionally export CSV from those results
3. optionally feed the viewer using exported output

### 4. Ownership rules are explicit

Returned NumPy arrays must remain valid as long as the owning C++ result object
is alive. No dangling views, no hidden copies unless explicitly requested.

### 5. The architecture supports future work

Phase 3 should not block:

1. RK45 integration
2. richer diagnostics
3. HORIZONS-driven notebook examples
4. future wheels or binary distribution

---

## Target Architecture

The cleanest architecture for this repo is:

```text
Python notebooks / tests
        |
     orbit (pybind11 module)
        |
  C++ public library API
        |
  simulation engine + loaders + diagnostics
        |
 optional exporters (CSV, eclipse log, viewer feed)
```

### New separation of responsibilities

#### 1. Simulation engine

Responsible for:
- state evolution
- integrator dispatch
- diagnostics calculation
- producing trajectory snapshots

Not responsible for:
- opening files
- printing CLI-oriented messages
- deciding CSV filenames

#### 2. Result object

Responsible for owning:
- body names
- time samples
- positions
- velocities
- energies
- momentum / angular momentum if desired
- eclipse events if enabled

This becomes the stable contract used by both Python and optional exporters.

#### 3. Export layer

Responsible for:
- writing trajectory CSV
- writing conservation CSV
- writing eclipse CSV

This should consume a `SimulationResult` instead of embedding file I/O inside
the stepping loop.

#### 4. Python binding layer

Responsible for:
- exposing `Simulation`
- exposing `SimulationResult`
- converting enums/options
- exposing NumPy views
- documenting lifetime behavior

---

## Proposed C++ API Refactor

Before adding pybind11, introduce a C++ library API that Python can bind
cleanly.

### New core types

#### `SimulationOptions`

Suggested fields:

```cpp
struct SimulationOptions {
    Integrator integrator = Integrator::RK4;
    double dt = 60.0;
    int stride = 1;
    bool record_positions = true;
    bool record_velocities = false;
    bool record_conservation = true;
    bool detect_eclipses = true;
};
```

Purpose:
- remove argument sprawl from `runSimulation(...)`
- make defaults explicit
- give Python a stable options surface

#### `SimulationResult`

Suggested responsibilities:

```cpp
struct SimulationResult {
    std::vector<std::string> body_names;
    std::vector<double> time_s;
    std::vector<double> positions;   // flattened: step × body × xyz
    std::vector<double> velocities;  // flattened: step × body × xyz
    std::vector<double> energies;
    std::vector<double> linear_momentum;
    std::vector<double> angular_momentum;
};
```

Notes:
- flattened contiguous `std::vector<double>` is ideal for NumPy views
- shape metadata should be derived from `n_steps` and `n_bodies`
- avoid storing nested `std::vector<std::vector<...>>` for Python-facing arrays

#### `Simulation`

Possible C++ shape:

```cpp
class Simulation {
public:
    Simulation(std::vector<CelestialBody> bodies, SimulationOptions options);
    SimulationResult run_steps(int steps);
    SimulationResult run_duration(double duration_s);
};
```

This does not have to match the exact internal implementation, but conceptually
the project needs a reusable object or function family with the same separation.

### What should happen to `runSimulation()`

Do not delete it immediately. Convert it into a compatibility wrapper:

1. create a `SimulationResult`
2. export it to CSV
3. preserve current CLI behavior

That keeps the CLI stable while the Python-oriented API becomes the real core.

---

## Python Module Design

### Module name

Use a short module import:

```python
import orbit
```

Avoid exposing implementation names like `_orbit_core` except as private module
internals if needed.

### Python-facing classes

#### `orbit.Simulation`

Constructor inputs:
- `config: str`
- `integrator: str = "rk4"`
- `dt: float = 60.0`
- `stride: int = 1`

Methods:
- `run(duration_days: float) -> SimulationResult`
- optional: `run_seconds(duration_s: float) -> SimulationResult`

Validation behavior:
- invalid config path raises `ValueError` or `RuntimeError`
- invalid integrator string raises `ValueError`
- negative `dt` or duration raises `ValueError`

#### `orbit.SimulationResult`

Methods:
- `positions_numpy()`
- `velocities_numpy()` if recorded
- `energies_numpy()`
- `time_numpy()`
- `body_names()`

Properties can also work, but methods are acceptable if consistent.

### NumPy exposure rules

For zero-copy views:

1. `SimulationResult` owns the contiguous buffers
2. pybind11 returns `py::array` with:
   - correct shape
   - correct strides
   - base object tied to the owning `SimulationResult`
3. array lifetime must depend on the result object, not temporary wrappers

For positions:
- shape: `(n_steps, n_bodies, 3)`
- dtype: `float64`

For energies:
- shape: `(n_steps,)`

For time:
- shape: `(n_steps,)`

### Mutability policy

Returned arrays should be read-only by default if feasible. At minimum, Python
must not be able to mutate live in-progress integrator state. Mutating a result
buffer after the run is less dangerous, but read-only buffers are cleaner.

---

## Packaging and Build Design

### Build backend

Use `scikit-build-core` with CMake.

Reason:
- it aligns with the existing CMake project
- it is the least disruptive path to editable installs
- it keeps one build system instead of inventing another

### New files expected

1. `pyproject.toml`
2. Python package directory such as `orbit/`
3. binding sources, likely under `src/python/` or `python_bindings/`

### CMake requirements

Add:

1. `FetchContent` for `pybind11`
2. an extension target for the Python module
3. include paths for core headers
4. link against `orbit_core`

### Important build decision

Do not make the Python module depend on viewer/OpenGL components.

The Python package should build the scientific core only. Viewer support should
remain optional and independent.

### Distribution goal for this phase

Minimum acceptable:
- `pip install -e .` works locally
- `python -c "import orbit"` works in CI

Nice-to-have later:
- wheel builds
- manylinux/macOS distribution

---

## Notebook Strategy

The notebooks are not decoration. For JOSS they are evidence that the software
is useful to its target audience.

### Notebook requirements

Each notebook should:

1. install cleanly against the editable package
2. run top-to-bottom without manual edits
3. produce at least one meaningful plot or validation output
4. avoid hidden data dependencies outside the repo

### Recommended execution principle

Keep notebooks thin. Put real plotting and helper logic in importable Python
modules so the notebook is mostly:

1. setup
2. run simulation
3. plot
4. interpret result

This reduces notebook fragility and makes CI execution far easier.

### Suggested notebook order

#### Notebook 1: Two-body basics

Goal:
- prove the API is simple
- validate period and energy behavior

#### Notebook 2: Earth-Moon from bundled config

Goal:
- show realistic data flow
- demonstrate frame transforms and analysis

#### Notebook 3: Solar system

Goal:
- show scale and stability
- test plotting and performance on a larger system

#### Notebook 4: Integrator comparison

Goal:
- demonstrate scientific value
- compare RK4, Leapfrog, and later RK45 once it exists

Important:
The roadmap in `docs/ROADMAP.md` mentions `rk45`, but if RK45 is not complete,
the notebook should be sequenced after the integrator exists or temporarily
scoped to RK4 vs Leapfrog.

---

## Test Strategy

Phase 3 should add Python tests at three levels.

### 1. Smoke tests

Examples:
- import succeeds
- version string exists
- module exposes expected classes

### 2. Behavior tests

Examples:
- Earth-Moon run returns arrays of expected shape
- body names match the system configuration
- conservation arrays have the expected length

### 3. Scientific sanity tests

Examples:
- energy drift remains below a defined threshold for a short benchmark run
- center of mass remains near expected behavior if normalization is enabled

### 4. Notebook execution tests

At least one notebook should run in CI early, then expand to all notebooks once
execution time is under control.

### Recommended CI progression

Stage 1:
- `pytest tests/test_python_api.py`

Stage 2:
- execute `examples/01_two_body.ipynb`

Stage 3:
- execute all notebooks on push or on a scheduled workflow if runtime is heavy

---

## Milestone Breakdown

## Milestone 0 — API Boundary Refactor

*Goal: separate simulation, result storage, and file export before binding Python.*  
*Estimated effort: 1–2 days*

### Deliverables

1. Introduce `SimulationOptions`
2. Introduce `SimulationResult`
3. Refactor simulation loop to fill in-memory buffers
4. Move CSV writing into exporter helpers
5. Preserve current CLI behavior through wrappers

### Definition of done

1. CLI still produces the same outputs
2. core code can run without writing files during integration
3. tests still pass

### Why this milestone matters

This is the highest-leverage step in the entire phase. If skipped, every later
step becomes harder.

---

## Milestone 1 — pybind11 Integration

*Goal: build a minimal importable Python extension linked against the core.*  
*Estimated effort: 0.5–1 day*

### Deliverables

1. Add `pybind11` via `FetchContent`
2. Add extension target
3. Expose `__version__`
4. Expose a trivial function or enum as a smoke test

### Definition of done

```bash
pip install -e .
python3 -c "import orbit; print(orbit.__version__)"
```

---

## Milestone 2 — Bind `Simulation` and `SimulationResult`

*Goal: expose the first useful Python workflow.*  
*Estimated effort: 1–2 days*

### Deliverables

1. `orbit.Simulation(config=..., integrator=..., dt=...)`
2. `result = sim.run(duration_days=...)`
3. `result.body_names()`
4. `result.time_numpy()`
5. `result.positions_numpy()`
6. `result.energies_numpy()`

### Design constraints

1. No subprocess calls
2. No file output required
3. No mutable access to live simulation state

### Definition of done

A user can reproduce:

```python
import orbit
sim = orbit.Simulation("systems/earth_moon.json", dt=60.0)
result = sim.run(duration_days=1)
assert result.positions_numpy().shape[2] == 3
```

---

## Milestone 3 — Python Package Layout and Helper Modules

*Goal: make the package usable beyond the raw extension module.*  
*Estimated effort: 0.5–1 day*

### Deliverables

1. package directory with clean import surface
2. helper plotting/utilities modules where appropriate
3. clear package metadata in `pyproject.toml`
4. dependency declaration for NumPy and Matplotlib

### Definition of done

1. editable install works from repo root
2. import path is stable
3. helper functions can be reused by notebooks

---

## Milestone 4 — Python Tests

*Goal: lock the new API down before building marketing notebooks on top of it.*  
*Estimated effort: 0.5–1 day*

### Deliverables

1. `tests/test_python_api.py`
2. import smoke test
3. Earth-Moon shape test
4. basic conservation sanity test

### Extra recommendation

Add one failure-path test:
- invalid config path raises a clean Python exception

---

## Milestone 5 — Notebook Foundation

*Goal: create the first notebook and prove end-to-end usability.*  
*Estimated effort: 1 day*

### Deliverables

1. `examples/01_two_body.ipynb`
2. helper plotting code extracted into importable modules
3. notebook execution command documented

### Definition of done

```bash
jupyter nbconvert --to notebook --execute examples/01_two_body.ipynb
```

---

## Milestone 6 — Full Example Suite

*Goal: ship the research-facing examples required by Phase 3.*  
*Estimated effort: 2–3 days*

### Deliverables

1. `01_two_body.ipynb`
2. `02_earth_moon.ipynb`
3. `03_solar_system.ipynb`
4. `04_integrator_comparison.ipynb`

### Important sequencing note

If RK45 is not implemented yet, do not fake it in the notebook. Either:

1. defer the RK45 comparison until the integrator exists, or
2. explicitly scope the notebook to currently implemented integrators

Scientific credibility matters more than checking a box.

---

## Milestone 7 — CI and Clean Install Validation

*Goal: make the Python workflow reproducible on a clean machine.*  
*Estimated effort: 0.5–1 day*

### Deliverables

1. CI job for editable install
2. CI job for Python tests
3. CI execution of at least the first notebook
4. README updates for Python usage

### Definition of done

A new contributor can follow the docs and get:

1. successful package install
2. successful import
3. successful example execution

---

## Recommended File and Directory Layout

One reasonable target layout:

```text
orbit/
  __init__.py
  plotting.py
  analysis.py

src/python/
  module.cpp
  py_simulation.cpp
  py_result.cpp

include/
  simulation.h
  simulation_result.h
  simulation_options.h
  exporters.h

tests/
  test_python_api.py

examples/
  01_two_body.ipynb
  02_earth_moon.ipynb
  03_solar_system.ipynb
  04_integrator_comparison.ipynb

pyproject.toml
```

The exact layout can vary, but the principle should not:
- C++ binding code stays separate from scientific helper Python code
- notebooks do not become the primary implementation location

---

## Non-Goals for This Phase

To keep the phase bounded, avoid expanding scope into:

1. GPU acceleration
2. full wheel publishing for every platform
3. a large pure-Python analysis framework
4. viewer embedding into Python
5. a second independent simulation engine in Python

Those can come later. Phase 3 should produce a clean, reliable, importable core.

---

## Open Questions to Resolve Early

These should be decided before implementation gets deep:

1. Should `Simulation` load from config path only, or also accept Python-side
   body definitions?
2. Will results always record positions and energies, or should recording be
   configurable for memory control?
3. Should Python arrays be strictly read-only?
4. Will eclipse outputs be part of `SimulationResult` now or added later?
5. Does Phase 3 assume RK45 exists, or should that dependency be made explicit?

The most important of these is the last one. The roadmap in `docs/ROADMAP.md`
currently implies RK45 in the Python API examples, but the codebase does not yet
expose that integrator. This dependency should be documented clearly.

---

## Recommended Order of Execution

If implemented in the safest order, Phase 3 should proceed as:

1. Refactor the C++ core around `SimulationResult`
2. Add pybind11 and import smoke test
3. Bind a minimal `Simulation` workflow
4. Add Python tests
5. Add package metadata and editable install workflow
6. Build the first notebook
7. Expand to the full notebook suite
8. Add CI execution for Python and notebooks

This order minimizes rework. Doing notebooks before stabilizing the result API
will create churn.

---

## Definition of Done for Phase 3

Phase 3 should be considered complete only when all of the following are true:

1. `pip install -e .` succeeds on a clean machine
2. `import orbit` succeeds without manual path hacks
3. a user can run a simulation directly from Python
4. trajectories and diagnostics are accessible as NumPy arrays
5. the CLI still works as before
6. Python tests pass in CI
7. notebooks execute end-to-end without errors
8. the README documents the Python workflow clearly

---

## Final Assessment

This phase is substantial, but it is not too large for the current library.
The codebase is still compact enough that a good architectural refactor now will
pay off quickly.

The real difficulty is not binding C++ to Python. The real difficulty is
choosing the right internal API so that Python, CLI, tests, and notebooks all
sit on top of the same clean simulation core.

That is the design standard this roadmap is aiming for.
