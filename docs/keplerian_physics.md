# Keplerian Physics & Wisdom-Holman Integration
### A self-study guide for orbital mechanics engine developers

---

## 1. The Two-Body Problem — The Foundation

### What it is

When you have exactly two bodies (say, the Sun and one planet), Newton's gravity has a **perfect analytical solution**. No numerical integration needed. The orbit is always a conic section — an ellipse, parabola, or hyperbola.

For a bound orbit (ellipse), the six **orbital elements** fully describe the trajectory:

| Element | Symbol | What it means |
|---|---|---|
| Semi-major axis | *a* | Average distance from focus |
| Eccentricity | *e* | How elliptical (0 = circle, 1 = parabola) |
| Inclination | *i* | Tilt of the orbital plane |
| Longitude of ascending node | *Ω* | Where the orbit crosses the reference plane |
| Argument of periapsis | *ω* | Where the closest approach is in the orbit |
| Mean anomaly | *M* | Where in the orbit the body is right now |

Given initial position and velocity vectors (what we store in our JSON files), you can compute all six elements. Given the elements, you can compute position and velocity at any future time — **without stepping through every intermediate moment**.

### Kepler's Third Law

```
T² = (4π²/GM) · a³
```

The orbital period T depends only on the semi-major axis *a* and the central mass M. This is why Earth always takes 1 year and Jupiter always takes ~12 years — regardless of where they start.

---

## 2. The Problem with Direct N-Body Integration

Our engine computes pairwise gravitational forces between every pair of bodies at every timestep:

```
F_ij = G * m_i * m_j / r_ij²   (for every pair i,j)
```

This is **correct** but **inefficient** for planetary systems. Here's why:

The Sun is ~1000x more massive than Jupiter, which is ~300x more massive than Earth. The dominant force on any planet is the Sun. Planetary perturbations on each other are tiny corrections — Jupiter perturbs Earth's orbit by ~0.01% per orbit.

When we do direct N-body:
- We treat the Sun-Earth interaction and the Jupiter-Earth interaction with the **same numerical precision**
- We have to use a small timestep to capture the fast Moon orbit (dt = hours)
- But Jupiter's orbit takes 12 years — we're wasting thousands of steps on a nearly-constant force

**The blunt instrument problem:** we're solving the easy part (the well-understood Keplerian orbit) numerically, when the exact answer is available analytically.

---

## 3. Hamiltonian Mechanics — The Key Idea

To understand Wisdom-Holman, you need a little Hamiltonian mechanics. Don't panic — the intuition is simple.

### What a Hamiltonian is

The Hamiltonian H is the **total energy** of the system expressed in terms of positions *q* and momenta *p*:

```
H = KE + PE = Σ(pᵢ²/2mᵢ) + Σ(-G·mᵢ·mⱼ/rᵢⱼ)
```

Hamilton's equations describe how the system evolves:
```
dq/dt = ∂H/∂p      (position changes with momentum)
dp/dt = -∂H/∂q     (momentum changes with force)
```

This is just Newton's laws in disguise — but the Hamiltonian formulation lets us do something powerful: **split the problem**.

### Hamiltonian Splitting (the Wisdom-Holman idea, 1991)

For a planetary system, split the Hamiltonian into two parts:

```
H = H_Kepler + H_interaction
```

Where:
- **H_Kepler** = sum of independent two-body problems (each planet orbiting only the Sun)
- **H_interaction** = small perturbations between planets

**H_Kepler has an exact analytical solution.** You don't need to integrate it numerically at all.

**H_interaction is small** — for the solar system, it's about 10⁻³ of the total energy.

So instead of integrating everything numerically, you:
1. **Advance** each planet along its exact Keplerian orbit (free, analytical)
2. **Kick** the momenta with the perturbation forces (small correction)
3. Alternate these two operations in a symplectic pattern

This is the **Wisdom-Holman Symplectic Map**.

---

## 4. Kepler's Equation — The Hard Part

To advance a planet along its Keplerian orbit to a future time, you need to solve **Kepler's equation**:

```
M = E - e·sin(E)
```

Where:
- *M* = mean anomaly (time × angular velocity — increases linearly with time)
- *E* = eccentric anomaly (geometric angle in the ellipse)
- *e* = eccentricity

The problem: given *M*, solve for *E*. There's no closed-form solution. You have to solve it **iteratively** (Newton-Raphson works well):

```
E₀ = M                          (initial guess)
Eₙ₊₁ = Eₙ - (Eₙ - e·sin(Eₙ) - M) / (1 - e·cos(Eₙ))
```

Converges in ~5 iterations for most planetary orbits. For high eccentricity (e → 1, like comets), it needs more care.

Once you have *E*, position follows from:
```
x = a·(cos(E) - e)
y = a·√(1-e²)·sin(E)
```

This is the Kepler solver — the core of any Wisdom-Holman implementation.

---

## 5. The Drift-Kick-Drift Algorithm

The Wisdom-Holman method uses a **symplectic operator splitting** called Leapfrog (or Störmer-Verlet) applied to the split Hamiltonian:

```
1. DRIFT  (dt/2) — advance along Keplerian orbits for half a timestep
2. KICK   (dt)   — apply gravitational perturbations between planets
3. DRIFT  (dt/2) — advance along Keplerian orbits for another half timestep
```

The DRIFT step uses the Kepler solver — it's analytically exact, no matter how large dt is.

The KICK step is just computing pairwise forces between planets (excluding the Sun) and updating velocities — same as what our Leapfrog integrator does, but only for the small perturbation forces.

**Why this is better:**

| | Direct N-body RK4 | Wisdom-Holman |
|---|---|---|
| Timestep limit | Must resolve fastest orbit | Only needs to resolve perturbations |
| Typical dt (solar system) | Hours | Days to weeks |
| Steps for 1 million years | ~8.8 billion (dt=1h) | ~36,500 (dt=1 day) |
| Energy conservation | Accumulates error | Symplectic, bounded |
| Mercury perihelion | Approximate | Still Newtonian (no GR) |

A factor of **240,000x fewer steps** for million-year solar system simulations. That's not an optimization — it's a completely different category of simulation.

---

## 6. Why Our Current Engine Does What It Does

Our engine uses:

| Integrator | Type | Suitable for |
|---|---|---|
| RK4 | Direct, non-symplectic | Short runs (< 1 year), high accuracy |
| Leapfrog | Direct, symplectic | Medium runs, bounded drift |
| RK45 | Direct, adaptive | Short runs where accuracy matters |
| Yoshida 4th (planned) | Direct, 4th-order symplectic | Better long-term than Leapfrog |
| Wisdom-Holman (future) | Keplerian, symplectic | Million-year planetary integrations |

All of our current integrators are **direct** — they compute pairwise forces at every step. Yoshida improves the symplectic order (better bounded drift) but it's still direct.

Wisdom-Holman is a fundamentally different approach — it exploits the analytical structure of the problem rather than brute-forcing it.

---

## 7. What's Missing from Both Approaches

Neither direct N-body nor Wisdom-Holman includes:

**General Relativity corrections**
Mercury's perihelion precesses by 43 arcseconds per century due to GR. Newtonian gravity predicts ~531 arcsec/century from planetary perturbations alone — 43 arcsec/century short. Adding a post-Newtonian correction term (1/r³) fixes this.

**Non-gravitational forces**
Radiation pressure (important for small bodies), Yarkovsky effect (thermal radiation causing orbital drift in asteroids), solar wind.

**Extended body effects**
Planets aren't point masses — J2 oblateness of the Sun causes additional precession.

For solar system planetary orbits over hundreds of years, pure Newtonian N-body is already excellent. For million-year integrations, the Wisdom-Holman approach dominates. For precision work (spacecraft navigation, asteroid impact prediction), all of the above matter.

---

## 8. Implementation Sketch for This Engine

When you're ready to implement Wisdom-Holman:

```cpp
// Pseudocode
void wisdomHolmanStep(std::vector<CelestialBody>& bodies, double dt) {
    // 1. DRIFT — advance each planet along its Keplerian orbit (dt/2)
    for each planet (not Sun):
        elements = cartesianToKeplerian(planet, Sun)
        elements.M += meanMotion(elements.a, Sun.mass) * (dt / 2)
        planet.position, planet.velocity = keplerianToCartesian(elements)

    // 2. KICK — apply inter-planetary perturbations only (full dt)
    for each planet pair (i, j), excluding Sun:
        F = -G * m_i * m_j / r_ij²
        apply F to both planets' velocities

    // 3. DRIFT — advance along Keplerian orbit again (dt/2)
    for each planet (not Sun):
        elements = cartesianToKeplerian(planet, Sun)
        elements.M += meanMotion(elements.a, Sun.mass) * (dt / 2)
        planet.position, planet.velocity = keplerianToCartesian(elements)
}
```

Key functions to implement:
- `cartesianToKeplerian()` — convert (r, v) → (a, e, i, Ω, ω, M)
- `keplerianToCartesian()` — convert (a, e, i, Ω, ω, M) → (r, v) — needs Kepler solver
- `solveKepler(M, e)` — Newton-Raphson iteration for E given M

**Reference**: Wisdom & Holman (1991), *AJ*, 102, 1528. Rein & Tamayo (2015) for the WHFast variant used in REBOUND.

---

## Further Reading

- **Vallado** — *Fundamentals of Astrodynamics and Applications*, Ch. 2 (orbital elements, Kepler's equation)
- **Murray & Dermott** — *Solar System Dynamics*, Ch. 1-3 (two-body problem, Hamiltonian mechanics)
- **Wisdom & Holman (1991)** — the original WH paper, very readable
- **Rein & Tamayo (2015)** — WHFast paper, describes the REBOUND implementation
- **Hairer, Lubich & Wanner** — *Geometric Numerical Integration* (symplectic methods deep dive)
