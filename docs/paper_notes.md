# Paper Notes — Orbital Mechanics Engine

Pre-writing notes for paper.md (JOSS submission). Not a draft — just findings and claims to draw from.

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
