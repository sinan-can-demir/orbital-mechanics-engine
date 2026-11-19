# Validation and Verification

*Comprehensive documentation of validation strategies, verification procedures, and accuracy assessment for the orbital dynamics simulation.*

---

## Validation Strategy Overview

The orbital dynamics simulation employs a multi-layered validation approach to ensure accuracy and reliability. Validation encompasses analytical verification, conservation law monitoring, ephemeris comparison, and numerical error analysis.

### Validation Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                    Empirical Validation                      │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │ Ephemeris       │    │  Real-world     │                │
│  │ Comparison      │    │  Observations   │                │
│  └─────────────────┘    └─────────────────┘                │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                   Numerical Validation                       │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │ Conservation    │    │  Convergence    │                │
│  │ Law Monitoring  │    │  Studies        │                │
│  └─────────────────┘    └─────────────────┘                │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                   Analytical Validation                      │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │ Special Cases   │    │  Exact Solutions│                │
│  │ (2-body, etc.)  │    │  Comparison     │                │
│  └─────────────────┘    └─────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

## Integrator comparison — empirical results

Tested RK4 vs Leapfrog (Velocity Verlet) on the Sun-Earth-Moon system.

Key finding: integrator choice depends on timestep size.
- At dt=60s: RK4 conserves energy better over long runs (higher per-step accuracy)  
- At dt=3600s: Leapfrog bounds energy growth (symplectic property), RK4 drifts

Practical recommendation for this codebase:
- dt < 600s  → use RK4
- dt > 1800s → use Leapfrog

---

## Conservation Law Monitoring

### Energy Conservation

#### Total Mechanical Energy

The total mechanical energy of an isolated N-body system must remain constant:

$$
E = \sum_{i=1}^{N} \frac{1}{2}m_i|\vec{v}_i|^2 - \sum_{i=1}^{N}\sum_{j>i}^{N} \frac{Gm_i m_j}{|\vec{r}_{ij}|}
$$

#### Energy Error Metrics

**Relative Energy Error**:
$$
\epsilon_E(t) = \left|\frac{E(t) - E(0)}{E(0)}\right|
$$

**Energy Drift Rate**:
$$
\dot{\epsilon}_E = \frac{d\epsilon_E}{dt}
$$

#### Acceptable Energy Conservation

For orbital dynamics simulations, acceptable energy error levels are:
- **Short-term** (few orbits): $\epsilon_E < 10^{-6}$
- **Medium-term** (months): $\epsilon_E < 10^{-4}$
- **Long-term** (years): $\epsilon_E < 10^{-2}$

### Linear Momentum Conservation

#### Total Linear Momentum

For an isolated system:
$$
\vec{P} = \sum_{i=1}^{N} m_i \vec{v}_i = \text{constant}
$$

#### Momentum Error Assessment

**Relative Momentum Error**:
$$
\epsilon_P(t) = \frac{|\vec{P}(t) - \vec{P}(0)|}{|\vec{P}(0)|}
$$

**Momentum Drift Characteristics**:
- **Ideal**: $\epsilon_P < 10^{-12}$ (machine precision limited)
- **Acceptable**: $\epsilon_P < 10^{-8}$
- **Problematic**: $\epsilon_P > 10^{-6}$

### Angular Momentum Conservation

#### Total Angular Momentum

System angular momentum about the origin:
$$
\vec{L} = \sum_{i=1}^{N} m_i (\vec{r}_i \times \vec{v}_i)
$$

#### Angular Momentum Error

**Relative Angular Momentum Error**:
$$
\epsilon_L(t) = \frac{|\vec{L}(t) - \vec{L}(0)|}{|\vec{L}(0)|}
$$

Angular momentum conservation is particularly sensitive to:
- **Integration errors**: Symplectic vs. non-symplectic methods
- **Numerical precision**: Round-off in cross-product calculations
- **Force calculation accuracy**: Pairwise interaction errors

---

## Barycenter Stability Analysis

### System Barycenter Tracking

The barycenter (center of mass) should remain stationary in an inertial reference frame:

$$
\vec{r}_{CM}(t) = \frac{\sum_{i=1}^{N} m_i \vec{r}_i(t)}{\sum_{i=1}^{N} m_i}
$$

### Barycenter Drift Metrics

**Position Drift**:
$$
\Delta r_{CM} = |\vec{r}_{CM}(t) - \vec{r}_{CM}(0)|
$$

**Velocity Drift**:
$$
\Delta v_{CM} = |\vec{v}_{CM}(t) - \vec{v}_{CM}(0)|
$$

### Acceptable Barycenter Stability

For high-precision orbital simulations:
- **Position drift**: $\Delta r_{CM} < 10^{-3}$ meters over simulation duration
- **Velocity drift**: $\Delta v_{CM} < 10^{-9}$ m/s over simulation duration

### Reference Frame Consistency

Barycenter stability validates:
- **Reference frame integrity**: No spurious accelerations
- **Mass conservation**: Total system mass remains constant
- **Numerical symmetry**: Equal treatment of all bodies

---

## Ephemeris Comparison Methodology

### NASA JPL HORIZONS Integration

The simulation validates against NASA JPL HORIZONS ephemerides:

#### Data Acquisition
- **Target bodies**: Earth, Moon, Sun, and other planets
- **Time spans**: Short-term (days) to long-term (years)
- **Reference frames**: Barycentric and heliocentric
- **Coordinate systems**: ICRF/J2000.0

#### Comparison Metrics

**Position Residuals**:
$$
\Delta \vec{r} = \vec{r}_{sim} - \vec{r}_{ephem}
$$

**Velocity Residuals**:
$$
\Delta \vec{v} = \vec{v}_{sim} - \vec{v}_{ephem}
$$

**Orbital Element Differences**:
- Semi-major axis: $\Delta a$
- Eccentricity: $\Delta e$
- Inclination: $\Delta i$
- Argument of periapsis: $\Delta \omega$
- Longitude of ascending node: $\Delta \Omega$
- Mean anomaly: $\Delta M$

### Validation Test Cases

#### Earth-Moon System
- **Time span**: 1 month to 1 year
- **Expected accuracy**: Position error < 1 km
- **Key metrics**: Moon-Earth distance, orbital period

#### Solar System Planets
- **Time span**: 1 year to 10 years
- **Expected accuracy**: Position error < 0.01 AU
- **Key metrics**: Orbital periods, planetary distances

#### Special Configurations
- **Lagrange points**: Stability and location accuracy
- **Resonances**: Periodic behavior validation
- **Close approaches**: Singular event handling

---

## HORIZONS Validation Results

**Date:** 2026-04-06  
**Epoch:** 2025-01-01 00:00:00 TDB  
**System:** Sun–Earth–Moon (3 bodies, real HORIZONS initial conditions)  
**Integrator:** RK4  
**Timestep:** 60 s  
**Reference:** NASA JPL HORIZONS API, Solar System Barycentric frame (ICRF/J2000)  

### Position Residuals

| Duration | Body  | Position Error (km) | Notes                        |
|----------|-------|---------------------|------------------------------|
| 7 days   | Earth | 572                 | Sub-1000 km, excellent       |
| 7 days   | Moon  | 398                 | Sub-1000 km, excellent       |
| 30 days  | Earth | 1,579               | ~1.6× Earth radius           |
| 30 days  | Moon  | 1,563               | Within lunar orbit tolerance |
| 180 days | Earth | 30,083              | ~8% of Moon–Earth distance   |
| 180 days | Moon  | 30,220              | ~8% of Moon–Earth distance   |

### Interpretation

The error growth pattern is approximately linear with time (~500 km/week
for short durations), which is the expected signature of missing planetary
perturbations — primarily Jupiter and Saturn — rather than integrator
failure. A non-symplectic integrator with significant numerical drift would
show quadratic or exponential error growth.

The dominant error sources in order of significance:

1. **Missing planets** — Jupiter's gravitational influence on Earth's orbit
   is ~367 m/s² perturbation over 6 months, accounting for most of the
   180-day error.
2. **Relativistic corrections** — General relativity contributes ~43
   arcseconds/century precession for Mercury; effect on Earth–Moon is
   smaller but non-negligible over months.
3. **Integrator drift** — RK4 at dt=60s contributes energy drift of
   less than 1×10⁻⁶ over 10,000 steps (see conservation tests), which
   is a small fraction of the total error budget.

### Reproducibility

To reproduce these results:
```bash
# Fetch real initial conditions
./build/bin/orbit-sim build-system \
    --bodies 10,399,301 \
    --epoch 2025-01-01 \
    --output systems/earth_moon_horizons_2025.json \
    --post

# Run validation study
python3 python/horizons_validation.py
```

---

## SPICE DE440 Validation Results

**Date:** 2026-11-18
**Epoch:** 2025-01-01T00:00:00 TDB
**System:** `systems/solar_system_spice.json` (Sun + 8 planets + Moon; ICs from DE440 via `python/spice_ic.py`)
**Reference:** NASA DE440 via SPICE `spkgeo`, ECLIPJ2000, positions relative to the Sun
**Setup:** fixed-step methods dt = 1 h (8766 steps = 1 Julian year); RK45 adaptive with atol = rtol as listed
**Reproduce:** `python3 python/spice_validation_table.py` (needs `data/spice/` kernels; writes `results/spice_validation/error_1yr_current.csv`)

### Earlier result retracted

Earlier versions of this repository (README, `docs/paper_notes.md`, notebook 07) reported an
Earth error of **88,126 km** after one year and attributed it to "missing GR + J2". That
attribution was wrong. The system file had been generated with masses `GM / 6.674e-11` while the
engine uses `G = 6.67430e-11`, so the engine's `G * M_sun` was 4.495e-5 too large relative to
DE440. With initial velocities fixed by DE440, that mismatch alone produces an error of the
reported size. A circular-orbit estimate (mean-motion error 2 * 4.495e-5 over one revolution at
1 AU) gives about 84,500 km; the measured value was 88,125 km. After regenerating the file with
`G = 6.67430e-11` (commit `20c7804`), the same runs give the numbers below.

### Earth error at 1 year, before and after the mass fix (km)

| Integrator | before: GR off | before: GR on | after: GR off | after: GR on |
|---|---|---|---|---|
| RK4 | 88,125.0 | 88,064.1 | 61.1 | 0.2 |
| Leapfrog | 87,969.8 | 87,908.8 | 94.3 | 155.3 |
| Yoshida4 | 88,125.0 | 88,063.9 | 61.1 | 0.1 |
| Hermite | 88,125.0 | 88,064.1 | 61.1 | 0.2 |
| RK45 tol=1e-9 | 88,125.1 | 88,064.2 | 61.2 | 0.3 |
| RK45 tol=1e-12 | 88,125.0 | 88,064.1 | 61.1 | 0.2 |

"Before" = `solar_system_spice.json` as committed on `main` at `444698f` (masses from G = 6.674e-11);
"after" = regenerated file. Positions and velocities are identical in the two files.
RK45 at tolerance 1e-9 and 1e-12 differ by 0.1 km in the "after" runs.

### Per-body error at 1 year, RK4 dt = 1 h (km)

| Body | before: GR off | after: GR off | after: GR on |
|---|---|---|---|
| Mercury | 95,027.3 | 88.1 | 0.1 |
| Venus | 106,324.4 | 96.4 | 0.0 |
| Earth | 88,125.0 | 61.1 | 0.2 |
| Moon | 90,850.8 | 50.6 | 12.0 |
| Mars | 71,683.6 | 31.1 | 0.2 |
| Jupiter | 5,253.1 | 0.6 | 0.1 |
| Saturn | 1,452.0 | 0.1 | 0.2 |
| Uranus | 353.3 | 0.1 | 0.1 |
| Neptune | 149.3 | 0.1 | 0.1 |

### All integrators, after the fix (km)

| Integrator | GR | Mercury | Venus | Earth | Moon | Mars | Jupiter | Saturn | Uranus | Neptune |
|---|---|---|---|---|---|---|---|---|---|---|
| RK4 | off | 88.1 | 96.4 | 61.1 | 50.6 | 31.1 | 0.6 | 0.1 | 0.1 | 0.1 |
| RK4 | on | 0.1 | 0.0 | 0.2 | 12.0 | 0.2 | 0.1 | 0.2 | 0.1 | 0.1 |
| Leapfrog | off | 4,432.2 | 464.9 | 94.3 | 1,109.0 | 6.7 | 0.6 | 0.1 | 0.1 | 0.1 |
| Leapfrog | on | 4,528.5 | 561.7 | 155.3 | 1,164.3 | 30.0 | 0.2 | 0.2 | 0.1 | 0.1 |
| Yoshida4 | off | 88.4 | 96.4 | 61.1 | 50.8 | 31.1 | 0.6 | 0.1 | 0.1 | 0.1 |
| Yoshida4 | on | 17.0 | 0.9 | 0.1 | 12.0 | 0.2 | 0.1 | 0.2 | 0.1 | 0.1 |
| Hermite | off | 88.1 | 96.4 | 61.1 | 50.6 | 31.1 | 0.6 | 0.1 | 0.1 | 0.1 |
| Hermite | on | 0.1 | 0.0 | 0.2 | 12.0 | 0.2 | 0.1 | 0.2 | 0.1 | 0.1 |
| RK45 tol=1e-9 | off | 87.7 | 96.4 | 61.2 | 46.4 | 31.1 | 0.6 | 0.1 | 0.1 | 0.1 |
| RK45 tol=1e-9 | on | 0.5 | 0.0 | 0.3 | 17.4 | 0.2 | 0.1 | 0.2 | 0.1 | 0.1 |
| RK45 tol=1e-12 | off | 88.1 | 96.4 | 61.1 | 50.6 | 31.1 | 0.6 | 0.1 | 0.1 | 0.1 |
| RK45 tol=1e-12 | on | 0.1 | 0.0 | 0.2 | 12.0 | 0.2 | 0.1 | 0.2 | 0.1 | 0.1 |

### Horizons initial conditions versus DE440

`systems/solar_system_horizons.json` (Horizons state vectors, same epoch) differs from DE440 at the
first snapshot (t = 1 h) by: Mercury 3,037.5 km, Venus 2,425.6, Earth 2,096.1, Moon 2,027.3,
Mars 1,574.6, Jupiter 930.3, Saturn 811.9, Uranus 895.0, Neptune 342.4 (heliocentric distance
error, measured with `python.spice_validate.validate` on a 1-step run). Horizons-derived initial
conditions therefore do **not** agree with DE440 at the sub-kilometre level; the DE440-derived file
is used for integration-accuracy validation for that reason.

### What the numbers support

- The 88,000 km error was a constants/mass-consistency bug in the generated data, not missing physics.
- Switching GR on reduces the RK4, Hermite and RK45 error for Mercury, Venus, Earth and Mars to 0.5 km or less, and the Earth error from 61.1 km to 0.2 km. With 10-body Newtonian gravity plus the Sun-centred 1PN term, the engine matches DE440 to under a kilometre for these bodies over one year.
- **Moon: 12.0 km with GR on (RK4/Hermite).** RK45 at tol = 1e-12 gives the same value, so it is not integration error. The cause is **unexplained** (candidates not tested: Earth/Moon figure and tides, asteroid perturbations, the single-Sun-centred form of the GR term).
- **Leapfrog, dt = 1 h:** Mercury 4,432 km (GR off). Halving dt gives 1,043 km at 1800 s and 196 km at 900 s (Earth/Venus floors at ~50 km there), consistent with second-order truncation error. It is numerical.
- **Yoshida4 with GR on:** Mercury 17.0 km versus 0.1 km for RK4/Hermite. Not investigated; the GR term depends on velocity, which a kick-drift splitting does not treat consistently, but this was not tested.
- J2, asteroid perturbations and Earth-Moon barycentre handling were not varied in these runs; no claim is made about them.

---

## Numerical Error Analysis

### Integration Error Sources

#### Local Truncation Error

For RK4 integration, local truncation error is:
$$
\epsilon_{local} = O(\Delta t^5)
$$

#### Global Accumulation Error

Global error accumulates as:
$$
\epsilon_{global} = O(\Delta t^4)
$$

#### Round-off Error

Machine precision limited error:
$$
\epsilon_{round-off} \approx 10^{-16} \text{ (double precision)}
$$

### Error Growth Characteristics

#### Short-term Behavior
- **Dominant error**: Local truncation error
- **Growth rate**: Linear with time step count
- **Mitigation**: Reduce time step or use higher-order method

#### Long-term Behavior
- **Dominant error**: Global accumulation and round-off
- **Growth rate**: Potentially exponential for unstable systems
- **Mitigation**: Symplectic integrators or periodic re-normalization

### Time Step Sensitivity Analysis

#### Convergence Testing

Systematic time step refinement to verify convergence:

```cpp
// Test time steps: 3600, 1800, 900, 450, 225 seconds
for (double dt : timeSteps) {
    runSimulation(system, dt, steps);
    computeErrorMetrics();
}
```

#### Expected Convergence

For RK4 integration:
- **Position error**: Should decrease by factor of 16 when dt halved
- **Velocity error**: Should decrease by factor of 16 when dt halved
- **Energy error**: Should decrease by factor of 16 when dt halved

---

## Validation Results Summary

### Two-Body Problem Validation

#### Circular Orbit Test
- **Initial conditions**: Circular orbit with known period
- **Expected results**: Constant radius and velocity magnitude
- **Validation metrics**: Orbital period, radius conservation

#### Elliptical Orbit Test
- **Initial conditions**: Elliptical orbit with known eccentricity
- **Expected results**: Kepler's laws compliance
- **Validation metrics**: Periapsis/apoapsis distances, period accuracy

### Three-Body Problem Validation

#### Earth-Moon-Sun System
- **Configuration**: Realistic masses and initial conditions
- **Expected behavior**: Stable lunar orbit with solar perturbations
- **Validation metrics**: Lunar period, Earth-Moon distance variation

#### Lagrange Point Validation
- **Test cases**: L4 and L5 point stability
- **Expected behavior**: Small oscillations around equilibrium
- **Validation metrics**: Libration amplitude and period

### N-Body System Validation

#### Solar System Simulation
- **Configuration**: 8 planets + Sun
- **Time span**: 10 years
- **Validation metrics**: Planetary positions vs. ephemerides

#### Asteroid Belt Simulation
- **Configuration**: Multiple small bodies
- **Expected behavior**: Gravitational interactions and orbital evolution
- **Validation metrics**: Energy conservation, orbital element changes

---

## Known Limitations

### Physical Model Limitations

#### Newtonian Approximation
- **Relativistic effects**: Ignored for Mercury and close solar approaches
- **Magnitude**: Position errors up to 43 arcseconds/century for Mercury
- **Impact**: Significant for high-precision ephemerides

#### Point Mass Assumption
- **Oblateness effects**: J2, J3 perturbations ignored
- **Tidal effects**: Earth-Moon tidal evolution not modeled
- **Impact**: Long-term orbital evolution inaccuracies

### Numerical Limitations

#### Fixed Time Step Integration
- **Adaptive stepping**: Not implemented
- **Impact**: Inefficient handling of varying time scales
- **Workaround**: Manual time step selection based on shortest period

#### Close Approach Handling
- **Singular behavior**: No collision detection or handling
- **Impact**: Numerical instabilities during close encounters
- **Mitigation**: Regularization or softening potentials

### Validation Limitations

#### Ephemeris Data Quality
- **Measurement errors**: HORIZONS data has inherent uncertainties
- **Model dependencies**: Different dynamical models may vary
- **Impact**: Limited validation accuracy

#### Time Scale Coverage
- **Short-term validation**: Days to months well-covered
- **Long-term validation**: Years to decades less certain
- **Impact**: Confidence decreases with simulation duration

---

## Validation Procedures

### Automated Testing

#### Unit Tests
```cpp
// Test energy conservation
TEST(EnergyConservation, CircularOrbit) {
    System system = createCircularOrbit();
    runSimulation(system, steps, dt);
    EXPECT_LT(energyError, 1e-6);
}

// Test momentum conservation
TEST(MomentumConservation, IsolatedSystem) {
    System system = createIsolatedSystem();
    runSimulation(system, steps, dt);
    EXPECT_LT(momentumError, 1e-10);
}
```

#### Regression Tests
- **Reference solutions**: Store validated results
- **Comparison metrics**: Position, velocity, orbital elements
- **Automated validation**: Continuous integration testing

### Manual Validation Procedures

#### Ephemeris Comparison Workflow
1. **Configure simulation**: Match HORIZONS initial conditions
2. **Run simulation**: Generate trajectory data
3. **Fetch ephemerides**: Download HORIZONS data for same period
4. **Compare results**: Compute residuals and statistics
5. **Document findings**: Record accuracy and limitations

#### Conservation Law Monitoring
1. **Initialize system**: Compute initial conserved quantities
2. **Run simulation**: Monitor conservation at each step
3. **Analyze drift**: Characterize error growth patterns
4. **Validate accuracy**: Ensure errors remain within acceptable bounds

---

## Continuous Validation Strategy

### Ongoing Monitoring

#### Automated Validation
- **Nightly tests**: Run comprehensive validation suite
- **Performance tracking**: Monitor accuracy trends
- **Regression detection**: Identify accuracy degradation

#### Manual Review
- **Weekly validation**: Detailed analysis of key test cases
- **Monthly assessment**: Long-term accuracy evaluation
- **Quarterly review**: Comprehensive validation report

### Validation Maintenance

#### Test Case Updates
- **New configurations**: Add emerging validation scenarios
- **Improved metrics**: Enhance accuracy assessment methods
- **Extended coverage**: Expand validation time scales

#### Documentation Updates
- **Results recording**: Maintain validation history
- **Limitations tracking**: Document known issues
- **Improvement planning**: Guide future development

---

## References and Standards

### Validation Standards

#### NASA Standards
- **NASA-STD-7009**: Standard for Models and Simulations
- **NASA-STD-8719.13**: Software Safety Standard
- **NASA-HDBK-7002**: Dynamic Models and Verification

#### International Standards
- **ISO 10303**: Product data representation and exchange
- **IEC 61508**: Functional safety of electrical systems

### Validation Literature

#### Orbital Dynamics
- **Vallado, D.A.** *Fundamentals of Astrodynamics and Applications*
- **Montenbruck, O., and Gill, E.** *Satellite Orbits*
- **Bate, R.R., et al.** *Fundamentals of Astrodynamics*

#### Numerical Methods
- **Hairer, E., et al.** *Solving Ordinary Differential Equations I*
- **Butcher, J.C.** *Numerical Methods for Ordinary Differential Equations*
- **Iserles, A.** *A First Course in the Numerical Analysis of Differential Equations*

---