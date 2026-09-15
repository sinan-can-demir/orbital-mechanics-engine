// tests/test_csv_loader.cpp
//
// Regression tests for the viewer's CSVLoader.
//
// 1. exportCSV() (src/core/simulation.cpp) always writes two header lines --
//    a "# dt=... bodies=..." metadata comment, then the "step,x_Sun,..."
//    column-name header -- so the loader must skip both, not just one, or
//    the column-header line gets misparsed as a bogus all-zero data row and
//    every real frame ends up shifted by one index.
//
// 2. The loader hardcodes exactly 3 bodies (Sun, Earth, Moon) in that column
//    order, matching exportCSV()'s default 3-body demo output. Pointing it
//    at a CSV for any other system (different body count/order) must be
//    rejected with a clear error instead of silently mislabeling columns
//    (e.g. assigning Mercury's data to "earth").

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

    // ── Case 1: both header lines are skipped correctly, no bogus frame ────
    const std::string headerSkipPath = "build/test_csv_loader_header_skip.csv";
    writeCSV(headerSkipPath, "step,x_Sun,y_Sun,z_Sun,x_Earth,y_Earth,z_Earth,x_Moon,y_Moon,z_Moon",
             {
                 "0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0",
                 "1,1.1,2.1,3.1,4.1,5.1,6.1,7.1,8.1,9.1",
             });

    auto headerSkipFrames = loader.loadOrbitCSV(headerSkipPath);
    std::filesystem::remove(headerSkipPath);

    if (headerSkipFrames.size() != 2)
    {
        std::cerr << "FAIL: expected 2 frames for 2 data rows, got " << headerSkipFrames.size()
                  << "\n";
        return 1;
    }

    // frame[0] must be the real first data row (sun=(1,2,3)), not the
    // near-zero/garbage result of parsing the column-header text.
    if (headerSkipFrames[0].sun.x == 0.0f && headerSkipFrames[0].sun.y == 0.0f &&
        headerSkipFrames[0].sun.z == 0.0f)
    {
        std::cerr << "FAIL: frame[0] looks like the misparsed column-header row\n";
        return 1;
    }
    std::cout << "PASS: both header lines are skipped correctly (no bogus frame)\n";

    // ── Case 2: correct Sun/Earth/Moon layout loads normally ───────────────
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

    // ── Case 3: a differently-shaped system (e.g. 10-body solar system) is
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
