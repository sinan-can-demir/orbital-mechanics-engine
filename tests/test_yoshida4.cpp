// tests/test_yoshida4.cpp
// Validates the Yoshida 4th-order symplectic integrator:
//   1. Energy is bounded (not monotonically drifting) — symplectic property
//   2. Energy drift is tighter than Leapfrog at the same timestep — 4th-order advantage
#include "simulation.h"
#include "conservations.h"
#include "json_loader.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

static double maxEnergyDrift(std::vector<CelestialBody> bodies, int steps, double dt,
                             std::function<void(std::vector<CelestialBody>&, double)> stepFn)
{
    auto C0 = physics::compute(bodies);
    double E0 = C0.total_energy;
    double maxErr = 0.0;

    updateAccelerations(bodies); // required before first symplectic step
    for (int i = 0; i < steps; ++i)
    {
        stepFn(bodies, dt);
        auto C = physics::compute(bodies);
        double err = std::abs((C.total_energy - E0) / E0);
        if (err > maxErr)
            maxErr = err;
    }
    return maxErr;
}

int main()
{
    const double DT = 3600.0; // 1-hour step
    const int STEPS = 8760;   // 1 year

    auto bodies = loadSystemFromJSON("systems/earth_moon.json");

    auto lf = [](std::vector<CelestialBody>& b, double d) { leapfrogStep(b, d); };
    auto y4 = [](std::vector<CelestialBody>& b, double d) { yoshida4Step(b, d); };

    double maxErr_lf = maxEnergyDrift(bodies, STEPS, DT, lf);
    double maxErr_y4 = maxEnergyDrift(bodies, STEPS, DT, y4);

    std::cout << "Max |dE/E| over 1 year (dt=1h):\n";
    std::cout << "  Leapfrog : " << maxErr_lf << "\n";
    std::cout << "  Yoshida4 : " << maxErr_y4 << "\n";

    // 1. Yoshida4 must be bounded (symplectic — not growing without bound)
    if (maxErr_y4 > 1e-3)
    {
        std::cerr << "FAIL: Yoshida4 energy drift exceeds 0.1% over 1 year\n";
        return 1;
    }

    // 2. Yoshida4 must outperform Leapfrog at the same timestep
    if (maxErr_y4 >= maxErr_lf)
    {
        std::cerr << "FAIL: Yoshida4 energy drift (" << maxErr_y4
                  << ") is not better than Leapfrog (" << maxErr_lf << ")\n";
        return 1;
    }

    std::cout << "PASS: Yoshida4 symplectic and better than Leapfrog at same dt\n";
    return 0;
}
