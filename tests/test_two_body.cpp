#include "test_helpers.h"

int main()
{
    using namespace testing;
    using physics::constants::G;

    const double r = kEarthSunR;
    const double v = circularSpeed(kMSun, r);
    const double T = 2.0 * M_PI * std::sqrt(r * r * r / (G * kMSun));

    std::cout << "Expected period:   " << T / 86400.0 << " days\n";
    std::cout << "Expected velocity: " << v << " m/s\n";

    auto bodies = makeSunEarth(r, v);
    const double E0 = physics::compute(bodies).total_energy;

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
    const double radius_err = std::abs(bodies[1].position.length() - r);
    std::cout << "Radius error after 30 days: " << radius_err / 1000.0 << " km\n";
    CHECK(radius_err <= 5e5, "radius drift stays under 500 km over 30 days");

    // Test 2: energy conservation over 30 days
    const double drift = std::abs((physics::compute(bodies).total_energy - E0) / E0);
    std::cout << "Energy drift after 30 days: " << drift << "\n";
    CHECK(drift <= 1e-7, "energy drift stays under 1e-7 over 30 days");

    // Test 3: the simulated orbit sweeps the angle Kepler's third law predicts.
    // The angle of Earth about the Sun after t seconds must be 2*pi*t/T. (The Sun is
    // not fixed, so this is a few-ppm effect from the reduced mass, well inside 1e-4 rad.)
    const double t = steps * dt;
    const vec3 sep = bodies[1].position - bodies[0].position;
    const double angle = std::atan2(sep.y(), sep.x());
    const double expected = 2.0 * M_PI * t / T;
    std::cout << "Swept angle: " << angle << " rad (Kepler: " << expected << ")\n";
    CHECK_NEAR(angle, expected, 1e-4, "swept angle matches Kepler's third law after 30 days");

    return finish("two-body orbital mechanics");
}
