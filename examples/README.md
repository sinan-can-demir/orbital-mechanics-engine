# Examples

Jupyter notebooks and Python scripts demonstrating the orbital mechanics engine.

## Planned notebooks

| File | Description |
|------|-------------|
| `01_two_body.ipynb` | Kepler orbit, verify against analytical solution |
| `02_earth_moon.ipynb` | Moon orbit, eclipse prediction |
| `03_solar_system.ipynb` | Long-term planetary motion |
| `04_integrator_comparison.ipynb` | RK4 vs Leapfrog energy drift side-by-side |

These notebooks require the Python API (pybind11 bindings) which is planned
for Phase 4. See `ROADMAP.md` for the implementation timeline.

## Running existing Python scripts

Until the Python bindings exist, use the scripts in `python/` directly:

```bash
# Run the Earth–Moon simulation first
./build/bin/orbit-sim run --system systems/earth_moon.json --steps 8760 --dt 3600

# Then plot results
python3 python/energy_conservation.py
python3 python/angular_momentum_plot.py
python3 python/3Dplot_earth_moon.py
```
