# Yoshida 4th-Order Symplectic Integrator
### Study notes and implementation guide

---

## Why Leapfrog is not enough for long runs

Our Leapfrog integrator is **2nd-order symplectic**. "Symplectic" means it preserves
the geometric structure of Hamiltonian mechanics — energy doesn't drift without bound
(it oscillates around the true value instead). This makes Leapfrog far better than
RK4 for million-year runs.

But Leapfrog is only **2nd-order accurate**: the per-step error scales as `O(dt³)`.
To halve the error you must halve the timestep — doubling the cost. For very long
integrations this becomes expensive.

**Yoshida 4th-order** achieves `O(dt⁵)` per-step error while remaining symplectic.
Halving the error costs only 2^(1/4) ≈ 1.19× more work. For long timescales, the
larger timestep this allows makes Yoshida significantly more efficient than Leapfrog
at the same accuracy level.

---

## The idea: composing Leapfrog sub-steps

Yoshida (1990) showed that you can build a 4th-order symplectic integrator by composing
**three Leapfrog steps** with carefully chosen sub-step sizes:

```
Yoshida(dt) = Leapfrog(w₁·dt) ∘ Leapfrog(w₀·dt) ∘ Leapfrog(w₁·dt)
```

The magic is in the coefficients. The key constraint is that the sub-steps must sum
to the total step `dt`:

```
w₁ + w₀ + w₁ = 1   →   2·w₁ + w₀ = 1
```

and a 4th-order cancellation condition (from Taylor expansion of the composition):

```
w₁³ + w₀³ = 0
```

Solving these two equations:

```
w₁ = 1 / (2 − ∛2)  ≈  +1.3512
w₀ = 1 − 2·w₁      ≈  −1.7024
```

**w₀ is negative.** The middle sub-step goes backward in time. This is not a bug — it
is exactly what cancels the leading error terms and achieves 4th-order accuracy.
The net displacement over the three sub-steps is still forward by `dt`.

---

## The coefficients in full

Each Leapfrog sub-step is a Drift-Kick-Drift sequence. When three sub-steps are
composed, adjacent drift half-steps merge, giving a single pass of 4 drifts and
3 kicks (instead of 6 drifts and 3 kicks if composed naively):

```
Drift(c₁·dt) → Kick(d₁·dt) → Drift(c₂·dt) → Kick(d₂·dt)
             → Drift(c₂·dt) → Kick(d₁·dt) → Drift(c₁·dt)
```

The coefficients (symmetric, so only 2 unique values each):

```
c₁ = c₄ = w₁ / 2          ≈  +0.6756
c₂ = c₃ = (w₀ + w₁) / 2   ≈  −0.1756
d₁ = d₃ = w₁               ≈  +1.3512
d₂      = w₀               ≈  −1.7024
```

Sanity checks:
- `c₁ + c₂ + c₃ + c₄ = 1` ✓ (drifts sum to dt)
- `d₁ + d₂ + d₃ = 1` ✓ (kicks sum to dt)
- Scheme is time-reversible ✓ (palindromic: same coefficients read forwards and backwards)

---

## Order of accuracy: why this works

A Leapfrog step can be written as a formal exponential operator:

```
L(τ) = exp(τ·A/2) · exp(τ·B) · exp(τ·A/2)
```

where `A` is the drift operator (advance positions) and `B` is the kick operator
(advance velocities). The exact solution is `exp(τ·(A+B))`.

The error from one Leapfrog step is `O(τ³)` — the commutator `[A, [A, B]]` and
`[B, [B, A]]` appear at third order. By choosing three sub-steps with coefficients
`(w₁, w₀, w₁)`, the third-order error terms cancel exactly:

```
w₁³ + w₀³ + w₁³ = 0   →   2·w₁³ = −w₀³   →   w₀ = −∛2 · w₁
```

Combined with `2w₁ + w₀ = 1`, this gives the Yoshida coefficients above. The
remaining leading error is `O(τ⁵)`, making the composition 4th-order accurate.

---

## Intuition: why negative steps work

Think of it like Richardson extrapolation but in the time direction:

1. Take a forward step of `1.35 dt` (over-shoot)
2. Take a backward step of `1.70 dt` (partially un-do the over-shoot in a specific way)
3. Take a forward step of `1.35 dt` (arrive at the correct 4th-order position)

The backward step is not simply reversing — it advances through a different part of
phase space, and the precise combination cancels the accumulated error terms. This is
why the specific value of `∛2` appears: it is the unique solution to the cancellation
condition.

---

## Implementation in C++

Add to `src/core/simulation.cpp` alongside the existing `leapfrogStep`:

```cpp
static void driftBodies(std::vector<CelestialBody>& bodies, double dt) {
    for (auto& b : bodies)
        b.position += b.velocity * dt;
}

static void kickBodies(std::vector<CelestialBody>& bodies, double dt) {
    // compute forces, then update velocities
    auto accels = computeAccelerations(bodies);
    for (size_t i = 0; i < bodies.size(); ++i)
        bodies[i].velocity += accels[i] * dt;
}

void yoshida4Step(std::vector<CelestialBody>& bodies, double dt) {
    // Yoshida (1990) coefficients
    constexpr double CR2  = 1.2599210498948732;  // ∛2
    constexpr double w1   = 1.0 / (2.0 - CR2);
    constexpr double w0   = 1.0 - 2.0 * w1;     // negative

    constexpr double c1   = w1 / 2.0;
    constexpr double c2   = (w0 + w1) / 2.0;    // negative
    constexpr double d1   = w1;
    constexpr double d2   = w0;                  // negative

    // 4 drifts, 3 kicks — palindromic (c2=c3, d1=d3, c1=c4)
    driftBodies(bodies, c1 * dt);
    kickBodies (bodies, d1 * dt);
    driftBodies(bodies, c2 * dt);   // drifts backward (c2 < 0)
    kickBodies (bodies, d2 * dt);   // kicks backward  (d2 < 0)
    driftBodies(bodies, c2 * dt);
    kickBodies (bodies, d1 * dt);
    driftBodies(bodies, c1 * dt);
}
```

Then add `Integrator::Yoshida4` to the enum in `include/simulation.h` and dispatch
in the loop:

```cpp
else if (integrator == Integrator::Yoshida4)
    yoshida4Step(bodies, dt);
```

About 30 lines of new code total (excluding enum + CLI wiring).

---

## Cost vs Leapfrog

| Property | Leapfrog | Yoshida 4th |
|---|---|---|
| Order | 2nd | 4th |
| Force evaluations per step | 1 | 3 |
| Error scaling | O(dt³) | O(dt⁵) |
| Cost to halve error | 2× | 2^(1/4) ≈ 1.19× |
| Symplectic? | Yes | Yes |
| Energy drift? | Bounded oscillation | Bounded oscillation |

For a 1-year solar system run: Yoshida at 3× the timestep of Leapfrog costs the
same (~3 force evals per Leapfrog step × 3 sub-steps vs 1 eval × 9 steps) but
achieves much smaller error. The break-even happens around 10–100 year runs.

---

## How to verify: energy drift comparison

After implementing, run a 1,000-year solar system integration and compare energy
drift (relative change in total energy) vs Leapfrog at the same timestep:

```python
import orbit

r_lf = orbit.simulate('systems/solar_system_spice.json',
                      steps=8_760_000, dt=3600.0,
                      integrator=orbit.Integrator.Leapfrog)

r_y4 = orbit.simulate('systems/solar_system_spice.json',
                      steps=8_760_000, dt=3600.0,
                      integrator=orbit.Integrator.Yoshida4)

# Compare final energy drift
print(r_lf.snapshots[-1].conservation.dE_rel)
print(r_y4.snapshots[-1].conservation.dE_rel)
```

Expected: Yoshida shows energy drift 2–4 orders of magnitude smaller than Leapfrog
at the same timestep. Or equivalently, Yoshida can use a 3–4× larger timestep to
achieve the same drift as Leapfrog.

---

## Connection to Wisdom-Holman (for context)

Yoshida 4th is a **direct** integrator — it computes pairwise forces between all N
bodies at every sub-step. Wisdom-Holman (WHFast in REBOUND) is a **Keplerian**
integrator that splits the Hamiltonian differently: fast Keplerian orbits are solved
analytically, only the slow inter-planet perturbations are stepped numerically.

Yoshida is simpler to implement and sufficient for 1,000-year runs. Wisdom-Holman
enables million-year runs by removing the fast timescale from the step constraint.
Both are 2nd-order symplectic (WHFast) or 4th-order (higher WHFast variants) — the
difference is the force decomposition, not the composition principle.

---

## References

- Yoshida, H. (1990) — *Construction of higher order symplectic integrators.* Physics Letters A, 150(5-7), 262-268. **The original paper. One page of math, fully readable.**
- Hairer, Lubich & Wanner (2006) — *Geometric Numerical Integration*, Ch. V. Rigorous derivation of the composition method.
- Wisdom & Holman (1991) — *Symplectic maps for the N-body problem.* AJ, 102, 1528. Context for Yoshida vs Wisdom-Holman.
