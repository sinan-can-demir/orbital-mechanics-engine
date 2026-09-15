// tests/test_csv_loader.cpp
//
// Regression test for the viewer's CSVLoader: it hardcodes exactly 3 bodies
// (Sun, Earth, Moon) in that column order, matching exportCSV()'s default
// 3-body demo output. Pointing it at a CSV for any other system (different
// body count/order) must be rejected with a clear error instead of silently
// mislabeling columns (e.g. assigning Mercury's data to "earth").

#include "viewer/csv_loader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void writeCSV(const std::string& path, const std::string& header,
              const std::vector<std::string>& rows)
{
    std::ofstream out(path);
    out << "# dt=60 stride=1 bodies=...\n";
    out << header << "\n";
    for (const auto& row : rows)
        out << row << "\n";
}
} // namespace

int main()
{
    std::filesystem::create_directories("build");
    CSVLoader loader;

    // ── Case 1: correct Sun/Earth/Moon layout loads normally ───────────────
    const std::string okPath = "build/test_csv_loader_ok.csv";
    writeCSV(okPath, "step,x_Sun,y_Sun,z_Sun,x_Earth,y_Earth,z_Earth,x_Moon,y_Moon,z_Moon",
             {"0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0"});

    auto okFrames = loader.loadOrbitCSV(okPath);
    std::filesystem::remove(okPath);

    if (okFrames.size() != 1)
    {
        std::cerr << "FAIL: expected 1 frame for a correctly-shaped 3-body CSV, got "
                  << okFrames.size() << "\n";
        return 1;
    }
    std::cout << "PASS: correctly-shaped Sun/Earth/Moon CSV loads normally\n";

    // ── Case 2: a differently-shaped system (e.g. 10-body solar system) is
    //    rejected instead of silently mislabeled ────────────────────────────
    const std::string wrongPath = "build/test_csv_loader_wrong.csv";
    writeCSV(wrongPath,
             "step,x_Sun,y_Sun,z_Sun,x_Mercury,y_Mercury,z_Mercury,x_Earth,y_Earth,z_Earth",
             {"0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0"});

    auto wrongFrames = loader.loadOrbitCSV(wrongPath);
    std::filesystem::remove(wrongPath);

    if (!wrongFrames.empty())
    {
        std::cerr << "FAIL: loader accepted a CSV with a different body layout instead of "
                     "rejecting it\n";
        return 1;
    }
    std::cout << "PASS: differently-shaped CSV is rejected instead of silently mislabeled\n";

    std::cout << "PASS: all CSVLoader tests passed\n";
    return 0;
}
