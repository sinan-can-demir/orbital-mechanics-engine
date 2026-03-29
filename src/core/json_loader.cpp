/*****************
 * Author: Sinan Demir
 * File: json_loader.cpp
 * Date: 11/18/2025
 * Purpose: Implementation of JSON loader for N-body systems.
 ******************/

#include "json_loader.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

std::vector<CelestialBody> loadSystemFromJSON(const std::string& path)
{
    /**********************
     * loadSystemFromJSON
     * @brief: Loads celestial bodies from a JSON file.
     * @param: path - file path to JSON configuration
     * @return: vector of CelestialBody instances
     * @exception: throws runtime_error if file cannot be opened
     * @note: Expects JSON structure with "bodies" array containing
     *        name, mass, position [x,y,z], velocity [vx,vy,vz]
     **********************/
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Could not open JSON file: " + path);
    }

    json j;
    file >> j;

    std::vector<CelestialBody> bodies;

    for (const auto& b : j["bodies"])
    {
        // Parse body attributes
        std::string name = b.at("name").get<std::string>();
        double mass = b.at("mass").get<double>();
        // position and velocity arrays
        auto p = b.at("position");
        auto v = b.at("velocity");
        // Extract components
        double x = p.at(0).get<double>();
        double y = p.at(1).get<double>();
        double z = p.at(2).get<double>();
        // velocity components
        double vx = v.at(0).get<double>();
        double vy = v.at(1).get<double>();
        double vz = v.at(2).get<double>();
        // Create CelestialBody and add to vector
        bodies.emplace_back(name, mass, x, y, z, vx, vy, vz, 0.0, 0.0,
                            0.0 // ax, ay, az
        );
    } // end for loop

    return bodies;
} // end loadSystemFromJSON
