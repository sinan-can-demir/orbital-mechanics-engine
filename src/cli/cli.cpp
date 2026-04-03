/****************
 * Author: Sinan Demir
 * File: cli.cpp
 * Date: 11/19/2025
 * Purpose: Implementation file for CLI parser.
 *****************/

#include "cli.h"

/**********************
 * parseCLI
 * @brief: Parses command-line arguments into a CLIOptions struct.
 * @param: argc - argument count
 * @param: argv - argument vector
 * @return: CLIOptions struct with parsed values
 * @exception: exits on invalid usage
 **********************/
CLIOptions parseCLI(int argc, char** argv)
{
    CLIOptions opt;

    // ----------------------------------------------
    // 0. No arguments → show global help
    // ----------------------------------------------
    if (argc == 1)
    {
        opt.command = "help";
        return opt;
    }

    // ----------------------------------------------
    // 1. Detect "help" or "--help" before anything else
    // ----------------------------------------------
    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h")
    {
        opt.command = "help";

        // If user ran: orbit-sim help run
        if (argc >= 3)
            opt.systemFile = argv[2]; // store subcommand here

        return opt;
    }

    // ----------------------------------------------
    // 2. Normal command (run, list, info, validate, fetch…)
    // ----------------------------------------------
    opt.command = cmd;

    // ----------------------------------------------
    // 3. Parse options
    // ----------------------------------------------
    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];

        // ----- Simulation Options -----
        if (a == "--system" && i + 1 < argc)
        {
            opt.systemFile = argv[++i];
        }
        else if (a == "--steps" && i + 1 < argc)
        {
            opt.steps = std::stoi(argv[++i]);
        }
        else if (a == "--dt" && i + 1 < argc)
        {
            opt.dt = std::stod(argv[++i]);
        }
        else if (a == "--stride" && i + 1 < argc)
        {
            opt.stride = std::stoi(argv[++i]);
        }
        else if (a == "--output" && i + 1 < argc)
        {
            opt.output = argv[++i];
        }
        else if (a == "--integrator" && i + 1 < argc)
        {
            opt.integrator = argv[++i];
        }

        // ----- FETCH Options -----
        else if (a == "--body" && i + 1 < argc)
        {
            opt.fetchBody = argv[++i];
        }
        else if (a == "--center" && i + 1 < argc)
        {
            opt.fetchCenter = argv[++i];
        }
        else if (a == "--start" && i + 1 < argc)
        {
            opt.fetchStart = argv[++i];
        }
        else if (a == "--stop" && i + 1 < argc)
        {
            opt.fetchStop = argv[++i];
        }
        else if (a == "--step" && i + 1 < argc)
        {
            opt.fetchStep = argv[++i];
        }
        else if (a == "--verbose")
        {
            opt.verbose = true;
        }
        else if (a == "--post")
        {
            opt.usePost = true;
        }
        else if (a == "--normalize")
        {
            opt.normalize = true;
        }
        // ----- Unknown Option -----
        else
        {
            std::cerr << "Unknown option: " << a << "\n";
            exit(1);
        }
    }

    return opt;
}

void printGlobalHelp()
{
    std::cout << "Orbit-Sim — Command Line Reference\n\n"
              << "Usage:\n"
              << "  orbit-sim <command> [options]\n\n"
              << "Commands:\n"
              << "  help                     Show this help message\n"
              << "  list                     List available system JSON files\n"
              << "  info     --system FILE   Show information about a system\n"
              << "  validate --system FILE   Validate a system JSON file\n"
              << "  run      --system FILE --steps N --dt T\n"
              << "                           Run a simulation\n"
              << "  fetch    [options]       Fetch ephemeris from NASA Horizons\n\n"
              << "For command-specific help:\n"
              << "  orbit-sim <command> --help\n\n";
}

void printCommandHelp(const std::string& cmd)
{
    if (cmd == "run")
    {
        std::cout << "  --system FILE              Path to system JSON\n"
                  << "  --steps N                  Number of integration steps\n"
                  << "  --dt T                     Timestep in seconds\n"
                  << "  --stride N                 Write one CSV row every N steps (default: 1)\n"
                  << "  --integrator rk4|leapfrog  Integration method (default: rk4)\n"
                  << "  --normalize                Shift system so COM=0 and net momentum=0\n\n"
                  << "Example:\n"
                  << "  orbit-sim run --system systems/earth_moon.json"
                     " --steps 5000000 --dt 60 --stride 1440\n";
    }

    if (cmd == "info")
    {
        std::cout << "orbit-sim info — Print system info\n\n"
                  << "Options:\n"
                  << "  --system FILE\n\n"
                  << "Example:\n"
                  << "  orbit-sim info --system systems/earth_moon.json\n";
        return;
    }

    if (cmd == "validate")
    {
        std::cout << "orbit-sim validate — Validate JSON system file\n\n"
                  << "Options:\n"
                  << "  --system FILE\n\n"
                  << "Example:\n"
                  << "  orbit-sim validate --system systems/earth_moon.json\n";
        return;
    }

    if (cmd == "list")
    {
        std::cout << "orbit-sim list — List available systems\n\n"
                  << "This command takes no options.\n\n";
        return;
    }

    if (cmd == "fetch")
    {
        std::cout << "orbit-sim fetch — Fetch ephemeris from NASA Horizons\n\n"
                  << "Options:\n"
                  << "  --body ID          Horizons command ID (e.g., 399 for Earth)\n"
                  << "  --center ID        Center reference (e.g., @0 for solar system "
                     "barycenter)\n"
                  << "  --start YYYY-MM-DD\n"
                  << "  --stop  YYYY-MM-DD\n"
                  << "  --step \"6 h\"       Step size\n"
                  << "  --output FILE      Where to save results\n\n";
        return;
    }

    std::cout << "No help available for command: " << cmd << "\n";
}
