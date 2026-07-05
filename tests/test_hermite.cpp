// tests/test_hermite.cpp
// Validates the 4th-order Hermite predictor-corrector integrator:
//   1. Global position error converges as O(dt^4) — halving dt should cut error ~16x
//   2. At equal step count, Hermite beats RK4 (both are 4th-order, but Hermite uses jerk
//      information instead of extra stage evaluations, so it isn't a huge margin)
#include "simulation.h"
#include "conservations.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
struct CircularOrbit
{
    double r, v, n; // radius, speed, angular velocity
};

std::vector<CelestialBody> makeSunEarth(double r, double v)
{
    return {
        CelestialBody("Sun", 1.98847e30, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        CelestialBody("Earth", 5.9722e24, r, 0.0, 0.0, 0.0, v, 0.0),
    };
}

// Compares the Sun->Earth SEPARATION vector against the analytic circle, not Earth's
// absolute position. These initial conditions give the system nonzero net momentum
// (Sun at rest, Earth moving), so the barycenter drifts linearly — an absolute-position
// comparison would pick up that drift as a constant, integrator-independent offset.
// The relative vector obeys r'' = -G*(M1+M2)*r_hat/r^2 exactly, regardless of frame.
double positionError(const std::vector<CelestialBody>& bodies, const CircularOrbit& orbit, double t)
{
    double x = orbit.r * std::cos(orbit.n * t);
    double y = orbit.r * std::sin(orbit.n * t);
    vec3 rel = bodies[1].position - bodies[0].position;
    double dx = rel.x() - x;
    double dy = rel.y() - y;
    return std::sqrt(dx * dx + dy * dy);
}

double runHermite(const CircularOrbit& orbit, double totalTime, int steps)
{
    double dt = totalTime / steps;
    auto bodies = makeSunEarth(orbit.r, orbit.v);
    updateAccelerations(bodies); // required before first Hermite step
    for (int i = 0; i < steps; ++i)
        hermiteStep(bodies, dt);
    return positionError(bodies, orbit, totalTime);
}

double runRK4(const CircularOrbit& orbit, double totalTime, int steps)
{
    double dt = totalTime / steps;
    auto bodies = makeSunEarth(orbit.r, orbit.v);
    for (int i = 0; i < steps; ++i)
        rk4Step(bodies, dt);
    return positionError(bodies, orbit, totalTime);
}
} // namespace

int main()
{
    const double G = 6.67430e-11;
    const double M_SUN = 1.98847e30;
    const double M_EARTH = 5.9722e24;
    const double r = 1.496e11;
    // Circular relative orbit for a genuine two-body problem uses the TOTAL mass, not
    // just the Sun's — the relative separation vector obeys G*(M1+M2), not G*M1.
    const double v = std::sqrt(G * (M_SUN + M_EARTH) / r);
    const double n = v / r;

    CircularOrbit orbit{r, v, n};

    const double T = 2.0 * M_PI / n;  // one full year
    const double totalTime = T / 8.0; // ~45.6 days of arc

    // ── Test 1: 4th-order convergence — halving dt should cut error by ~2^4 = 16x ──
    const int N = 400;
    double err_N = runHermite(orbit, totalTime, N);
    double err_2N = runHermite(orbit, totalTime, 2 * N);

    double ratio = err_N / err_2N;
    std::cout << "Hermite position error, N=" << N << " steps:  " << err_N << " m\n";
    std::cout << "Hermite position error, N=" << 2 * N << " steps: " << err_2N << " m\n";
    std::cout << "Error ratio (expect ~16 for 4th-order): " << ratio << "\n";

    if (ratio < 10.0 || ratio > 24.0)
    {
        std::cerr << "FAIL: Hermite convergence ratio " << ratio
                  << " is not consistent with O(dt^4)\n";
        return 1;
    }

    // ── Test 2: at equal step count, Hermite is at least as accurate as RK4 ─────────
    double err_rk4 = runRK4(orbit, totalTime, N);
    std::cout << "RK4 position error,     N=" << N << " steps:  " << err_rk4 << " m\n";

    if (err_N >= err_rk4)
    {
        std::cerr << "FAIL: Hermite (" << err_N << " m) is not more accurate than RK4 (" << err_rk4
                  << " m) at equal step count\n";
        return 1;
    }

    std::cout << "PASS: Hermite 4th-order convergence and RK4 comparison validated\n";
    return 0;
}
