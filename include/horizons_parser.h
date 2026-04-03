#ifndef ORBIT_SIM_HORIZONS_PARSER_H
#define ORBIT_SIM_HORIZONS_PARSER_H

#include <string>

struct HorizonsState
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    double vx = 0.0;
    double vy = 0.0;
    double vz = 0.0;

    std::string epoch;
};

bool parseHorizonsVectors(const std::string& path, HorizonsState& state);

#endif // ORBIT_SIM_HORIZONS_PARSER_H
