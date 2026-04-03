#ifndef ORBIT_SIM_HORIZONS_BUILDER_H
#define ORBIT_SIM_HORIZONS_BUILDER_H

#include <string>
#include <vector>

struct BuildSystemOptions
{
    std::vector<std::string> bodyIds;
    std::string epoch;
    std::string center = "@0";
    std::string output;
    bool usePost = true;
    bool verbose = false;
};

bool buildSystemFromHorizons(const BuildSystemOptions& opts);

#endif // ORBIT_SIM_HORIZONS_BUILDER_H
