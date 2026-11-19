# Paper Notes — Orbital Mechanics Engine

Pre-writing notes for paper.md (JOSS submission). Not a draft — just findings and claims to draw from.

---

## Submission roadmap

| Phase | Status | Description |
|---|---|---|
| Python API (pybind11) | Done | `orbit.simulate()`, `simulate_adaptive()`, NumPy output |
| Tests + CI | Done | 7 C++ tests, 4 Python tests, GitHub Actions |
| Documentation | Done | README, CONTRIBUTING, FUTURE.md, 6 notebooks |
| REBOUND comparison | Done | Notebooks 05 & 06, benchmark script |
| **SPICE validation** | **Next** | Compare simulated vs real trajectories using NASA kernels |
| paper.md + paper.bib | Pending | JOSS submission — blocked on SPICE results |

---

## SPICE validation results (rerun 2026-11-18 after constants fix)

Full tables and method: `docs/validation.md` ("SPICE DE440 Validation Results"). Raw data:
`results/spice_validation/error_1yr_current.csv` and `..._baseline_G6.674_masses.csv`;
regenerate with `python3 python/spice_validation_table.py`.

**Setup:** DE440-derived ICs (`systems/solar_system_spice.json`, built by `python/spice_ic.py`),
ground truth from SPICE `spkgeo()` in ECLIPJ2000 frame, positions relative to the Sun,
10-body system, exactly one Julian year from 2025-01-01, dt = 1 h for fixed-step methods.

**Correction:** earlier versions of these notes reported Earth at 88,126 km after one year and
attributed it to "missing GR + J2". That was wrong. The system file's masses had been computed
with G = 6.674e-11 while the engine uses 6.67430e-11. Regenerating the file removes the error.

### Earth error at 1 year (km)

| Integrator | before: GR off | before: GR on | after: GR off | after: GR on |
|---|---|---|---|---|
| RK4 | 88,125.0 | 88,064.1 | 61.1 | 0.2 |
| Leapfrog | 87,969.8 | 87,908.8 | 94.3 | 155.3 |
| Yoshida4 | 88,125.0 | 88,063.9 | 61.1 | 0.1 |
| Hermite | 88,125.0 | 88,064.1 | 61.1 | 0.2 |
| RK45 tol=1e-9 | 88,125.1 | 88,064.2 | 61.2 | 0.3 |
| RK45 tol=1e-12 | 88,125.0 | 88,064.1 | 61.1 | 0.2 |

### Per-body errors at 1 year (RK4, dt = 1 h)

| Body | before: GR off | after: GR off | after: GR on |
|---|---|---|---|
| Mercury | 95,027.3 | 88.1 | 0.1 |
| Venus | 106,324.4 | 96.4 | 0.0 |
| Earth | 88,125.0 | 61.1 | 0.2 |
| Moon | 90,850.8 | 50.6 | 12.0 |
| Mars | 71,683.6 | 31.1 | 0.2 |
| Jupiter | 5,253.1 | 0.6 | 0.1 |
| Saturn | 1,452.0 | 0.1 | 0.2 |
| Uranus | 353.3 | 0.1 | 0.1 |
| Neptune | 149.3 | 0.1 | 0.1 |

### Paper claim (defensible)

> "Starting from DE440 initial conditions, the 10-body simulator with the first post-Newtonian
> correction reproduces Earth's heliocentric position to 0.2-0.3 km and Mercury, Venus and Mars to 0.5 km
> or better over one Julian year (RK4, Hermite and RK45 integrators);
> without the correction Earth is 61 km off. The Moon remains 12 km off, which we have not explained."

Leapfrog at dt = 1 h is worse (Mercury 4,432 km); this scales like dt^2 and is numerical.
Yoshida4 with GR on is 17 km off for Mercury; cause not investigated.

---

## Benchmark results

### Earth-Moon system (3 bodies), 1-year integration

| Integrator | Time (ms) | Steps | Energy drift |
|---|---|---|---|
| Ours: RK4 (dt=1h) | 21.9 | 8,760 | 4.517e-15 |
| Ours: Leapfrog (dt=1h) | 10.9 | 8,760 | 1.792e-11 |
| Ours: RK45 (atol=1e-9) | 3.1 | 561 | 7.311e-12 |
| Ours: RK45 (atol=1e-12) | 11.4 | 2,282 | 5.958e-14 |
| REBOUND: IAS15 (default) | 12.7 | 305 | 0.000e+00 |
| REBOUND: WHFast (dt=1h) | 22.6 | 8,766 | 1.181e-11 |

### Solar system (10 bodies), 1-year integration

| Integrator | Time (ms) | Steps | Energy drift |
|---|---|---|---|
| Ours: RK4 (dt=1h) | 103.3 | 8,760 | 3.553e-15 |
| Ours: Leapfrog (dt=1h) | 31.2 | 8,760 | 2.186e-10 |
| Ours: RK45 (atol=1e-9) | 13.3 | 517 | 1.576e-11 |
| Ours: RK45 (atol=1e-12) | 51.5 | 2,122 | 1.758e-14 |
| REBOUND: IAS15 (default) | 40.8 | 305 | 3.735e-16 |
| REBOUND: WHFast (dt=1h) | 84.2 | 8,766 | 2.478e-13 |

Run with: `python3 benchmark_rebound.py` (repo root). Single-run timing — variability ±~30%.

---

## What these results actually support

**Defensible claims:**
- Our adaptive RK45 is ~3x faster than REBOUND IAS15 on the 10-body solar system for moderate-precision work (atol=1e-9: 13ms vs 41ms)
- Our RK4 achieves better single-run energy drift than REBOUND WHFast for 1-year integrations (3.5e-15 vs 2.5e-13)
- Integrator choice is a single argument — direct, transparent user control

**Claims to avoid:**
- Do not claim we beat REBOUND in accuracy. REBOUND IAS15 achieves near machine-precision (3.7e-16) — we don't match that
- Do not claim the timing difference is statistically significant without multiple runs
- WHFast result caveat: WHFast's bounded drift advantage over RK4 would appear over long timescales (>10 years); 1-year comparison favours RK4 unfairly

---

## Positioning vs REBOUND

REBOUND is a mature, research-grade library used in planetary science and stellar dynamics. It is not a fair direct competitor — it's the reference implementation.

The honest comparison is:
- REBOUND: maximum accuracy, large ecosystem, Python API, no integrator transparency
- Ours: explicit integrator selection, educational focus, C++17 core, OpenGL viewer, HORIZONS integration, simpler API

The paper should not position this as "better than REBOUND." The angle is: **transparency and education** — the user can see exactly which method is running and compare their behaviours directly.

---

## Statement of Need (draft)

Existing N-body simulators such as REBOUND prioritise maximum accuracy through opaque adaptive integrators. This is appropriate for research but makes them unsuitable for teaching the effects of numerical integration on orbital mechanics. Orbital Mechanics Engine exposes three integration methods — RK4, Leapfrog, and RK45 Dormand-Prince — through a single swappable argument, letting students directly observe and compare energy conservation, timestep behaviour, and long-term drift without modifying any simulation code.

---

## Key features for paper

- Three integrators: RK4 (4th order, fixed), Leapfrog (2nd order symplectic, fixed), RK45 Dormand-Prince (adaptive)
- NASA JPL HORIZONS integration — real initial conditions, not toy problems
- Python API with NumPy output
- OpenGL real-time viewer
- Eclipse detection (umbra/penumbra geometry)
- Conservation monitoring at every step
- C++17, CMake, pybind11, scikit-build-core

---

## Target journals / venues

- JOSS (Journal of Open Source Software) — primary target
  - Requires: open source, documented, tests, paper.md ≤ 1000 words
  - Does not require novel research — software must be useful and well-described

---

## Author info needed for paper.md

- Affiliation (university/institution)
- ORCID (optional but recommended for JOSS)
