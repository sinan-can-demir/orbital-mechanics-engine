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
