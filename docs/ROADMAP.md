# Roadmap — Orbital Mechanics Engine

**Author**: Sinan Can Demir  
**Last Updated**: June 2026  
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
| Jupyter notebooks | ✅ Done | 6 notebooks in `examples/` |
| REBOUND comparison | ✅ Done | Notebooks 05 & 06, `benchmark_rebound.py` |
| CI pipeline | ✅ Done | Lint, C++ build, ctest, pytest on every push |
| C++ test suite | ✅ Done | 7 tests via ctest |
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
| **NASA SPICE validation** | ⏳ In progress |
| **Yoshida 4th-order integrator** | ❌ Not started |
| **Hermite integrator** | ❌ Not started |
| **`paper.md` + `paper.bib`** | ❌ Not started |
| Software version tagged on GitHub | ❌ Pending |
| DOI via Zenodo | ❌ Pending (after tag) |

---

## Timeline

```
June 2026      SPICE validation (2 weeks)
July 2026      Yoshida 4th (1 week) + Hermite integrator (2 weeks)
August 2026    Update benchmarks + notebooks (1 week) + paper.md (2 weeks)
September 2026 Cleanup, tag v2.0.0, Zenodo DOI, submit to JOSS
```

---

## What's Left

### Step 1 — SPICE Validation ⏳ current

Compare simulated trajectories against NASA SPICE kernels (ground truth) to produce a quantitative position accuracy claim for the paper.

**Why:** Energy drift is an internal consistency check. SPICE gives an external ground truth — *"our simulated Jupiter is within X km of the real trajectory after 1 year."* That's a publishable accuracy claim.

- [ ] `pip install spiceypy`
- [ ] Download `de440.bsp`, `naif0012.tls`, `pck00011.tpc` from NAIF
- [ ] Write `python/spice_validate.py` — queries SPICE position at each snapshot time, returns error per body
- [ ] Notebook `examples/07_spice_validation.ipynb` — simulate, compare, plot position error over time
- [ ] Record results in `docs/paper_notes.md`

**Kernels:**
```
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc
```

---

### Step 2 — Yoshida 4th-Order Symplectic Integrator

Add a 4th-order symplectic integrator for long-term integrations (million-year runs). Composes three Leapfrog sub-steps with published Yoshida coefficients. Directly strengthens the paper's integrator comparison story.

- [ ] Add `Integrator::Yoshida4` to enum in `include/simulation.h`
- [ ] Implement in `src/core/simulation.cpp` alongside Leapfrog (~30 lines)
- [ ] Expose via `py::enum_<Integrator>` in `orbit_py/bindings.cpp`
- [ ] Add CLI support (`--integrator yoshida4`)
- [ ] Add conservation test
- [ ] Update notebooks 03 and 05/06 to include Yoshida in comparisons

---

### Step 3 — paper.md + paper.bib

Write the JOSS paper after SPICE and Yoshida are done. See `docs/paper_notes.md` for benchmark tables, draft Statement of Need, and claims inventory.

- [ ] Summary (~200 words)
- [ ] Statement of Need (~300 words) — *must be written by the author*
- [ ] Mathematics and Numerical Methods
- [ ] Validation (SPICE results + REBOUND comparison)
- [ ] Example Usage
- [ ] `paper.bib` with all references
- [ ] Verify renders with `pandoc`

**Target length:** ≤ 1000 words (JOSS limit).

---

### Step 4 — Release and Submit

- [ ] Bump version `2.0.0-dev` → `2.0.0` in `bindings.cpp` and `pyproject.toml`
- [ ] Merge all open branches to `main`
- [ ] Tag `v2.0.0` on GitHub
- [ ] Register DOI via Zenodo
- [ ] Submit to JOSS

---

## Post-Submission (v2.1+)

- **Wisdom-Holman symplectic map** — Keplerian decomposition for million-year integrations. See [keplerian_physics.md](keplerian_physics.md) for full background.
- See [FUTURE.md](FUTURE.md) for all other directions: binary I/O, native app, systems database, collision/merge/rogue-body detection.

---

*Last updated: June 2026. Update this file as work completes.*
