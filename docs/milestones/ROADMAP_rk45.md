# Roadmap — RK45 Adaptive Integrator

*Adding an adaptive Dormand–Prince RK45 integrator to replace fixed-timestep RK4
for long-duration simulations where chaos and energy drift are a concern.*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026  
**Motivation**: Fixed-timestep RK4 and Leapfrog both show monotonic energy drift
in the chaotic three-body problem. The drift is not a bug — it is physics. But
it can be controlled. RK45 automatically shrinks the timestep when the simulation
is moving through a difficult region (like a close Moon pass) and expands it when
things are calm (like Earth's midpoint orbit). This keeps error bounded without
the user having to guess a safe `dt`.

---

## The Mathematics — From Scratch

*Read this section carefully before looking at any code. The implementation
will make no sense without it.*

### What is a differential equation?

Your simulation solves this equation at every step:

```
d²r/dt² = a(r)
```

In plain English: **"the rate of change of velocity equals the gravitational
acceleration"**. You can't solve this exactly for three bodies — so you
approximate it numerically, stepping forward in small increments of time `dt`.

Every integrator you have — Euler, RK4, Leapfrog — is just a different strategy
for making that approximation as accurate as possible.

### What is RK4 doing? (Your existing code)

Look at `rk4Step()` in `simulation.cpp`. It computes four intermediate estimates
of the derivative (called `k1`, `k2`, `k3`, `k4`) and combines them:

```
k1 = f(y_n)                      ← derivative at current state
k2 = f(y_n + dt/2 * k1)          ← derivative at midpoint, using k1
k3 = f(y_n + dt/2 * k2)          ← derivative at midpoint, using k2
k4 = f(y_n + dt   * k3)          ← derivative at endpoint, using k3

y_{n+1} = y_n + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
```

The key insight: **more intermediate evaluations = better approximation**.
Using four evaluations makes the error shrink as `dt⁴` — that's what
"fourth-order" means. Halve `dt`, error shrinks by a factor of 16.

But here's the problem: **you chose `dt` before the simulation started**.
You don't know in advance whether `dt=3600s` is safe for the next step.
Sometimes the Moon is far away and `dt=3600s` is overkill. Sometimes the Moon
is swinging close and `dt=3600s` causes a large error. Fixed `dt` can't adapt.

### The key idea behind RK45: compute two answers at once

Dormand and Prince (1980) made a clever observation:

> *What if you computed BOTH a 4th-order estimate AND a 5th-order estimate
> using mostly the same function evaluations? Then you could compare them.
> If they agree closely, the step is accurate. If they disagree, the step
> is too large — shrink `dt` and retry.*

This comparison gives you an **automatic error estimate at every step**,
with almost no extra computational cost.

Here is the Dormand-Prince RK45 scheme. It computes **6 stages** (k1–k6):

```
k1 = f(y_n)
k2 = f(y_n + dt * (1/5)*k1)
k3 = f(y_n + dt * (3/40)*k1      + (9/40)*k2)
k4 = f(y_n + dt * (44/45)*k1     - (56/15)*k2   + (32/9)*k3)
k5 = f(y_n + dt * (19372/6561)*k1 - (25360/2187)*k2
                + (64448/6561)*k3  - (212/729)*k4)
k6 = f(y_n + dt * (9017/3168)*k1  - (355/33)*k2
                + (46732/5247)*k3  + (49/176)*k4  - (5103/18656)*k5)
```

Then two estimates of the next state:

```
4th order (y4):  uses b  coefficients — less accurate
5th order (y5):  uses b* coefficients — more accurate

y4 = y_n + dt*(35/384*k1 + 0*k2 + 500/1113*k3 + 125/384*k4
               - 2187/6784*k5 + 11/84*k6)

y5 = y_n + dt*(5179/57600*k1 + 0*k2 + 7571/16695*k3 + 393/640*k4
               - 92097/339200*k5 + 187/2100*k6 + 1/40*k7)
```

*(k7 is computed at y5 — this is the FSAL property explained below)*

### The error estimate

The difference between y4 and y5 is your error estimate:

```
error = |y5 - y4|
```

Think of it like this: y5 is a more accurate answer, y4 is a less accurate
answer. If they're nearly identical, your step was accurate. If they differ
significantly, your step was too large.

More precisely, you compute a **scalar error norm** across all bodies
and all coordinates:

```
err_i = (y5_i - y4_i) / (atol + rtol * max(|y4_i|, |y5_i|))

error_norm = sqrt( (1/N) * sum(err_i²) )
```

Where:
- `atol` = absolute tolerance (e.g. `1e-9` meters — acceptable position error)
- `rtol` = relative tolerance (e.g. `1e-9` — acceptable fractional error)
- `N` = total number of values (3 coordinates × 2 state vectors × N bodies)

If `error_norm < 1.0` → step is accepted, advance time
If `error_norm ≥ 1.0` → step is rejected, shrink `dt` and retry

### How to choose the next timestep

After each step (accepted or rejected), compute a new `dt`:

```
dt_new = dt * safety * (1 / error_norm)^(1/5)
```

Where:
- `safety = 0.9` — a fudge factor so we don't step right to the edge
- `(1/5)` — because this is a 5th-order method, error scales as `dt⁵`
- `error_norm < 1` → `dt_new > dt` (step up)
- `error_norm > 1` → `dt_new < dt` (step down, retry)

Clamp `dt_new` to prevent wild swings:

```
dt_new = clamp(dt_new, 0.1 * dt, 10.0 * dt)
dt_new = clamp(dt_new, dt_min, dt_max)
```

**Physical intuition:** When the Moon is near perigee (closest approach to Earth),
gravitational forces change rapidly. The error norm will spike, `dt` will shrink
automatically, and the integrator takes many small careful steps. When the Moon
is at apogee (farthest point), forces change slowly, error norm drops, `dt`
expands and the integrator takes large efficient steps. You get accuracy where
you need it and speed where you don't.

### The FSAL property (First Same As Last)

This is an important optimization. Notice that `k7` (computed at `y5`) is the
same as `k1` of the *next* step — because both evaluate `f` at the new state.
So you can **reuse k6 as k1 of the next step** — saving one force evaluation
per step. This reduces the effective cost from 6 evaluations to 5.

For your O(N²) force calculation with N=10 bodies, each saved evaluation
matters. This is why Dormand-Prince is the industry standard choice —
it's the most efficient RK45 variant.

---

## Current State (Honest Baseline)

| Component | Status | Notes |
|-----------|--------|-------|
| RK4 fixed-step | ✅ Working | `rk4Step()` in `simulation.cpp` |
| Leapfrog fixed-step | ✅ Working | `leapfrogStep()` in `simulation.cpp` |
| `buildIntermediateState()` | ✅ Working | Reusable for RK45 stages |
| `evaluateDerivatives()` | ✅ Working | Reusable for RK45 stages |
| Integrator enum | ✅ Working | `enum class Integrator { RK4, Leapfrog, euler }` |
| Adaptive timestep | ❌ Missing | Fixed `dt` only |
| Error estimation | ❌ Missing | No per-step accuracy check |
| RK45 implementation | ❌ Missing | Does not exist yet |
| Variable-step CSV output | ❌ Missing | Current loop assumes fixed `dt` |

**Known constraints:**
- `runSimulation()` currently loops `for (int i = 0; i < steps; ++i)` —
  this assumes fixed `dt` and a known number of steps. RK45 needs a
  time-based loop instead: `while (t < t_end)`
- The CSV header includes `step` as an integer — with adaptive stepping,
  `step` becomes meaningless. It should be replaced with `time_s` (elapsed
  seconds) so Python plotting scripts can still work
- `steps` and `dt` in `CLIOptions` need to be rethought:
  with RK45 you specify `--duration` (total simulated time) and
  `--tol` (error tolerance), not a fixed step count

---

## Phase 1 — Implement the Dormand-Prince Coefficients

*Goal: Define the RK45 Butcher tableau as constants in the codebase.*
*Estimated effort: 1–2 hours*

A **Butcher tableau** is just a table of the coefficients used in the
RK scheme. For Dormand-Prince these are fixed mathematical constants
that never change. They go in a header so both the integrator and any
future tests can access them.

### 1.1 Create `rk45_coefficients.h`

**File**: `include/rk45_coefficients.h` (new)

```cpp
#ifndef RK45_COEFFICIENTS_H
#define RK45_COEFFICIENTS_H

/**
 * Dormand-Prince RK45 Butcher Tableau
 *
 * These are exact rational fractions — do not approximate them.
 * Source: Dormand & Prince (1980), J. Computational and Applied Mathematics.
 *
 * The tableau defines:
 *   c  — time offsets for each stage (where in [t, t+dt] to evaluate)
 *   a  — how to combine previous stages to get the next stage input
 *   b  — weights for the 5th-order solution
 *   e  — weights for the error estimate (b5 - b4)
 */
namespace dp45 {

// Time offsets (c coefficients)
constexpr double C2 = 1.0/5.0;
constexpr double C3 = 3.0/10.0;
constexpr double C4 = 4.0/5.0;
constexpr double C5 = 8.0/9.0;
constexpr double C6 = 1.0;
constexpr double C7 = 1.0;

// Stage 2 coefficients (a21)
constexpr double A21 = 1.0/5.0;

// Stage 3 coefficients (a31, a32)
constexpr double A31 = 3.0/40.0;
constexpr double A32 = 9.0/40.0;

// Stage 4 coefficients (a41, a42, a43)
constexpr double A41 =  44.0/45.0;
constexpr double A42 = -56.0/15.0;
constexpr double A43 =  32.0/9.0;

// Stage 5 coefficients (a51..a54)
constexpr double A51 =  19372.0/6561.0;
constexpr double A52 = -25360.0/2187.0;
constexpr double A53 =  64448.0/6561.0;
constexpr double A54 =   -212.0/729.0;

// Stage 6 coefficients (a61..a65)
constexpr double A61 =   9017.0/3168.0;
constexpr double A62 =   -355.0/33.0;
constexpr double A63 =  46732.0/5247.0;
constexpr double A64 =     49.0/176.0;
constexpr double A65 =  -5103.0/18656.0;

// 5th-order solution weights (b coefficients)
constexpr double B1 =    35.0/384.0;
constexpr double B3 =   500.0/1113.0;
constexpr double B4 =   125.0/192.0;
constexpr double B5 = -2187.0/6784.0;
constexpr double B6 =    11.0/84.0;

// Error estimate weights (difference between 4th and 5th order)
// These are (b5_i - b4_i) — you never need b4 explicitly
constexpr double E1 =    71.0/57600.0;
constexpr double E3 =   -71.0/16695.0;
constexpr double E4 =    71.0/1920.0;
constexpr double E5 = -17253.0/339200.0;
constexpr double E6 =    22.0/525.0;
constexpr double E7 =    -1.0/40.0;

// Adaptive step control
constexpr double SAFETY   = 0.9;    // conservative scaling factor
constexpr double MIN_SCALE = 0.1;   // never shrink dt by more than 10x
constexpr double MAX_SCALE = 10.0;  // never grow dt by more than 10x
constexpr double EXP       = 1.0/5.0; // error order exponent

} // namespace dp45

#endif // RK45_COEFFICIENTS_H
```

**Why exact fractions matter:** `44.0/45.0` is not the same as `0.9777...`
in floating point. Using the exact rational form ensures your coefficients
match the published tableau exactly, which is important for reproducibility
and comparison with other implementations.

---

## Phase 2 — Implement `rk45Step()`

*Goal: A single adaptive step that returns the new state AND the suggested next dt.*
*Estimated effort: 4–5 hours*

This is the core of the implementation. The function signature is fundamentally
different from `rk4Step()` — it returns information rather than just mutating state.

### 2.1 Define the return type

**File**: `include/simulation.h`

```cpp
/**
 * RK45StepResult
 * @brief: Result of one adaptive RK45 step.
 *
 * On success (accepted = true):
 *   bodies contains the advanced state
 *   dt_used is the timestep that was actually used
 *   dt_next is the suggested timestep for the next step
 *
 * On failure (accepted = false):
 *   bodies is unchanged
 *   dt_next is a smaller timestep to retry with
 *   error_norm tells you how far over tolerance you were
 */
struct RK45StepResult {
    bool   accepted   = false;
    double dt_used    = 0.0;
    double dt_next    = 0.0;
    double error_norm = 0.0;
    int    n_rejected = 0;    // how many retries before acceptance
};
```

### 2.2 Implement `rk45Step()`

**File**: `include/simulation.h` — add declaration:

```cpp
RK45StepResult rk45Step(
    std::vector<CelestialBody>& bodies,
    double dt,
    double atol = 1e-9,   // absolute tolerance (meters / m/s)
    double rtol = 1e-9    // relative tolerance (dimensionless)
);
```

**File**: `src/core/simulation.cpp` — implementation outline:

```cpp
RK45StepResult rk45Step(std::vector<CelestialBody>& bodies,
                         double dt, double atol, double rtol)
{
    // ── Stage evaluations ──────────────────────────────────────────
    // Reuse your existing buildIntermediateState() and evaluateDerivatives()
    // These already work correctly — just call them with DP coefficients

    auto k1 = evaluateDerivatives(bodies);

    auto s2 = buildIntermediateState(bodies, k1, dt * dp45::A21);
    auto k2 = evaluateDerivatives(s2);

    auto s3 = buildIntermediateState(bodies, k1, dt * dp45::A31);
    // s3 also needs k2 contribution — extend buildIntermediateState or inline
    // (see Phase 2.3 below)
    auto k3 = evaluateDerivatives(s3);

    // ... k4, k5, k6 similarly ...

    // ── 5th-order solution ─────────────────────────────────────────
    // y5 = bodies + dt * (B1*k1 + B3*k3 + B4*k4 + B5*k5 + B6*k6)
    // Note: B2 = 0, so k2 doesn't appear in the solution

    // ── Error estimate ─────────────────────────────────────────────
    // err_i = dt * (E1*k1 + E3*k3 + E4*k4 + E5*k5 + E6*k6 + E7*k7)
    // where k7 = evaluateDerivatives(y5)  [FSAL: reuse as k1 next step]

    // ── Error norm ─────────────────────────────────────────────────
    double error_norm = computeErrorNorm(bodies, y5, err, atol, rtol);

    // ── Accept or reject ───────────────────────────────────────────
    double scale = dp45::SAFETY * std::pow(1.0 / error_norm, dp45::EXP);
    scale = std::clamp(scale, dp45::MIN_SCALE, dp45::MAX_SCALE);
    double dt_next = dt * scale;

    RK45StepResult result;
    result.error_norm = error_norm;
    result.dt_next    = dt_next;

    if (error_norm <= 1.0) {
        // Accept: update bodies to y5
        bodies = y5;
        result.accepted = true;
        result.dt_used  = dt;
    }
    // If rejected: bodies unchanged, caller retries with dt_next

    return result;
}
```

### 2.3 Extend `buildIntermediateState()` for multi-stage combinations

Your current `buildIntermediateState()` only combines ONE derivative with the
base state:

```cpp
// Current — only handles: base + scale * d
std::vector<CelestialBody> buildIntermediateState(
    const std::vector<CelestialBody>& bodies,
    const std::vector<StateDerivative>& d,
    double scale);
```

For RK45 stage 3 you need: `base + dt*(A31*k1 + A32*k2)` — two derivatives.
Add an overload:

```cpp
// New overload — handles arbitrary linear combination of derivatives
std::vector<CelestialBody> buildIntermediateState(
    const std::vector<CelestialBody>& base,
    const std::vector<std::pair<double, std::vector<StateDerivative>>>& weighted_stages,
    double dt);
```

This takes a list of `(coefficient, derivative)` pairs and sums them.
Mathematically: `result = base + dt * sum_i(coeff_i * deriv_i)`

**Linear algebra connection:** This is a **linear combination** — exactly
what you're studying. Each `k_i` is a vector (of derivatives for all bodies),
and you're computing a weighted sum of those vectors. The Butcher tableau
coefficients are the weights.

### 2.4 Implement `computeErrorNorm()`

**File**: `src/core/simulation.cpp`

```cpp
/**
 * computeErrorNorm
 * @brief: Computes the scalar RMS error norm for adaptive step control.
 *
 * For each coordinate of each body, computes a scaled error:
 *   sc_i = atol + rtol * max(|y_old_i|, |y_new_i|)
 *   err_i = error_i / sc_i
 *
 * Returns: sqrt( mean(err_i^2) )
 *
 * If this value < 1.0 → step is within tolerance (accept)
 * If this value ≥ 1.0 → step exceeds tolerance (reject, shrink dt)
 */
double computeErrorNorm(
    const std::vector<CelestialBody>& y_old,
    const std::vector<CelestialBody>& y_new,
    const std::vector<CelestialBody>& error,   // y5 - y4
    double atol, double rtol)
{
    double sum = 0.0;
    int    n   = 0;

    for (size_t i = 0; i < y_old.size(); ++i) {
        // Check position components
        for (int j = 0; j < 3; ++j) {
            double y_o = y_old[i].position[j];
            double y_n = y_new[i].position[j];
            double err = error[i].position[j];
            double sc  = atol + rtol * std::max(std::abs(y_o), std::abs(y_n));
            sum += (err / sc) * (err / sc);
            ++n;
        }
        // Check velocity components
        for (int j = 0; j < 3; ++j) {
            double y_o = y_old[i].velocity[j];
            double y_n = y_new[i].velocity[j];
            double err = error[i].velocity[j];
            double sc  = atol + rtol * std::max(std::abs(y_o), std::abs(y_n));
            sum += (err / sc) * (err / sc);
            ++n;
        }
    }

    return std::sqrt(sum / n);
}
```

**Why this norm?** Dividing by `sc` makes the error dimensionless — positions
are in meters (`~10¹¹`) and velocities are in m/s (`~10⁴`). Without scaling,
the position error would always dominate just because of units. The scaled norm
treats a 1-meter position error and a 0.001 m/s velocity error as equally
important (relative to tolerance).

**Definition of Done for Phase 2:**
```cpp
// This should work:
auto result = rk45Step(bodies, 3600.0, 1e-9, 1e-9);
// result.accepted == true for a well-behaved system
// result.dt_next  != 3600.0 (it adapted)
// result.error_norm < 1.0
```

---

## Phase 3 — Adaptive Simulation Loop

*Goal: Replace the fixed `for (int i = 0; i < steps; ++i)` loop with a
time-based adaptive loop.*
*Estimated effort: 3–4 hours*

This is the most architecturally significant change — it affects
`runSimulation()`, the CSV output format, and the CLI.

### 3.1 Add RK45 to the Integrator enum

**File**: `include/simulation.h`

```cpp
enum class Integrator {
    RK4,
    Leapfrog,
    RK45,    // NEW
    euler
};
```

**File**: `src/cli/cli.cpp` — parse it:

```cpp
else if (opt.integrator == "rk45") {
    integrator = Integrator::RK45;
}
```

### 3.2 Add duration and tolerance to CLIOptions

**File**: `include/cli.h`

```cpp
struct CLIOptions {
    // ... existing fields ...

    // RK45-specific
    double duration = 0.0;    // total simulated time in seconds
    double atol     = 1e-9;   // absolute tolerance
    double rtol     = 1e-9;   // relative tolerance
    double dt_min   = 1.0;    // minimum allowed timestep (seconds)
    double dt_max   = 86400.0;// maximum allowed timestep (1 day)
};
```

**File**: `src/cli/cli.cpp` — parse new options:

```cpp
else if (a == "--duration" && i + 1 < argc) {
    opt.duration = std::stod(argv[++i]);
}
else if (a == "--atol" && i + 1 < argc) {
    opt.atol = std::stod(argv[++i]);
}
else if (a == "--rtol" && i + 1 < argc) {
    opt.rtol = std::stod(argv[++i]);
}
```

### 3.3 Redesign `runSimulation()` for adaptive stepping

The current loop:

```cpp
// CURRENT — fixed step count, fixed dt
for (int i = 0; i < steps; ++i) {
    rk4Step(bodies, dt);
    if (i % stride != 0) continue;
    // write CSV row
}
```

The new loop for RK45:

```cpp
// NEW — time-based, adaptive dt
double t       = 0.0;
double dt      = dt_initial;   // user-provided starting guess
int    n_steps = 0;            // actual steps taken
int    n_reject = 0;           // rejected steps (for diagnostics)

while (t < t_end) {
    // Don't overshoot the end
    if (t + dt > t_end) dt = t_end - t;

    auto result = rk45Step(bodies, dt, atol, rtol);

    if (result.accepted) {
        t       += result.dt_used;
        n_steps += 1;

        // Write to CSV based on time, not step count
        // (use stride_time instead of stride_steps)
        if (t >= next_output_time) {
            writeCSVRow(file, t, bodies, ...);
            next_output_time += output_interval;
        }
    } else {
        n_reject += 1;
    }

    dt = std::clamp(result.dt_next, dt_min, dt_max);
}

std::cout << "Steps taken:    " << n_steps  << "\n";
std::cout << "Steps rejected: " << n_reject << "\n";
std::cout << "Final dt:       " << dt       << " s\n";
```

### 3.4 Change CSV output from step-based to time-based

**Current CSV header:**
```
step,x_Sun,y_Sun,z_Sun,...,E_total,...
```

**New CSV header for RK45:**
```
time_s,x_Sun,y_Sun,z_Sun,...,E_total,...,dt_used
```

Replace `step` with `time_s` (elapsed simulation seconds) and add `dt_used`
so you can see how the timestep varied over the simulation — this is
scientifically interesting data.

The Python plotting scripts need a one-line update:
```python
# Old
steps = df["step"]
# New
steps = df["time_s"] / 86400.0  # convert seconds to days
```

**Important:** Keep `--steps` and fixed-dt behavior for RK4 and Leapfrog.
Only RK45 uses `--duration`. Don't break existing workflows.

### 3.5 Add `--output-interval` flag

With adaptive stepping you can't use `--stride` (which skips N steps) because
steps are variable length. Instead use a time-based output interval:

```bash
# Write one CSV row every 3600 simulated seconds (1 hour of sim time)
orbit-sim run --system systems/earth_moon.json \
              --integrator rk45 \
              --duration 31536000 \
              --output-interval 3600 \
              --atol 1e-9 --rtol 1e-9
```

**Definition of Done for Phase 3:**
```bash
orbit-sim run \
    --system systems/earth_moon.json \
    --integrator rk45 \
    --duration 31536000 \
    --output-interval 3600 \
    --atol 1e-9 --rtol 1e-9 \
    --output results/rk45_earth_moon.csv

# Console should show:
#  - Steps taken:    (much fewer than 876000 if dt adapts upward)
#  - Steps rejected: (small number, ideally < 5% of accepted)
#  - Final dt:       (should be large when system is calm)
```

---

## Phase 4 — Validation and Benchmarking

*Goal: Prove RK45 is better than fixed-step methods for long simulations.*
*Estimated effort: 3–4 hours*

This phase is what makes the implementation scientifically credible.
Don't skip it.

### 4.1 Energy drift comparison: RK4 vs Leapfrog vs RK45

Run all three integrators on the same Earth-Moon system for 3500 simulated
days (matching your existing benchmark plots):

```bash
# RK4 (existing)
orbit-sim run --system systems/earth_moon.json \
              --steps 5040000 --dt 60 \
              --output results/rk4_long.csv

# Leapfrog (existing)
orbit-sim run --system systems/earth_moon.json \
              --steps 5040000 --dt 60 --integrator leapfrog \
              --output results/leapfrog_long.csv

# RK45 (new)
orbit-sim run --system systems/earth_moon.json \
              --integrator rk45 \
              --duration 302400000 \
              --output-interval 3600 \
              --atol 1e-9 --rtol 1e-9 \
              --output results/rk45_long.csv
```

Then extend `compare_integrators.py` to load all three and plot them together.

**Expected result:** RK45 energy drift should be bounded well below both
RK4 and Leapfrog for the same simulated duration, because it automatically
shrinks `dt` whenever error grows.

### 4.2 Step size history plot

Add a `dt_used` column to the RK45 CSV (Phase 3.4 already includes this).
Write a new plotting script:

**File**: `python/plot_adaptive_dt.py`

```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("results/rk45_long.csv")
df["days"] = df["time_s"] / 86400.0

plt.figure(figsize=(12, 4))
plt.plot(df["days"], df["dt_used"] / 60.0)  # convert to minutes
plt.xlabel("Simulated time (days)")
plt.ylabel("Timestep dt (minutes)")
plt.title("Adaptive Timestep History — RK45 Earth-Moon System")
plt.yscale("log")
plt.grid(True, alpha=0.3)
plt.savefig("results/adaptive_dt_history.png", dpi=150)
plt.show()
```

**What this plot will show:** The timestep should be small (~minutes) when
the Moon is near perigee (closest approach, fast-changing forces) and large
(~hours) when the Moon is near apogee. This oscillation with a ~27-day period
is the Moon's orbital period — and seeing it emerge in the timestep history
is a beautiful validation that the error control is working correctly.

### 4.3 Efficiency comparison

Count actual force evaluations, not just steps:

```
RK4:      1 step = 4 force evaluations
Leapfrog: 1 step = 1 force evaluation
RK45:     1 step = 6 force evaluations (5 effective with FSAL)
```

But RK45 takes far fewer steps for the same accuracy. Measure:

```
metric = force_evaluations_to_achieve_|dE/E| < 1e-10 over 3500 days
```

RK45 should win this comparison decisively — that's the point of adaptive
stepping. Log the total step count in the simulation output for comparison.

### 4.4 Tolerance sensitivity study

Run RK45 at three tolerance levels and compare accuracy vs cost:

| atol/rtol | Steps taken | Energy drift | Wall-clock time |
|-----------|-------------|--------------|-----------------|
| 1e-6      | (fewer)     | (higher)     | (faster)        |
| 1e-9      | (medium)    | (medium)     | (medium)        |
| 1e-12     | (more)      | (lower)      | (slower)        |

This teaches you the fundamental tradeoff: tighter tolerance = more accurate
but slower. Document the sweet spot for typical Earth-Moon simulations.

**Definition of Done for Phase 4:**
- Three-integrator comparison plot exists in `results/`
- Adaptive timestep history plot shows expected ~27-day oscillation
- Efficiency comparison documented in `docs/validation.md`
- Recommended tolerance values documented in CLI help

---

## Phase 5 — Documentation and CLI Polish

*Goal: Everything is discoverable and explained.*
*Estimated effort: 1–2 hours*

### 5.1 Update CLI help for `run` command

**File**: `src/cli/cli.cpp` → `printCommandHelp("run")`

```
Integrator options:
  --integrator rk4        Fixed-step RK4 (default). Use with --steps and --dt.
                          Best for: short simulations, dt < 600s
  --integrator leapfrog   Fixed-step Leapfrog. Use with --steps and --dt.
                          Best for: long simulations where symplectic
                          conservation matters more than per-step accuracy
  --integrator rk45       Adaptive Dormand-Prince RK45. Use with --duration.
                          Best for: high-accuracy long simulations,
                          unknown optimal dt, production runs

RK45-specific options:
  --duration  T           Total simulated time in seconds (replaces --steps)
  --atol      1e-9        Absolute tolerance (meters / m/s). Default: 1e-9
  --rtol      1e-9        Relative tolerance (dimensionless). Default: 1e-9
  --dt-min    1.0         Minimum allowed timestep in seconds. Default: 1.0
  --dt-max    86400.0     Maximum allowed timestep in seconds. Default: 86400
  --output-interval T     Write CSV row every T simulated seconds. Default: 3600

Examples:
  # Fixed RK4 (existing workflow — unchanged)
  orbit-sim run --system systems/earth_moon.json --steps 876000 --dt 60

  # Adaptive RK45 — 1 year, output every hour
  orbit-sim run --system systems/earth_moon.json \
                --integrator rk45 \
                --duration 31536000 \
                --output-interval 3600 \
                --atol 1e-9 --rtol 1e-9
```

### 5.2 Update `docs/validation.md`

Add a new section *"RK45 Adaptive Integrator Validation"* with:
- Three-integrator energy drift comparison plot
- Adaptive timestep history plot
- Efficiency table (force evaluations per accuracy level)
- Tolerance sensitivity table
- Recommendation: use RK45 for any simulation longer than 30 days

### 5.3 Update `docs/orbital_mechanics_engine_cli_reference.md`

Add RK45 examples alongside existing RK4/Leapfrog examples.

### 5.4 Add Makefile targets

**File**: `Makefile`

```makefile
run-rk45: $(SIM_EXE)
	@$(SIM_EXE) run \
		--system $(SYSTEMS_DIR)/earth_moon.json \
		--integrator rk45 \
		--duration 31536000 \
		--output-interval 3600 \
		--atol 1e-9 --rtol 1e-9 \
		--output $(RESULTS_DIR)/rk45_earth_moon.csv

plot-adaptive-dt:
	@$(PYTHON) python/plot_adaptive_dt.py

benchmark-integrators:
	@echo "Running all three integrators for comparison..."
	@$(MAKE) run-earth-moon
	@$(MAKE) run-rk45
	@$(PYTHON) python/compare_integrators.py
```

### 5.5 Update `CMakeLists.txt`

No new files needed — `rk45Step()` and `computeErrorNorm()` live in
`src/core/simulation.cpp`, and `rk45_coefficients.h` is header-only.

Just add the new header to the include path (already covered by
`include_directories(${PROJECT_SOURCE_DIR}/include)`).

---

## Implementation Priority Summary

| Priority | Phase | Task | Why |
|----------|-------|------|-----|
| 🔴 Now | 1.1 | Dormand-Prince coefficients header | Foundation — everything depends on it |
| 🔴 Now | 2.1–2.2 | `RK45StepResult` struct + `rk45Step()` skeleton | Core algorithm |
| 🔴 Now | 2.3 | Extend `buildIntermediateState()` for multi-stage | Needed for stages 3–6 |
| 🔴 Now | 2.4 | `computeErrorNorm()` | Needed for step acceptance |
| 🟠 Soon | 3.1–3.3 | Adaptive simulation loop in `runSimulation()` | Makes it actually usable |
| 🟠 Soon | 3.4–3.5 | Time-based CSV output + `--output-interval` | CLI compatibility |
| 🟡 Medium | 4.1–4.2 | Three-integrator comparison + dt history plot | Scientific validation |
| 🟡 Medium | 4.3–4.4 | Efficiency + tolerance sensitivity study | Publishable results |
| 🟢 Later | 5.1–5.5 | CLI help + docs + Makefile | Polish |

---

## Versioning

| Version | Milestone |
|---------|-----------|
| Current | RK4 + Leapfrog fixed-step only |
| v1.2 | Phase 1–2 complete — `rk45Step()` implemented and tested |
| v1.2.1 | Phase 3 complete — adaptive loop, time-based CSV |
| v1.3 | Phase 4 complete — validated, benchmarked, documented |
| v1.3.1 | Phase 5 complete — full CLI polish |

---

## Files Changed Summary

| File | Change |
|------|--------|
| `include/rk45_coefficients.h` | New — Dormand-Prince Butcher tableau constants |
| `include/simulation.h` | Add `RK45StepResult` struct, `rk45Step()`, `RK45` to enum |
| `src/core/simulation.cpp` | Add `rk45Step()`, `computeErrorNorm()`, adaptive loop |
| `include/cli.h` | Add `duration`, `atol`, `rtol`, `dt_min`, `dt_max`, `output_interval` |
| `src/cli/cli.cpp` | Parse new RK45 options, update help text |
| `src/cli/main.cpp` | Pass new options to `runSimulation()` |
| `python/plot_adaptive_dt.py` | New — timestep history visualization |
| `python/compare_integrators.py` | Extend to load and plot RK45 results |
| `docs/validation.md` | New section: RK45 validation results |
| `docs/orbital_mechanics_engine_cli_reference.md` | RK45 examples |
| `Makefile` | New `run-rk45`, `plot-adaptive-dt`, `benchmark-integrators` targets |

---

## A Note on Learning

You're taking Linear Algebra right now — here is where it directly connects
to what you just built:

The state of your entire N-body system at any moment is a **vector** in
a very high-dimensional space. For 3 bodies, that's 18 numbers
(3 bodies × 3 position coords + 3 velocity coords = 18 dimensions).

RK45 is computing a **weighted linear combination** of derivative vectors
`k1` through `k6` to advance that state vector forward in time. The Butcher
tableau coefficients are exactly the weights of that linear combination.

The error estimate `|y5 - y4|` is the **norm of the difference vector**
between two estimates — exactly what you're learning about vector norms
in Linear Algebra.

When you study eigenvalues later this year, you'll learn that the stability
of an integrator depends on the eigenvalues of the Jacobian of `f` —
the same `f` you're calling in `evaluateDerivatives()`. That's when
everything will click together.

---

*This roadmap is a living document. Update it as phases are completed.*
