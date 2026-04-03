#include "horizons_parser.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    const std::string path = "build/test_horizons_parser_input.txt";
    std::filesystem::create_directories("build");

    std::ofstream out(path);
    out << "Header\n";
    out << "$$SOE\n";
    out << "2460676.500000000 = A.D. 2025-Jan-01 00:00:00.0000 TDB\n";
    out << " X =-2.627653140025671E+07 Y = 1.444218499425369E+08 Z = 1.898104419804503E+04\n";
    out << " VX=-2.981142022421017E+01 VY=-5.399982640548080E+00 VZ= 9.016363228819699E-04\n";
    out << " LT= 4.944735147427052E+02 RG= 1.482209034698547E+08 RR=-3.019720994543136E-01\n";
    out << "$$EOE\n";
    out.close();

    HorizonsState state;
    const bool ok = parseHorizonsVectors(path, state);
    if (!ok)
    {
        std::cerr << "FAIL: parseHorizonsVectors returned false\n";
        return 1;
    }

    // Expected SI values from the snippet.
    const double expectedX = -2.627653140025671e10;
    const double expectedVy = -5.39998264054808e3;

    if (std::abs(state.x - expectedX) > 1.0)
    {
        std::cerr << "FAIL: x mismatch, got " << state.x << " expected " << expectedX << "\n";
        return 1;
    }

    if (std::abs(state.vy - expectedVy) > 1e-6)
    {
        std::cerr << "FAIL: vy mismatch, got " << state.vy << " expected " << expectedVy << "\n";
        return 1;
    }

    if (state.epoch.empty())
    {
        std::cerr << "FAIL: epoch should not be empty\n";
        return 1;
    }

    std::filesystem::remove(path);

    std::cout << "PASS: HORIZONS parser parsed first epoch and converted SI units\n";
    return 0;
}
