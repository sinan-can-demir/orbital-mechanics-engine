# Post-Newtonian GR Correction
### Implementation guide and physics background

---

## Why Newtonian gravity is not enough

General Relativity modifies gravity near massive objects. For the solar system the effect is tiny but measurable — and DE440 includes it. Our Newtonian engine doesn't, so we systematically drift from DE440.

The most famous manifestation: **Mercury's perihelion precession**.

Mercury's orbit slowly rotates (precesses) around the Sun. Newtonian gravity from all planets accounts for ~531"/century. Observations show ~574"/century. The remaining **43"/century** is pure GR — Einstein's first successful prediction in 1915.

For context:
- 43" per century = 43 arcseconds per 100 years
- That's ~0.012° per century, or 1/30,000 of a full orbit per century
- Tiny — but measurable, and DE440 accounts for it

---

## The correction term

The simplest accurate correction is the **Schwarzschild approximation** — valid when one body (the Sun) dominates gravity, which is true for all planets:

```
Δa_i = (G·M_sun / c² / r³) · [(4·G·M_sun/r − |v|²) · r̂  +  4·(r̂·v) · v]
```

Where:
- `G` = gravitational constant (6.674×10⁻¹¹ m³/kg/s²)
- `M_sun` = solar mass (1.989×10³⁰ kg)
- `c` = speed of light (299,792,458 m/s)
- `r` = position vector of planet relative to Sun
- `v` = velocity vector of planet
- `r` = |r| (scalar distance)

The full version is the **Einstein-Infeld-Hoffmann (EIH) equations**, which handle all body pairs. For our solar system the Schwarzschild approximation gets 95%+ of the effect since the Sun is ~1000× more massive than Jupiter.

### Why this looks the way it does

The term `(4·G·M/r − |v|²)` measures how much the kinetic energy deviates from the potential well depth. When a planet moves faster (near perihelion), the correction is stronger — which is exactly where GR effects are largest.

The `4·(r̂·v)·v` term captures the frame-dragging-like effect: the gravitational field is slightly "ahead" of the planet's motion.

Both terms are proportional to `G·M/c²r` — the ratio of gravitational potential energy to rest-mass energy. For Mercury this is ~3×10⁻⁸. Tiny but cumulative over centuries.

---

## Order of magnitude check

For Mercury:
- `G·M_sun/c²` = 1477 m (Schwarzschild radius of the Sun / 2)
- `r_Mercury` ≈ 5.79×10¹⁰ m
- Correction magnitude ≈ `G·M_sun/c²/r²` ≈ 4.4×10⁻¹⁰ m/s²
- Newtonian acceleration ≈ `G·M_sun/r²` ≈ 0.04 m/s²
- Ratio ≈ 10⁻⁸

So the correction is 10⁻⁸ of the main force — tiny per step, but Mercury completes ~415 orbits per century and the correction has a preferred direction (always near perihelion), so it accumulates into the observable 43"/century.

---

## Implementation in C++

```cpp
// In src/core/simulation.cpp

constexpr double C_LIGHT = 299792458.0;  // m/s

void applyGRCorrection(std::vector<CelestialBody>& bodies,
                       std::vector<glm::dvec3>& accelerations) {
    // Find Sun (index 0, most massive body)
    int sun_idx = 0;
    double M_sun = bodies[sun_idx].mass;
    glm::dvec3 r_sun = bodies[sun_idx].position;

    for (size_t i = 1; i < bodies.size(); ++i) {
        glm::dvec3 r = bodies[i].position - r_sun;  // planet relative to Sun
        glm::dvec3 v = bodies[i].velocity;
        double r_mag = glm::length(r);

        double GM = G * M_sun;
        double c2 = C_LIGHT * C_LIGHT;

        double scalar = GM / (c2 * r_mag * r_mag * r_mag);
        double term1  = 4.0 * GM / r_mag - glm::dot(v, v);
        double term2  = 4.0 * glm::dot(r, v);

        accelerations[i] += scalar * (term1 * r + term2 * v);
    }
}
```

Call `applyGRCorrection()` after `computeForces()` in the simulation loop when `use_gr = true`.

---

## Testing: Mercury perihelion precession

After implementing, verify with a 100-year Mercury simulation:

1. Run with `--gr` and record Mercury's orbit for 100 years
2. At each perihelion passage, record the angle of perihelion
3. Fit a line to angle vs time → slope is precession rate in "/century
4. Expected: ~574"/century total (531" Newtonian + 43" GR)
5. Without `--gr`: ~531"/century (Newtonian only)

A 43"/century difference confirms the implementation is correct.

---

## Expected SPICE validation improvement

After adding GR correction with SPICE-derived ICs:

| Body | Without GR (km/yr) | With GR (km/yr) | Improvement |
|---|---|---|---|
| Mercury | ~300,000 | ~100,000 | ~67% |
| Venus | ~194,000 | ~130,000 | ~33% |
| Earth | ~58,000 | ~40,000 | ~30% |
| Mars | ~138,000 | ~120,000 | ~13% |
| Jupiter+ | unchanged | unchanged | GR negligible |

*Estimates — actual numbers will come from running the validation.*

Remaining error after GR is dominated by solar oblateness (J2), missing asteroid belt mass, and higher-order GR terms — all negligible for educational purposes.

---

## References

- Einstein, A. (1915) — original GR perihelion paper
- Misner, Thorne & Wheeler — *Gravitation*, Ch. 40 (EIH equations)
- Soffel et al. (2003) — IAU conventions for relativistic reference frames
- Will, C.M. (2014) — *Living Reviews in Relativity*, "The confrontation between GR and experiment"
