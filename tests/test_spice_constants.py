"""Drift guard: systems/solar_system_spice.json must be consistent with the
engine's G (include/constants.h) and the DE440 GM values.

The file stores mass = GM / G, so G * mass must reproduce GM. If G in
constants.h changes, regenerate the file with python/spice_ic.py.
"""

import json
import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent

# GM in km^3/s^2, copied from data/spice/gm_de431.tpc (the DE440 values).
# Mars..Neptune are planetary-system barycenter GMs, matching spice_ic.py.
DE440_GM_KM3_S2 = {
    'Sun':     1.3271244004193938e+11,
    'Mercury': 2.2031780000000021e+04,
    'Venus':   3.2485859200000006e+05,
    'Earth':   3.9860043543609598e+05,
    'Moon':    4.9028000661637961e+03,
    'Mars':    4.2828375214000022e+04,
    'Jupiter': 1.2671276480000021e+08,
    'Saturn':  3.7940585200000003e+07,
    'Uranus':  5.7945486000000080e+06,
    'Neptune': 6.8365271005800236e+06,
}

_KERNEL_KEYS = {'Sun': 10, 'Mercury': 1, 'Venus': 2, 'Earth': 399, 'Moon': 301,
                'Mars': 4, 'Jupiter': 5, 'Saturn': 6, 'Uranus': 7, 'Neptune': 8}


def _engine_G():
    text = (ROOT / 'include' / 'constants.h').read_text()
    m = re.search(r'constexpr double G\s*=\s*([0-9.eE+-]+)\s*;', text)
    assert m, 'could not find G in include/constants.h'
    return float(m.group(1))


def test_spice_system_masses_match_de440_gm():
    G = _engine_G()
    system = json.loads((ROOT / 'systems' / 'solar_system_spice.json').read_text())
    names = [b['name'] for b in system['bodies']]
    assert set(names) == set(DE440_GM_KM3_S2)
    for b in system['bodies']:
        gm = DE440_GM_KM3_S2[b['name']] * 1e9  # km^3/s^2 -> m^3/s^2
        rel = abs(G * b['mass'] - gm) / gm
        assert rel < 1e-9, f"{b['name']}: G*mass differs from DE440 GM by {rel:.3e}"


def test_embedded_gm_table_matches_kernel_when_present():
    tpc = ROOT / 'data' / 'spice' / 'gm_de431.tpc'
    if not tpc.exists():
        pytest.skip('SPICE kernels not downloaded (data/spice is gitignored)')
    text = tpc.read_text()
    for name, key in _KERNEL_KEYS.items():
        m = re.search(r'BODY%d_GM\s*=\s*\(\s*([0-9.eE+-]+)\s*\)' % key, text)
        assert m, f'BODY{key}_GM not found in {tpc.name}'
        assert float(m.group(1)) == DE440_GM_KM3_S2[name]
