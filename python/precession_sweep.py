"""
Sweep spurious (numerical) apsidal precession over integrator, timestep/tolerance
and eccentricity for a Mercury-like Sun-planet pair, 100 years, GR off and on.

Cost model (pairwise-force sweeps over the whole system, per step):
    RK4 4, Leapfrog 1, Yoshida4 3, Hermite 4 (two acceleration + two jerk passes),
    RK45 6 per accepted step and 7 per rejected step (FSAL; see src/core/simulation.cpp).

Usage:  python3 python/precession_sweep.py [--years 100] [--procs 8]
        python3 python/precession_sweep.py --only Leapfrog --dt-hours 0.03125,0.015625,0.0078125 --append
Output: results/precession_sweep/numerical_precession.csv
"""

import argparse
import csv
import ctypes
import itertools
import multiprocessing as mp
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import orbit
from python import precession as P

ECCENTRICITIES = [0.05, 0.1, 0.2056, 0.3, 0.4, 0.5, 0.6]
DT_HOURS = [1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16]
TOLERANCES = [10.0 ** -k for k in range(5, 14)]
FIXED = ['RK4', 'Leapfrog', 'Yoshida4', 'Hermite']
EVALS_PER_STEP = {'RK4': 4, 'Leapfrog': 1, 'Yoshida4': 3, 'Hermite': 4}
OUT_SPACING_H = 4.0  # snapshot spacing for the periapsis fit (validated <= 4 h)


def _capture_stdout(fn):
    """Run fn() while fd 1 is redirected to a temp file; return (result, captured text)."""
    sys.stdout.flush()
    saved = os.dup(1)
    with tempfile.TemporaryFile(mode='w+') as tmp:
        os.dup2(tmp.fileno(), 1)
        try:
            res = fn()
        finally:
            sys.stdout.flush()
            ctypes.CDLL(None).fflush(None)  # flush C/C++ stdio buffers into the temp file
            os.dup2(saved, 1)
            os.close(saved)
        tmp.seek(0)
        return res, tmp.read()


def run_case(task):
    integ, param, e, gr, years = task
    path = tempfile.mktemp(suffix='.json')
    P.write_system(path, e)
    try:
        duration = years * 365.25 * 86400.0
        if integ == 'RK45':
            res, out = _capture_stdout(lambda: orbit.simulate_adaptive(
                path, duration_s=duration, dt_initial=3600.0, atol=param, rtol=param, gr=gr))
            acc = int(re.search(r'accepted:\s*(\d+)', out).group(1))
            rej = int(re.search(r'rejected:\s*(\d+)', out).group(1))
            evals = 6 * acc + 7 * rej
            steps, dt_s = acc, float('nan')
        else:
            dt_s = param * 3600.0
            steps = int(round(duration / dt_s))
            stride = max(1, int(round(OUT_SPACING_H * 3600.0 / dt_s)))
            res = orbit.simulate(path, steps=steps, dt=dt_s, integrator=getattr(orbit.Integrator, integ),
                                 stride=stride, gr=gr)
            evals = EVALS_PER_STEP[integ] * steps
        t, rel = P.rel_positions(res)
        rate, se, n = P.precession_rate(t, rel)
        floor = P.estimator_floor(t, e)
        spacing_h = float(((t[1:] - t[:-1]).max()) / 3600.0)
        return dict(integrator=integ, dt_s=dt_s, tol=(param if integ == 'RK45' else float('nan')), e=e,
                    gr=int(gr), rate_arcsec_per_century=rate, stderr=se, n_orbits=n, estimator_floor=floor, steps=steps,
                    force_sweeps=evals, max_output_spacing_h=spacing_h)
    finally:
        os.unlink(path)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--years', type=int, default=100)
    ap.add_argument('--procs', type=int, default=8)
    ap.add_argument('--only', default=None, help='comma list of integrators (RK45 included by name)')
    ap.add_argument('--dt-hours', default=None, help='comma list overriding the dt grid (hours)')
    ap.add_argument('--append', action='store_true', help='append to the existing CSV instead of overwriting')
    a = ap.parse_args()
    only = a.only.split(',') if a.only else None
    dts = [float(x) for x in a.dt_hours.split(',')] if a.dt_hours else DT_HOURS
    tasks = [(i, d, e, gr, a.years) for i, d, e, gr in itertools.product(FIXED, dts, ECCENTRICITIES, (False, True))
             if only is None or i in only]
    if only is None or 'RK45' in only:
        tasks += [('RK45', tol, e, gr, a.years) for tol, e, gr in itertools.product(TOLERANCES, ECCENTRICITIES, (False, True))]
    # biggest jobs first for load balance
    tasks.sort(key=lambda t: -(1.0 / t[1] if t[0] != 'RK45' else 0.5 / t[1] ** 0.25))
    os.makedirs('results/precession_sweep', exist_ok=True)
    rows = []
    with mp.Pool(a.procs) as pool:
        for k, r in enumerate(pool.imap_unordered(run_case, tasks, chunksize=1), 1):
            rows.append(r)
            if k % 50 == 0:
                print(f'{k}/{len(tasks)} done', file=sys.stderr, flush=True)
    rows.sort(key=lambda r: (r['integrator'], r['e'], r['gr'], -(r['dt_s'] if r['dt_s'] == r['dt_s'] else 0), r['tol'] if r['tol'] == r['tol'] else 0))
    out = 'results/precession_sweep/numerical_precession.csv'
    new_file = not (a.append and os.path.exists(out))
    with open(out, 'a' if a.append else 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        if new_file:
            w.writeheader()
        w.writerows(rows)
    print('wrote results/precession_sweep/numerical_precession.csv', len(rows), 'rows')
