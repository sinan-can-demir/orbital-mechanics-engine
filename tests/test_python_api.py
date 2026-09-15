import json

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

def test_negative_steps_raises():
    with pytest.raises(ValueError, match='steps'):
        orbit.simulate('systems/earth_moon.json', steps=-5, dt=60.0)

def test_zero_dt_raises():
    with pytest.raises(ValueError, match='dt'):
        orbit.simulate('systems/earth_moon.json', steps=5, dt=0.0)

def test_missing_bodies_key_raises(tmp_path):
    system_file = tmp_path / 'no_bodies.json'
    system_file.write_text(json.dumps({
        'name': 'test',
        'epoch': '2024-01-01 00:00:00 TDB',
    }))
    with pytest.raises(RuntimeError, match='bodies'):
        orbit.simulate(str(system_file), steps=5, dt=60.0)

def test_rk45_via_simulate_raises_clear_error():
    # RK45 is adaptive-step only; simulate() (fixed-step) must explain that
    # instead of raising the generic "Unknown integrator".
    with pytest.raises(ValueError, match="simulate_adaptive"):
        orbit.simulate('systems/earth_moon.json', steps=10, dt=60.0,
                        integrator=orbit.Integrator.RK45)

def test_euler_not_exposed():
    # eulerStep() has no working dispatcher entry point, so it must not be
    # exposed as a selectable Integrator value.
    assert not hasattr(orbit.Integrator, 'Euler')

def test_empty_system_with_gr_does_not_crash(tmp_path):
    # Regression test: applyGRCorrection() used to read bodies[0] out of
    # bounds on an empty system when gr=True (undefined behavior).
    empty_system = tmp_path / 'empty.json'
    empty_system.write_text(json.dumps({
        'name': 'empty',
        'epoch': '2024-01-01 00:00:00 TDB',
        'bodies': [],
    }))
    result = orbit.simulate(str(empty_system), steps=10, dt=60.0, gr=True)
    assert result.body_names == []

def test_adaptive_unreachable_tolerance_raises_instead_of_hanging():
    # atol/rtol far tighter than achievable at dt_min=dt_max=1.0s: every step
    # rejects and dt can never shrink further, so this must raise quickly
    # instead of looping forever (regression test for the RK45 hang).
    with pytest.raises(RuntimeError):
        orbit.simulate_adaptive('systems/earth_moon.json', duration_s=3600.0, dt_initial=1.0,
                                 atol=1e-300, rtol=1e-300, dt_min=1.0, dt_max=1.0)

def test_stride_zero_raises_instead_of_crashing():
    with pytest.raises(ValueError):
        orbit.simulate('systems/earth_moon.json', steps=10, dt=60.0, stride=0)
