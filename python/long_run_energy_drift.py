"""
Long-run energy drift of every fixed-step integrator (and RK45) on the 10-body
solar system, GR off, up to 1000 Julian years.

Reports the maximum |dE/E0| over [0, T] for T = 10, 100, 1000 years (snapshots
are taken once per year) and wall time.  Writes
results/paper_benchmark/long_run_energy_drift_dt<dt>.csv and ..._series.csv.

Usage: python3 python/long_run_energy_drift.py [--years 1000] [--dt 3600] [--skip-rk45]
Output files carry the dt in their name (..._dt3600.csv).
"""

import argparse
import csv
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import orbit

SYSTEM = 'systems/solar_system.json'
YEAR = 365.25 * 86400.0
CHECKPOINTS = (10, 100, 1000)


def run_fixed(name, years, dt):
    steps_per_year = int(round(YEAR / dt))
    t0 = time.perf_counter()
    r = orbit.simulate(SYSTEM, steps=steps_per_year * years, dt=dt,
                       integrator=getattr(orbit.Integrator, name),
                       stride=steps_per_year, gr=False)
    wall = time.perf_counter() - t0
    E = r.energies_numpy()
    # energy at t=0 is the system's initial energy; snapshots start at step 1
    return np.array([s.time_s for s in r.snapshots]) / YEAR, E, wall


def run_rk45(tol, years):
    t0 = time.perf_counter()
    r = orbit.simulate_adaptive(SYSTEM, duration_s=YEAR * years, dt_initial=3600.0,
                                output_interval_s=YEAR, atol=tol, rtol=tol, gr=False)
    wall = time.perf_counter() - t0
    return np.array([s.time_s for s in r.snapshots]) / YEAR, r.energies_numpy(), wall


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--years', type=int, default=1000)
    ap.add_argument('--dt', type=float, default=3600.0)
    ap.add_argument('--skip-rk45', action='store_true', help='RK45 does not depend on --dt')
    a = ap.parse_args()
    os.makedirs('results/paper_benchmark', exist_ok=True)
    runs = [(n, f'{n} (dt={a.dt:g}s)', lambda n=n: run_fixed(n, a.years, a.dt))
            for n in ('RK4', 'Leapfrog', 'Yoshida4', 'Hermite')]
    if not a.skip_rk45:
        runs.append(('RK45', 'RK45 (tol=1e-9)', lambda: run_rk45(1e-9, a.years)))
    summary, series = [], []
    for name, label, fn in runs:
        t, E, wall = fn()
        drift = np.abs((E - E[0]) / E[0])  # relative to first snapshot (t = dt)
        row = [label, f'{wall:.1f}']
        for cp in CHECKPOINTS:
            if cp <= a.years:
                m = t <= cp + 1e-9
                row.append(f'{drift[m].max():.3e}')
            else:
                row.append('')
        summary.append(row)
        series += [[label, f'{ti:.4f}', f'{d:.4e}'] for ti, d in zip(t, drift)]
        print(' | '.join(row), flush=True)
    with open(f'results/paper_benchmark/long_run_energy_drift_dt{a.dt:g}.csv', 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['integrator', 'wall_s'] + [f'max_dE_over_{c}yr' for c in CHECKPOINTS])
        w.writerows(summary)
    with open(f'results/paper_benchmark/long_run_energy_drift_dt{a.dt:g}_series.csv', 'w', newline='') as f:
        w = csv.writer(f); w.writerow(['integrator', 'time_yr', 'abs_dE_over_E0']); w.writerows(series)
