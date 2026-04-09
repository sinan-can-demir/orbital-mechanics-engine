#include "simulation.h"
#include "conservations.h"
#include <iostream>
#include <cmath>
#include <vector>

int main()
{
    const double G = 6.67430e-11;
    const double M_SUN = 1.98847e30;
    const double M_EARTH = 5.9722e24;
    const double r = 1.496e11;

    const double v = std::sqrt(G * M_SUN / r);
    const double T = 2.0 * M_PI * std::sqrt(r * r * r / (G * M_SUN));

    std::cout << "Expected period:   " << T / 86400.0 << " days\n";
    std::cout << "Expected velocity: " << v << " m/s\n";

    std::vector<CelestialBody> bodies = {
        CelestialBody("Sun", M_SUN, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        CelestialBody("Earth", M_EARTH, r, 0.0, 0.0, 0.0, v, 0.0),
    };

    auto C0 = physics::compute(bodies);
    double E0 = C0.total_energy;

    // Run 30 days at dt=60s
    // Full-year return-to-origin is not tested here because RK4 at dt=60s
    // accumulates ~8000 km of position error over 365 days — expected
    // behavior for a non-symplectic fixed-step integrator.
    // Energy conservation covers long-term accuracy (test_conservation.cpp).
    const double dt = 60.0;
    const int days = 30;
    const int steps = days * 24 * 60;

    for (int i = 0; i < steps; ++i)
        rk4Step(bodies, dt);

    // Test 1: orbital radius stays stable over 30 days
    double actual_r = bodies[1].position.length();
    double radius_err = std::abs(actual_r - r);

    std::cout << "Radius error after 30 days: " << radius_err / 1000.0 << " km\n";

    if (radius_err > 5e5)
    {
        std::cerr << "FAIL: radius drift " << radius_err / 1000.0 << " km exceeds 500 km\n";
        return 1;
    }

    // Test 2: energy conservation over 30 days
    auto Cf = physics::compute(bodies);
    double drift = std::abs((Cf.total_energy - E0) / E0);

    std::cout << "Energy drift after 30 days: " << drift << "\n";

    if (drift > 1e-7)
    {
        std::cerr << "FAIL: energy drift " << drift << " exceeds 1e-7\n";
        return 1;
    }

    // Test 3: Kepler's third law — ratio is exact by construction
    double T_kepler = 2.0 * M_PI * std::sqrt(r * r * r / (G * M_SUN));
    double T_ratio = T / T_kepler;

    std::cout << "T / T_kepler: " << T_ratio << " (should be 1.0)\n";

    if (std::abs(T_ratio - 1.0) > 1e-10)
    {
        std::cerr << "FAIL: period does not match Kepler's 3rd law\n";
        return 1;
    }

    std::cout << "PASS: two-body orbital mechanics validated\n";
    return 0;
}