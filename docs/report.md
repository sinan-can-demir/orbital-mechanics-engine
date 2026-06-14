# Code Review Report — Orbital Mechanics Engine

**Date:** 2026-06-12
**Branch:** `feature/spice-validation`
**Scope:** Full source pass + runtime edge-case testing

---

## Methodology

- Read all source files (~4,900 lines across `src/`, `include/`, `orbit_py/`, `tests/`)
- Ran all 8 ctests and 4 pytests (all pass at baseline)
- Manual edge case testing: zero dt, negative steps, negative mass, body collision (r=0),
  malformed JSON, all integrator names via CLI and Python
- Verified RK45 Dormand-Prince coefficients against Dormand & Prince (1980)
- Verified Yoshida4 coefficients and code structure

---

## CRITICAL

### 1. `Integrator.Euler` and `Integrator.RK45` silently run RK4

The Python API exposes five integrators: RK4, Leapfrog, RK45, Euler, Yoshida4. Only three
actually work. In `runSimulationCore` (`src/core/simulation.cpp:430`):

```cpp
if (integrator == Integrator::Leapfrog)
    leapfrogStep(bodies, dt);
else if (integrator == Integrator::Yoshida4)
    yoshida4Step(bodies, dt);
else
    rk4Step(bodies, dt);     // Euler and RK45 land here silently
```

Calling `orbit.simulate(..., integrator=orbit.Integrator.Euler)` runs RK4. Same for
`Integrator.RK45`. No error, no warning. Confirmed numerically:

```
RK4 == RK45?  True
RK4 == Euler? True
```

The `eulerStep` function (`src/core/simulation.cpp:63`) takes a single `CelestialBody`, not a
vector — it was never wired into the N-body dispatch loop. The adaptive
`runSimulationAdaptiveCore` is only reachable via `orbit.simulate_adaptive()`; the
`Integrator.RK45` enum value does nothing in `simulate()`.

**Fix:** Either (a) implement an N-body Euler wrapper and add it to the dispatch, or (b) remove
`Euler` and `RK45` from the Python enum and document that `simulate()` supports only
`RK4 | Leapfrog | Yoshida4`. Option (b) is safer and honest.

---

### 2. G constant inconsistency between C++ engine and SPICE IC builder

| File | Value |
|------|-------|
| `include/utils.h:14` | `6.67430e-11` |
| `python/spice_ic.py:21` | `6.674e-11` |

A 0.005% difference. `spice_ic.py` derives body masses as `mass = GM / G`. Using a different G
than the simulation produces masses that are systematically biased. For Earth:
`GM_Earth = 3.986004418e14 m³/s²`, giving a mass discrepancy of ~2×10²⁰ kg (~0.003% of Earth
mass). This error propagates into the SPICE validation numbers. The SPICE notebook currently
attributes all residual error to missing GR/J2, but part of it is this G mismatch.

**Fix:** Pick one value and use it everywhere. CODATA 2018: G = 6.67430e-11 (the value already
in `utils.h`) is correct. Update `python/spice_ic.py`.

---

### 3. `validate` returns exit code 0 for invalid physics

```
$ orbit-sim validate --system neg_mass.json
✅ System is valid: 2 bodies
 - NegMass | mass = -1e+24 ...
   ⚠️  Warning: non-positive mass.
Exit code: 0
```

The warning is printed but `validateSystemFile` (`src/io/validate.cpp:34`) returns `true`
regardless. A CI script using `if orbit-sim validate; then orbit-sim run ...` would not catch
this. Invalid systems propagate silently downstream.

**Fix:** Set a `valid = false` flag when a non-positive mass is found and return it at the end.
One flag, one return-value change.

---

## HIGH

### 4. `SimulationSnapshot.conservation` not exposed in Python bindings

The C++ `SimulationSnapshot` struct has a `ConservationSnapshot conservation` field containing
`dE`, `dL`, `dP`, `total_energy`, `kinetic_energy`, `Lmag`, etc. The Python bindings
(`orbit_py/bindings.cpp:68`) only expose four fields:

```cpp
py::class_<SimulationSnapshot>(m, "SimulationSnapshot")
    .def_readonly("step", ...)
    .def_readonly("time_s", ...)
    .def_readonly("dt_used", ...)
    .def_readonly("positions", ...);
    // conservation is missing
```

Any Python code that tries `snap.conservation.dE` throws:

```
AttributeError: 'orbit.SimulationSnapshot' object has no attribute 'conservation'
```

The only workaround is `result.energies_numpy()`, which gives total energy only. Relative drift
`dE`, angular momentum `dL`, and linear momentum `dP` are unreachable from Python.

**Fix:** Bind `ConservationSnapshot` and add `.def_readonly("conservation", &SimulationSnapshot::conservation)`
to the `SimulationSnapshot` binding.

---

### 5. `--dt 0` and `--steps 0` silently fall back to defaults

In `src/cli/main.cpp:279`:

```cpp
int steps = (opt.steps > 0 ? opt.steps : 8766);
double dt  = (opt.dt > 0   ? opt.dt   : 3600.0);
```

Running `orbit-sim run --system ... --steps 0 --dt 0` prints `Steps: 8766` and `dt: 3600
seconds` — the user's values are silently discarded. Same for negative inputs. A parameter sweep
script that accidentally passes `--steps 0` will run 8766 steps and never know.

**Fix:** When a flag is explicitly supplied with a zero or negative value, print an error and
exit 1. Use defaults only when the flag is absent (i.e., when `opt.steps` was never set).

---

## MEDIUM

### 6. `evaluateStateDerivatives` declared but never defined

`include/simulation.h:50` declares:

```cpp
std::vector<StateDerivative> evaluateStateDerivatives(std::vector<CelestialBody>& bodies);
```

The actual implementation in `simulation.cpp` is named `evaluateDerivatives` (no `State` infix)
and is a file-scope function. The declared name is never defined anywhere. Any external code that
calls `evaluateStateDerivatives` gets a linker error. It is dead API surface that contradicts the
implementation.

**Fix:** Either rename the `.cpp` function to `evaluateStateDerivatives` to match the header, or
remove the declaration from the header entirely.

---

### 7. Singularity guard is a hard silent cutoff

In `src/core/simulation.cpp:32`:

```cpp
if (r2 < 1.0)
{
    return;  // silently skip force computation
}
```

When two bodies come within 1 meter of each other, gravitational force is set to zero with no
message, warning, or log entry. Two massive bodies placed at identical coordinates produce no
force at all — they are completely frozen:

```
step,x_A,...,x_B,...
0,0,0,0,0,0,0
1,0,0,0,0,0,0    ← frozen, no error
```

This is fine for planetary simulations where r < 1m never occurs, but in any other context
(unit-normalized systems, scaled test cases) it silently disables physics. There is also no
softening — the force jumps discontinuously from zero at r < 1m to the full Newtonian value at
r = 1m.

**Fix:** At minimum, emit a one-time `std::cerr` warning the first time this guard fires,
including body names and actual separation. Better: replace the hard cutoff with a proper
softening parameter `r² + ε²` in the denominator.

---

### 8. Yoshida4 comment says DKD but code is KDK

`src/core/simulation.cpp:224`:

```cpp
// Palindromic DKD sequence: kick c1, drift w1, kick c2, drift w0, kick c2, drift w1, kick c1
```

The sequence literally starts with a **kick** (velocity update), making it **KDK** (velocity
Verlet form), not DKD (position Verlet form). The comment lists "kick c1" first. The study
notes in `docs/yoshida.md` describe the DKD variant (drift first), creating a secondary
discrepancy between documentation and implementation. Both are valid Yoshida4 forms, but the
comment mislabels the one that is actually implemented.

**Fix:** Change "Palindromic DKD sequence" to "Palindromic KDK sequence" in the comment, and
optionally add a note in `docs/yoshida.md` that the C++ uses KDK while the notes show DKD.

---

### 9. RK45 FSAL property not exploited (known gap)

Each adaptive RK45 step computes 7 derivative evaluations (k1–k7). The code acknowledges this:

```cpp
// ── k7 at y5 (FSAL: this will be k1 of the next step) ────────────────────
// (Not yet exploited here — left as future optimization.)
```

Saving k7 and reusing it as k1 of the next accepted step would reduce evaluations from 7 to 6,
a ~14% speedup for long adaptive runs. This is a known limitation, noted for completeness.

---

## LOW / TEST QUALITY

### 10. `test_leapfrog.cpp` doesn't pre-seed accelerations

`tests/test_leapfrog.cpp:31` calls `leapfrogStep` in a loop without first calling
`updateAccelerations`. Since bodies load from JSON with `acceleration = (0,0,0)`, the first
half-kick does nothing — the first integration step is a position-only update, not a true
leapfrog step. The test still passes because the bounded-drift assertion is lenient (factor 100×
headroom), but the starting trajectory is subtly wrong.

Compare with `tests/test_yoshida4.cpp:36`, which correctly calls `updateAccelerations(bodies)`
before the first step.

**Fix:** Add `updateAccelerations(bodies);` before the integration loop in
`test_leapfrog.cpp`.

---

### 11. Legacy 3-body `physics::compute()` overload

`src/core/conservations.cpp:56` contains a `compute(sun, earth, moon)` function that manually
hard-codes three bodies. The N-body `compute(vector<CelestialBody>&)` at line 111 does the same
thing generically and is what all production code uses. The 3-body version is pure technical
debt: it duplicates logic, won't scale, and would silently give wrong results for a Sun-Earth-Moon
system with differently named bodies (e.g., "Sol" instead of "Sun").

**Fix:** Delete the 3-body overload and update its declaration in `include/conservations.h`.

---

### 12. `utils.h` is misnamed, mostly dead, and has wrong values

The file is named `utils.h` but contains only physical constants — it should be `constants.h`.
The namespace encapsulation (`physics::constants`) is already in place and correct.

Of the 13 constants defined, only 4 are actually used anywhere in the codebase:

| Constant | Used | Where |
|----------|------|-------|
| `G` | Yes | `simulation.cpp:47`, `conservations.cpp:74,147` |
| `R_SUN`, `R_EARTH`, `R_MOON` | Yes | `eclipse.cpp:46–48` |
| `DT`, `M_SUN`, `M_EARTH`, `M_MOON` | **No** | — |
| `MOON_INCLINATION`, `AU` | **No** | — |
| `EARTH_PERIHELION`, `EARTH_APHELION` | **No** | — |
| `MOON_ORBIT_RADIUS` | **No** | — |

9 of 13 constants are dead code. `DT = 3600` is particularly wrong here — it is a default
simulation parameter, not a physical constant, and does not belong in a constants file.

The dead mass constants also have wrong values vs what the JSON system files actually use:

| Constant | `utils.h` | JSON files | Error |
|----------|-----------|------------|-------|
| `M_SUN` | `1.9891e30` | `1.98847e30` | 0.032% |
| `M_EARTH` | `5.972e24` | `5.9722e24` | 0.003% |
| `M_MOON` | `7.3477e22` | `7.342e22` | 0.077% |

None of these mass constants affect the simulation (masses come from JSON), but a developer
reading the file and trusting these values would use the wrong numbers in any new code. There are
also no source citations (CODATA, IAU) to verify any value against.

**Fix:**
1. Rename `utils.h` → `constants.h` and update `#include "utils.h"` everywhere (4 files).
2. Delete the 9 unused constants (`DT`, mass constants, orbital constants).
3. Correct the mass values if they are kept, and add CODATA/IAU citations.
4. Update the include guard from `UTILS_H` to `CONSTANTS_H`.

---

### 13. Test coverage is thin

| Gap | Impact |
|-----|--------|
| No test for adaptive RK45 path | `runSimulationAdaptiveCore` has zero test coverage |
| No test catching Euler/RK45 silent fallthrough | The critical bug above goes untested |
| `test_two_body.cpp`: only 30 days | Full-orbit return-to-origin drift never measured |
| Conservation threshold `1e-5` is loose | Passes at 0.001% drift — tight would be `1e-8` |
| No test for `validate` exit codes | The exit-0-on-bad-mass bug above goes untested |
| No Python test for `simulate_adaptive` | Zero coverage for the adaptive integrator path |
| No Python test for stride behavior | Stride semantics are untested |
| 12 tests total for ~4,900 LOC | ~1 test per 400 lines |

---

## Summary

| # | Severity | Issue |
|---|----------|-------|
| 1 | Critical | `Euler` + `RK45` silently use RK4 in Python API |
| 2 | Critical | G constant mismatch between C++ and SPICE IC builder (0.005%) |
| 3 | Critical | `validate` exits 0 even when it finds non-positive mass |
| 4 | High | `conservation` field not exposed in Python `SimulationSnapshot` |
| 5 | High | `--dt 0` / `--steps 0` silently fall back to defaults, no warning |
| 6 | Medium | `evaluateStateDerivatives` declared in header but never defined |
| 7 | Medium | Singularity guard is a hard silent cutoff with no warning or softening |
| 8 | Medium | Yoshida4 comment says "DKD" but implementation is KDK |
| 9 | Medium | FSAL optimization in RK45 not implemented (noted in code) |
| 10 | Low | `test_leapfrog.cpp` misses `updateAccelerations` pre-seed |
| 11 | Low | Legacy 3-body `physics::compute()` overload duplicates N-body version |
| 12 | Low | `utils.h` misnamed, 9/13 constants dead, mass values wrong vs JSON |
| 13 | Low | Thin test coverage (12 tests for ~4,900 LOC) |

**Suggested fix order for a debug branch:**
1. Fix #3 (validate exit code) — one line
2. Fix #8 (wrong comment) — one word
3. Fix #12 (rename utils.h → constants.h, delete dead constants, fix mass values) — ~15 min
4. Fix #2 (G constant in spice_ic.py) — one number
5. Fix #5 (CLI silent fallback) — two guard clauses
6. Fix #1 (remove Euler/RK45 from Python enum or implement them properly)
7. Fix #4 (bind conservation to Python) — ~5 lines in bindings.cpp
8. Fix #6 (remove dead declaration or rename) — one line
9. Fix #10 (pre-seed in leapfrog test) — one line
10. Fix #11 (delete 3-body overload) — delete ~45 lines
