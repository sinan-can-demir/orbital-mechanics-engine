#include "system_writer.h"

#include "json_loader.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

bool writeSystemJSON(const std::vector<CelestialBody>& bodies, const std::string& systemName,
                     const std::string& epoch, const std::string& outputPath)
{
    json root;
    root["name"] = systemName;
    root["epoch"] = epoch;
    root["note"] = "Generated from NASA JPL HORIZONS. Do not edit manually.";
    root["bodies"] = json::array();

    for (const auto& b : bodies)
    {
        json body;
        body["name"] = b.name;
        body["mass"] = b.mass;
        body["position"] = {b.position.x(), b.position.y(), b.position.z()};
        body["velocity"] = {b.velocity.x(), b.velocity.y(), b.velocity.z()};
        root["bodies"].push_back(body);
    }

    std::ofstream out(outputPath);
    if (!out)
    {
        std::cerr << "❌ writeSystemJSON: could not open output file: " << outputPath << "\n";
        return false;
    }
    out << root.dump(2) << '\n';
    out.close();

    // Round-trip validation against existing loader to catch serialization issues.
    std::vector<CelestialBody> reloaded;
    try
    {
        reloaded = loadSystemFromJSON(outputPath);
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ writeSystemJSON: round-trip load failed: " << e.what() << "\n";
        return false;
    }

    if (reloaded.size() != bodies.size())
    {
        std::cerr << "❌ writeSystemJSON: body count mismatch after round-trip (" << reloaded.size()
                  << " vs " << bodies.size() << ")\n";
        return false;
    }

    for (size_t i = 0; i < bodies.size(); ++i)
    {
        const auto& expected = bodies[i];
        const auto& got = reloaded[i];

        if (expected.name != got.name)
        {
            std::cerr << "❌ writeSystemJSON: name mismatch at body " << i << "\n";
            return false;
        }

        if (!nearlyEqual(expected.mass, got.mass))
        {
            std::cerr << "❌ writeSystemJSON: mass mismatch for " << expected.name << "\n";
            return false;
        }

        if (!nearlyEqual(expected.position.x(), got.position.x()) ||
            !nearlyEqual(expected.position.y(), got.position.y()) ||
            !nearlyEqual(expected.position.z(), got.position.z()))
        {
            std::cerr << "❌ writeSystemJSON: position mismatch for " << expected.name << "\n";
            return false;
        }

        if (!nearlyEqual(expected.velocity.x(), got.velocity.x()) ||
            !nearlyEqual(expected.velocity.y(), got.velocity.y()) ||
            !nearlyEqual(expected.velocity.z(), got.velocity.z()))
        {
            std::cerr << "❌ writeSystemJSON: velocity mismatch for " << expected.name << "\n";
            return false;
        }
    }

    return true;
}
