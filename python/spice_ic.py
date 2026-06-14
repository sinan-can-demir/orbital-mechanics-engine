"""
Build a system JSON from NASA SPICE DE440 initial conditions.

Replaces HORIZONS-based build-system for validation workflows.
Using DE440 ICs with DE440 as ground truth eliminates the ~107,000 km
IC mismatch at t=0, giving a clean test of integration accuracy only.

Usage:
    python3 python/spice_ic.py --epoch 2025-01-01 --output systems/solar_system_spice.json
    python3 python/spice_ic.py  # uses defaults above
"""

import os
import sys
import json
import argparse
import numpy as np

_KERNEL_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'data', 'spice'))

G = 6.67430e-11  # m³ kg⁻¹ s⁻²  (CODATA 2018, matches physics::constants::G in utils.h)

# Body definitions: name, NAIF position ID, SPICE GM body name
# Position IDs match spice_validate.py for consistency
SOLAR_SYSTEM_BODIES = [
    {'name': 'Sun',     'naif_id': 10,  'gm_name': 'SUN'},
    {'name': 'Mercury', 'naif_id': 1,   'gm_name': 'MERCURY'},
    {'name': 'Venus',   'naif_id': 2,   'gm_name': 'VENUS'},
    {'name': 'Earth',   'naif_id': 399, 'gm_name': 'EARTH'},
    {'name': 'Moon',    'naif_id': 301, 'gm_name': 'MOON'},
    {'name': 'Mars',    'naif_id': 4,   'gm_name': 'MARS BARYCENTER'},
    {'name': 'Jupiter', 'naif_id': 5,   'gm_name': 'JUPITER BARYCENTER'},
    {'name': 'Saturn',  'naif_id': 6,   'gm_name': 'SATURN BARYCENTER'},
    {'name': 'Uranus',  'naif_id': 7,   'gm_name': 'URANUS BARYCENTER'},
    {'name': 'Neptune', 'naif_id': 8,   'gm_name': 'NEPTUNE BARYCENTER'},
]


def _load_kernels():
    import spiceypy as spice
    spice.furnsh(os.path.join(_KERNEL_DIR, 'naif0012.tls'))
    spice.furnsh(os.path.join(_KERNEL_DIR, 'pck00011.tpc'))
    spice.furnsh(os.path.join(_KERNEL_DIR, 'gm_de431.tpc'))
    spice.furnsh(os.path.join(_KERNEL_DIR, 'de440.bsp'))


def build_system(epoch='2025-01-01T00:00:00', bodies=None, output=None):
    """
    Query SPICE DE440 for initial conditions at the given epoch.

    Parameters
    ----------
    epoch : str
        ISO or SPICE-format epoch string, e.g. '2025-01-01T00:00:00'
    bodies : list of dict, optional
        Body definitions. Defaults to SOLAR_SYSTEM_BODIES.
    output : str, optional
        Path to write the JSON. If None, returns the dict without writing.

    Returns
    -------
    dict
        System JSON compatible with orbit.simulate().
    """
    import spiceypy as spice

    if bodies is None:
        bodies = SOLAR_SYSTEM_BODIES

    _load_kernels()

    try:
        et = spice.str2et(epoch)

        result_bodies = []
        for b in bodies:
            # Position and velocity relative to SSB in ECLIPJ2000 frame
            state, _ = spice.spkgeo(b['naif_id'], et, 'ECLIPJ2000', 0)
            pos_m = [x * 1000.0 for x in state[:3]]  # km → m
            vel_ms = [v * 1000.0 for v in state[3:]]  # km/s → m/s

            # Mass from GM value in pck kernel (km³/s² → kg)
            try:
                gm_km3s2 = spice.bodvrd(b['gm_name'], 'GM', 1)[1][0]
                gm_m3s2  = gm_km3s2 * 1e9  # km³ → m³
                mass_kg  = gm_m3s2 / G
            except Exception:
                # Fall back to known values if SPICE doesn't have the GM
                fallback = {
                    'Sun': 1.98847e30, 'Mercury': 3.301e23, 'Venus': 4.8675e24,
                    'Earth': 5.9722e24, 'Moon': 7.342e22, 'Mars': 6.4171e23,
                    'Jupiter': 1.8982e27, 'Saturn': 5.6834e26,
                    'Uranus': 8.6810e25, 'Neptune': 1.02413e26,
                }
                mass_kg = fallback.get(b['name'], 1.0)
                print(f"  Warning: GM not found for {b['name']}, using fallback mass")

            result_bodies.append({
                'name':     b['name'],
                'mass':     mass_kg,
                'position': pos_m,
                'velocity': vel_ms,
            })
            print(f"  ✅ {b['name']:10s} | pos=({pos_m[0]:.4e}, {pos_m[1]:.4e}, {pos_m[2]:.4e}) m")

    finally:
        spice.kclear()

    system = {
        'name':   f'Solar System (SPICE DE440, epoch {epoch})',
        'epoch':  epoch + ' TDB',
        'note':   'Initial conditions from NASA DE440 via SPICE. '
                  'Use with python/spice_validate.py for clean IC-matched validation.',
        'bodies': result_bodies,
    }

    if output:
        with open(output, 'w') as f:
            json.dump(system, f, indent=2)
        print(f'\n✅ Written: {output} ({len(result_bodies)} bodies)')

    return system


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build system JSON from SPICE DE440')
    parser.add_argument('--epoch',  default='2025-01-01T00:00:00',
                        help='Epoch in ISO format (default: 2025-01-01T00:00:00)')
    parser.add_argument('--output', default='systems/solar_system_spice.json',
                        help='Output JSON path')
    args = parser.parse_args()

    print(f'Querying SPICE DE440 at epoch: {args.epoch}')
    build_system(epoch=args.epoch, output=args.output)
