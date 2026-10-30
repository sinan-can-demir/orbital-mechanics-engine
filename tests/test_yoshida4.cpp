// tests/test_yoshida4.cpp
// Validates the Yoshida 4th-order symplectic integrator:
//   1. Energy is bounded (not monotonically drifting) — symplectic property
//   2. Energy drift is tighter than Leapfrog at the same timestep — 4th-order advantage
#include "json_loader.h"
#include "test_helpers.h"

int main()
{
    using namespace testing;

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
    CHECK(maxErr_y4 <= 1e-3, "Yoshida4 energy drift stays under 0.1% over 1 year");

    // 2. Yoshida4 must outperform Leapfrog at the same timestep
    CHECK(maxErr_y4 < maxErr_lf, "Yoshida4 energy drift is better than Leapfrog at the same dt");

    return finish("Yoshida4 symplectic and better than Leapfrog at same dt");
}
