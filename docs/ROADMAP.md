# Roadmap — Orbital Mechanics Engine

**Author**: Sinan Can Demir  
**Last Updated**: July 2026  
**GitHub**: https://github.com/sinan-can-demir/orbital-mechanics-engine  
**Target**: JOSS submission at v2.0 — end of summer 2026 (September)  
**Scope**: Scale B — educational tool with Yoshida + Hermite integrators, SPICE validation, full paper

---

## Current State (v2.0.0-dev)

All core phases are complete. The project is in pre-submission preparation.

| Component | Status | Notes |
|---|---|---|
| RK4 integrator | ✅ Done | Fixed-step, `simulation.cpp` |
| Leapfrog integrator | ✅ Done | Symplectic, bounded energy drift |
| RK45 Dormand-Prince | ✅ Done | Adaptive timestep, error-controlled |
| Pairwise N-body gravity | ✅ Done | O(N²), Newton's 3rd law |
| Conservation monitoring | ✅ Done | Energy, linear + angular momentum |
| JSON system loader | ✅ Done | Arbitrary N-body configs |
| CLI (`orbit-sim`) | ✅ Done | `run`, `validate`, `fetch`, `build-system` |
| NASA HORIZONS integration | ✅ Done | GET + POST modes via libcurl |
| HORIZONS system builder | ✅ Done | Real planetary initial conditions |
| Eclipse detection | ✅ Done | Umbra / penumbra / antumbra geometry |
| OpenGL viewer | ✅ Done | Real-time 3D, camera controls, body focus |
| Python API (`orbit` module) | ✅ Done | `simulate()`, `simulate_adaptive()`, NumPy output |
| pybind11 bindings | ✅ Done | `orbit.Integrator` enum, `SimulationResult`, `positions_numpy()` |
| scikit-build-core packaging | ✅ Done | `pip install -e .` |
| Jupyter notebooks | ✅ Done | 8 notebooks in `examples/` |
| NASA SPICE validation (DE440) | ✅ Done | `python/spice_validate.py`, notebook 07 |
| Post-Newtonian GR correction | ✅ Done | Schwarzschild 1PN, `applyGRCorrection()`, `--gr` / `gr=True` |
| Yoshida 4th-order symplectic integrator | ✅ Done | `yoshida4Step()`, notebook 08 |
| Hermite predictor-corrector integrator | ✅ Done | `hermiteStep()`, `computeJerks()`, `--integrator hermite`, O(dt⁴) verified in `tests/test_hermite.cpp` |
| REBOUND comparison | ✅ Done | Notebooks 05 & 06, `benchmark_rebound.py` |
| CI pipeline | ✅ Done | Lint, C++ build, ctest, pytest on every push |
| C++ test suite | ✅ Done | 9 tests via ctest |
| Python test suite | ✅ Done | 4 tests via pytest |
| README audit | ✅ Done | All commands verified on clean machine |
| CONTRIBUTING.md | ✅ Done | Setup, style, integrator guide, PR checklist |
| FUTURE.md | ✅ Done | Yoshida, binary I/O, native app, systems DB, detection features |

---

## JOSS Submission Checklist

| Requirement | Status |
|---|---|
| Open source license (MIT) | ✅ Done |
| Substantial functionality | ✅ Done |
| Community guidelines (CONTRIBUTING.md) | ✅ Done |
| Passing test suite (ctest + pytest) | ✅ Done |
| Python API with NumPy output | ✅ Done |
| Jupyter notebook examples (6 notebooks) | ✅ Done |
| Clean install verified on separate machine | ✅ Done |
| REBOUND comparison study | ✅ Done |
| **NASA SPICE validation** | ✅ Done |
| **SPICE-based initial conditions** | ✅ Done |
| **Post-Newtonian GR correction** | ✅ Done |
| **Yoshida 4th-order integrator** | ✅ Done |
| **Hermite integrator** | ✅ Done |
| **`paper.md` + `paper.bib`** | ✅ Drafted (#28) — needs final author review |
| Software version tagged on GitHub | ❌ Pending — `pyproject.toml` says `2.0.0` but no `v2.0.0` git tag exists yet |
| DOI via Zenodo | ❌ Pending (after tag) |

---

## Timeline

```
June 2026      SPICE validation + SPICE-based ICs + GR correction ✅ done
               Yoshida 4th ✅ done + paper.md/paper.bib drafted ✅ done
July 2026      Hermite integrator ✅ done
               Remaining: final paper review, benchmarks/notebooks update for Hermite
August 2026    Cleanup, tag v2.0.0, Zenodo DOI
September 2026 Submit to JOSS
```

---

## What's Left

### Step 1 — SPICE Validation ✅ Done

Compare simulated trajectories against NASA SPICE kernels (ground truth) to produce a quantitative position accuracy claim for the paper.

**Why:** Energy drift is an internal consistency check. SPICE gives an external ground truth — *"our simulated Jupiter is within X km of the real trajectory after 1 year."* That's a publishable accuracy claim.

- [x] `pip install spiceypy`
- [x] Download `de440.bsp`, `naif0012.tls`, `pck00011.tpc` from NAIF
- [x] Write `python/spice_validate.py` — ECLIPJ2000 frame, per-body error
- [x] Write `python/download_kernels.py` — one-command kernel download
- [x] Notebook `examples/07_spice_validation.ipynb` — executed, all plots working
- [x] Record final results in `docs/paper_notes.md`

**Kernels:**
```
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc
```

---

### Step 1b — SPICE-Based Initial Conditions ✅ Done

- [x] Write `python/spice_ic.py` — queries SPICE for body positions/velocities at a given epoch, writes `systems/solar_system_spice.json`
- [x] Fix snapshot timestamp bug: `runSimulationCore` was labeling snapshots `i*dt` but saving the state at `(i+1)*dt`. Fixed to `(i+1)*dt`.
- [x] Verify: re-run SPICE validation with SPICE-derived ICs — t=1h error = **0 km** (from 107,000 km with HORIZONS ICs)
- [x] Document in `docs/paper_notes.md` with corrected numbers

**Result:** With DE440 ICs, Earth 1-year error = **88,126 km (0.059%)**, dominated by missing GR. All 4 integrators give identical error — physics-dominated, not numerical.

---

### Step 1c — Post-Newtonian GR Correction ✅ Done

Add the first post-Newtonian correction to gravitational acceleration. This is the dominant missing physics for inner planets — fixes Mercury's 43"/century perihelion precession and reduces Earth/Venus position errors.

**Why:** DE440 is built with full GR. Our Newtonian engine systematically drifts from it for inner planets. One force term fixes most of this.

**The correction** (Schwarzschild approximation, Sun-dominated):
```
For each body i, add to acceleration from the Sun:
Δa_i = (G·M_sun / c² / r³) · [(4·G·M_sun/r − |v|²)·r̂ + 4·(r̂·v)·v]
```
Where c = 299,792,458 m/s.

- [x] Add `constexpr double C_LIGHT = 299792458.0;` to `include/constants.h`
- [x] Add `bool use_gr = false` parameter to `runSimulationCore()` and `runSimulationAdaptiveCore()`
- [x] Implement `applyGRCorrection()` in `src/core/simulation.cpp` — called after Newtonian forces
- [x] Add `--gr` flag to CLI (`orbit-sim run --gr`)
- [x] Expose in Python: `orbit.simulate(..., gr=True)`
- [x] Expose in Python bindings in `orbit_py/bindings.cpp`
- [x] Re-run SPICE validation with `gr=True` — record improvement

**Expected improvement:** Mercury error ~60-70% reduction. Earth/Venus ~20-30% reduction. Outer planets unchanged (GR negligible at large r).

---

### Step 2 — Yoshida 4th-Order Symplectic Integrator ✅ Done

Add a 4th-order symplectic integrator for long-term integrations (million-year runs). Composes three Leapfrog sub-steps with published Yoshida coefficients. Directly strengthens the paper's integrator comparison story.

- [x] Add `Integrator::Yoshida4` to enum in `include/simulation.h`
- [x] Implement in `src/core/simulation.cpp` alongside Leapfrog
- [x] Expose via `py::enum_<Integrator>` in `orbit_py/bindings.cpp`
- [x] Add CLI support (`--integrator yoshida4`)
- [x] Add conservation test (`tests/test_yoshida4.cpp`)
- [x] Update notebooks 03 and 05/06 to include Yoshida in comparisons

---

### Step 2b — Hermite Predictor-Corrector Integrator ✅ Done

4th-order Hermite predictor-corrector (Makino & Aarseth 1992) — uses jerk (da/dt) instead of extra stage evaluations, standard for direct N-body codes.

- [x] Add `Integrator::Hermite` to enum in `include/simulation.h`
- [x] Implement `computeJerks()` and `hermiteStep()` in `src/core/simulation.cpp`
- [x] Wire into `runSimulationCore()`
- [x] Add CLI support (`--integrator hermite`)
- [x] `--gr` support (GR correction applied to acceleration only, not jerk — documented approximation)
- [x] Expose via `py::enum_<Integrator>` in `orbit_py/bindings.cpp`
- [x] Test: `tests/test_hermite.cpp` — verifies O(dt⁴) global error convergence (measured ratio 16.68 vs theoretical 16 on a two-body relative-orbit check) and beats RK4 at equal step count
- [ ] Update notebooks 03 and 05/06 to include Hermite in comparisons — not yet done

---

### Step 3 — paper.md + paper.bib ✅ Drafted (#28)

Written after SPICE and Yoshida were done. See `docs/paper_notes.md` for benchmark tables and claims inventory.

- [x] Summary (~200 words)
- [x] Statement of Need
- [x] Mathematics and Numerical Methods
- [x] Validation (SPICE results + REBOUND comparison)
- [x] Example Usage
- [x] `paper.bib` with all references
- [ ] Add Hermite to the integrator comparison section
- [ ] Verify renders with `pandoc`
- [ ] Final author review of Statement of Need

**Target length:** ≤ 1000 words (JOSS limit).

---

### Step 4 — Release and Submit

- [x] Bump version to `2.0.0` in `bindings.cpp` and `pyproject.toml`
- [ ] Merge all open branches to `main` (`debug` has 1 unmerged commit as of this writing)
- [ ] Tag `v2.0.0` on GitHub — not yet tagged (latest tag is `v1.2.0`)
- [ ] Register DOI via Zenodo
- [ ] Submit to JOSS

---

## Post-Submission (v2.1+)

- **Wisdom-Holman symplectic map** — Keplerian decomposition for million-year integrations. See [keplerian_physics.md](keplerian_physics.md) for full background.
- See [FUTURE.md](FUTURE.md) for all other directions: binary I/O, native app, systems database, collision/merge/rogue-body detection.

---

*Last updated: July 2026. Update this file as work completes.*
