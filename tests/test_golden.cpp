// tests/test_golden.cpp
// Golden-value regression tests. They pin the numerical output of every integrator and
// the CSV export, so that behavior-preserving refactors (see docs/duplication-audit.md,
// phases 4-5) can be proven to preserve behavior.
//
//   test_golden             compare against tests/golden/ (runs under ctest)
//   test_golden --record    regenerate tests/golden/ from the current code
//   GOLDEN_STRICT=1 test_golden
//                           require bit-identical values / byte-identical CSV text.
//                           Use this locally while refactoring; the default tolerances
//                           keep CI portable across compilers.
//
// Only re-record when a change is *meant* to alter results, and say so in the PR.
#include "json_loader.h"
#include "test_helpers.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace
{
namespace fs = std::filesystem;
using Values = std::map<std::string, double>;

const fs::path kGoldenDir = "tests/golden";

bool strictMode()
{
    const char* s = std::getenv("GOLDEN_STRICT");
    return s && *s && std::string(s) != "0";
}

// ── Scenario values ──────────────────────────────────────────────────────────
void addBodies(Values& v, const std::string& prefix, const std::vector<CelestialBody>& bodies)
{
    for (const auto& b : bodies)
    {
        const std::string k = prefix + "." + b.name + ".";
        v[k + "x"] = b.position.x();
        v[k + "y"] = b.position.y();
        v[k + "z"] = b.position.z();
        v[k + "vx"] = b.velocity.x();
        v[k + "vy"] = b.velocity.y();
        v[k + "vz"] = b.velocity.z();
    }
}

void addSnapshot(Values& v, const std::string& prefix, const SimulationSnapshot& s)
{
    const auto& c = s.conservation;
    v[prefix + ".step"] = s.step;
    v[prefix + ".time_s"] = s.time_s;
    v[prefix + ".dt_used"] = s.dt_used;
    v[prefix + ".dE"] = c.dE;
    v[prefix + ".dL"] = c.dL;
    v[prefix + ".dP"] = c.dP;
    v[prefix + ".E"] = c.total_energy;
    v[prefix + ".KE"] = c.kinetic_energy;
    v[prefix + ".PE"] = c.potential_energy;
    v[prefix + ".Lmag"] = c.Lmag;
    v[prefix + ".Pmag"] = c.Pmag;
}

void addResult(Values& v, const std::string& prefix, const SimulationResult& r)
{
    v[prefix + ".n_bodies"] = static_cast<double>(r.body_names.size());
    v[prefix + ".n_snapshots"] = static_cast<double>(r.snapshots.size());
    v[prefix + ".dt"] = r.dt;
    v[prefix + ".stride"] = r.stride;
    v[prefix + ".is_adaptive"] = r.is_adaptive ? 1.0 : 0.0;
    for (std::size_t i = 0; i < r.body_masses.size(); ++i)
        v[prefix + ".mass." + r.body_names[i]] = r.body_masses[i];
    if (!r.snapshots.empty())
    {
        addSnapshot(v, prefix + ".first", r.snapshots.front());
        addSnapshot(v, prefix + ".last", r.snapshots.back());
    }
}

struct FixedCase
{
    const char* name;
    const char* system;
    Integrator integrator;
    bool gr;
};

Values computeValues()
{
    Values v;

    const FixedCase cases[] = {
        {"rk4", "systems/earth_moon.json", Integrator::RK4, false},
        {"leapfrog", "systems/earth_moon.json", Integrator::Leapfrog, false},
        {"yoshida4", "systems/earth_moon.json", Integrator::Yoshida4, false},
        {"hermite", "systems/earth_moon.json", Integrator::Hermite, false},
        {"rk4_gr", "systems/mercury.json", Integrator::RK4, true},
    };
    for (const auto& c : cases)
    {
        auto bodies = loadSystemFromJSON(c.system);
        const SimulationResult r = runSimulationCore(bodies, 1000, 3600.0, c.integrator, 100, c.gr);
        addBodies(v, c.name, bodies);
        addResult(v, c.name, r);
    }

    // RK45: 30 days, adaptive. n_snapshots and last.dt_used pin the step-control decisions.
    {
        auto bodies = loadSystemFromJSON("systems/earth_moon.json");
        const SimulationResult r = runSimulationAdaptiveCore(
            bodies, 30.0 * 86400.0, 3600.0, 86400.0, 1e-9, 1e-9, 1.0, 86400.0, false);
        addBodies(v, "rk45", bodies);
        addResult(v, "rk45", r);
    }

    return v;
}

// ── Values file ──────────────────────────────────────────────────────────────
void writeValues(const fs::path& path, const Values& v)
{
    std::ofstream f(path);
    for (const auto& [k, val] : v)
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.17g", val);
        f << k << " " << buf << "\n";
    }
}

bool readValues(const fs::path& path, Values& out)
{
    std::ifstream f(path);
    if (!f)
        return false;
    std::string k;
    double val;
    while (f >> k >> val)
        out[k] = val;
    return true;
}

// Relative 1e-9 with a small absolute floor for values that are numerical noise (dE etc.).
bool goldenClose(double got, double want, bool strict)
{
    if (strict)
        return got == want;
    return std::abs(got - want) <= 1e-9 * std::max(std::abs(got), std::abs(want)) + 1e-10;
}

void compareValues(const Values& got, const Values& want, bool strict)
{
    CHECK(got.size() == want.size(), "golden value count matches (" + std::to_string(got.size()) +
                                         " vs " + std::to_string(want.size()) + ")");
    int bad = 0;
    for (const auto& [k, w] : want)
    {
        auto it = got.find(k);
        if (it == got.end())
        {
            std::cerr << "  missing key: " << k << "\n";
            ++bad;
        }
        else if (!goldenClose(it->second, w, strict))
        {
            char buf[160];
            std::snprintf(buf, sizeof buf, "  %s: got %.17g, golden %.17g", k.c_str(), it->second,
                          w);
            std::cerr << buf << "\n";
            ++bad;
        }
    }
    CHECK(bad == 0, "all golden values match (" + std::to_string(bad) + " mismatches)");
}

// ── CSV files ────────────────────────────────────────────────────────────────
std::vector<std::string> splitCsv(const std::string& line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, ','))
        out.push_back(tok);
    return out;
}

bool parseDouble(const std::string& s, double& out)
{
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0' && !s.empty();
}

// Headers and non-numeric fields must match exactly. The CSV is written at the default
// stream precision (6 significant digits), so numbers are compared at 1e-5 relative
// unless strict mode demands identical text.
bool csvMatches(const fs::path& gotPath, const fs::path& wantPath, bool strict, std::string& why)
{
    std::ifstream got(gotPath), want(wantPath);
    if (!got || !want)
    {
        why = "cannot open " + (got ? wantPath : gotPath).string();
        return false;
    }
    std::string g, w;
    for (int line = 1;; ++line)
    {
        const bool hg = static_cast<bool>(std::getline(got, g));
        const bool hw = static_cast<bool>(std::getline(want, w));
        if (!hg && !hw)
            return true;
        if (hg != hw)
        {
            why = "line count differs at line " + std::to_string(line);
            return false;
        }
        if (g == w)
            continue;
        if (strict)
        {
            why = "line " + std::to_string(line) + " differs:\n  got:    " + g + "\n  golden: " + w;
            return false;
        }
        const auto tg = splitCsv(g), tw = splitCsv(w);
        bool ok = tg.size() == tw.size();
        for (std::size_t i = 0; ok && i < tg.size(); ++i)
        {
            double a, b;
            if (parseDouble(tg[i], a) && parseDouble(tw[i], b))
                ok = std::abs(a - b) <= 1e-5 * std::max(std::abs(a), std::abs(b)) + 1e-12;
            else
                ok = tg[i] == tw[i];
        }
        if (!ok)
        {
            why = "line " + std::to_string(line) + " differs:\n  got:    " + g + "\n  golden: " + w;
            return false;
        }
    }
}

// CSV scenarios, written into `dir` as fixed*.csv / adaptive*.csv.
void writeCsvScenarios(const fs::path& dir)
{
    {
        auto bodies = loadSystemFromJSON("systems/earth_moon.json");
        runSimulation(bodies, 500, 3600.0, (dir / "fixed.csv").string(), Integrator::RK4, 50,
                      false);
    }
    {
        auto bodies = loadSystemFromJSON("systems/earth_moon.json");
        runSimulationAdaptive(bodies, 10.0 * 86400.0, 3600.0, (dir / "adaptive.csv").string(),
                              86400.0, 1e-9, 1e-9, 1.0, 86400.0, false);
    }
}

const char* const kCsvFiles[] = {"fixed.csv", "fixed_conservation.csv", "fixed_eclipse.csv",
                                 "adaptive.csv", "adaptive_conservation.csv"};

} // namespace

int main(int argc, char** argv)
{
    const bool record = argc > 1 && std::string(argv[1]) == "--record";
    const bool strict = strictMode();

    const Values got = computeValues();
    testing::TempDir tmp;
    const fs::path out = tmp.file("");
    writeCsvScenarios(out);

    if (record)
    {
        fs::create_directories(kGoldenDir);
        writeValues(kGoldenDir / "values.txt", got);
        for (const char* f : kCsvFiles)
            fs::copy_file(out / f, kGoldenDir / f, fs::copy_options::overwrite_existing);
        std::cout << "Recorded " << got.size() << " values and " << std::size(kCsvFiles)
                  << " CSV files to " << kGoldenDir << "\n";
        return 0;
    }

    std::cout << (strict ? "GOLDEN_STRICT: bit-identical comparison\n" : "");

    Values want;
    CHECK(readValues(kGoldenDir / "values.txt", want), "golden values file is readable");
    compareValues(got, want, strict);

    for (const char* f : kCsvFiles)
    {
        std::string why;
        const bool ok = csvMatches(out / f, kGoldenDir / f, strict, why);
        if (!ok)
            std::cerr << "  " << f << ": " << why << "\n";
        CHECK(ok, std::string("CSV matches golden: ") + f);
    }

    return testing::finish("golden regression");
}
