# CI Checklist

This checklist reflects the current GitHub Actions CI workflow defined in `.github/workflows/ci.yml`.

## ✅ Lint
- [ ] Checkout repository with full history (`actions/checkout@v4` with `fetch-depth: 0`)
- [ ] Install `clang-format-21`
- [ ] Determine base commit for diff comparison
- [ ] Identify changed C++ source files:
  - `.cpp` and `.h`
  - exclude `external/`
  - exclude `build/`
- [ ] Run `clang-format --dry-run --Werror` on each changed file
- [ ] Skip formatting check if no relevant C++ files changed

## ✅ Build
- [ ] Checkout repository
- [ ] Install dependencies:
  - `libcurl4-openssl-dev`
- [ ] Configure CMake with `-DBUILD_VIEWER=OFF`
- [ ] Build the project with `cmake --build build --parallel`
- [ ] Verify CLI by running `./build/bin/orbit-sim --help`

## ✅ Test
- [ ] Checkout repository
- [ ] Install dependencies:
  - `libcurl4-openssl-dev`
- [ ] Configure CMake with `-DBUILD_VIEWER=OFF`
- [ ] Build the project with `cmake --build build --parallel`
- [ ] Validate the example system file:
  - `./build/bin/orbit-sim validate --system systems/earth_moon.json`
- [ ] Run a short test simulation:
  - `./build/bin/orbit-sim run --system systems/earth_moon.json --steps 100 --dt 3600 --output results/test_out.csv`

## 💡 Notes
- The CI workflow runs on `push` and `pull_request` to `main` and `master`.
- The `build` and `test` jobs both depend on the `lint` job.
- Viewer support is disabled in CI by setting `BUILD_VIEWER=OFF`.
