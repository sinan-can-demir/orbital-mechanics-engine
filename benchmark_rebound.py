"""
Benchmark: Orbital Mechanics Engine vs REBOUND
Sun + 8 planets + Moon (10 bodies), 1-year duration.

Each configuration is timed REPS times (default 10); median and min-max are
reported, plus the interquartile range. Energy drift is deterministic and
taken from the last repetition. Writes results/paper_benchmark/benchmark_timing.csv.

Usage: python3 benchmark_rebound.py [--reps N]
"""

import argparse
import csv
import json
import os
import statistics
import time
import rebound
import orbit
import numpy as np

G = orbit.G  # engine constant (include/constants.h)
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



ap = argparse.ArgumentParser()
ap.add_argument("--reps", type=int, default=10)
REPS = ap.parse_args().reps

# Warm-up so first-call effects (imports, allocator, page faults) are not timed
orbit.simulate(SYSTEM, steps=10, dt=3600.0)


def timed(fn, *args, **kw):
    times, last = [], None
    for _ in range(REPS):
        e, d, s = fn(*args, **kw)
        times.append(e * 1000.0)
        last = (d, s)
    return times, last[0], last[1]


def fixed_n(name):
    return lambda: orbit_fixed(getattr(orbit.Integrator, name), steps=8766, dt=3600.0)


configs = [
    ("Ours: RK4 (dt=1h)", fixed_n("RK4")),
    ("Ours: Leapfrog (dt=1h)", fixed_n("Leapfrog")),
    ("Ours: Yoshida4 (dt=1h)", fixed_n("Yoshida4")),
    ("Ours: Hermite (dt=1h)", fixed_n("Hermite")),
    ("Ours: RK45 (tol=1e-9)", lambda: orbit_adaptive(1e-9)),
    ("Ours: RK45 (tol=1e-12)", lambda: orbit_adaptive(1e-12)),
    ("REBOUND: IAS15 (default)", lambda: rebound_run("ias15")),
    ("REBOUND: WHFast (dt=1h)", lambda: rebound_run("whfast", dt=3600.0)),
]

print(f"\nSystem: {SYSTEM} ({len(bodies)} bodies), duration = 1 Julian year, {REPS} repetitions each")
print(f"\n{'Integrator':<28} {'median ms':>10} {'min':>8} {'max':>8} {'IQR':>8} {'Steps':>8} {'Energy drift':>14}")
print("-" * 92)

rows = []
for label, fn in configs:
    times, drift, steps = timed(lambda f=fn: f())
    q = statistics.quantiles(times, n=4) if len(times) >= 2 else [times[0]] * 3
    med = statistics.median(times)
    iqr = q[2] - q[0]
    steps_str = str(steps) if steps is not None else "N/A"
    print(f"{label:<28} {med:>10.1f} {min(times):>8.1f} {max(times):>8.1f} {iqr:>8.1f} {steps_str:>8} {drift:>14.3e}")
    rows.append([label, REPS, f"{med:.3f}", f"{min(times):.3f}", f"{max(times):.3f}", f"{iqr:.3f}", steps_str, f"{drift:.4e}"])

os.makedirs("results/paper_benchmark", exist_ok=True)
with open("results/paper_benchmark/benchmark_timing.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["integrator", "reps", "median_ms", "min_ms", "max_ms", "iqr_ms", "steps", "energy_drift"])
    w.writerows(rows)
print("\nWrote results/paper_benchmark/benchmark_timing.csv")
