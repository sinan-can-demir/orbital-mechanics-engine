# Paper Notes — Orbital Mechanics Engine

Pre-writing notes for paper.md (JOSS submission). Not a draft — just findings and claims to draw from.

---

## Submission roadmap

| Phase | Status | Description |
|---|---|---|
| Python API (pybind11) | Done | `orbit.simulate()`, `simulate_adaptive()`, NumPy output |
| Tests + CI | Done | 10 ctest tests, 22 pytest tests (counted 2026-11-18), GitHub Actions |
| Documentation | Done | README, CONTRIBUTING, FUTURE.md, 9 notebooks |
| REBOUND comparison | Done | Notebooks 05 & 06, benchmark script |
| SPICE validation | Done | Compared against DE440; see below (rerun 2026-11-18) |
| paper.md + paper.bib | In progress | JOSS submission |

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

## Benchmark results (rerun 2026-11-18)

Solar system (10 bodies, `systems/solar_system.json`), 1 Julian year, Newtonian. Each timing repeated
10 times; median, min-max and interquartile range reported. Produced by `python3 benchmark_rebound.py`
(data: `results/paper_benchmark/benchmark_timing.csv`). The earlier Earth-Moon table and the single-run
timings were replaced; they were not reproduced in this rerun.

| Integrator | Median (ms) | Min-max (ms) | IQR (ms) | Steps (snapshots for RK45) | Energy drift |
|---|---|---|---|---|---|
| Ours: RK4 (dt=1h) | 100.6 | 98.8-105.9 | 2.6 | 8766 | 8.976e-15 |
| Ours: Leapfrog (dt=1h) | 28.0 | 27.6-28.6 | 0.5 | 8766 | 2.211e-10 |
| Ours: Yoshida4 (dt=1h) | 61.7 | 61.0-64.0 | 1.9 | 8766 | 8.415e-15 |
| Ours: Hermite (dt=1h) | 90.2 | 89.8-102.9 | 1.4 | 8766 | 5.329e-14 |
| Ours: RK45 (tol=1e-9) | 12.1 | 11.8-14.4 | 1.8 | 517 | 1.575e-11 |
| Ours: RK45 (tol=1e-12) | 47.1 | 46.7-49.5 | 0.6 | 2123 | 2.169e-14 |
| REBOUND: IAS15 (default) | 43.3 | 42.5-47.2 | 2.2 | 305 | 5.602e-16 |
| REBOUND: WHFast (dt=1h) | 84.3 | 83.4-87.5 | 1.1 | 8766 | 2.639e-13 |

Timings are not matched for accuracy (IAS15 reaches the lowest drift). Repetition spread is small
(IQR <= 2.6 ms) compared with the old "about 30% variability" note, which came from single runs.

### Long-run energy drift (GR off, 10-body Solar System)

Produced by `python3 python/long_run_energy_drift.py [--dt ...]`; figure
`results/paper_benchmark/long_run_energy_drift.png`. Maximum |dE/E0| up to each time.

dt = 1 h (`long_run_energy_drift_dt3600.csv`; wall times in that CSV were measured while other jobs ran, do not quote them):

| Integrator | max drift to 10 yr | to 100 yr | to 1000 yr |
|---|---|---|---|
| RK4 (dt=3600s) | 4.824e-14 | 4.824e-14 | 7.113e-13 |
| Leapfrog (dt=3600s) | 3.739e-10 | 3.924e-10 | 3.931e-10 |
| Yoshida4 (dt=3600s) | 7.237e-14 | 1.255e-13 | 4.688e-13 |
| Hermite (dt=3600s) | 1.634e-13 | 2.248e-13 | 6.141e-13 |
| RK45 (tol=1e-9) | 1.376e-10 | 1.471e-09 | 1.590e-08 |

dt = 1 day (`long_run_energy_drift_dt86400.csv`):

| Integrator | max drift to 10 yr | to 100 yr | to 1000 yr |
|---|---|---|---|
| RK4 (dt=86400s) | 3.677e-08 | 3.345e-07 | 2.801e-06 |
| Leapfrog (dt=86400s) | 2.059e-07 | 2.099e-07 | 2.099e-07 |
| Yoshida4 (dt=86400s) | 2.469e-09 | 2.545e-09 | 2.562e-09 |
| Hermite (dt=86400s) | 8.698e-09 | 8.676e-08 | 8.338e-07 |

---

## What these results actually support

**Defensible claims:**
- At dt = 1 h, RK4, Yoshida4 and Hermite stay at 1e-13 level for 1000 years; Leapfrog is bounded near 3.9e-10; RK45 (tol 1e-9) drifts secularly (1.4e-10 at 10 yr, 1.6e-8 at 1000 yr).
- At dt = 1 day, RK4 and Hermite drift secularly (2.8e-6 and 8.3e-7 at 1000 yr), Leapfrog (2.1e-7) and Yoshida4 (2.6e-9) stay bounded.
- Integrator choice is a single argument.

**Claims to avoid:**
- Do not claim we beat REBOUND in accuracy: IAS15 reaches 5.6e-16 after 1 year, our best is 8.4e-15 (Yoshida4).
- Do not quote the RK45-vs-IAS15 timing ratio as a speed advantage; the runs are not accuracy-matched.
- The 1-year comparison favours RK4 over symplectic methods; use the long-run table.

---

## Positioning vs REBOUND

REBOUND is a mature, research-grade library used in planetary science and stellar dynamics. It is not a fair direct competitor — it's the reference implementation.

The honest comparison is:
- REBOUND: maximum accuracy, large ecosystem, Python API, no integrator transparency
- Ours: explicit integrator selection, educational focus, C++17 core, OpenGL viewer, HORIZONS integration, simpler API

The paper should not position this as "better than REBOUND." The angle is: **transparency and education** — the user can see exactly which method is running and compare their behaviours directly.

---

## Statement of Need (draft)

Existing N-body simulators such as REBOUND prioritise maximum accuracy through opaque adaptive integrators. This is appropriate for research but makes them unsuitable for teaching the effects of numerical integration on orbital mechanics. Orbital Mechanics Engine exposes five integration methods — RK4, Leapfrog, Yoshida4, RK45 Dormand-Prince and Hermite — through a single swappable argument, letting students directly observe and compare energy conservation, timestep behaviour, and long-term drift without modifying any simulation code.

---

## Key features for paper

- Five integrators: RK4 (4th order, fixed), Leapfrog (2nd order symplectic, fixed), Yoshida4 (4th order symplectic, fixed), RK45 Dormand-Prince (adaptive), Hermite (4th order predictor-corrector, fixed)
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
