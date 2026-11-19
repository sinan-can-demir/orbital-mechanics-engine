"""
Analysis of results/precession_sweep/numerical_precession.csv (see precession_sweep.py).

    fit_scaling(df)      power-law fits  |rate| = C(e) * dt^p  per integrator and eccentricity
    thresholds(df, 0.01) timestep / tolerance and cost at which the spurious (GR-off)
                         apsidal rate falls to `frac` of the 1PN rate, by log-log interpolation
"""

import math
import os
import sys

import numpy as np
import pandas as pd

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from python import precession as P

SWEEP_CSV = 'results/precession_sweep/numerical_precession.csv'
FIXED = ['RK4', 'Leapfrog', 'Yoshida4', 'Hermite']
ALL = FIXED + ['RK45']
FLOOR = 1e-5   # arcsec/century; below this the fixed-step values are round-off dominated


def load(path=SWEEP_CSV):
    return pd.read_csv(path)


def fit_scaling(df, lo=1e-3, hi=2e3):
    """Fit |rate| = C dt^p over the asymptotic regime lo < |rate| < hi (arcsec/century)."""
    rows = []
    d0 = df[df.gr == 0]
    for integ in FIXED:
        for e in sorted(d0.e.unique()):
            g = d0[(d0.integrator == integ) & (d0.e == e)].sort_values('dt_s')
            x, y = g.dt_s.values, np.abs(g.rate_arcsec_per_century.values)
            m = (y > lo) & (y < hi)
            p, lnC = np.polyfit(np.log(x[m]), np.log(y[m]), 1)
            rows.append(dict(integrator=integ, e=e, order_p=p, C=math.exp(lnC), n_points=int(m.sum()),
                             sign=int(np.sign(g.rate_arcsec_per_century.values[m].mean()))))
    return pd.DataFrame(rows)


def fit_eccentricity(df, dt_s=7200.0):
    """At fixed dt, fit |rate| ~ A (1-e^2)^-q ; returns q, max |ln residual|."""
    rows = []
    d0 = df[(df.gr == 0) & (df.dt_s == dt_s)]
    for integ in FIXED:
        s = d0[d0.integrator == integ].sort_values('e')
        y = np.abs(s.rate_arcsec_per_century.values)
        x = 1.0 - s.e.values ** 2
        slope, lnA = np.polyfit(np.log(x), np.log(y), 1)
        rows.append(dict(integrator=integ, q=-slope, max_abs_ln_residual=float(np.max(np.abs(np.log(y) - (lnA + slope * np.log(x)))))))
    return pd.DataFrame(rows)


def _cross(xs, ys, thr):
    """First bracket (i, i+1) where ys crosses thr going up with xs ascending; log-log interp. Returns (frac, i)."""
    for i in range(len(xs) - 1):
        if ys[i] <= thr <= ys[i + 1]:
            f = (math.log(thr) - math.log(ys[i])) / (math.log(ys[i + 1]) - math.log(ys[i]))
            return f, i
    return None, None


def thresholds(df, frac=0.01):
    """Parameter and cost at which |spurious rate| = frac * GR rate."""
    rows = []
    d0 = df[df.gr == 0]
    for integ in ALL:
        for e in sorted(d0.e.unique()):
            thr = frac * P.gr_rate_arcsec_per_century(e)
            g = d0[(d0.integrator == integ) & (d0.e == e)]
            if integ == 'RK45':
                g = g.sort_values('tol')
                xs = g.tol.values
            else:
                g = g.sort_values('dt_s')
                xs = g.dt_s.values
            ys = np.abs(g.rate_arcsec_per_century.values)
            ok = ys > FLOOR   # drop round-off-dominated points
            xs, ys, sw = xs[ok], ys[ok], g.force_sweeps.values[ok]
            f, i = _cross(xs, ys, thr)
            if f is None:
                rows.append(dict(integrator=integ, e=e, threshold=thr, param=np.nan, sweeps_100yr=np.nan))
                continue
            param = math.exp(math.log(xs[i]) + f * (math.log(xs[i + 1]) - math.log(xs[i])))
            if integ == 'RK45':
                sweeps = math.exp(math.log(sw[i]) + f * (math.log(sw[i + 1]) - math.log(sw[i])))
            else:
                sweeps = sw[i] * xs[i] / param   # cost is exactly proportional to 1/dt
            rows.append(dict(integrator=integ, e=e, threshold=thr, param=param, sweeps_100yr=sweeps))
    out = pd.DataFrame(rows)
    out['param_kind'] = np.where(out.integrator == 'RK45', 'tolerance', 'dt_s')
    out['sweeps_per_orbit'] = out.sweeps_100yr / (100 * 365.25 * 86400 / P.period())
    return out


def gr_check(df):
    """GR-on minus GR-off rate versus the analytic 1PN rate, at the finest timestep/tolerance."""
    off, on = df[df.gr == 0], df[df.gr == 1]
    m = off.merge(on, on=['integrator', 'dt_s', 'tol', 'e'], suffixes=('_off', '_on'))
    m['measured'] = m.rate_arcsec_per_century_on - m.rate_arcsec_per_century_off
    m['theory'] = [P.gr_rate_arcsec_per_century(e) for e in m.e]
    fine = m[((m.integrator != 'RK45') & (m.dt_s == 900)) | ((m.integrator == 'RK45') & (m.tol == 1e-12))].copy()
    fine['rel_error_pct'] = (fine.measured / fine.theory - 1.0) * 100.0
    return fine[['integrator', 'e', 'measured', 'theory', 'rel_error_pct']]
