"""
SPICE validation: compare simulated trajectories against NASA DE440 ephemeris.

Usage:
    import orbit
    from python.spice_validate import validate

    result = orbit.simulate('systems/solar_system_horizons.json', steps=8760, dt=3600.0)
    errors = validate(result, epoch='2025-01-01T00:00:00')
    # errors: {body_name: np.array of position errors in km}
"""

import os
import numpy as np
import spiceypy as spice

# Path to kernel files relative to project root
_KERNEL_DIR = os.path.join(os.path.dirname(__file__), '..', 'data', 'spice')

# NAIF body IDs for de440.bsp
# de440.bsp stores planetary barycenters relative to SSB.
# Earth (399) and Moon (301) are available directly.
# For other planets we use the system barycenter (close enough for km-level validation).
NAIF_IDS = {
    'Sun':     10,
    'Mercury': 1,    # Mercury barycenter ≈ Mercury center (no moons)
    'Venus':   2,    # Venus barycenter ≈ Venus center (no moons)
    'Earth':   399,  # Earth center (distinct from Earth-Moon barycenter 3)
    'Moon':    301,  # Moon center
    'Mars':    4,    # Mars barycenter ≈ Mars center (Phobos/Deimos negligible)
    'Jupiter': 5,    # Jupiter system barycenter
    'Saturn':  6,    # Saturn system barycenter
    'Uranus':  7,    # Uranus system barycenter
    'Neptune': 8,    # Neptune system barycenter
}


def _load_kernels():
    d = os.path.abspath(_KERNEL_DIR)
    spice.furnsh(os.path.join(d, 'naif0012.tls'))
    spice.furnsh(os.path.join(d, 'pck00011.tpc'))
    spice.furnsh(os.path.join(d, 'de440.bsp'))


def validate(result, epoch='2025-01-01T00:00:00'):
    """
    Compare a SimulationResult against SPICE DE440 ground truth.

    Positions are compared relative to the Sun in each system, so the
    frame origin doesn't matter — only relative geometry does.

    Parameters
    ----------
    result : orbit.SimulationResult
        Output from orbit.simulate() or orbit.simulate_adaptive().
        Must have been run from HORIZONS initial conditions (solar_system_horizons.json).
    epoch : str
        ISO-format start time of the simulation matching the system JSON epoch.
        Default: '2025-01-01T00:00:00'

    Returns
    -------
    dict
        {body_name: np.ndarray} — position error vs SPICE in km, shape (n_snapshots,)
    """
    _load_kernels()

    try:
        et_epoch = spice.str2et(epoch)
        n_snapshots = len(result.snapshots)
        body_names = result.body_names

        # Build index map: body name → index in positions array
        body_index = {name: i for i, name in enumerate(body_names)}

        # Sun must be present to compute relative positions
        if 'Sun' not in body_index:
            raise ValueError("System must contain 'Sun' for frame-relative comparison")

        sun_idx = body_index['Sun']

        # shape: (n_snapshots, n_bodies, 3) in metres
        pos_all = result.positions_numpy()
        times   = np.array([snap.time_s for snap in result.snapshots])

        errors = {name: np.zeros(n_snapshots)
                  for name in body_names if name != 'Sun' and name in NAIF_IDS}

        for s in range(n_snapshots):
            et = et_epoch + times[s]

            # Our simulation: planet position relative to Sun (convert m → km)
            sun_pos_sim = pos_all[s, sun_idx, :] / 1000.0

            for name, err_arr in errors.items():
                planet_idx = body_index[name]
                planet_pos_sim = pos_all[s, planet_idx, :] / 1000.0
                rel_sim = planet_pos_sim - sun_pos_sim  # km

                # SPICE: planet and Sun positions relative to SSB
                state_planet, _ = spice.spkgeo(NAIF_IDS[name], et, 'ECLIPJ2000', 0)
                state_sun,    _ = spice.spkgeo(10,               et, 'ECLIPJ2000', 0)
                rel_spice = np.array(state_planet[:3]) - np.array(state_sun[:3])  # km

                err_arr[s] = np.linalg.norm(rel_sim - rel_spice)

    finally:
        spice.kclear()

    return errors


def print_summary(errors, snapshots):
    """Print a summary table of position errors."""
    times_yr = np.array([s.time_s for s in snapshots]) / (365.25 * 24 * 3600)

    print(f"\n{'Body':<12} {'Max error (km)':>16} {'Mean error (km)':>16} {'Error at 1yr (km)':>18}")
    print('-' * 66)
    for name, err in errors.items():
        max_e  = np.max(err)
        mean_e = np.mean(err)
        # Find error closest to 1 year
        idx_1yr = np.argmin(np.abs(times_yr - 1.0))
        e_1yr  = err[idx_1yr]
        print(f"{name:<12} {max_e:>16.1f} {mean_e:>16.1f} {e_1yr:>18.1f}")
