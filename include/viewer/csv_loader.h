/**********************
 * csv_loader.h
 * @brief CSV loader for orbit viewer
 * @author Sinan Demir
 * @date 11/25/2025
 **********************/

#ifndef CSV_LOADER_H
#define CSV_LOADER_H

#include <fstream>
#include <glm/vec3.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Frame
{
    glm::vec3 sun;
    glm::vec3 earth;
    glm::vec3 moon;
};

class CSVLoader
{
  public:
    CSVLoader() = default;

    // Load from file path
    std::vector<Frame> loadOrbitCSV(const std::string& path);

  private:
    float scaleMeters = 1.0f / 5e9f; // 1 GL unit = 5,000,000 km
};

#endif // CSV_LOADER_H