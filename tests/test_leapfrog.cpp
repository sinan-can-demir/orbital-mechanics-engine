// tests/test_leapfrog.cpp
#include "simulation.h"
#include "conservations.h"
#include "json_loader.h"
#include <iostream>
#include <cmath>
#include <vector>

int main()
{
    auto bodies = loadSystemFromJSON("systems/earth_moon.json");

    auto C0 = physics::compute(bodies);
    double E0 = C0.total_energy;

    const double DT = 3600.0;    // 1-hour steps
    const int STEPS = 8760;      // 1 year
    const int SAMPLE_EVERY = 24; // sample once per day

    std::vector<double> energy_errors;

    // Seed accelerations — leapfrog requires this before first step
    // (call updateAccelerations indirectly via first leapfrog call
    //  or expose it — for now use the public leapfrog interface)

    for (int i = 0; i < STEPS; ++i)
    {
        leapfrogStep(bodies, DT);

        if (i % SAMPLE_EVERY == 0)
        {
            auto C = physics::compute(bodies);
            double err = (C.total_energy - E0) / std::abs(E0);
            energy_errors.push_back(err);
        }
    }

    // ── Test: energy is BOUNDED, not monotonically drifting ───────────────
    // Check that the error doesn't consistently increase
    // Compare first quarter vs last quarter average
    size_t quarter = energy_errors.size() / 4;

    double first_avg = 0.0, last_avg = 0.0;
    for (size_t i = 0; i < quarter; ++i)
        first_avg += std::abs(energy_errors[i]);
    for (size_t i = energy_errors.size() - quarter; i < energy_errors.size(); ++i)
        last_avg += std::abs(energy_errors[i]);

    first_avg /= quarter;
    last_avg /= quarter;

    std::cout << "First-quarter avg |dE|: " << first_avg << "\n";
    std::cout << "Last-quarter avg  |dE|: " << last_avg << "\n";

    // Leapfrog: last should not be dramatically worse than first
    // (RK4 would show monotonic growth at large dt)
    if (last_avg > first_avg * 100.0)
    {
        std::cerr << "FAIL: energy drift appears monotonic — symplectic property may be broken\n";
        return 1;
    }

    // ── Test: absolute bound is reasonable ────────────────────────────────
    double max_err = 0.0;
    for (double e : energy_errors)
        max_err = std::max(max_err, std::abs(e));

    std::cout << "Max |dE/E0| over 1 year: " << max_err << "\n";

    if (max_err > 1e-3)
    {
        std::cerr << "FAIL: energy drift exceeds 0.1% over 1 year\n";
        return 1;
    }

    std::cout << "PASS: leapfrog symplectic property validated\n";
    return 0;
}