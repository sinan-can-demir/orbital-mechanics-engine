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

## SPICE validation results (✅ Done — Steps 1 + 1b)

**Setup:** DE440-derived ICs (`systems/solar_system_spice.json`, built by `python/spice_ic.py`),
ground truth from SPICE `spkgeo()` in ECLIPJ2000 frame. All four integrators, 10-body solar
system, 1-year run from 2025-01-01.

**Bug found and fixed:** The fixed-step simulation loop saved snapshots labeled `i*dt` but
containing the state AFTER step `i` (actual time `(i+1)*dt`). Fix: changed to `(i+1)*dt`.
The adaptive (RK45) loop was already correct. Old notebook numbers were affected by this bug.

### Integrator comparison — Earth position error (SPICE ICs)

| Integrator | t=1h (km) | t=1yr (km) | Drift (km) |
|---|---|---|---|
| RK4 (dt=1h) | 0 | 88,126 | 88,126 |
| Leapfrog (dt=1h) | 0 | 87,970 | 87,970 |
| RK45 (atol=1e-9) | 0 | 88,125 | 88,125 |
| RK45 (atol=1e-12) | 0 | 88,125 | 88,125 |

**Key finding:** All four integrators give identically ~88,000 km at 1 year. Tightening
RK45 tolerance 1000× makes zero difference. Error is entirely **physics-dominated**
(missing GR + J2), not numerical precision.

### Per-body errors at 1 year (RK4, dt=1h, SPICE ICs)

| Body | Error at 1yr (km) | % of orbital radius | Notes |
|---|---|---|---|
| Venus | 106,214 | 0.070% | Close orbit, GR strong |
| Mercury | 95,099 | 0.210% | 43″/century GR precession |
| Moon | 90,759 | — | Coupled to Earth |
| Earth | 88,126 | 0.059% | |
| Mars | 71,585 | 0.047% | Boundary of GR significance |
| Jupiter | 5,246 | 0.001% | GR negligible at 5.2 AU |
| Saturn | 1,450 | 0.0002% | |
| Uranus | 353 | 0.00002% | |
| Neptune | 149 | 0.000006% | |

Sharp drop from Mars → Jupiter confirms inner-planet errors are GR-dominated.

### Paper claim (defensible)

> "Starting from DE440 initial conditions, our Newtonian simulator maintains Earth's position
> to within 88,126 km (0.059% of orbital radius) over a one-year integration of the
> full 10-body solar system. Tightening the adaptive integrator tolerance by 1000× produces
> no change in this error, confirming the residual is dominated by missing post-Newtonian
> corrections rather than numerical integration precision."

---

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
