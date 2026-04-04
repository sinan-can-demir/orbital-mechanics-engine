# Setup Guide

This guide helps first-time users install and build the Orbital Mechanics Engine.

## Prerequisites

- C++17-compatible compiler
- CMake ≥ 3.14
- OpenGL 3.3 or higher (required only for the viewer)
- `libcurl` development libraries
- `glad`, `glfw`, `glm` (for the viewer)
- Python 3 (optional, for plotting scripts)

## Recommended Linux Dependencies

On Ubuntu / Debian, install dependencies like this:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev python3 python3-pip
```

For the OpenGL viewer, also install:

```bash
sudo apt-get install -y libglfw3-dev libglm-dev
```

If `glad` is not included in the system packages, the project ships its own copy under `external/`.

## Clone the Repository

```bash
git clone https://github.com/sinan-can-demir/orbital-mechanics-engine.git
cd orbital-mechanics-engine
```

## Build the Project

### Standard build with viewer support

```bash
make
```

### Build only the simulator

```bash
make build-sim
```

### Build only the viewer

```bash
make build-viewer
```

### Build without the OpenGL viewer

```bash
make BUILD_VIEWER=0 build
```

### Manual CMake build

```bash
mkdir -p build
cd build
cmake -S .. -B . -DBUILD_VIEWER=ON
cmake --build . --parallel
```

After building, the executables are available under `build/bin/`.

## Run a Quick Simulation

The default simulator target uses the example Earth-Moon system and writes output to `results/out.csv`:

```bash
make run
```

## Viewer

Launch the viewer with the default output file:

```bash
make view
```

Or launch the viewer script directly:

```bash
./utils/view.sh
```

## Optional: Python Plotting

If you want to generate analysis plots, run one of the provided scripts:

```bash
make plot
make plot-energy
make plot-momentum
make plot-3d
make plot-3d-exaggerated
make plot-3d-earth-moon
```

## Notes

- CI uses `BUILD_VIEWER=OFF`, so the viewer is optional for headless or CI environments.
- If you encounter missing dependencies, check the system packages for GLFW, GLM, and development headers.
- `libcurl` is required for HORIZONS-related functionality and network operations.
