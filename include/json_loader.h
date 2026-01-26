/*****************
 * Author: Sinan Demir
 * File: json_loader.h
 * Date: 11/18/2025
 * Purpose: Header file for JSON loader module.
 ******************/

#ifndef ORBIT_SIM_JSON_LOADER_H
#define ORBIT_SIM_JSON_LOADER_H

#include "body.h" // CelestialBody
#include <string>
#include <vector>

std::vector<CelestialBody> loadSystemFromJSON(const std::string &path);

#endif // ORBIT_SIM_JSON_LOADER_H
