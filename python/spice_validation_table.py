"""
Per-body position error vs NASA DE440 at exactly one Julian year, for every
integrator with GR off and on.  Source of every number quoted in
docs/validation.md, docs/paper_notes.md and paper.md for SPICE validation.

Usage (from the project root, SPICE kernels in data/spice/):
    python3 python/spice_validation_table.py
    python3 python/spice_validation_table.py --system other.json --tag baseline

Writes results/spice_validation/error_1yr_<tag>.csv  (one row per run/body).
"""

import argparse
import csv
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import orbit
from python.spice_validate import validate

YEAR = 365.25 * 86400.0   # Julian year, s
DT = 3600.0               # fixed-step size, s
STRIDE = 5                # snapshots land on steps 1, 6, ..., 8766, so the last is exactly 1 yr
EPOCH = '2025-01-01T00:00:00'
DEFAULT_SYSTEM = 'systems/solar_system_spice.json'

# (label, integrator name or 'RK45', tolerance)
CONFIGS = [
    ('RK4',            'RK4',      None),
    ('Leapfrog',       'Leapfrog', None),
    ('Yoshida4',       'Yoshida4', None),
    ('Hermite',        'Hermite',  None),
    ('RK45 tol=1e-9',  'RK45',     1e-9),
    ('RK45 tol=1e-12', 'RK45',     1e-12),
]


def run_one(system, integrator, tol, gr):
    if integrator == 'RK45':
        return orbit.simulate_adaptive(
            system, duration_s=YEAR, dt_initial=DT,
            atol=tol, rtol=tol, gr=gr)
    return orbit.simulate(
        system, steps=int(round(YEAR / DT)), dt=DT,
        integrator=getattr(orbit.Integrator, integrator), stride=STRIDE, gr=gr)


def sweep(system=DEFAULT_SYSTEM, configs=CONFIGS):
    """Return list of dicts: label, gr, t_end_s, errors {body: km at last snapshot}."""
    rows = []
    for label, integ, tol in configs:
        for gr in (False, True):
            res = run_one(system, integ, tol, gr)
            err = validate(res, epoch=EPOCH)
            t_end = res.snapshots[-1].time_s
            if abs(t_end - YEAR) > 3600.0:
                raise RuntimeError(f'{label} gr={gr}: last snapshot at {t_end/86400:.3f} d, expected 365.25 d')
            rows.append({'label': label, 'gr': gr, 't_end_s': t_end,
                         'errors': {b: float(e[-1]) for b, e in err.items()}})
    return rows


def write_csv(rows, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bodies = list(rows[0]['errors'])
    with open(path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['integrator', 'gr', 't_end_days'] + [f'{b}_km' for b in bodies])
        for r in rows:
            w.writerow([r['label'], int(r['gr']), f"{r['t_end_s']/86400:.4f}"] +
                       [f"{r['errors'][b]:.3f}" for b in bodies])


def print_table(rows):
    bodies = list(rows[0]['errors'])
    print(f"{'Integrator':<16}{'GR':<5}" + ''.join(f'{b:>10}' for b in bodies))
    for r in rows:
        print(f"{r['label']:<16}{'on' if r['gr'] else 'off':<5}" +
              ''.join(f"{r['errors'][b]:>10.1f}" for b in bodies))


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    ap.add_argument('--system', default=DEFAULT_SYSTEM)
    ap.add_argument('--tag', default='current')
    a = ap.parse_args()
    rows = sweep(a.system)
    out = os.path.join('results', 'spice_validation', f'error_1yr_{a.tag}.csv')
    write_csv(rows, out)
    print_table(rows)
    print(f'\nWrote {out}')
