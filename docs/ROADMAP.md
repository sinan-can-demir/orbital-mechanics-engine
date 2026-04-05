# Roadmap — Orbital Mechanics Engine

*High-accuracy C++17 N-body gravitational simulator*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026  
**GitHub**: https://github.com/sinan-can-demir/orbital-mechanics-engine  
**Target**: JOSS submission at v2.0

---

## Current State (v1.2 — Post Visualization Refactor)

| Component | Status | Notes |
|-----------|--------|-------|
| RK4 integrator | ✅ Working | Fixed timestep, `simulation.cpp` |
| Leapfrog integrator | ✅ Working | Symplectic, `simulation.cpp` |
| Pairwise N-body gravity | ✅ Working | O(N²), Newton's 3rd law respected |
| Conservation monitoring | ✅ Working | Energy, linear momentum, angular momentum |
| JSON system loader | ✅ Working | Arbitrary N-body configs via `systems/*.json` |
| CLI (`orbit-sim`) | ✅ Working | `run`, `list`, `info`, `validate`, `fetch`, `build-system`, `help` |
| NASA HORIZONS fetch | ✅ Working | GET and POST modes via `libcurl` |
| HORIZONS system builder | ✅ Working | `build-system` command, auto-parses vectors + mass lookup |
| OpenGL viewer | ✅ Working | Generic N-body, auto-scaling, hash colors, click-to-focus |
| Eclipse detection | ✅ Working | Umbra / penumbra / antumbra cone geometry |
| Barycenter normalization | ✅ Working | `--normalize` flag |
| Split CSV output | ✅ Working | Positions file + conservation file, ~80x smaller |
| CSV metadata comment | ✅ Working | stride, dt, mass per body in header |
| Generic viewer scaling | ✅ Working | Auto dist scale, mass-based radius, hash color |
| Python plotting | ✅ Working | `utils.py`, `energy_conservation.py`, `angular_momentum_plot.py`, `compare_integrators.py` |
| CI pipeline | ✅ Working | Lint, build, validate, test on every push |
| test_conservation | ✅ Working | Energy drift < 1e-5 over 10k steps |
| test_horizons_parser | ✅ Working | SI unit conversion validated |
| test_system_writer | ✅ Working | JSON round-trip validated |

**Known limitations before JOSS:**
- No adaptive timestep — fixed dt only (RK45 planned in Phase 1)
- No Python API — researchers must use CLI + CSV (planned in Phase 3)
- No HORIZONS validation study — accuracy unquantified (planned in Phase 2)
- No Jupyter notebooks — no working examples for target audience (planned in Phase 3)
- No `paper.md` — JOSS submission file not written (planned in Phase 5)
- Test suite too small — JOSS expects broader coverage (expand in each phase)
- `solar_system.json` uses approximate circular orbits, not real ephemeris

---

## JOSS Submission Checklist

Track these as hard requirements. Do not submit until all are green.

| Requirement | Status | Phase |
|-------------|--------|-------|
| Open source license (MIT) | ✅ Done | — |
| Substantial functionality | ✅ Done | — |
| Community guidelines (CONTRIBUTING.md) | ✅ Done | — |
| Passing test suite (`ctest`) | ⚠️ Partial | 1, 2, 3 |
| HORIZONS validation study | ❌ Missing | 2 |
| Python API (pybind11) | ❌ Missing | 3 |
| Jupyter notebook examples | ❌ Missing | 3 |
| Statement of need | ❌ Missing | 5 |
| `paper.md` | ❌ Missing | 5 |
| Clean install from README in < 10 min | ⚠️ Untested | 4 |
| All README commands work exactly as written | ⚠️ Untested | 4 |

---

## Phase 1 — RK45 Adaptive Integrator (Weeks 1–6)

*Most important technical addition before JOSS. Fixes the fundamental limitation
of fixed-timestep integration and significantly strengthens the numerics section
of the paper.*

**Why this matters for JOSS**: Reviewers will ask why you use fixed-step RK4
when adaptive methods exist. RK45 is the standard answer. It also directly
demonstrates the Linear Algebra connection (weighted linear combinations of
derivative vectors, vector norms for error estimation) which makes the paper
more compelling.

### 1.1 Dormand-Prince coefficients header

Create `include/rk45_coefficients.h` with the exact rational Butcher tableau
constants. Use exact fractions (`44.0/45.0` not `0.9777`). This is the
mathematical foundation — everything else builds on it.

### 1.2 `RK45StepResult` struct and `rk45Step()`

```cpp
struct RK45StepResult {
    bool   accepted   = false;
    double dt_used    = 0.0;
    double dt_next    = 0.0;
    double error_norm = 0.0;
};

RK45StepResult rk45Step(
    std::vector<CelestialBody>& bodies,
    double dt,
    double atol = 1e-9,
    double rtol = 1e-9
);
```

Extend `buildIntermediateState()` to handle multi-stage combinations
(needed for stages 3-6 of Dormand-Prince). Implement `computeErrorNorm()`
with proper scaled RMS norm.

### 1.3 Adaptive simulation loop

Replace the fixed `for (int i = 0; i < steps; ++i)` loop with a
time-based adaptive loop for RK45:

```cpp
while (t < t_end) {
    auto result = rk45Step(bodies, dt, atol, rtol);
    if (result.accepted) { t += result.dt_used; }
    dt = clamp(result.dt_next, dt_min, dt_max);
}
```

Add `--duration`, `--atol`, `--rtol`, `--dt-min`, `--dt-max`,
`--output-interval` to CLI. Keep `--steps` and `--dt` working for
RK4 and Leapfrog — don't break existing workflows.

### 1.4 Time-based CSV output

Replace `step` column with `time_s` for RK45 output. Add `dt_used`
column so the adaptive timestep history is visible. Update Python
scripts to handle both `step` and `time_s` columns via `get_x_axis()`
in `utils.py` (already handles this).

### 1.5 Integrator benchmarking

Run all three integrators on the same Earth-Moon system for 3500
simulated days. Compare energy drift, angular momentum drift, and
total force evaluations. The adaptive timestep history plot (dt vs
simulated time) should show ~27-day oscillation matching the Moon's
orbital period — this is a beautiful validation that error control
is working correctly.

Add `python/plot_adaptive_dt.py` for the timestep history plot.
Extend `compare_integrators.py` to load and plot all three.

### 1.6 Tests

Add `tests/test_rk45.cpp`:
- Single step accepted for well-behaved system
- Error norm < 1.0 for appropriate dt
- `dt_next` adapts correctly (shrinks for large error, grows for small)
- 1-year Earth-Moon simulation: `|dE/E₀|` < 1e-10

**Definition of Done**:
```bash
orbit-sim run --system systems/earth_moon.json \
              --integrator rk45 \
              --duration 31536000 \
              --output-interval 3600 \
              --atol 1e-9 --rtol 1e-9 \
              --output results/rk45_earth_moon.csv
# Steps taken: fewer than RK4 equivalent
# Steps rejected: < 5% of accepted
# Energy drift: < 1e-10
```

---

## Phase 2 — Validation Study (Weeks 5–10, overlaps Phase 1)

*The scientific core of the JOSS paper. Without this, reviewers will ask
"how do you know it's correct?" and you won't have a quantified answer.*

### 2.1 Expand test suite

Add tests that will run in CI:

```
tests/test_two_body.cpp       — circular orbit period matches Kepler's 3rd law
                                 elliptical orbit conserves energy and angular momentum
                                 periapsis/apoapsis match analytical solution

tests/test_leapfrog.cpp       — energy drift bounded (not monotonic) over long sim
                                 verify symplectic property: drift oscillates, doesn't grow

tests/test_cli.cpp            — orbit-sim --help exits 0
                                 orbit-sim validate systems/earth_moon.json exits 0
                                 orbit-sim run produces output file
                                 orbit-sim run with bad system file exits non-zero

tests/test_barycenter.cpp     — COM position < 1e-10 after normalization
                                 COM velocity < 1e-10 after normalization
```

Add all new tests to `CMakeLists.txt` and wire into `ctest`.

### 2.2 HORIZONS validation study

This is the most important scientific credibility piece:

```
1. Use build-system to fetch real initial conditions at 2025-01-01
2. Simulate forward 30 days with RK4 (dt=60s) and RK45 (atol=1e-9)
3. Fetch HORIZONS positions at 2025-01-31
4. Compute position residual: |r_sim - r_horizons| in km
5. Repeat for 6 months and 1 year
6. Tabulate: integrator × duration × position error
```

Target accuracy: position residual < 1000 km after 30 days.

Document results in `docs/validation/horizons_comparison.md` with:
- Exact commands to reproduce
- Initial condition files
- Results table
- Discussion of error sources

### 2.3 Two-body analytical validation

Implement a circular orbit test with known analytical solution:

```cpp
// Circular orbit: v = sqrt(G*M/r)
// Period: T = 2π * sqrt(r³/GM)
// After one period: body returns to starting position
// Position error after 1 orbit should be < 1 km for RK4 at dt=60s
```

Document in `docs/validation/two_body_analytical.md`.

**Definition of Done**:
- `ctest` passes with 10+ tests
- `docs/validation/horizons_comparison.md` exists with real numbers
- Position residual documented for at least 3 time horizons

---

## Phase 3 — Python API and Examples (Weeks 8–16)

*Required for JOSS. Without Python bindings, the target audience (researchers
and students) cannot use the engine from their existing workflows.*

### 3.1 pybind11 build integration

Add pybind11 as a `FetchContent` dependency in `CMakeLists.txt`.
Create `orbit_py/` directory with a new CMake target `orbit_py`.
Verify with:
```bash
python3 -c "import orbit; print(orbit.__version__)"
```

### 3.2 Core simulation objects

```python
import orbit

sim = orbit.Simulation(
    config="systems/earth_moon.json",
    integrator="rk45",
    dt=60.0
)

result = sim.run(duration_days=365)

# Zero-copy NumPy access
positions = result.positions_numpy()  # shape: (n_steps, n_bodies, 3)
energies  = result.energies_numpy()   # shape: (n_steps,)
bodies    = result.body_names()       # ['Sun', 'Earth', 'Moon']
```

Design rules:
- C++ owns all data, Python holds references (`py::keep_alive`)
- Never let Python mutate positions during integration
- Return NumPy arrays via `py::array_t<double>` with zero-copy buffer protocol

### 3.3 Package and distribution

```toml
# pyproject.toml
[project]
name = "orbital-mechanics-engine"
requires-python = ">=3.8"
dependencies = ["numpy>=1.20", "matplotlib>=3.3"]
```

`pip install -e .` for development via `scikit-build-core`.

### 3.4 Jupyter notebooks

Write in `examples/`:

```
01_two_body.ipynb
    — Kepler orbit setup
    — Verify period against Kepler's 3rd law
    — Plot energy conservation

02_earth_moon.ipynb
    — Real initial conditions from HORIZONS
    — Plot Moon orbit in Earth-centered frame
    — Predict next eclipse

03_solar_system.ipynb
    — All 8 planets + Moon
    — Long-term orbital stability
    — Compare with HORIZONS positions

04_integrator_comparison.ipynb
    — RK4 vs Leapfrog vs RK45
    — Energy drift side-by-side
    — Adaptive timestep history plot
    — Efficiency: force evaluations per accuracy level
```

These notebooks are the primary marketing material for research adoption.
They must run end-to-end without errors on a clean install.

### 3.5 Tests

Add `tests/test_python_api.py`:
```python
def test_import():
    import orbit
    assert orbit.__version__ is not None

def test_earth_moon_runs():
    sim = orbit.Simulation("systems/earth_moon.json")
    result = sim.run(duration_days=30)
    positions = result.positions_numpy()
    assert positions.shape[1] == 3  # 3 bodies
    assert positions.shape[2] == 3  # x, y, z

def test_energy_conservation():
    sim = orbit.Simulation("systems/earth_moon.json")
    result = sim.run(duration_days=30)
    energies = result.energies_numpy()
    drift = abs((energies[-1] - energies[0]) / energies[0])
    assert drift < 1e-5
```

**Definition of Done**:
```bash
pip install -e .
jupyter nbconvert --to notebook --execute examples/01_two_body.ipynb
# All 4 notebooks execute without errors
```

---

## Phase 4 — Polish and Installation (Weeks 14–18)

*JOSS reviewers will try to install and run your software from scratch.
Every friction point is a potential rejection reason.*

### 4.1 Clean install test

Find someone with a fresh machine (or use a Docker container) and
have them follow only the README. Document every place they get stuck.
Fix all of them before submitting.

Target: working simulation in under 10 minutes from `git clone`.

### 4.2 Fix all README commands

Run every single command in the README. Fix any that don't work exactly
as written. This is the most common JOSS rejection reason — documentation
that doesn't match reality.

```bash
# Every command in README must work exactly as written
# No "adjust this path" or "if you have X installed"
```

### 4.3 Binary simulation format (optional but recommended)

If the validation study requires stride=1 for full-resolution trajectories,
implement the binary format from `docs/milestones/ROADMAP_binary_format.md`.

```
.orb file:   64-byte header + float64 positions
.orb.json:   metadata sidecar (bodies, dt, stride, epoch)
```

Only do this if you actually need stride=1. Skip if stride=10 is
sufficient for all your use cases.

### 4.4 Viewer time controls

From `ROADMAP_binary_format.md` Phase 1 — these are already partially
implemented (space/+/- keys). Verify they work correctly:

```
Space     — pause/resume
+/-       — speed up/slow down  
R         — reset to frame 0
[/]       — adjust distance scale
1-9       — focus body by index
```

### 4.5 Docker container (recommended)

Provide a `Dockerfile` so reviewers can run without installing
dependencies manually:

```dockerfile
FROM ubuntu:22.04
RUN apt-get install -y build-essential cmake libcurl4-openssl-dev python3-pip
COPY . /app
WORKDIR /app
RUN make BUILD_VIEWER=0 build
RUN pip install -e .
```

### 4.6 Update SETUP.md and README

Ensure both cover:
- All platform-specific dependency installation (Ubuntu, macOS, Windows)
- Build with and without viewer
- Python API installation
- Running the test suite with `ctest`
- Running the Jupyter notebooks

**Definition of Done**:
- Fresh install works in under 10 minutes
- All README commands verified working
- `ctest` documented and passing

---

## Phase 5 — JOSS Paper (Weeks 16–20)

*Write this last, after the software is stable. The paper should describe
what you built, not what you plan to build.*

### 5.1 `paper.md` structure

```markdown
---
title: 'Orbital Mechanics Engine: A C++17 N-body gravitational simulator
        with adaptive integration and real ephemeris support'
authors:
  - name: Sinan Can Demir
    affiliation: 1
affiliations:
  - name: Your University, Department
    index: 1
date: 2026
bibliography: paper.bib
---

# Summary
# Statement of Need
# Mathematics and Numerical Methods
# Validation
# Example Usage
# Acknowledgements
# References
```

### 5.2 Statement of Need (hardest section — write it yourself)

Answer these questions in 2-3 paragraphs:

- Who is this for? (advanced undergraduates, early graduate students,
  hobbyist astronomers, educators)
- What problem does it solve? (learning N-body physics through implementation,
  not just using a black-box solver)
- Why not use existing tools? (REBOUND is powerful but not educational,
  MERCURY is Fortran, most tools lack the integrated CLI+viewer+Python pipeline)
- What is unique about your approach? (modern C++17, educational clarity,
  NASA HORIZONS integration, real-time OpenGL viewer, conservation monitoring)

### 5.3 Mathematics section

Document the physics and numerical methods already in
`docs/physics-and-methods.md`. Summarize:

- Newtonian N-body equations of motion
- RK4 implementation
- Leapfrog (symplectic) implementation
- RK45 Dormand-Prince adaptive method
- Conservation law monitoring
- Eclipse detection geometry

### 5.4 Validation section

Reference the results from Phase 2:
- Two-body analytical validation
- HORIZONS position residuals (table: integrator × time horizon × error)
- Conservation law monitoring results

### 5.5 `paper.bib`

Key references to include:
```
Vallado (2013)          — Fundamentals of Astrodynamics
Dormand & Prince (1980) — RK45 original paper
Hairer et al. (1993)    — Solving ODEs I
Murray & Dermott (1999) — Solar System Dynamics
REBOUND paper           — acknowledge related work
```

### 5.6 Pre-submission checklist

Before clicking submit on JOSS:

```
[ ] paper.md renders correctly with pandoc
[ ] All references in paper.bib are real and accessible
[ ] Software version tagged (e.g. v2.0.0) on GitHub
[ ] DOI registered via Zenodo for the tagged release
[ ] All 4 Jupyter notebooks execute without errors
[ ] ctest passes with 0 failures
[ ] README install time < 10 minutes verified
[ ] Statement of Need reviewed by at least one other person
```

**Definition of Done**: Submitted to JOSS and editor pre-check passed.

---

## Implementation Priority Summary

| Priority | Phase | Task | JOSS Impact |
|----------|-------|------|-------------|
| 🔴 Now | 1.1–1.4 | RK45 integrator | Strengthens numerics, reviewer expectation |
| 🔴 Now | 1.6 | Expand test suite | Required by JOSS |
| 🟠 Soon | 2.1 | More tests (two-body, CLI) | Required by JOSS |
| 🟠 Soon | 2.2 | HORIZONS validation study | Scientific credibility |
| 🟡 Medium | 3.1–3.3 | Python bindings | Target audience usability |
| 🟡 Medium | 3.4 | Jupyter notebooks | Required working examples |
| 🟡 Medium | 4.1–4.2 | Clean install + README audit | Most common rejection reason |
| 🟢 Later | 4.3 | Binary format | Only if stride=1 needed |
| 🟢 Later | 5.1–5.6 | paper.md | Write last, after software stable |

---

## Versioning Plan

| Version | Milestone | JOSS Status |
|---------|-----------|-------------|
| v1.2 | Current — split CSV, generic viewer, HORIZONS pipeline | Not ready |
| v1.3 | Phase 1 complete — RK45, expanded tests | Not ready |
| v1.4 | Phase 2 complete — validation study, 10+ tests | Getting closer |
| v2.0 | Phase 3 complete — Python API, Jupyter notebooks | Submit to JOSS |
| v2.1 | Phase 4 complete — polish, Docker, binary format | Post-review fixes |

---

## Realistic Timeline

```
Month 1–2:   Phase 1 (RK45) + Phase 2 start (tests, two-body validation)
Month 2–4:   Phase 2 complete (HORIZONS study) + Phase 3 start (Python API)
Month 4–5:   Phase 3 complete (notebooks working end-to-end)
Month 5–6:   Phase 4 (polish, install audit, README fixes)
Month 6:     Phase 5 (paper.md) → Submit to JOSS
```

Total: approximately 6 months of consistent work.

---

## A Note on AI Assistance

This project was developed with AI assistance for implementation details,
debugging, and architecture discussions. All physics decisions, architectural
choices, and scientific validation are the author's own work.

For JOSS: the paper.md Statement of Need must be written by the author.
The validation study must be run and verified by the author. All claims
of accuracy must be backed by results the author has personally reproduced.

---

*This roadmap is a living document. Update it as features are completed
and priorities shift. Last updated: April 2026.*