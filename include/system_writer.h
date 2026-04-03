#ifndef ORBIT_SIM_SYSTEM_WRITER_H
#define ORBIT_SIM_SYSTEM_WRITER_H

#include "body.h"
#include <string>
#include <vector>

bool writeSystemJSON(const std::vector<CelestialBody>& bodies, const std::string& systemName,
                     const std::string& epoch, const std::string& outputPath);

#endif // ORBIT_SIM_SYSTEM_WRITER_H
