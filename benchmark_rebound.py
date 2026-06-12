"""
Benchmark: Orbital Mechanics Engine vs REBOUND
Sun + 8 planets + Moon (10 bodies), 1-year duration.
"""

import json
import time
import rebound
import orbit
import numpy as np

G = 6.674e-11
YEAR = 365.25 * 24 * 3600
SYSTEM = "systems/solar_system.json"

# Load bodies from JSON so both frameworks use identical initial conditions
with open(SYSTEM) as f:
    data = json.load(f)
bodies = data["bodies"]


def rebound_run(integrator, dt=None):
    sim = rebound.Simulation()
    sim.G = G
    sim.integrator = integrator
    for b in bodies:
        p, v = b["position"], b["velocity"]
        sim.add(m=b["mass"], x=p[0], y=p[1], z=p[2], vx=v[0], vy=v[1], vz=v[2])
    sim.move_to_com()
    if dt is not None:
        sim.dt = dt
    E0 = sim.energy()
    t0 = time.perf_counter()
    sim.integrate(YEAR)
    elapsed = time.perf_counter() - t0
    drift = abs((sim.energy() - E0) / E0)
    steps = int(sim.steps_done) if hasattr(sim, "steps_done") else None
    return elapsed, drift, steps


def orbit_fixed(integrator, steps, dt):
    t0 = time.perf_counter()
    result = orbit.simulate(SYSTEM, steps=steps, dt=dt, integrator=integrator)
    elapsed = time.perf_counter() - t0
    E = result.energies_numpy()
    drift = abs((E[-1] - E[0]) / E[0])
    return elapsed, drift, steps


def orbit_adaptive(atol):
    t0 = time.perf_counter()
    result = orbit.simulate_adaptive(SYSTEM, duration_s=YEAR, dt_initial=3600.0,
                                     atol=atol, rtol=atol)
    elapsed = time.perf_counter() - t0
    E = result.energies_numpy()
    drift = abs((E[-1] - E[0]) / E[0])
    return elapsed, drift, len(result.snapshots)


print(f"\nSystem: solar_system.json ({len(bodies)} bodies), duration = 1 year")
print(f"\n{'Integrator':<32} {'Time (ms)':>10} {'Steps':>8} {'Energy drift':>14}")
print("-" * 70)

e, d, s = orbit_fixed(orbit.Integrator.RK4, steps=8760, dt=3600.0)
print(f"{'Ours: RK4 (dt=1h)':<32} {e*1000:>10.1f} {s:>8} {d:>14.3e}")

e, d, s = orbit_fixed(orbit.Integrator.Leapfrog, steps=8760, dt=3600.0)
print(f"{'Ours: Leapfrog (dt=1h)':<32} {e*1000:>10.1f} {s:>8} {d:>14.3e}")

e, d, s = orbit_adaptive(atol=1e-9)
print(f"{'Ours: RK45 (atol=1e-9)':<32} {e*1000:>10.1f} {s:>8} {d:>14.3e}")

e, d, s = orbit_adaptive(atol=1e-12)
print(f"{'Ours: RK45 (atol=1e-12)':<32} {e*1000:>10.1f} {s:>8} {d:>14.3e}")

e, d, s = rebound_run("ias15")
steps_str = str(s) if s is not None else "N/A"
print(f"{'REBOUND: IAS15 (default)':<32} {e*1000:>10.1f} {steps_str:>8} {d:>14.3e}")

e, d, s = rebound_run("whfast", dt=3600.0)
steps_str = str(s) if s is not None else "N/A"
print(f"{'REBOUND: WHFast (dt=1h)':<32} {e*1000:>10.1f} {steps_str:>8} {d:>14.3e}")

print()
