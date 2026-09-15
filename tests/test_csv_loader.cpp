// tests/test_csv_loader.cpp
//
// Regression test for the viewer's CSVLoader: exportCSV() (src/core/simulation.cpp)
// always writes two header lines -- a "# dt=... bodies=..." metadata comment,
// then the "step,x_Sun,..." column-name header -- so the loader must skip both,
// not just one, or the column-header line gets misparsed as a bogus all-zero
// data row and every real frame ends up shifted by one index.

#include "viewer/csv_loader.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    const std::string path = "build/test_csv_loader_input.csv";
    std::filesystem::create_directories("build");

    std::ofstream out(path);
    out << "# dt=60 stride=1 bodies=Sun:1,Earth:2,Moon:3\n";
    out << "step,x_Sun,y_Sun,z_Sun,x_Earth,y_Earth,z_Earth,x_Moon,y_Moon,z_Moon\n";
    out << "0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0\n";
    out << "1,1.1,2.1,3.1,4.1,5.1,6.1,7.1,8.1,9.1\n";
    out.close();

    CSVLoader loader;
    auto frames = loader.loadOrbitCSV(path);
    std::filesystem::remove(path);

    if (frames.size() != 2)
    {
        std::cerr << "FAIL: expected 2 frames, got " << frames.size() << "\n";
        return 1;
    }
    std::cout << "PASS: loaded exactly 2 frames for 2 data rows (no bogus header frame)\n";

    // frame[0] must be the real first data row (sun=(1,2,3)), not the
    // near-zero/garbage result of parsing the column-header text.
    if (frames[0].sun.x == 0.0f && frames[0].sun.y == 0.0f && frames[0].sun.z == 0.0f)
    {
        std::cerr << "FAIL: frame[0] looks like the misparsed column-header row\n";
        return 1;
    }
    std::cout << "PASS: frame[0] holds the real first data row, not the header\n";

    std::cout << "PASS: all CSVLoader tests passed\n";
    return 0;
}
