#include "simulation.h"
#include "conservations.h"
#include "json_loader.h"
#include <iostream>
#include <cmath>
#include <cassert>

int main()
{
    // Load the Earth-Moon system
    auto bodies = loadSystemFromJSON("systems/earth_moon.json");

    // Compute initial energy
    physics::Conservations C0 = physics::compute(bodies);
    double E0 = C0.total_energy;

    std::cout << "Initial energy: " << E0 << "\n";

    // Run 10,000 steps at dt=60s (real Moon orbit needs fine steps)
    const int STEPS = 10000;
    const double DT = 60.0;

    for (int i = 0; i < STEPS; ++i)
    {
        rk4Step(bodies, DT);
    }

    // Compute final energy
    physics::Conservations Cf = physics::compute(bodies);
    double Ef = Cf.total_energy;

    double drift = std::abs((Ef - E0) / E0);

    std::cout << "Final energy:   " << Ef << "\n";
    std::cout << "Relative drift: " << drift << "\n";

    // Assert drift stays below 1e-5 (0.001%)
    if (drift > 1e-5)
    {
        std::cerr << "FAIL: energy drift " << drift << " exceeds 1e-5\n";
        return 1;
    }

    std::cout << "PASS: energy conservation OK\n";
    return 0;
}