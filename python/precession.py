"""
Apsidal-precession measurement for a two-body (Sun + planet) system.

The perihelion longitude is measured once per orbit: for each local minimum of
the separation r(t) a local degree-4 polynomial fit to r^2(t) gives the
interpolated periapsis time, and degree-4 fits to x(t), y(t) give the position
there.  A straight line through (orbit number, unwrapped longitude) gives the
apsidal rotation rate.  Because every sample is taken at the same orbital
phase, phase-dependent sampling bias cancels; only positions are needed (no
finite-difference velocities).  Validated on analytic Kepler orbits in
examples/10_numerical_precession_sweep.ipynb.
"""

import json
import math
import os
import tempfile

import numpy as np

import orbit

CENTURY_S = 100 * 365.25 * 86400.0
ARCSEC = 180.0 / math.pi * 3600.0
A_MERCURY = 5.7909e10      # m
M_MERCURY = 3.3011e23      # kg


def mu_total(m_planet=M_MERCURY):
    return orbit.G * (orbit.M_SUN + m_planet)


def period(a=A_MERCURY, m_planet=M_MERCURY):
    return 2.0 * math.pi * math.sqrt(a ** 3 / mu_total(m_planet))


def gr_rate_arcsec_per_century(e, a=A_MERCURY, m_planet=M_MERCURY):
    """1PN apsidal rate: 6 pi GM / (c^2 a (1-e^2)) per orbit, times orbits per century."""
    dw = 6.0 * math.pi * orbit.G * orbit.M_SUN / (orbit.C_LIGHT ** 2 * a * (1.0 - e * e))
    return dw / period(a, m_planet) * CENTURY_S * ARCSEC


def write_system(path, e, a=A_MERCURY, m_planet=M_MERCURY):
    """Sun at rest at the origin, planet at perihelion on +x moving +y."""
    r_p = a * (1.0 - e)
    v_p = math.sqrt(mu_total(m_planet) * (1.0 + e) / r_p)
    system = {"bodies": [
        {"name": "Sun", "mass": orbit.M_SUN, "position": [0.0, 0.0, 0.0], "velocity": [0.0, 0.0, 0.0]},
        {"name": "Mercury", "mass": m_planet, "position": [r_p, 0.0, 0.0], "velocity": [0.0, v_p, 0.0]},
    ]}
    with open(path, "w") as f:
        json.dump(system, f)
    return path


def perihelion_longitudes(t, rel, half_window=3):
    """Return (periapsis_times, unwrapped longitudes in rad). t in s, rel (N, 3) in m."""
    x, y = rel[:, 0], rel[:, 1]
    r2 = x * x + y * y
    n = len(t)
    times, lons = [], []
    i = half_window
    while i < n - half_window:
        if r2[i] < r2[i - 1] and r2[i] <= r2[i + 1] and r2[i] == r2[i - half_window:i + half_window + 1].min():
            sl = slice(i - half_window, i + half_window + 1)
            tau = t[sl] - t[i]
            s = max(abs(tau[0]), abs(tau[-1]))
            u = tau / s
            c = np.polyfit(u, r2[sl] / r2[i], 4)
            roots = np.roots(np.polyder(c))
            roots = roots[np.abs(roots.imag) < 1e-9].real
            roots = roots[np.abs(roots) <= 1.0]
            if len(roots):
                u0 = roots[np.argmin(np.polyval(c, roots))]
                xm = np.polyval(np.polyfit(u, x[sl], 4), u0)
                ym = np.polyval(np.polyfit(u, y[sl], 4), u0)
                times.append(t[i] + u0 * s)
                lons.append(math.atan2(ym, xm))
            i += half_window + 1
        else:
            i += 1
    return np.array(times), np.unwrap(np.array(lons))


def precession_rate(t, rel, half_window=3):
    """Apsidal rotation rate in arcsec/century and its standard error, plus n_orbits."""
    tp, lon = perihelion_longitudes(t, rel, half_window)
    if len(tp) < 10:
        return float("nan"), float("nan"), len(tp)
    k = np.arange(len(tp), dtype=float)
    # fit against periapsis time so unequal periods do not matter
    A = np.vstack([tp - tp[0], np.ones_like(tp)]).T
    coef, res, *_ = np.linalg.lstsq(A, lon, rcond=None)
    resid = lon - A @ coef
    dof = max(len(tp) - 2, 1)
    cov = (resid @ resid / dof) * np.linalg.inv(A.T @ A)
    return coef[0] * CENTURY_S * ARCSEC, math.sqrt(cov[0, 0]) * CENTURY_S * ARCSEC, len(tp)


def rel_positions(result):
    names = result.body_names
    pos = result.positions_numpy()
    rel = pos[:, names.index("Mercury"), :] - pos[:, names.index("Sun"), :]
    t = np.array([s.time_s for s in result.snapshots])
    return t, rel


def kepler_relative_positions(t, e, a=A_MERCURY, m_planet=M_MERCURY):
    """Exact Newtonian relative position (N, 3) at times t, periapsis on +x at t=0."""
    n = math.sqrt(mu_total(m_planet) / a ** 3)
    M = n * np.asarray(t, dtype=float)
    E = M.copy()
    for _ in range(60):
        E = E - (E - e * np.sin(E) - M) / (1.0 - e * np.cos(E))
    return np.stack([a * (np.cos(E) - e), a * math.sqrt(1.0 - e * e) * np.sin(E), np.zeros_like(E)], axis=1)


def estimator_floor(t, e, half_window=3):
    """Rate (arcsec/century) the estimator reports for an exact ellipse sampled at times t.
    The true value is zero, so this is the estimator's own error for that sampling."""
    return precession_rate(np.asarray(t), kepler_relative_positions(t, e), half_window)[0]
