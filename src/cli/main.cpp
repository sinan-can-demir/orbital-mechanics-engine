/********************
 * Author: Sinan Demir
 * File: src/climain.cpp
 * Date: 04/02/2026
 * Purpose:
 *    Entry point for orbit-sim CLI application.
 *    Supports:
 *      - Running N-body simulations from JSON system files
 *      - Printing basic system info
 *      - Listing available system definitions
 *      - Fetching raw ephemeris from NASA HORIZONS
 *********************/

#include "main.h"

/********************
 * printSystemInfo
 * @brief: Prints details of bodies loaded from JSON
 *********************/
void printSystemInfo(const std::string& path)
{
    try
    {
        auto bodies = loadSystemFromJSON(path);
        std::cout << "System file: " << path << "\n";
        std::cout << "Bodies:\n";

        for (const auto& b : bodies)
        {
            std::cout << " - " << b.name << " | mass=" << b.mass << " | pos=(" << b.position << ")"
                      << " | vel=(" << b.velocity << ")\n";
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Error loading system: " << e.what() << "\n";
    }
}

/********************
 * listSystems
 * @brief: Lists JSON files inside the "systems" directory.
 *********************/
void listSystems(const std::string& dir = "systems")
{
    std::cout << "Available systems in \"" << dir << "\":\n";

    if (!std::filesystem::exists(dir))
    {
        std::cout << "(No systems directory found)\n";
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() == ".json")
        {
            std::cout << " - " << entry.path().string() << "\n";
        }
    }
}

/********************
 * main
 * @brief: Parses CLI arguments and performs actions.
 *********************/
int main(int argc, char** argv)
{
    CLIOptions opt = parseCLI(argc, argv);

    // ----- HELP -----
    if (opt.command == "help")
    {
        if (!opt.systemFile.empty())
        {
            printCommandHelp(opt.systemFile);
        }
        else
        {
            printGlobalHelp();
        }
        return 0;
    }

    // ----- LIST -----
    if (opt.command == "list")
    {
        listSystems();
        return 0;
    }

    // ----- INFO -----
    if (opt.command == "info")
    {
        if (opt.systemFile.empty())
        {
            std::cerr << "❌ Must specify --system <file.json>\n";
            return 1;
        }
        printSystemInfo(opt.systemFile);
        return 0;
    }

    // ----- VALIDATE -----
    if (opt.command == "validate")
    {
        if (opt.systemFile.empty())
        {
            std::cerr << "❌ Must specify --system <file.json>\n";
            return 1;
        }
        bool ok = validateSystemFile(opt.systemFile);
        return ok ? 0 : 1;
    }

    // ----- FETCH (NASA HORIZONS) -----
    if (opt.command == "fetch")
    {
        if (opt.fetchBody.empty())
        {
            std::cerr << "❌ Must specify --body <ID or NAME>\n";
            return 1;
        }
        if (opt.fetchStart.empty() || opt.fetchStop.empty())
        {
            std::cerr << "❌ Must specify --start <date> and --stop <date>\n";
            return 1;
        }
        if (opt.output.empty())
        {
            std::cerr << "❌ Must specify --output <file>\n";
            return 1;
        }

        HorizonsFetchOptions hopt;
        hopt.command = opt.fetchBody;
        hopt.center = opt.fetchCenter.empty() ? "@0" : opt.fetchCenter;
        hopt.start_time = opt.fetchStart;
        hopt.stop_time = opt.fetchStop;
        hopt.step_size = opt.fetchStep.empty() ? "1 d" : opt.fetchStep;

        std::cout << "Fetching NASA JPL Horizons ephemeris:\n"
                  << " - Body:   " << hopt.command << "\n"
                  << " - Center: " << hopt.center << "\n"
                  << " - Start:  " << hopt.start_time << "\n"
                  << " - Stop:   " << hopt.stop_time << "\n"
                  << " - Step:   " << hopt.step_size << "\n"
                  << " - Output: " << opt.output << "\n";

        bool ok = false;

        if (opt.usePost)
        {
            ok = fetchHorizonsEphemerisPOST(hopt, opt.output, opt.verbose);
        }
        else
        {
            ok = fetchHorizonsEphemeris(hopt, opt.output, opt.verbose);
        }
        return ok ? 0 : 1;
    }

    // ----- RUN SIMULATION -----
    if (opt.command == "run")
    {
        if (opt.systemFile.empty())
        {
            std::cerr << "❌ Must specify --system <file.json>\n";
            return 1;
        }

        try
        {
            // Load system from JSON
            auto bodies = loadSystemFromJSON(opt.systemFile);

            // Normalize to barycenter if requested
            if (opt.normalize)
            {
                std::cout << " - Normalizing to barycenter frame...\n";
                physics::normalizeToBarycenter(bodies);
            }

            // Determine simulation parameters
            int steps = (opt.steps > 0 ? opt.steps : 8766);
            double dt = (opt.dt > 0 ? opt.dt : 3600.0);

            // Default output path
            std::string outPath = opt.output.empty() ? "build/orbit_three_body.csv" : opt.output;

            // Resolve integrator
            Integrator integrator = Integrator::RK4;
            if (opt.integrator == "leapfrog")
            {
                integrator = Integrator::Leapfrog;
            }
            else if (!opt.integrator.empty() && opt.integrator != "rk4")
            {
                std::cerr << "❌ Unknown integrator: " << opt.integrator
                          << ". Valid options: rk4, leapfrog\n";
                return 1;
            }

            // In the run block, alongside steps and dt:
            int stride = (opt.stride > 0 ? opt.stride : 1);

            std::cout << "Running simulation:\n"
                      << " - System:     " << opt.systemFile << "\n"
                      << " - Steps:      " << steps << "\n"
                      << " - dt:         " << dt << " seconds\n"
                      << " - Stride:     " << stride << "\n"
                      << " - Output:     " << outPath << "\n"
                      << " - Integrator: "
                      << (integrator == Integrator::Leapfrog ? "Leapfrog" : "RK4") << "\n";

            runSimulation(bodies, steps, dt, outPath, integrator, stride);
        }
        catch (const std::exception& e)
        {
            std::cerr << "❌ Simulation failed: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

    // ----- UNKNOWN COMMAND -----
    std::cerr << "❌ Unknown command: " << opt.command << "\n";
    std::cerr << "Valid commands are:\n"
              << "  orbit-sim help\n"
              << "  orbit-sim list\n"
              << "  orbit-sim info     --system <file.json>\n"
              << "  orbit-sim validate --system <file.json>\n"
              << "  orbit-sim run      --system <file.json> --steps N --dt T\n"
              << "  orbit-sim fetch    --body <ID> --start <date> --stop <date> "
                 "--output <file>\n";

    return 1;
}