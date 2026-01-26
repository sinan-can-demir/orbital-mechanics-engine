/********************
 * Author: Sinan Demir
 * File: validate.cpp
 * Purpose: Implementation of system JSON validator
 *********************/

#include "validate.h"
// for glm::to_string install the GLM_GTX_string_cast extension/library

/********************
 * validateSystemFile
 * @brief: Validate a JSON system file.
 * Loads the system using loadSystemFromJSON() and prints summary.
 *
 * @param path Path to JSON file
 * @return true if valid, false if invalid
 *********************/
bool validateSystemFile(const std::string &path) {
  try {
    auto bodies = loadSystemFromJSON(path);

    if (bodies.empty()) {
      std::cout << "⚠️  System loaded but contains 0 bodies.\n";
      return false;
    }

    std::cout << "✅ System is valid: " << bodies.size() << " bodies\n";

    for (const auto &b : bodies) {
      std::cout << " - " << b.name << " | mass = " << b.mass

                << " | pos = (" << b.position.x() << ", " << b.position.y()
                << ",  " << b.position.z() << ")"

                << " | vel = (" << b.velocity.x() << ", " << b.velocity.y()
                << ", " << b.velocity.z() << ")"

                << "\n";

      if (b.mass <= 0.0) {
        std::cout << "   ⚠️  Warning: non-positive mass.\n";
      }
    }

    return true;
  } catch (const std::exception &e) {
    std::cerr << "❌ Validation failed: " << e.what() << "\n";
    return false;
  } catch (...) {
    std::cerr << "❌ Validation failed: unknown error\n";
    return false;
  }
}
