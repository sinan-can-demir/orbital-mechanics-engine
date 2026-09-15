#include "horizons_parser.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
// Extracts the whitespace-delimited token immediately preceding position
// `eqPos` in `line` (i.e. the field label just before its '=' sign), e.g.
// for " X =1.0" and eqPos pointing at '=', returns "X".
std::string labelBeforeEquals(const std::string& line, size_t eqPos)
{
    size_t end = eqPos;
    while (end > 0 && std::isspace(static_cast<unsigned char>(line[end - 1])))
        --end;

    size_t begin = end;
    while (begin > 0 && !std::isspace(static_cast<unsigned char>(line[begin - 1])))
        --begin;

    return line.substr(begin, end - begin);
}

// Parses three "LABEL =value" fields from `line`, validating that the field
// labels match expLabelA/B/C in order (e.g. "X","Y","Z"), not just their
// position. HORIZONS always emits fields in a fixed order today, but nothing
// enforced that assumption before — a reordered/malformed line used to be
// silently accepted and its values assigned positionally.
bool parseTripleAfterEquals(const std::string& line, const std::string& expLabelA,
                            const std::string& expLabelB, const std::string& expLabelC, double& a,
                            double& b, double& c)
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

    if (labelBeforeEquals(line, eq1) != expLabelA || labelBeforeEquals(line, eq2) != expLabelB ||
        labelBeforeEquals(line, eq3) != expLabelC)
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
        std::cerr << "❌ parseHorizonsVectors: truncated data block after $$SOE in: " << path
                  << "\n";
        return false;
    }

    double rawX = 0.0, rawY = 0.0, rawZ = 0.0;
    double rawVx = 0.0, rawVy = 0.0, rawVz = 0.0;

    if (!parseTripleAfterEquals(xyzLine, "X", "Y", "Z", rawX, rawY, rawZ))
    {
        std::cerr << "❌ parseHorizonsVectors: malformed or mislabeled XYZ line in: " << path
                  << "\n";
        return false;
    }

    if (!parseTripleAfterEquals(vxyzLine, "VX", "VY", "VZ", rawVx, rawVy, rawVz))
    {
        std::cerr << "❌ parseHorizonsVectors: malformed or mislabeled VXYZ line in: " << path
                  << "\n";
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
