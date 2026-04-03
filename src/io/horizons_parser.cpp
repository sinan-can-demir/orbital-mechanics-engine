#include "horizons_parser.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
bool parseTripleAfterEquals(const std::string& line, double& a, double& b, double& c)
{
    const size_t eq1 = line.find('=');
    if (eq1 == std::string::npos)
    {
        return false;
    }

    const size_t eq2 = line.find('=', eq1 + 1);
    if (eq2 == std::string::npos)
    {
        return false;
    }

    const size_t eq3 = line.find('=', eq2 + 1);
    if (eq3 == std::string::npos)
    {
        return false;
    }

    try
    {
        a = std::stod(line.substr(eq1 + 1));
        b = std::stod(line.substr(eq2 + 1));
        c = std::stod(line.substr(eq3 + 1));
    }
    catch (const std::exception&)
    {
        return false;
    }

    return true;
}
} // namespace

bool parseHorizonsVectors(const std::string& path, HorizonsState& state)
{
    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "❌ parseHorizonsVectors: cannot open file: " << path << "\n";
        return false;
    }

    std::string line;
    bool foundSOE = false;

    while (std::getline(in, line))
    {
        if (line.find("$$SOE") != std::string::npos)
        {
            foundSOE = true;
            break;
        }
    }

    if (!foundSOE)
    {
        std::cerr << "❌ parseHorizonsVectors: missing $$SOE marker in: " << path << "\n";
        return false;
    }

    std::string epochLine;
    std::string xyzLine;
    std::string vxyzLine;

    if (!std::getline(in, epochLine) || !std::getline(in, xyzLine) || !std::getline(in, vxyzLine))
    {
        std::cerr << "❌ parseHorizonsVectors: truncated data block after $$SOE in: " << path << "\n";
        return false;
    }

    double rawX = 0.0, rawY = 0.0, rawZ = 0.0;
    double rawVx = 0.0, rawVy = 0.0, rawVz = 0.0;

    if (!parseTripleAfterEquals(xyzLine, rawX, rawY, rawZ))
    {
        std::cerr << "❌ parseHorizonsVectors: malformed XYZ line in: " << path << "\n";
        return false;
    }

    if (!parseTripleAfterEquals(vxyzLine, rawVx, rawVy, rawVz))
    {
        std::cerr << "❌ parseHorizonsVectors: malformed VXYZ line in: " << path << "\n";
        return false;
    }

    constexpr double kKmToM = 1000.0;

    state.x = rawX * kKmToM;
    state.y = rawY * kKmToM;
    state.z = rawZ * kKmToM;

    state.vx = rawVx * kKmToM;
    state.vy = rawVy * kKmToM;
    state.vz = rawVz * kKmToM;

    state.epoch = epochLine;

    return true;
}
