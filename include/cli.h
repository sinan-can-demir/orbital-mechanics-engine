/****************
 * Author: Sinan Demir
 * File: cli.h
 * Date: 11/19/2025
 * Purpose: Header file for CLI module.
 *****************/

#ifndef ORBIT_SIM_CLI_H
#define ORBIT_SIM_CLI_H

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include "simulation.h"

/***********************
 * struct CLIOptions
 * @brief: Holds all command-line arguments for orbit-sim.
 *
 * Supported commands:
 *    - run
 *    - list
 *    - info
 *    - fetch
 *   - validate
 * @note: Additional fields can be added as needed.
 ***********************/
struct CLIOptions
{
    std::string command;
    std::string systemFile;
    std::string integrator;

    // Unset (nullopt) means "not passed on the command line, use the
    // command's default" — distinct from any value a user could actually
    // type, so an explicit "--steps -1" is validated like any other
    // negative value instead of colliding with the "unspecified" sentinel.
    std::optional<int> steps;
    std::optional<int> stride;
    std::optional<double> dt;

    // fetch
    std::string fetchBody;
    std::string fetchCenter;
    std::string fetchStart;
    std::string fetchStop;
    std::string fetchStep;
    std::string output;

    // build-system
    std::string buildBodies;
    std::string buildEpoch;
    std::string buildCenter;
    bool buildRun = false;

    bool usePost = true;
    bool verbose = false;
    bool normalize = false;
    bool use_gr = false;
};

CLIOptions parseCLI(int argc, char** argv);
void printGlobalHelp();
void printCommandHelp(const std::string& command);

#endif // ORBIT_SIM_CLI_H
