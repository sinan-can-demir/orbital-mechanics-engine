#include "json_loader.h"
#include "system_writer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
bool nearlyEqual(double a, double b, double absTol = 1e-9, double relTol = 1e-12)
{
    const double diff = std::abs(a - b);
    if (diff <= absTol)
    {
        return true;
    }
    return diff <= relTol * std::max(std::abs(a), std::abs(b));
}
} // namespace

int main()
{
    std::filesystem::create_directories("build");
    const std::string outputPath = "build/test_generated_system.json";

    std::vector<CelestialBody> expected = {
        CelestialBody("Sun", 1.98847e30, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        CelestialBody("Earth", 5.9722e24, 1.496e11, 0.0, 0.0, 0.0, 29780.0, 0.0),
        CelestialBody("Moon", 7.342e22, 1.499844e11, 0.0, 0.0, 0.0, 30802.0, 0.0),
    };

    const bool ok = writeSystemJSON(expected, "Test System", "2025-01-01 00:00:00 TDB", outputPath);
    if (!ok)
    {
        std::cerr << "FAIL: writeSystemJSON returned false\n";
        return 1;
    }

    auto loaded = loadSystemFromJSON(outputPath);
    if (loaded.size() != expected.size())
    {
        std::cerr << "FAIL: body count mismatch\n";
        return 1;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        const auto& e = expected[i];
        const auto& g = loaded[i];

        if (e.name != g.name)
        {
            std::cerr << "FAIL: name mismatch at index " << i << "\n";
            return 1;
        }

        if (!nearlyEqual(e.mass, g.mass))
        {
            std::cerr << "FAIL: mass mismatch for " << e.name << "\n";
            return 1;
        }

        if (!nearlyEqual(e.position.x(), g.position.x()) || !nearlyEqual(e.position.y(), g.position.y()) ||
            !nearlyEqual(e.position.z(), g.position.z()))
        {
            std::cerr << "FAIL: position mismatch for " << e.name << "\n";
            return 1;
        }

        if (!nearlyEqual(e.velocity.x(), g.velocity.x()) || !nearlyEqual(e.velocity.y(), g.velocity.y()) ||
            !nearlyEqual(e.velocity.z(), g.velocity.z()))
        {
            std::cerr << "FAIL: velocity mismatch for " << e.name << "\n";
            return 1;
        }
    }

    std::filesystem::remove(outputPath);

    std::cout << "PASS: system writer JSON round-trip validated\n";
    return 0;
}
