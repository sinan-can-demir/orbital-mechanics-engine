#include "horizons_builder.h"

#include "horizons.h"
#include "horizons_parser.h"
#include "json_loader.h"
#include "system_writer.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
struct BodyMeta
{
    const char* name;
    double massKg;
};

const std::unordered_map<std::string, BodyMeta> kKnownBodies = {
    {"10", {"Sun", 1.98847e30}},      {"199", {"Mercury", 3.3011e23}},
    {"299", {"Venus", 4.8675e24}},    {"399", {"Earth", 5.9722e24}},
    {"301", {"Moon", 7.342e22}},      {"499", {"Mars", 6.4171e23}},
    {"599", {"Jupiter", 1.8982e27}},  {"699", {"Saturn", 5.6834e26}},
    {"799", {"Uranus", 8.6810e25}},   {"899", {"Neptune", 1.02413e26}},
};

std::string trim(const std::string& s)
{
    const std::string whitespace = " \t\r\n";
    const size_t begin = s.find_first_not_of(whitespace);
    if (begin == std::string::npos)
    {
        return "";
    }
    const size_t end = s.find_last_not_of(whitespace);
    return s.substr(begin, end - begin + 1);
}

bool parseYMD(const std::string& date, std::tm& tmOut)
{
    std::istringstream iss(date);
    iss >> std::get_time(&tmOut, "%Y-%m-%d");
    if (iss.fail())
    {
        return false;
    }
    tmOut.tm_hour = 0;
    tmOut.tm_min = 0;
    tmOut.tm_sec = 0;
    tmOut.tm_isdst = -1;
    return true;
}

bool nextDay(const std::string& date, std::string& out)
{
    std::tm tmValue = {};
    if (!parseYMD(date, tmValue))
    {
        return false;
    }

    std::time_t t = std::mktime(&tmValue);
    if (t == static_cast<std::time_t>(-1))
    {
        return false;
    }

    t += 24 * 60 * 60;
    std::tm* next = std::localtime(&t);
    if (!next)
    {
        return false;
    }

    std::ostringstream oss;
    oss << std::put_time(next, "%Y-%m-%d");
    out = oss.str();
    return true;
}
} // namespace

bool buildSystemFromHorizons(const BuildSystemOptions& opts)
{
    if (opts.bodyIds.empty())
    {
        std::cerr << "❌ build-system: no body IDs provided\n";
        return false;
    }
    if (opts.epoch.empty())
    {
        std::cerr << "❌ build-system: epoch is required (YYYY-MM-DD)\n";
        return false;
    }
    if (opts.output.empty())
    {
        std::cerr << "❌ build-system: output path is required\n";
        return false;
    }

    std::string stopDate;
    if (!nextDay(opts.epoch, stopDate))
    {
        std::cerr << "❌ build-system: invalid epoch format (expected YYYY-MM-DD): " << opts.epoch
                  << "\n";
        return false;
    }

    std::vector<CelestialBody> bodies;
    bodies.reserve(opts.bodyIds.size());

    for (const std::string& rawId : opts.bodyIds)
    {
        const std::string bodyId = trim(rawId);
        if (bodyId.empty())
        {
            continue;
        }

        std::string name = bodyId;
        double mass = 0.0;

        const auto it = kKnownBodies.find(bodyId);
        if (it != kKnownBodies.end())
        {
            name = it->second.name;
            mass = it->second.massKg;
        }
        else
        {
            std::cerr << "⚠️  Unknown body ID " << bodyId
                      << " (mass lookup missing). Using mass=0.0 kg.\n";
        }

        const std::string tempPath = "/tmp/orb_fetch_" + bodyId + ".txt";

        HorizonsFetchOptions hopt;
        hopt.command = bodyId;
        hopt.center = opts.center.empty() ? "@0" : opts.center;
        hopt.start_time = opts.epoch;
        hopt.stop_time = stopDate;
        hopt.step_size = "1 d";

        const bool fetched = opts.usePost ? fetchHorizonsEphemerisPOST(hopt, tempPath, opts.verbose)
                                          : fetchHorizonsEphemeris(hopt, tempPath, opts.verbose);

        if (!fetched)
        {
            std::cerr << "❌ build-system: fetch failed for body ID " << bodyId << "\n";
            return false;
        }

        HorizonsState state;
        if (!parseHorizonsVectors(tempPath, state))
        {
            std::cerr << "❌ build-system: parse failed for body ID " << bodyId
                      << " (left temp file at " << tempPath << ")\n";
            return false;
        }

        std::error_code ec;
        std::filesystem::remove(tempPath, ec);

        bodies.emplace_back(name, mass, state.x, state.y, state.z, state.vx, state.vy, state.vz);

        std::cout << "✅ " << name << " (" << bodyId << ")"
                  << " | pos=(" << state.x << ", " << state.y << ", " << state.z << ")"
                  << " | vel=(" << state.vx << ", " << state.vy << ", " << state.vz << ")"
                  << " | mass=" << mass << "\n";
    }

    if (bodies.empty())
    {
        std::cerr << "❌ build-system: no valid bodies to write\n";
        return false;
    }

    std::ostringstream systemName;
    systemName << "HORIZONS generated system (" << opts.epoch << ")";

    const bool written = writeSystemJSON(bodies, systemName.str(), opts.epoch + " 00:00:00 TDB",
                                         opts.output);
    if (!written)
    {
        return false;
    }

    try
    {
        const auto loaded = loadSystemFromJSON(opts.output);
        std::cout << "✅ Written: " << opts.output << " (" << loaded.size() << " bodies)\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ build-system: round-trip load failed: " << e.what() << "\n";
        return false;
    }

    return true;
}
