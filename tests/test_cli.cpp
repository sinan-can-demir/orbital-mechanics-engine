// tests/test_cli.cpp
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>

int main()
{
    // ── Test 1: --help exits 0 ─────────────────────────────────────────────
    int ret = std::system("./bin/orbit-sim --help > /dev/null 2>&1");
    if (ret != 0)
    {
        std::cerr << "FAIL: orbit-sim --help returned " << ret << "\n";
        return 1;
    }
    std::cout << "PASS: --help exits 0\n";

    // ── Test 2: validate good system exits 0 ──────────────────────────────
    ret = std::system(
        "./bin/orbit-sim validate --system ../systems/earth_moon.json > /dev/null 2>&1");
    if (ret != 0)
    {
        std::cerr << "FAIL: validate returned " << ret << "\n";
        return 1;
    }
    std::cout << "PASS: validate exits 0\n";

    // ── Test 3: validate bad system exits non-zero ─────────────────────────
    ret = std::system("./bin/orbit-sim validate --system nonexistent.json > /dev/null 2>&1");
    if (ret == 0)
    {
        std::cerr << "FAIL: validate of nonexistent file should exit non-zero\n";
        return 1;
    }
    std::cout << "PASS: validate of bad file exits non-zero\n";

    // ── Test 4: run produces output file ──────────────────────────────────
    ret = std::system("./bin/orbit-sim run "
                      "--system ../systems/earth_moon.json "
                      "--steps 10 --dt 3600 "
                      "--output /tmp/test_cli_out.csv "
                      "> /dev/null 2>&1");
    if (ret != 0)
    {
        std::cerr << "FAIL: run returned " << ret << "\n";
        return 1;
    }

    if (!std::filesystem::exists("/tmp/test_cli_out.csv"))
    {
        std::cerr << "FAIL: output file not created\n";
        return 1;
    }

    // Check file is not empty
    std::ifstream f("/tmp/test_cli_out.csv");
    std::string line;
    int lineCount = 0;
    while (std::getline(f, line))
        lineCount++;

    if (lineCount < 3)
    { // metadata + header + at least 1 data row
        std::cerr << "FAIL: output file has too few lines (" << lineCount << ")\n";
        return 1;
    }
    std::cout << "PASS: run produces valid output file\n";

    std::filesystem::remove("/tmp/test_cli_out.csv");
    std::filesystem::remove("/tmp/test_cli_out_conservation.csv");

    // ── Test 5: run on a system JSON missing "bodies" exits non-zero ───────
    {
        std::ofstream missingBodies("/tmp/test_cli_no_bodies.json");
        missingBodies << R"({"name": "test", "epoch": "2024-01-01 00:00:00 TDB"})";
    }
    ret = std::system("./bin/orbit-sim run "
                      "--system /tmp/test_cli_no_bodies.json "
                      "--steps 5 --dt 60 "
                      "--output /tmp/test_cli_no_bodies_out.csv "
                      "> /dev/null 2>&1");
    if (ret == 0)
    {
        std::cerr << "FAIL: run on a system missing \"bodies\" should exit non-zero\n";
        return 1;
    }
    std::cout << "PASS: run on a system missing \"bodies\" exits non-zero\n";
    std::filesystem::remove("/tmp/test_cli_no_bodies.json");
    std::filesystem::remove("/tmp/test_cli_no_bodies_out.csv");

    // ── Test 6: --stride 0 exits cleanly instead of SIGFPE ─────────────────
    ret = std::system("./bin/orbit-sim run "
                      "--system ../systems/earth_moon.json "
                      "--steps 10 --dt 60 --stride 0 > /dev/null 2>&1");
    if (ret == 0 || WIFSIGNALED(ret))
    {
        std::cerr << "FAIL: --stride 0 should exit non-zero without a signal, got " << ret << "\n";
        return 1;
    }
    std::cout << "PASS: --stride 0 exits cleanly (no crash)\n";

    // ── Test 7: malformed numeric args exit cleanly instead of crashing ────
    ret = std::system("./bin/orbit-sim run "
                      "--system ../systems/earth_moon.json "
                      "--dt abc > /dev/null 2>&1");
    if (ret == 0 || WIFSIGNALED(ret))
    {
        std::cerr << "FAIL: --dt abc should exit non-zero without a signal, got " << ret << "\n";
        return 1;
    }
    std::cout << "PASS: --dt abc exits cleanly (no crash)\n";

    ret = std::system("./bin/orbit-sim run "
                      "--system ../systems/earth_moon.json "
                      "--steps 99999999999999999999 > /dev/null 2>&1");
    if (ret == 0 || WIFSIGNALED(ret))
    {
        std::cerr << "FAIL: out-of-range --steps should exit non-zero without a signal, got " << ret
                  << "\n";
        return 1;
    }
    std::cout << "PASS: out-of-range --steps exits cleanly (no crash)\n";

    // ── Test 8: explicit --steps -1 / --dt -1 are rejected, not defaulted ──
    // Regression test: -1 used to double as the "unspecified" sentinel, so
    // an explicit "--steps -1" silently fell back to the default instead of
    // being validated like any other negative value.
    ret = std::system("./bin/orbit-sim run "
                      "--system ../systems/earth_moon.json "
                      "--steps -1 --dt 60 --output /tmp/test_cli_neg1.csv "
                      "> /dev/null 2>&1");
    if (ret == 0)
    {
        std::cerr << "FAIL: --steps -1 should be rejected, not silently defaulted\n";
        return 1;
    }
    std::cout << "PASS: --steps -1 is rejected instead of silently defaulted\n";

    ret = std::system("./bin/orbit-sim run "
                      "--system ../systems/earth_moon.json "
                      "--steps 10 --dt -1 --output /tmp/test_cli_neg1.csv "
                      "> /dev/null 2>&1");
    if (ret == 0)
    {
        std::cerr << "FAIL: --dt -1 should be rejected, not silently defaulted\n";
        return 1;
    }
    std::cout << "PASS: --dt -1 is rejected instead of silently defaulted\n";
    std::filesystem::remove("/tmp/test_cli_neg1.csv");

    std::cout << "PASS: all CLI smoke tests passed\n";
    return 0;
}