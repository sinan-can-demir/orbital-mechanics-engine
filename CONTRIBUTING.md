# Contributing to Orbital Mechanics Engine

Thank you for your interest in contributing!
This project is a high-accuracy C++17 N-body gravitational simulator with RK4 integration, OpenGL visualization, JPL HORIZONS ephemeris support, and eclipse detection.
Contributions of all kinds are welcome — code, documentation, bug reports, ideas, examples, and more.

## 🧩 How to Contribute

### 1. Fork the Repository

Click "Fork" in the top-right corner of the GitHub page and clone your fork:
```bash
git clone https://github.com/YOUR_USERNAME/orbital-mechanics-engine.git
cd orbital-mechanics-engine
git remote add upstream https://github.com/sinan-can-demir/orbital-mechanics-engine.git
```

### 2. Create a Branch

Always make changes in a new branch following our naming conventions:
```bash
# For new features
git checkout -b feature/descriptive-feature-name

# For bug fixes
git checkout -b fix/issue-description

# For documentation
git checkout -b docs/update-documentation
```

### 3. Set Up Development Environment

See the [Development Environment Setup](#development-environment-setup) section below.

### 4. Make Your Changes

Follow the coding guidelines and physics accuracy requirements below.
Keep commits clean and meaningful with descriptive messages.

### 5. Test Your Changes

**CRITICAL**: Your changes must pass CI before merging to main:
- Build successfully in both Debug and Release configurations
- All simulations run without crashing
- Conservation laws remain within acceptable bounds
- New features include validation tests

### 6. Submit a Pull Request (PR)

- Target the `main` branch for production changes
- Target the `development` branch for experimental features
- Fill out the PR template completely
- Ensure CI passes before requesting review

## 🛠 Development Environment Setup

### System Requirements

- **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake** ≥ 3.14
- **OpenGL** 3.3+ (for viewer)
- **Git** for version control

### Platform-Specific Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
sudo apt-get install -y libcurl4-openssl-dev libglfw3-dev libglm-dev
sudo apt-get install -y libgl1-mesa-dev libglu1-mesa-dev
```

#### macOS
```bash
brew install cmake git curl glfw glm
```

#### Windows (vcpkg)
```cmd
vcpkg install curl glfw3 glm opengl
```

### Build Verification

```bash
# Clone and navigate to project
git clone https://github.com/sinan-can-demir/orbital-mechanics-engine.git
cd orbital-mechanics-engine

# Build Debug configuration (CI-compatible)
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_VIEWER=OFF
cmake --build . --target orbit_core orbit-sim --parallel

# Test basic functionality
./bin/orbit-sim --help

# Build Release configuration
mkdir ../build-release && cd ../build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_VIEWER=OFF
cmake --build . --target orbit_core orbit-sim --parallel
```

## 🚀 CI/CD Pipeline Requirements

### CI Configuration

Our CI pipeline (`.github/workflows/ci.yml`) enforces:

- **Dual Configuration**: Builds both Debug and Release
- **Core-Only CI**: Viewer disabled (`BUILD_VIEWER=OFF`) for reliability
- **Dependency Installation**: Automated system dependency setup
- **Smoke Testing**: Basic functionality verification

### CI Requirements for Main Branch

**PRs to main MUST pass CI**. The pipeline checks:
1. **Build Success**: Both Debug and Release configurations compile
2. **Link Success**: All dependencies resolve correctly
3. **Basic Execution**: `orbit-sim --help` runs without errors

### Local CI Testing

Before submitting PRs, test locally:
```bash
# Replicate CI environment
cmake -S . -B build-ci-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_VIEWER=OFF
cmake --build build-ci-debug --target orbit_core orbit-sim --parallel

# Verify executable
./build-ci-debug/bin/orbit-sim --help
```

## 🧪 Testing and Validation

### Physics Validation Requirements

All physics contributions must maintain:
- **Energy Conservation**: Relative error < 10⁻⁶ (short-term)
- **Momentum Conservation**: Relative error < 10⁻⁸
- **Barycenter Stability**: Position drift < 10⁻³ meters

### Validation Procedures

1. **Conservation Law Monitoring**
   ```bash
   ./orbit-sim run --system ../systems/earth_moon.json --steps 10000 --dt 60
   # Check output CSV for energy/momentum drift
   ```

2. **Ephemeris Comparison** (for orbital mechanics)
   ```bash
   ./orbit-sim fetch --body 399 --center 0 --start "2025-01-01" --stop "2025-01-02" --step "1h" --out earth_ephem.txt
   # Compare simulation results with HORIZONS data
   ```

3. **Regression Testing**
   - Run existing test cases before and after changes
   - Verify orbital periods remain consistent
   - Check eclipse detection accuracy

### Test Coverage

- **Unit Tests**: Core physics functions (integrator, force calculation)
- **Integration Tests**: Complete simulation workflows
- **Validation Tests**: Conservation law verification
- **Regression Tests**: Known-good solution comparison

## 🧪 Coding Guidelines

### Language & Standards

- **C++17** or newer with modern features
- **CMake** for build configuration
- **RAII** for resource management
- **const correctness** throughout codebase

### Code Style

- **Meaningful names**: `orbital_period`, not `op`
- **Small functions**: Single responsibility, < 50 lines preferred
- **Modern C++**: `std::vector`, `std::unique_ptr`, range-based for loops
- **No raw pointers**: Use smart pointers or automatic storage
- **Exception safety**: Strong exception guarantee where possible

### Physics Code Requirements

- **SI Units Only**: All internal calculations in meters, kilograms, seconds
- **Unit Documentation**: Clearly specify units in all interfaces
- **Reference Sources**: Cite orbital mechanics formulas (Vallado, Montenbruck)
- **Numerical Stability**: Avoid subtractive cancellation, use stable formulations

### File Organization

```
src/
├── core/           # Physics engine (simulation, conservation, eclipse)
├── io/             # Data handling (json_loader, horizons, validate)
├── cli/            # Command-line interface
└── viewer/         # OpenGL visualization

include/
├── [headers].h     # Public interfaces
└── viewer/         # Viewer-specific headers

external/           # Third-party libraries (glad)
systems/            # JSON orbital configurations
shaders/            # GLSL shader programs
docs/               # Technical documentation
```

## 📐 Documentation Requirements

### Code Documentation

All new classes, functions, and modules must include:

```cpp
/**
 * Computes gravitational force between two bodies
 * @param m1, m2 Masses in kilograms
 * @param r1, r2 Position vectors in meters
 * @return Force vector in Newtons
 * @throws std::invalid_argument if bodies are identical
 * @see Vallado, "Fundamentals of Astrodynamics and Applications", Eq. 2-1
 */
vec3 compute_gravitational_force(double m1, double m2, const vec3& r1, const vec3& r2);
```

### Documentation Updates

- **README.md**: Update for new features or breaking changes
- **docs/**: Add technical documentation for significant algorithms
- **orbit_sim_cli_reference.md**: Document new CLI commands
- **CONTRIBUTING.md**: Update guidelines as needed

## 🔄 Development Workflow

### Branch Strategy

- **main**: Stable, production-ready code (CI protected)
- **development**: Integration branch for features
- **feature/***: New functionality branches
- **fix/***: Bug fix branches
- **docs/***: Documentation updates

### Commit Message Format

```
type(scope): description

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Code formatting (no functional change)
- `refactor`: Code restructuring
- `test`: Testing additions
- `chore`: Build process or maintenance

Examples:
```
feat(core): add RK45 adaptive integrator
fix(viewer): resolve crash on window resize
docs(readme): update build instructions for Windows
```

### Pull Request Process

1. **Create PR** from feature branch to target branch
2. **Fill Template**: Complete all sections in PR template
3. **CI Verification**: Ensure all checks pass
4. **Code Review**: Address reviewer feedback promptly
5. **Approval**: Get at least one maintainer approval
6. **Merge**: Squash merge with descriptive commit message

### PR Requirements

- **Clean History**: Squash related commits before merge
- **Tests Included**: New features must include validation tests
- **Documentation Updated**: Relevant docs updated for changes
- **CI Passing**: All automated checks must pass
- **Backwards Compatibility**: Note any breaking changes clearly

## 🖥️ OpenGL Viewer Development

### Viewer Guidelines

- **Separation**: Keep viewer code separate from physics engine
- **GLAD Usage**: Use GLAD loader from `/external/glad/`
- **Shader Management**: Organize GLSL shaders in `/shaders/`
- **Cross-Platform**: Test on Linux, Windows, and macOS

### Viewer Build Configuration

```bash
# Enable viewer for local development
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_VIEWER=ON

# Build viewer executable
cmake --build . --target orbit-viewer --parallel
```

### Viewer Testing

```bash
# Test viewer with sample data
./bin/orbit-viewer ../results/sample_orbit.csv

# Verify controls work (mouse, keyboard)
# Check rendering performance
# Test focus switching between bodies
```

## 🐛 Bug Reporting

### Bug Report Template

When reporting bugs, include:

1. **Environment**: OS, compiler, CMake version
2. **Reproduction Steps**: Exact commands to reproduce
3. **Expected Behavior**: What should happen
4. **Actual Behavior**: What actually happens
5. **Error Messages**: Complete console output
6. **Minimal Example**: Smallest possible test case

### Example Bug Report

```markdown
**Environment**: Ubuntu 22.04, GCC 11.2, CMake 3.24
**Reproduction**:
```bash
./orbit-sim run --system ../systems/earth_moon.json --steps 1000 --dt 3600
```
**Expected**: Simulation completes successfully
**Actual**: Segmentation fault after 500 steps
**Error**: `Segmentation fault (core dumped)`
**Minimal**: Attached minimal JSON configuration


## 💡 Feature Requests

### Request Guidelines

- **Search First**: Check existing issues for similar requests
- **Use Case**: Describe specific problem you're solving
- **Proposed Solution**: Suggest implementation approach
- **Alternatives**: Consider other approaches
- **Scope**: Break large features into smaller PRs

### Feature Request Examples

- **Physics**: "Add relativistic corrections for Mercury orbit"
- **Visualization**: "Implement orbit trail rendering in viewer"
- **Performance**: "Add Barnes-Hut tree for large N-body systems"
- **Usability**: "Create GUI for system configuration"

## 🤝 Community Guidelines

### Code of Conduct

- **Respectful**: Be constructive and professional
- **Inclusive**: Welcome contributors of all backgrounds
- **Collaborative**: Work together to solve problems
- **Educational**: Help others learn orbital mechanics

### Getting Help

- **Issues**: Open an issue for bugs or questions
- **Discussions**: Use GitHub Discussions for general questions
- **Documentation**: Read existing docs first
- **Email**: Contact maintainers for sensitive issues

## ⭐ Thank You for Contributing

Your contributions help make this orbital mechanics simulation better for:
- **Students** learning celestial mechanics
- **Researchers** needing accurate simulations
- **Educators** teaching orbital dynamics
- **Hobbyists** exploring space physics

If you have questions, feel free to open an issue — I'm happy to help you get started!

---

## 📚 Additional Resources

### Technical Documentation
- [System Architecture](docs/architecture.md)
- [Validation Procedures](docs/validation.md)
- [Physics and Methods](docs/physics-and-methods.md)
- [Design Notes](docs/design-notes.md)

### External References
- Vallado, D.A. *Fundamentals of Astrodynamics and Applications*
- Montenbruck, O., Gill, E. *Satellite Orbits*
- NASA JPL HORIZONS system documentation

### Development Tools
- [CLI Reference](orbit_sim_cli_reference.md)
- [Python Plotting Scripts](plotting_scripts/)
- [Example Configurations](systems/)

## 🧪 Coding Guidelines

### Language & Tools

 - C++17 or newer

 - CMake for builds

 - GLAD / OpenGL for rendering

 - nlohmann/json for config parsing

### File Structure

Please keep new files consistent with the existing layout:
```
/src        → implementation
/include    → public headers
/external   → third-party libraries
/assets     → textures/data
```

### Code Style

 - Use meaningful variable and function names

 - Prefer const correctness

 - Use std::vector, std::unique_ptr, and modern C++ patterns

 - Avoid raw pointers when possible

 - Keep functions small and focused

### Documentation

 - All new classes, functions, and modules should include:

 - brief comment explaining purpose

 - parameter descriptions

 - units used (very important in physics code!)

 - references if based on standard orbital mechanics formulas

Example:
```
// Computes gravitational force between two bodies
// Params:
//   m1, m2   - masses (kg)
//   r1, r2   - positions (meters)
// Returns:
//   force vector in Newtons
```
## 🪐 Physics & Scientific Accuracy

If you contribute physics/math code:

 - ✔ Clearly state assumptions
 - ✔ Use SI units unless otherwise documented
 - ✔ Add references (papers, textbooks, NASA sources)
 - ✔ Make accuracy vs. performance trade-offs explicit
 - ✔ Avoid magic constants; define them in a header

## 🖥️ Visualization Guidelines

 - If you’re working on OpenGL code:

 - Use the GLAD loader inside /external

 - Keep rendering functions in their own modules

 - Avoid mixing simulation logic and rendering logic

 - Test on at least one of: Windows / Linux / macOS

## 🐛 Reporting Bugs

Please open an issue and include:

 - Steps to reproduce

 - OS + compiler

 - CMake version

 - Expected behavior

 - Actual behavior

 - Error logs or screenshots

## 💡 Feature Requests

Feature ideas are welcome!

Examples:

 - Improved integrators (RK4, Verlet, symplectic)

 - New visualizations

 - Stability analysis tools

 - Multi-star systems

 - JSON-based scenario loader

Open an issue and describe your idea.

## 🤝 Code of Conduct

 - Be respectful and constructive.

 - All contributions should improve the project for others.

## ⭐ Thank You for Your Interest in Contributing

Your contributions help make this simulation engine better for students, researchers, hobbyists, and developers.
If you have questions, feel free to open an issue — I’m happy to help.
