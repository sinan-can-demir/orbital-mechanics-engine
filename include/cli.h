/****************
 * Author: Sinan Demir
 * File: cli.h
 * Date: 11/19/2025
 * Purpose: Header file for CLI module.
 *****************/

#ifndef ORBIT_SIM_CLI_H
#define ORBIT_SIM_CLI_H

#include <iostream>
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

    int steps = 0;
    int stride= 1;
    double dt = 0;

    // fetch
    std::string fetchBody;
    std::string fetchCenter;
    std::string fetchStart;
    std::string fetchStop;
    std::string fetchStep;
    std::string output;

    bool usePost = false;
    bool verbose = false;
    bool normalize = false;
};

CLIOptions parseCLI(int argc, char** argv);
void printGlobalHelp();
void printCommandHelp(const std::string& command);

#endif // ORBIT_SIM_CLI_H
