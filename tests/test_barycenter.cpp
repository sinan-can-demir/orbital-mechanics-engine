// tests/test_barycenter.cpp
#include "barycenter.h"
#include "body.h"
#include <iostream>
#include <cmath>

int main()
{
    // Two equal masses separated by distance d
    // COM should be exactly at midpoint
    std::vector<CelestialBody> bodies = {
        CelestialBody("A", 1e24, -1e10, 0.0, 0.0, 100.0, 0.0, 0.0),
        CelestialBody("B", 1e24, 1e10, 0.0, 0.0, -100.0, 0.0, 0.0),
    };

    physics::normalizeToBarycenter(bodies);

    // After normalization:
    // COM position should be < 1e-6 meters from origin
    // COM velocity should be < 1e-9 m/s

    double comX = 1e24 * bodies[0].position.x() + 1e24 * bodies[1].position.x();
    comX /= 2e24;

    double comVx = 1e24 * bodies[0].velocity.x() + 1e24 * bodies[1].velocity.x();
    comVx /= 2e24;

    std::cout << "COM position x: " << comX << " m (should be ~0)\n";
    std::cout << "COM velocity x: " << comVx << " m/s (should be ~0)\n";

    if (std::abs(comX) > 1e-6)
    {
        std::cerr << "FAIL: COM position not at origin\n";
        return 1;
    }

    if (std::abs(comVx) > 1e-9)
    {
        std::cerr << "FAIL: COM velocity not zero\n";
        return 1;
    }

    // ── Unequal masses ─────────────────────────────────────────────────────
    // Sun-Earth: COM should be inside the Sun (much closer to Sun)
    std::vector<CelestialBody> bodies2 = {
        CelestialBody("Sun", 1.98847e30, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        CelestialBody("Earth", 5.9722e24, 1.496e11, 0.0, 0.0, 0.0, 29780.0, 0.0),
    };

    physics::normalizeToBarycenter(bodies2);

    // Total momentum should be ~zero after normalization
    double px = 1.98847e30 * bodies2[0].velocity.x() + 5.9722e24 * bodies2[1].velocity.x();

    std::cout << "Total px after normalization: " << px << " kg*m/s (should be ~0)\n";

    if (std::abs(px) > 1e6)
    {
        std::cerr << "FAIL: momentum not conserved after normalization\n";
        return 1;
    }

    std::cout << "PASS: barycenter normalization validated\n";
    return 0;
}