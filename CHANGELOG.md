# Changelog

## [2.1.0] - 2026-11-19

### Added
- Notebook 10: numerical apsidal precession sweep, with a REBOUNDx 1PN cross-check.
- Golden regression tests (`tests/test_golden.cpp`, `tests/golden/`) and shared test helpers (`tests/test_helpers.h`).
- CLI smoke test and Python API tests.
- Constants drift guard (`tools/check_drift.py`, `tests/test_spice_constants.py`).
- Vulkan viewer (`viewer_vk`): boot milestone (instance, device, swapchain, render pass), first pipeline and triangle, and a sphere mesh via VMA vertex/index buffers.

### Changed
- Refactor phases 1–2 from the duplication audit: bug fixes, dead-code removal, and consolidated test helpers.
- Fixed constants drift; updated `systems/solar_system*.json` (SPICE-derived values).
- Notebook 09 baseline corrected.
- Python `__version__`, `pyproject.toml` and CMake project version are now all 2.1.

### Removed
- The 88,126 km SPICE claim was retracted.
- Removed the stray `viewer_config.json` and the unused `test_csv_loader.cpp`.

### Housekeeping
- Stopped tracking `tests/__pycache__/*.pyc`.

## [2.0.2], [2.0.1], [2.0.0]
See the GitHub releases and tags.
