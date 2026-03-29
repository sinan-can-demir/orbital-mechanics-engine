# Roadmap — Orbital Mechanics Engine

*High-accuracy C++17 N-body gravitational simulator*

**Author**: Sinan Can Demir  
**Last Updated**: March 2026  
**GitHub**: https://github.com/sinan-can-demir/orbital-mechanics-engine

---

## Current State (v1.0 — Baseline)

The engine is functional and well-structured. Before extending it, here is an honest picture of what exists:

| Component | Status | Notes |
|-----------|--------|-------|
| RK4 integrator | ✅ Working | Fixed timestep, manually implemented in `simulation.cpp` |
| Pairwise N-body gravity | ✅ Working | O(N²), symmetric, Newton's 3rd law respected |
| Conservation monitoring | ✅ Working | Energy, linear momentum, angular momentum logged per step |
| JSON system loader | ✅ Working | Arbitrary N-body configs via `systems/*.json` |
| CLI (`orbit-sim`) | ✅ Working | `run`, `list`, `info`, `validate`, `fetch`, `help` |
| NASA HORIZONS fetch | ✅ Working | GET and POST modes via `libcurl` |
| OpenGL viewer | ✅ Working | Sun–Earth–Moon + Solar System; interactive camera |
| Eclipse detection | ✅ Working | Umbra / penumbra / antumbra cone geometry |
| Barycenter normalization | ✅ Working | `--normalize` flag shifts COM to origin |
| Python plotting | ✅ Working | Energy, momentum, 3D orbit, raytracing animations |
| CI pipeline | ✅ Working | Lint, build, validate, test on every push |

**Known limitations to keep in mind as you build:**
- No adaptive time stepping — step size is fixed and user-chosen
- No close-approach handling — bodies that get very close will produce numerical garbage
- The viewer's CSV loader hardcodes Moon exaggeration (15×) and distance compression (2%) — not fully generic
- `solar_system.json` uses approximate circular orbits, not real ephemeris initial conditions
- The `earth.frag` shader has a bug: `N` and `L` are referenced but never defined (it uses `vNormal` but the code was not connected to the uniforms correctly)

---

## Phase 1 — Fix & Polish (Weeks 1–4)

*Goal: Clean up existing code before adding new features. A solid foundation prevents technical debt from compounding.*

### 1.1 Fix the broken fragment shader

**File**: `shaders/earth.frag`

The shader references `N` and `L` but never defines them. The correct version is already embedded in `orbit_viewer.cpp` as an inline GLSL string — the standalone shader file was never connected. Either:

- Fix `earth.frag` to properly use `vNormal`, `lightDir`, and compute lighting correctly
- Or remove it and rely entirely on the inline shader strings in `orbit_viewer.cpp`

This is a good first task because it teaches you how GLSL vertex/fragment shaders interact.

### 1.2 Make the CSV viewer fully generic

**File**: `src/viewer/csv_loader.cpp` and `src/viewer/orbit_viewer.cpp`

Right now the viewer assumes `x_Sun`, `x_Earth`, `x_Moon` columns and hardcodes Moon exaggeration at 15×. The simulation already writes generic `x_{name}` columns for any N-body system.

- Parse ALL `x_*` columns dynamically (partially done in `orbit_viewer.cpp` — finish it)
- Move Moon exaggeration and distance compression to a config or CLI flag instead of a compile-time constant
- This makes the viewer work correctly for any system you define in JSON

### 1.3 Add output directory safety

**File**: `src/cli/main.cpp`

If the user runs `orbit-sim run --output results/subdir/out.csv` and `results/subdir/` does not exist, the simulation silently fails to open the file. Use `std::filesystem::create_directories()` to create the path before writing.

### 1.4 Improve error messages on integration instability

When the timestep is too large, bodies fly off to infinity. The simulation doesn't detect this — it just writes garbage to CSV. Add a check in the integration loop:

- If any body's position magnitude exceeds a sanity bound (e.g., 10× the initial maximum separation), print a warning and exit early
- Suggest a smaller `--dt` in the error message

### 1.5 Write a proper test for energy conservation

**File**: add `tests/` directory (referenced in docs but not yet present in source)

Write a test that:
1. Loads `earth_moon.json`
2. Runs 10,000 steps at `dt=60`
3. Asserts that relative energy drift `|dE/E₀|` stays below `1e-5`

This gives you a regression target before you start modifying the integrator.

---

## Phase 2 — Numerical Improvements (Weeks 5–10)

*Goal: Make the integrator more accurate and more scientifically credible. This is the most important technical work.*

### 2.1 Implement the Leapfrog (Velocity Verlet) integrator

**Why this matters**: RK4 is accurate but it is not *symplectic*. Symplectic integrators preserve phase-space volume, which means energy doesn't drift monotonically over long simulations — it oscillates around a conserved value instead.

For orbital mechanics, Leapfrog is often preferred over RK4 for multi-year simulations precisely because of this property.

**Algorithm** (half-step velocity, full-step position):
```
v(t + dt/2) = v(t) + a(t) * dt/2
x(t + dt)   = x(t) + v(t + dt/2) * dt
a(t + dt)   = compute_forces(x(t + dt))
v(t + dt)   = v(t + dt/2) + a(t + dt) * dt/2
```

**Implementation plan**:
- Add a `leapfrogStep()` function in `simulation.cpp` alongside the existing `rk4Step()`
- Add an `integrator` field to the JSON system files and to `CLIOptions`
- Add `--integrator leapfrog|rk4` to the CLI
- Compare energy drift over 100,000 steps: Leapfrog should be visibly flatter

### 2.2 Implement adaptive RK45 (Dormand–Prince)

**Why this matters**: Fixed timestep is wasteful. The Moon's orbital period is ~27 days, Jupiter's is ~12 years. With a fixed 1-hour step, you're wasting enormous effort on Jupiter and potentially underresolving the Moon.

RK45 computes two estimates of the next state (4th and 5th order) and uses their difference to estimate local error. If error is too large, the step is rejected and retried with a smaller `dt`. If error is very small, `dt` is increased.

This is a larger implementation but dramatically improves simulation efficiency for multi-planet systems.

### 2.3 Add a softening parameter

**Why this matters**: When two bodies get very close, `1/r²` blows up and the simulation explodes. A softening parameter ε replaces `r²` with `r² + ε²` in force calculations:

```cpp
// Instead of:
double invr3 = 1.0 / (r * r2);

// Use:
double r_soft = std::sqrt(r2 + epsilon * epsilon);
double invr3  = 1.0 / (r_soft * r_soft * r_soft);
```

This is not physical for real planetary systems (ε = 0 is correct) but is essential for stability in dense or chaotic systems. Make it configurable with a default of 0.

### 2.4 Benchmark RK4 vs Leapfrog vs RK45

Once all three integrators exist:
- Run the same Earth–Moon system for 1 simulated year at the same accuracy target
- Compare: wall-clock time, energy drift over time, angular momentum drift
- Plot the comparison with a new `benchmark_integrators.py` script

This is excellent material for a README or a blog post.

---

## Phase 3 — Physics Extensions (Weeks 11–18)

*Goal: Add physics that makes the simulation more realistic and more interesting to study.*

### 3.1 J2 oblateness perturbation

Earth is not a perfect sphere — it bulges at the equator. This "J2 perturbation" causes the Moon's orbital plane to precess over time (~18.6-year nodal period) and causes satellites to drift in their orbital planes.

The additional acceleration on a satellite due to Earth's J2:

```
J2 = 1.08263e-3   (Earth's second zonal harmonic)
a_J2 = (3/2) * J2 * G*M_e * R_e² / r⁴ * (z/r terms...)
```

This is optional but physically interesting and referenced in the literature the project already cites (Vallado).

### 3.2 Solar radiation pressure

For spacecraft, the Sun exerts a small force due to photon momentum. Proportional to cross-sectional area and inversely proportional to distance squared from the Sun. Essential for accurate modeling of any satellite.

Make it optional and toggled per-body in the JSON.

### 3.3 Atmospheric drag (low Earth orbit)

For bodies in low orbits (altitude < 1000 km), atmospheric drag is significant. Requires a density model (even a simple exponential model is sufficient for educational use). This would be a meaningful addition for students interested in satellite reentry.

### 3.4 General relativistic precession (Mercury)

Mercury's orbit precesses at ~43 arcseconds per century due to general relativity — a famous test of GR that Newtonian gravity cannot explain. Adding the first-order post-Newtonian correction to the force calculation:

```
a_GR = G*M/r² * (4*G*M/(c²*r) - v²/c² + 4*(v⃗·r̂)²/c²) * direction
```

This is a small code change with large educational value. It turns the simulation into a GR demonstration.

---

## Phase 4 — Python API (Weeks 19–26)

*Goal: Expose the C++ engine to Python so researchers and students can use it from Jupyter notebooks.*

This is the single biggest opportunity for community adoption. Follow the "orchestration-only" principle: Python configures and analyses, C++ does all physics computation.

### 4.1 Set up pybind11 build integration

- Add pybind11 as a `FetchContent` dependency in `CMakeLists.txt`
- Create `orbit_py/` directory with a new CMake target `orbit_py`
- Create a minimal `PYBIND11_MODULE(orbit, m)` that exports a version string
- Verify with `python3 -c "import orbit; print(orbit.__version__)"`

### 4.2 Expose the core simulation objects

```python
import orbit

sim = orbit.Simulation(
    config="systems/earth_moon.json",
    integrator="leapfrog",
    dt=60.0
)

result = sim.run(duration_days=365)

# Zero-copy NumPy access
positions = result.positions_numpy()   # shape: (n_steps, n_bodies, 3)
energies  = result.energies_numpy()    # shape: (n_steps,)
```

Key design rules:
- C++ owns all data; Python holds references (use `py::keep_alive`)
- Never let Python mutate positions or velocities during integration
- Return NumPy arrays via `py::array_t<double>` with zero-copy buffer protocol

### 4.3 Python package and distribution

```toml
# pyproject.toml
[project]
name = "orbital-mechanics-engine"
requires-python = ">=3.8"
dependencies = ["numpy>=1.20", "matplotlib>=3.3"]
```

- `pip install -e .` for development
- Editable install that rebuilds C++ on change via `scikit-build-core`

### 4.4 Jupyter demo notebooks

Write these notebooks in `examples/`:
- `01_two_body.ipynb` — Kepler orbit, energy conservation verification
- `02_earth_moon.ipynb` — Moon orbit, eclipse prediction
- `03_solar_system.ipynb` — Long-term planet motion
- `04_integrator_comparison.ipynb` — RK4 vs Leapfrog energy drift side-by-side

These notebooks are the primary marketing material for research adoption.

---

## Phase 5 — Visualization & UX (Weeks 20–28, parallel)

*Goal: Make the simulation results more compelling and easier to explore.*

### 5.1 Orbit trail rendering in the OpenGL viewer

Currently the viewer only shows bodies as spheres — there are no orbit trails. Adding trails:
- Store the last N positions for each body (circular buffer)
- Render as a `GL_LINE_STRIP` with fading alpha toward the tail
- Add `--trail-length N` CLI flag for the viewer

### 5.2 In-viewer time controls

The viewer currently plays back at a fixed rate (one CSV frame per render frame). Add:
- Spacebar to pause/resume
- Left/Right arrows to step forward/backward one frame
- `+`/`-` to speed up / slow down playback
- A time readout (simulated date) in the HUD

### 5.3 WebAssembly build

Compile the simulation engine to WebAssembly so it can run in a browser. This is the single most effective way to get researchers and students to try the engine without installing anything.

Target: a web page where users can configure a system, press Run, and see a 3D orbit in their browser.

Tools: Emscripten + three.js for rendering.

### 5.4 ImGui overlay for the viewer

Add an [Dear ImGui](https://github.com/ocornut/imgui) overlay to `orbit_viewer` that shows:
- Current simulated time
- Per-body energy contribution
- Conservation law drift readouts
- Integrator selector (if multiple integrators are implemented)

This turns the viewer into a proper interactive scientific tool rather than just a playback visualizer.

---

## Phase 6 — Performance & Scale (Weeks 29–40)

*Goal: Handle larger N-body systems efficiently.*

### 6.1 Barnes–Hut tree (O(N log N) force calculation)

The current O(N²) force loop becomes a bottleneck above ~100 bodies. Barnes–Hut approximates distant clusters of bodies as single point masses using an octree, reducing cost to O(N log N).

This is a significant algorithmic project — plan 3–4 weeks. Only implement once the rest of the engine is stable.

### 6.2 Multi-threaded force computation

The pairwise force loop in `updateAccelerations()` is embarrassingly parallel — each pair `(i, j)` can be computed independently. Add OpenMP:

```cpp
#pragma omp parallel for schedule(dynamic)
for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i + 1; j < N; ++j) {
        // ... but need thread-safe accumulation of accelerations
    }
}
```

Note: accumulation requires care to avoid data races. Use per-thread force buffers or atomic operations.

### 6.3 GPU acceleration (CUDA/OpenCL)

The force kernel is a perfect candidate for GPU parallelism — N² independent dot products and divisions, no branching, regular memory access. This is a substantial project but would allow simulating galaxy-scale systems with millions of bodies.

Only pursue this after Barnes–Hut is working and validated.

---

## Phase 7 — Research & Publication (Ongoing)

*Goal: Establish the engine as a credible research tool.*

### 7.1 NASA HORIZONS validation study

A structured comparison between simulated trajectories and HORIZONS ephemerides:
- Same initial conditions (from HORIZONS at a fixed epoch)
- Simulate forward 1 month, 6 months, 1 year
- Compute position and velocity residuals
- Document which timestep and integrator achieves < 1 km position error over 1 year

Write this up as a markdown document in `docs/validation/horizons_comparison.md` with plots. This is what makes the "NASA-validated" claim concrete and credible.

### 7.2 Reproducibility archive

- Pin a specific version of the engine with a DOI via Zenodo
- Provide exact commands to reproduce each validation result
- Include initial condition files and expected output checksums

### 7.3 Conference submission

Suitable venues for this work:
- AIAA SciTech Forum (aerospace / computational mechanics)
- AAS/AIAA Space Flight Mechanics Meeting
- JOSS (Journal of Open Source Software) — requires tests, docs, and a statement of need

JOSS is the most accessible first publication target: the review process is open on GitHub, and the requirements align closely with what this project already has.

---

## Implementation Priority Summary

| Priority | Task | Why |
|----------|------|-----|
| 🔴 Now | Fix `earth.frag` shader bug | Broken code in the repo is embarrassing |
| 🔴 Now | Add conservation test | Regression baseline before any integrator changes |
| 🟠 Soon | Leapfrog integrator | Biggest scientific credibility gain per effort |
| 🟠 Soon | Generic CSV viewer | Makes the tool actually work for any system |
| 🟡 Medium | Adaptive RK45 | Efficiency, important for Solar System sims |
| 🟡 Medium | Python bindings | Research community adoption |
| 🟡 Medium | Orbit trail rendering | Makes visualizations much more compelling |
| 🟢 Later | Barnes–Hut tree | Only needed for N > ~100 bodies |
| 🟢 Later | WebAssembly build | Maximum reach, significant engineering effort |
| 🟢 Later | JOSS paper | Long-term credibility, plan when engine is stable |

---

## Versioning Plan

| Version | Milestone |
|---------|-----------|
| v1.0 | Current baseline — RK4, conservation, viewer, CLI, HORIZONS |
| v1.1 | Phase 1 complete — bug fixes, generic viewer, test suite |
| v1.2 | Phase 2 complete — Leapfrog + RK45 integrators |
| v1.3 | Phase 3 partial — J2, softening |
| v2.0 | Phase 4 complete — Python API, Jupyter notebooks |
| v2.1 | Phase 5 complete — orbit trails, time controls |
| v3.0 | Phase 6 complete — Barnes–Hut, multi-threading |

---

## For New Contributors

If you want to contribute, the best starting points are:

1. **Fix the `earth.frag` shader** — small, self-contained, teaches GLSL
2. **Add output directory auto-creation** — 5 lines of C++17 `<filesystem>`
3. **Write the conservation law unit test** — teaches the codebase without changing physics
4. **Implement Leapfrog** — if you are comfortable with numerical methods

See `CONTRIBUTING.md` for code style, branch naming, and PR requirements.

---

*This roadmap is a living document. Update it as features are completed and priorities shift.*
