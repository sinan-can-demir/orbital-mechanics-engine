import pytest

import orbit

def test_import():
    assert orbit.__version__ is not None

def test_simulate_returns_correct_shape():
    result = orbit.simulate('systems/earth_moon.json', steps=100, dt=60.0)
    pos = result.positions_numpy()
    assert pos.shape == (100, 3, 3)  # 100 steps, 3 bodies, xyz

def test_energy_conservation():
    result = orbit.simulate('systems/earth_moon.json', steps=1000, dt=60.0)
    E = result.energies_numpy()
    drift = abs((E[-1] - E[0]) / E[0])
    assert drift < 1e-5

def test_body_names():
    result = orbit.simulate('systems/earth_moon.json', steps=10, dt=60.0)
    assert result.body_names == ['Sun', 'Earth', 'Moon']

def test_adaptive_unreachable_tolerance_raises_instead_of_hanging():
    # atol/rtol far tighter than achievable at dt_min=dt_max=1.0s: every step
    # rejects and dt can never shrink further, so this must raise quickly
    # instead of looping forever (regression test for the RK45 hang).
    with pytest.raises(RuntimeError):
        orbit.simulate_adaptive('systems/earth_moon.json', duration_s=3600.0, dt_initial=1.0,
                                 atol=1e-300, rtol=1e-300, dt_min=1.0, dt_max=1.0)
