# Roadmap — Orbital Mechanics Engine

**Author**: Sinan Can Demir  
**Last Updated**: June 2026  
**GitHub**: https://github.com/sinan-can-demir/orbital-mechanics-engine  
**Target**: JOSS submission at v2.0

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
| **`paper.md` + `paper.bib`** | ❌ Not started |
| Software version tagged on GitHub | ❌ Pending |
| DOI via Zenodo | ❌ Pending (after tag) |

---

## What's Left

### Phase 5a — SPICE Validation (current)

Compare simulated trajectories against NASA SPICE kernels (ground truth) to produce a quantitative position accuracy claim for the paper.

**Why:** Energy drift is an internal consistency check. SPICE comparison gives an external ground truth — *"our simulated Jupiter is within X km of the real trajectory after 1 year."* That's a publishable accuracy claim.

**Steps:**
1. Install `spiceypy`, download `de440.bsp`, `naif0012.tls`, `pck00011.tpc`
2. Write `python/spice_validate.py` — takes a `SimulationResult`, returns position error per body
3. Notebook `examples/07_spice_validation.ipynb` — run simulation, query SPICE, plot error
4. Record results in `docs/paper_notes.md` for use in paper.md

**Kernels:**
```
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls
https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/pck00011.tpc
```

### Phase 5b — paper.md

Write the JOSS paper after SPICE results are in hand. See `docs/paper_notes.md` for findings, draft Statement of Need, and benchmark tables.

**Structure:**
- Summary (~200 words)
- Statement of Need (~300 words) — *must be written by the author*
- Mathematics and Numerical Methods
- Validation (SPICE results + REBOUND comparison)
- Example Usage
- References (`paper.bib`)

**Target length:** ≤ 1000 words (JOSS limit).

### Phase 5c — Release and Submission

1. Bump version from `2.0.0-dev` → `2.0.0` in `bindings.cpp` and `pyproject.toml`
2. Merge all open branches to `main`
3. Tag `v2.0.0` on GitHub
4. Register DOI via Zenodo
5. Submit to JOSS

---

## Future Directions

See [FUTURE.md](FUTURE.md) for longer-term ideas: Yoshida 4th-order symplectic integrator, binary I/O format, native desktop GUI, systems database, collision/merge/rogue-body detection.

---

*Last updated: June 2026. Update this file as work completes.*
