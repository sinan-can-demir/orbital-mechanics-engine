/*******************
 * csv_loader.cpp
 * @brief CSV loader implementation for orbit viewer
 * @author Sinan
 * @date 11/25/2025
 ******************/

#include "viewer/csv_loader.h"

// Moon orbit exaggeration factor (for visibility)
static constexpr float MOON_EXAGGERATION = 15.0f;

namespace
{
// The only system this viewer knows how to render: 3 bodies in this exact
// order, matching exportCSV()'s "step,x_<name>,y_<name>,z_<name>,..." header.
const std::vector<std::string> kExpectedHeader = {
    "step",    "x_Sun",   "y_Sun",  "z_Sun",  "x_Earth",
    "y_Earth", "z_Earth", "x_Moon", "y_Moon", "z_Moon",
};

std::vector<std::string> splitCSVHeader(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
        fields.push_back(field);
    return fields;
}
} // namespace

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

    // exportCSV() writes a "# dt=... bodies=..." metadata comment followed by
    // the actual "step,x_Sun,..." column-name header — skip both, not just
    // one, or the column-header line gets misparsed as a bogus data row.
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        break; // `line` now holds the column-name header
    }

    // This viewer hardcodes Sun/Earth/Moon in this order (see Frame in
    // include/viewer/csv_loader.h) — it cannot render an arbitrary system.
    // Without this check, pointing it at any other system's CSV would
    // silently assign the wrong body's columns to sun/earth/moon instead of
    // failing clearly.
    if (splitCSVHeader(line) != kExpectedHeader)
    {
        std::cerr << "❌ Unsupported CSV column layout in: " << path << "\n"
                  << "   This viewer only supports 3-body Sun/Earth/Moon systems "
                     "(expected header: step,x_Sun,y_Sun,z_Sun,x_Earth,y_Earth,z_Earth,x_Moon,y_"
                     "Moon,z_Moon)\n";
        return frames;
    }

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

        if (ss.fail())
        {
            std::cerr << "⚠️  Skipping malformed CSV row: " << line << "\n";
            continue;
        }

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
