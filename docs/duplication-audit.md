# Duplication & Redundancy Audit

Date: 2026-10-30 · Branch: `feat/vk-lighting-trails` · Status: **findings only, no code changed**

Six read-only audits, one per module. Findings are `file:line` references against the branch at audit time.
Spot-checked by hand: the items marked ✔ were re-verified in the code after the fork reported them.
Items not marked are the fork's claim and should be re-checked when each refactor starts.

Severity key: 🐞 = real bug found while auditing · 🗑 = dead code (pure deletion) · ♻ = duplication · 🧪 = test-only

---

## 1. Bugs found along the way (fix independently of the refactor)

| ID | Where | Problem |
|----|-------|---------|
| 🐞 VIEW-4 ✔ | `src/viewer/orbit_viewer.cpp:1030` | `buildOrbitBuffers()` called every frame; allocates new VAO/VBO per body per frame, never frees them, re-uploads full trajectory. Fix: delete the line (already built at `:926`). |
| 🐞 CLI-6 ✔ | `src/cli/cli.cpp:~224-238` | `printCommandHelp("run")` has no `return`; prints the help, then "No help available for command: run". Help also omits `hermite`. |
| 🐞 PY-7 ✔ | `benchmark_rebound.py:12`, notebooks 05/06 (09 was already correct) | `G = 6.674e-11` vs engine's `6.67430e-11`. REBOUND comparison uses a different G (~4.5e-5 relative). Re-run after fixing. (`docs/report.md` already lists this; `spice_ic.py` was fixed, these were missed.) |
| 🐞 IO-3 ✔ | `horizons_builder.cpp:29` vs `python/spice_ic.py:90` | Mercury mass `3.3011e23` vs `3.301e23`. |
| 🐞 IO-1 | `horizons.cpp` POST path (`:207-335`) | Skips the API `"error"` field check and does not percent-encode the form body (GET path does both). |
| 🐞 PY-1 ✔ | `python/3Dexaggerated_plot.py:106` | `if` where the other three scripts use `elif`: saves MP4, then also tries GIF and prints a false "not found" warning. |
| 🐞 CLI-1/7 | `src/cli/main.cpp:245`, `:228-265` | `--stride` unchecked on `build-system --run`; `--integrator`/`--gr`/`--normalize` accepted then silently ignored there (hardcoded RK4). |
| 🐞 CLI-5 | `main.cpp:60` vs `validate.cpp:34` | `info` and `validate` print the same per-body line in different formats (drift). |
| 🐞 PY-4 | 4 plot scripts | `pd.read_csv` without `comment='#'` while the CSV writer emits a `#` metadata line. |

---

## 2. Dead code (pure deletion, lowest risk)

| ID | What | ~Lines |
|----|------|--------|
| CORE-8 ✔ | `include/ray.h`, `include/main.h` (umbrella; replace with direct includes in `cli/main.cpp`), `computeAcceleration` decl (`simulation.h:43`), `eulerStep` + `Integrator::euler` (bindings/tests reference its absence), `angular_term` (`conservations.cpp:30`), `detectSEM` (`simulation.cpp:404`), unused `dp45` C2..C7 | ~130 |
| VIEW-3 ✔ | `shaders/` (3 files, one 0-byte), `viewer_config.json`: referenced by nothing | ~47 |
| VIEW-1 ✔ | `src/viewer/csv_loader.cpp` + `include/viewer/csv_loader.h` + `tests/test_csv_loader.cpp`: unused by the viewer (see §3) | ~150 |
| PY-11 | unused imports in `horizons_validation.py`, `spice_ic.py`; unused `HORIZONS_PARSER`, `STRIDE`; overwritten axis limits in `3Dplot.py:69-73` | ~10 |
| IO-8 / IO-7 | stale comments, commented-out URL in `horizons.cpp`, redundant second `loadSystemFromJSON` in `horizons_builder.cpp:228-239` | ~20 |
| PY-6 | stray duplicate docstring `utils.py:1-9` | 9 |
| VIEW-4 | unused `size`/`sizeSel` `orbit_viewer.cpp:1038-1039` | 2 |

---

## 3. Duplication, by theme

### A. Constants & defaults: one source of truth
| ID | Duplicated | Proposed home |
|----|-----------|---------------|
| CORE-9, CLI-1, CLI-9 | `G` redeclared in 2 tests + Python; `86400.0` and RK45 tolerances in `simulation.h` ×2 and `bindings.cpp` ×2; `8766` / `3600.0` literals ×4 in `main.cpp` | `constants.h` + `physics::defaults::{...}` (steps, dt, dt_max, tolerances) |
| IO-3, VIEW-6 | Body masses/NAIF IDs in `horizons_builder.cpp`, `spice_ic.py`, `spice_validate.py`, `orbit_viewer.cpp:442`, 5 test files | `M_SUN`, `M_EARTH` in `constants.h`; `include/known_bodies.h` (`lookupHorizonsBody`); Python reads a data file or has a drift test |
| PY-7 | `G`, `86400`, `365.25*86400`, `1000.0`, AU across scripts + 5 notebooks | `python/constants.py` |
| CLI-4 | `"@0"`, `"1 d"` defaults inline; `fetchCenter`/`buildCenter` split fields | defaults in `HorizonsFetchOptions`; one `center` field |
| CORE-6 | close-approach threshold `1.0` in two places | `kMinSeparationSq` |
| VIEW-8 | default CSV path in CLI and viewer | `kDefaultOutputCSV` |

### B. Simulation core (`src/core/simulation.cpp`)
| ID | Duplication | Proposal | Risk |
|----|------------|----------|------|
| CORE-1 | fixed/adaptive runners share baseline (E0/L0/P0), names/masses, positions (~30 lines); `sqrt(x²+y²+z²)` ×4 | `ConservationBaseline baselineOf(bodies)`, `initResult(...)`, `positionsOf(...)`, `norm(array)` | low (bit-identical) |
| CORE-2 | leapfrog 3 loops, Yoshida4 7 loops of kick/drift | `kick(bodies,h)` / `drift(bodies,h)` + coefficient table | low–med: **keep `acc * (c*dt)` operation order** |
| CORE-3 | `buildIntermediateState` ≡ `…Multi` with one term; RK45 error-state roundtrip `(x+δ)-x` | merge via weighted-sum helper | **med: changes last bits of RK4**; roundtrip fix changes step-accept decisions, so decide separately |
| CORE-4 | Sun/Earth/Moon lookup twice (one copy dead) | `findBodyIndex(names, name)`; name constants | low |
| CORE-5 | CSV export: 4 near-copies of row writing (fixed vs adaptive) | row/header writers + shared `csv_schema.h` (also read by viewer, Python) | low once golden CSV test exists |
| CORE-7 | `p = m*v` computed in two loops; `P`/`L` are `std::array` while all else is `vec3` | merge loops; optional `vec3` type change (API-affecting) | low (merge) / med (type) |
| CORE-10, IO-6 | JSON vec3 read/write hand-rolled in loader, writer, `spice_ic.py` | `vec3ToJson`/`vec3FromJson` or ADL `to_json`/`from_json` for `CelestialBody` | low, low value |

### C. IO (`src/io`)
| ID | Duplication | Proposal | Risk |
|----|------------|----------|------|
| IO-1 | GET vs POST Horizons fetch: ~110 of ~140 lines identical (`fixDate`, curl setup, response check, debug dump, JSON parse) | `normalizeHorizonsTime`, `httpPerform`, `extractHorizonsResult`, `writeTextFile`, `CurlHandle` RAII. **Decide drift behavior first** (POST should get error check + encoding) | med; network code, needs pure-function tests first |
| IO-2 | `trim` identical in `horizons_builder.cpp:36` and `cli/main.cpp:19` | `include/string_utils.h` (`trim`, `splitCsv`) | low |
| IO-4 | Horizons text parsed again, less strictly, in `horizons_validation.py` | expose parser via `orbit_py` binding (or `orbit-sim parse`) | low–med |
| IO-5 | `nearlyEqual` + field compare duplicated in test | `bodiesNearlyEqual(a,b,&why)` shared | low |

### D. CLI & bindings
| ID | Duplication | Proposal | Risk |
|----|------------|----------|------|
| CLI-1 | steps/dt validation ×3 (two shapes in `main.cpp`, one in `bindings.cpp`) | move checks into `runSimulationCore` (already validates stride) + `resolveSimParams` | low |
| CLI-2 | integrator name in 4 places (CLI parse, display, error text, help, pybind enum); they disagree (help omits `hermite`) | `integrator_names.h`: table + `parseIntegrator`/`integratorName`/`validIntegratorList` | low |
| CLI-3 | 11 "Must specify --x" blocks | `requireArg(...)` | low |
| CLI-6 | command list/help text written 3×, drifting | `CommandSpec` table → generate global/command help | low–med |
| CLI-7/8 | `build-system --run` is a partial copy of `run`; `main()` is 280 lines | `cmdInfo/cmdRun/…` handlers + dispatch table; `build-system --run` calls `cmdRun`. **Decide flag behavior first** | med |
| CLI-5 | per-body print format ×2 | `formatBodyLine(body)` / `operator<<` | low |

### E. Viewers
| ID | Duplication | Proposal | Risk |
|----|------------|----------|------|
| VIEW-1 | two CSV parsers; the unused one is the tested one; real one (`initBodiesFromCSV`, `orbit_viewer.cpp:561-712`) is `static` in a `main` file with no tests | `trajectory_loader.{h,cpp}` (no GL), shared header-name constants; retarget test | low–med |
| VIEW-2 | sphere generator copied GL ↔ VK (~40 lines; GitHub #78) | `sphere_geometry.h` `generateUVSphere(...)` | low (testable) |
| VIEW-3 | Blinn-Phong lighting in GL inline GLSL and `sphere.frag` | keep; low value while VK paused | n/a |
| VIEW-5, VIEW-7 | small GL boilerplate repeats; VK pipeline builders share ~80 lines | defer (VK paused) | med |

### F. Python
| ID | Duplication | Proposal | Risk |
|----|------------|----------|------|
| PY-1/2/3 | animation save block ×4; `set_axes_equal` ×2; three near-identical 3D scripts (~345 lines) | `python/common.py` (`save_animation`, `set_axes_equal`); collapse to `plot_orbit_3d.py --mode` | low–med |
| PY-4/6 | repo-root/CSV path discovery ×6; conservation plot boilerplate | `common.REPO_ROOT`, `load_orbit_csv()` (with `comment='#'`), `_finish_plot` | low |
| PY-5 | 3 different `sys.path` hacks | pick package-style imports | low |
| PY-8 | REBOUND setup copied in benchmark + notebooks 05/06 | `rebound_helpers.py` | low–med |
| PY-9 | kernel dir/loader ×3 | `spice_common.py` | low |
| PY-10 | two subprocess styles (`shell=True` in one) | `run_orbit_sim(args: list)` | low–med |
| PY-12 | notebook helpers | defer until `common.py` exists | med (JSON diffs) |

### G. Tests
| ID | Duplication | Proposal |
|----|------------|----------|
| TEST-1 | 8 copy-pasted CMake test blocks (~60 lines) | `add_orbit_test(name)` function; **keep ctest names** (e.g. `cli_smoke`) |
| TEST-2/3 | hand-rolled PASS/FAIL harness ×~45; Sun/Earth fixtures ×6 | `tests/test_helpers.h`: `CHECK`, `CHECK_NEAR`, `makeSunEarth`, `circularSpeed`, `nearlyEqual` |
| TEST-4 | energy-drift loop ×4 (`maxEnergyDrift` exists in `test_yoshida4` only); leapfrog scenario run twice | share helper |
| TEST-5 | 🧪 Kepler "Test 3" in `test_two_body.cpp:66-76` cannot fail (ratio 1.0 by construction) | replace with a real one-orbit period check, or delete |
| TEST-6/7 | `std::system` + temp-file patterns, leftovers on failure | `runCmd`, `TempFile` RAII, CMake-provided exe/system paths |
| TEST-8 | `tests/README.md` out of date | fix by hand, last |

---

## 4. Coverage that must exist BEFORE each refactor

| New test | Protects |
|----------|----------|
| Golden final state, 1000 steps × {rk4, leapfrog, yoshida4, hermite} on `earth_moon.json` | CORE-2, CORE-3 |
| Golden RK45 run (final state + accepted-step count) | CORE-3 |
| Golden CSV (fixed + adaptive) | CORE-5, VIEW-1, PY-4 |
| `SimulationResult` fields / baselines; conservation values for fixed 3-body input | CORE-1, CORE-7 |
| `parseIntegrator` round-trip; CLI `--integrator hermite` and bad name | CLI-2 |
| `resolveSimParams` / steps, dt, stride validation | CLI-1 |
| `run --help` output has no "No help available" | CLI-6 |
| `normalizeHorizonsTime`, `extractHorizonsResult` on canned JSON | IO-1 |
| `kKnownBodies` table vs Python table | IO-3 |
| Real viewer CSV parser, fixed + adaptive, any body count | VIEW-1 |
| `generateUVSphere` counts/winding | VIEW-2 |
| Python: `simulate` with each integrator, `simulate_adaptive` happy path, `load_orbit_csv` with `#` line | CORE-1, CLI-2, PY-4 |

---

## 5. Audit caveats
- Forks for modules 2, 3, 6 made only 5–6 tool calls each; I re-verified their headline claims (✔) but not every line.
- Cross-module items appear under the module where the fork found them; IDs cross-reference.
- `3Draytracing.py` was only grepped for helpers/paths, not audited for internal duplication.
- Vulkan code (`src/viewer_vk`) findings are low priority; user deferred Vulkan work.

---

## 6. Refactor plan

Decisions (2026-10-30):
- **Numerics:** bit-identical refactors only. CORE-3 (merge intermediate-state builders) and the RK45 `err_delta` roundtrip fix are **deferred**.
- **CLI:** `build-system --run` reuses the `run` handler, so `--integrator`/`--gr`/`--normalize` are honored.
- **Body table:** shared data file (or two tables) plus a drift test that fails if C++ and Python disagree.

Each phase = one or more branches/PRs. Full `ctest` + `pytest` + `make format` before every PR. Phases 3+ depend on phase 2's tests.

| Phase | Scope | Items | Gate |
|-------|-------|-------|------|
| 1. Bugs + dead code | Independent small fixes and pure deletions | VIEW-4, CLI-6 (+ regression test), PY-7 (G), IO-3 Mercury, PY-1 `elif`; delete CORE-8, VIEW-3, VIEW-1 (CSVLoader + its test), PY-11, IO-7/8 | builds, tests green |
| 2. Safety net | Test infra and golden tests | `tests/test_helpers.h` (TEST-2/3/4/6/7), `add_orbit_test` (TEST-1; keep ctest names), TEST-5 fix; golden integrator states, golden CSV, `SimulationResult`/conservation values, CLI `--integrator` | goldens recorded on **pre-refactor** code |
| 3. Constants | One source of truth | CORE-9, CORE-6, CLI-4, IO-3 (+ drift test), VIEW-6, VIEW-8, `python/constants.py` | goldens unchanged |
| 4. Core + CLI | Bit-identical core refactors; CLI cleanup | CORE-1, CORE-2 (keep `acc*(c*dt)` order), CORE-4, CORE-5 (+ `csv_schema.h`), CORE-7 (loop merge only); CLI-1 (`resolveSimParams`), CLI-2 (`integrator_names.h`), CLI-3, CLI-5, CLI-6/7/8 (handlers, dispatch, `build-system --run` → `cmdRun`) | goldens bit-identical |
| 5. IO + viewer | Network and viewer code | IO-2 (`string_utils.h`), IO-5/6, IO-1 (pure-function tests first; POST gets error check + encoding), IO-4 (parser binding); VIEW-1 (`trajectory_loader` + tests), VIEW-2 (`sphere_geometry.h`) | golden CSV loads in new loader |
| 6. Python | Shared helpers | `python/common.py` (PY-1/2/4/5/6), collapse 3D scripts (PY-3), PY-9, PY-10, PY-8; notebooks (PY-12) last | scripts run by hand; `load_orbit_csv` test |
| Deferred | Not now | CORE-3 and roundtrip fix, CORE-7 `vec3` type change, VIEW-3 lighting share, VIEW-5/7 (Vulkan paused), CORE-10 (low value) | |

### Phase 1 status (2026-10-30, branch `refactor/phase1-bugs-deadcode`, uncommitted)
Done: VIEW-4, CLI-6 (+ `test_cli` Test 9), PY-7 (benchmark + notebooks 05/06 source), IO-3 Mercury, PY-1 `elif`; deleted CORE-8 items (`ray.h`, `main.h`, `computeAcceleration`, `eulerStep`, `Integrator::euler`, `angular_term`, `detectSEM`), VIEW-3, VIEW-1 (CSVLoader + test + CMake), PY-11, PY-6, IO-7.
Left for later phases: `dp45` C2..C7 (documentation value, kept), IO-8 comment cleanup.
Follow-up: notebooks 05/06 committed outputs are stale (computed with the old G); re-execute them.

### Phase 2 status (2026-10-30, branch `refactor/phase2-test-infra`, stacked on phase 1)
Done: `tests/test_helpers.h` (CHECK macros, `makeSunEarth`, `circularSpeed`, `maxEnergyDrift`, `nearlyEqual`, `TempDir`), `add_orbit_test()` CMake function (ctest names unchanged), TEST-5 (Kepler check now compares the simulated swept angle), `test_golden` (integrators, RK45, `SimulationResult` fields, fixed + adaptive CSV; `GOLDEN_STRICT=1` for bit-identity), Python cross-check against the golden values, README refresh.
Verified: reordering the Yoshida4 kick operations is caught by `GOLDEN_STRICT=1` (6 mismatches) and passes the default tolerances.
Not yet done (carry into the phase that needs them): `runCmd` + `TempDir` in `test_cli.cpp` and the CMake-provided exe paths (TEST-6), `TempFile` use in `test_horizons_parser`/`test_system_writer` (TEST-7), `test_leapfrog` still hand-rolled, `test_conservation`/`test_barycenter` not yet on the shared helpers. Tests for `parseIntegrator`, `resolveSimParams`, `normalizeHorizonsTime`, the viewer CSV parser and `generateUVSphere` are added in the phase that introduces each function.
