/********************
 * Author: Sinan Demir
 * File: validate.h
 * Purpose: Declaration of system JSON validator
 * Date: 11/19/2025
 *********************/

#ifndef ORBIT_SIM_VALIDATE_H
#define ORBIT_SIM_VALIDATE_H

#include "body.h"
#include "json_loader.h"
#include <iostream>
#include <stdexcept>
#include <string>

/**
 * @brief Validate a JSON system file.
 * Loads the system using loadSystemFromJSON() and prints summary.
 *
 * @param path Path to JSON file
 * @return true if valid, false if invalid
 */
bool validateSystemFile(const std::string &path);

#endif
