/*******************
 * csv_loader.cpp
 * @brief CSV loader implementation for orbit viewer
 * @author Sinan
 * @date 11/25/2025
 ******************/

#include "viewer/csv_loader.h"

// Moon orbit exaggeration factor (for visibility)
static constexpr float MOON_EXAGGERATION = 15.0f;

std::vector<Frame> CSVLoader::loadOrbitCSV(const std::string& path)
{
    std::vector<Frame> frames;
    std::ifstream file(path);

    if (!file)
    {
        std::cerr << "❌ Cannot open CSV: " << path << "\n";
        return frames;
    }

    std::string line;
    std::getline(file, line); // header

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);
        Frame f;
        int step;
        char comma;

        ss >> step >> comma >> f.sun.x >> comma >> f.sun.y >> comma >> f.sun.z >> comma >>
            f.earth.x >> comma >> f.earth.y >> comma >> f.earth.z >> comma >> f.moon.x >> comma >>
            f.moon.y >> comma >> f.moon.z;

        // 1) DO NOT scale — values already scaled in CSV
        f.sun *= this->scaleMeters;
        f.earth *= this->scaleMeters;
        f.moon *= this->scaleMeters;

        // 2) Earth stays as-is
        glm::vec3 earthOffset = f.earth - f.sun;
        f.earth = f.sun + earthOffset;

        // 3) Moon exaggeration ONLY
        glm::vec3 moonOffset = f.moon - f.earth;
        float moonExaggeration = 15.0f;
        f.moon = f.earth + moonOffset * moonExaggeration;

        frames.push_back(f);
    }

    std::cout << "📄 Loaded " << frames.size() << " frames from " << path << "\n";

    return frames;
}
