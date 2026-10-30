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

def test_simulate_adaptive_rejects_dt_min_greater_than_dt_max():
    with pytest.raises(ValueError, match='dt_min'):
        orbit.simulate_adaptive('systems/earth_moon.json', duration_s=3600.0,
                                 dt_initial=100.0, dt_min=1000.0, dt_max=1.0)

def test_simulate_adaptive_rejects_non_positive_dt_min():
    with pytest.raises(ValueError, match='dt_min'):
        orbit.simulate_adaptive('systems/earth_moon.json', duration_s=3600.0,
                                 dt_initial=100.0, dt_min=0.0, dt_max=100.0)


# ── Cross-check against the C++ golden values (tests/golden/values.txt) ───────
# The same scenarios are run by tests/test_golden.cpp; the bindings must agree with
# the engine on every integrator, not just the default RK4.

def _golden():
    values = {}
    with open('tests/golden/values.txt') as f:
        for line in f:
            key, val = line.split()
            values[key] = float(val)
    return values

@pytest.mark.parametrize('name, integrator', [
    ('rk4', 'RK4'),
    ('leapfrog', 'Leapfrog'),
    ('yoshida4', 'Yoshida4'),
    ('hermite', 'Hermite'),
])
def test_fixed_step_integrators_match_cpp_golden(name, integrator):
    golden = _golden()
    result = orbit.simulate('systems/earth_moon.json', steps=1000, dt=3600.0,
                             integrator=getattr(orbit.Integrator, integrator), stride=100)
    E = result.energies_numpy()
    assert len(E) == golden[f'{name}.n_snapshots']
    assert E[-1] == pytest.approx(golden[f'{name}.last.E'], rel=1e-9)

def test_simulate_adaptive_matches_cpp_golden():
    golden = _golden()
    result = orbit.simulate_adaptive('systems/earth_moon.json', duration_s=30 * 86400.0,
                                      dt_initial=3600.0, output_interval_s=86400.0)
    E = result.energies_numpy()
    assert len(E) == golden['rk45.n_snapshots']
    assert E[-1] == pytest.approx(golden['rk45.last.E'], rel=1e-9)
    assert result.positions_numpy().shape == (len(E), 3, 3)
