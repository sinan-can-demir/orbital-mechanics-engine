/********************
 * Author: Sinan Demir
 * File: horizons.h
 * Date: 11/19/2025
 * Purpose:
 *    Thin wrapper around NASA/JPL HORIZONS File API using libcurl.
 *    Fetches raw ephemeris output (as text inside JSON "result")
 *    and saves it to a local file for further processing.
 *********************/

#ifndef ORBIT_SIM_HORIZONS_H
#define ORBIT_SIM_HORIZONS_H

#include <string>

/********************
 * HorizonsFetchOptions
 * @brief: Options for a single HORIZONS fetch operation.
 *
 *  - command: OBJECT identifier (e.g. "399" for Earth, "499" for Mars).
 *  - center : Reference center (e.g. "0" for Solar System barycenter).
 *  - start_time, stop_time, step_size: Ephemeris time span and step.
 *********************/
struct HorizonsFetchOptions {
  std::string command;    ///< Target body (NAIF ID or name string)
  std::string center;     ///< Center body (NAIF ID or named, e.g. "0")
  std::string start_time; ///< Start time, e.g. "2025-01-01"
  std::string stop_time;  ///< Stop time, e.g. "2025-01-02"
  std::string step_size;  ///< e.g. "1 d", "1 h"
};

/********************
 * fetchHorizonsEphemeris
 * @brief: Calls the HORIZONS File API and writes raw ephemeris to a file.
 * @param fetchOpt   - HORIZONS request options
 * @param outputPath - File where the ephemeris text (from JSON "result") is
 * saved
 * @param verbose    - Enable verbose output
 * @return true on success, false on failure
 *********************/
bool fetchHorizonsEphemeris(const HorizonsFetchOptions &opts,
                            const std::string &outputPath, bool verbose);
bool fetchHorizonsEphemerisPOST(const HorizonsFetchOptions &opts,
                                const std::string &outputPath, bool verbose);

#endif // ORBIT_SIM_HORIZONS_H
