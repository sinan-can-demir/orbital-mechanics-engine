// tests/test_helpers.h
// Shared, dependency-free helpers for the C++ tests: non-fatal CHECK macros, common
// fixtures (Sun/Earth), a drift measurer, and a self-cleaning temp directory.
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "body.h"
#include "conservations.h"
#include "constants.h"
#include "simulation.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace testing
{

// ── Check counting ───────────────────────────────────────────────────────────
// Failures are recorded and reported at the end by finish(), so one failing check does
// not hide the ones after it.
struct State
{
    int checks = 0;
    int failures = 0;
};

inline State& state()
{
    static State s;
    return s;
}

inline void report(bool ok, const std::string& msg, const char* file, int line)
{
    ++state().checks;
    if (ok)
    {
        std::cout << "PASS: " << msg << "\n";
        return;
    }
    ++state().failures;
    std::cerr << "FAIL: " << msg << "  (" << file << ":" << line << ")\n";
}

// Returns the process exit code: 0 if every check passed.
inline int finish(const char* suite)
{
    const State& s = state();
    if (s.failures == 0)
    {
        std::cout << "PASS: " << suite << " (" << s.checks << " checks)\n";
        return 0;
    }
    std::cerr << "FAIL: " << suite << " (" << s.failures << " of " << s.checks
              << " checks failed)\n";
    return 1;
}

// ── Numeric comparison ───────────────────────────────────────────────────────
inline bool nearlyEqual(double a, double b, double absTol = 1e-9, double relTol = 1e-12)
{
    const double diff = std::abs(a - b);
    if (diff <= absTol)
        return true;
    return diff <= relTol * std::max(std::abs(a), std::abs(b));
}

// ── Fixtures ─────────────────────────────────────────────────────────────────
constexpr double kMSun = 1.98847e30;    // kg
constexpr double kMEarth = 5.9722e24;   // kg
constexpr double kEarthSunR = 1.496e11; // m, ~1 AU

// Circular speed of a test body at radius r around total mass M.
inline double circularSpeed(double totalMass, double r)
{
    return std::sqrt(physics::constants::G * totalMass / r);
}

// Sun at rest at the origin, Earth at (r, 0, 0) moving (0, v, 0).
inline std::vector<CelestialBody> makeSunEarth(double r, double v)
{
    return {
        CelestialBody("Sun", kMSun, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        CelestialBody("Earth", kMEarth, r, 0.0, 0.0, 0.0, v, 0.0),
    };
}

// ── Energy drift ─────────────────────────────────────────────────────────────
using StepFn = std::function<void(std::vector<CelestialBody>&, double)>;

// Steps `bodies` (taken by value) and returns the largest |dE/E0| seen at any step.
inline double maxEnergyDrift(std::vector<CelestialBody> bodies, int steps, double dt,
                             const StepFn& stepFn)
{
    const double E0 = physics::compute(bodies).total_energy;
    double maxErr = 0.0;

    updateAccelerations(bodies); // required before the first symplectic step
    for (int i = 0; i < steps; ++i)
    {
        stepFn(bodies, dt);
        const double err = std::abs((physics::compute(bodies).total_energy - E0) / E0);
        maxErr = std::max(maxErr, err);
    }
    return maxErr;
}

// ── Temp directory ───────────────────────────────────────────────────────────
// Unique directory under the system temp dir, removed (recursively) on destruction,
// including when a check fails or the test returns early.
class TempDir
{
  public:
    TempDir()
    {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("orbit_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const std::string& name) const { return (path_ / name).string(); }

  private:
    std::filesystem::path path_;
};

} // namespace testing

#define CHECK(cond, msg) ::testing::report(static_cast<bool>(cond), (msg), __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol, msg)                                                                 \
    ::testing::report(std::abs((a) - (b)) <= (tol), (msg), __FILE__, __LINE__)

#endif // TEST_HELPERS_H
