/********************
 * Author: Sinan Demir
 * File: horizons.cpp
 * Date: 11/19/2025
 * Purpose:
 *    Implementation of NASA/JPL HORIZONS File API wrapper using libcurl.
 *********************/

#include "horizons.h"

#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// libcurl write callback: append received data to std::string buffer
static size_t writeToStringCallback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
  size_t total = size * nmemb;
  auto *buffer = static_cast<std::string *>(userp);
  buffer->append(static_cast<char *>(contents), total);
  return total;
}

/**************************
 * fetchHorizonsEphemeris
 * @brief: Calls the HORIZONS File API and writes raw ephemeris to a file.
 * @param opts       - HORIZONS request options
 * @param outputPath - File where the ephemeris text (from JSON "result") is
 * saved
 * @param verbose    - Enable verbose output
 * @return true on success, false on failure
 **************************/
bool fetchHorizonsEphemeris(const HorizonsFetchOptions &opts,
                            const std::string &outputPath, bool verbose) {
  const std::string baseUrl = "https://ssd-api.jpl.nasa.gov/horizons_file.api";

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "❌ Failed to initialize libcurl\n";
    return false;
  }

  // Fix dates: add " 00:00" if user provided only yyyy-mm-dd
  auto fixDate = [](const std::string &s) {
    if (s.find(':') == std::string::npos)
      return s + " 00:00";
    return s;
  };

  std::string fixed_start = fixDate(opts.start_time);
  std::string fixed_stop = fixDate(opts.stop_time);

  // Encode parameters EXCEPT CENTER (@ must NOT be escaped)
  char *esc_cmd = curl_easy_escape(curl, opts.command.c_str(), 0);
  char *esc_start = curl_easy_escape(curl, fixed_start.c_str(), 0);
  char *esc_stop = curl_easy_escape(curl, fixed_stop.c_str(), 0);
  char *esc_step = curl_easy_escape(curl, opts.step_size.c_str(), 0);

  if (!esc_cmd || !esc_start || !esc_stop || !esc_step) {
    std::cerr << "❌ curl_easy_escape returned NULL\n";
    curl_easy_cleanup(curl);
    return false;
  }

  // Build GET URL
  std::ostringstream url;
  url << baseUrl << "?format=json"
      << "&COMMAND='" << esc_cmd << "'"
      << "&CENTER='" << opts.center << "'" // NOT escaped
      << "&EPHEM_TYPE=VECTORS"
      << "&START_TIME='" << esc_start << "'"
      << "&STOP_TIME='" << esc_stop << "'"
      << "&STEP_SIZE='" << esc_step << "'"
      << "&MAKE_EPHEM=YES";

  curl_free(esc_cmd);
  curl_free(esc_start);
  curl_free(esc_stop);
  curl_free(esc_step);

  std::string response;

  // curl settings
  curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToStringCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "orbit-sim/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // ────────────────────────────────────────────
  // VERBOSE MODE: print request URL
  // ────────────────────────────────────────────
  if (verbose) {
    std::cout << "\n[VERBOSE] Requesting Horizons...\n";
    std::cout << "[VERBOSE] GET URL:\n" << url.str() << "\n\n";
  }

  // Perform GET request
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "❌ curl_easy_perform failed: " << curl_easy_strerror(res)
              << "\n";
    curl_easy_cleanup(curl);
    return false;
  }

  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_easy_cleanup(curl);

  // VERBOSE: show HTTP status
  if (verbose) {
    std::cout << "[VERBOSE] HTTP status: " << httpCode << "\n";
  }

  if (httpCode != 200) {
    std::cerr << "❌ HORIZONS HTTP error code: " << httpCode << "\n";
    return false;
  }

  // ────────────────────────────────────────────
  // VERBOSE: Save raw JSON body
  // ────────────────────────────────────────────
  if (verbose) {
    std::ofstream dbg("horizons_debug.json");
    dbg << response;
    dbg.close();
    std::cout << "[VERBOSE] Raw response saved to horizons_debug.json\n";
  }

  // Parse JSON response
  json j;
  try {
    j = json::parse(response);
  } catch (const std::exception &e) {
    std::cerr << "❌ Failed to parse HORIZONS JSON: " << e.what() << "\n";

    if (verbose) {
      std::cerr << "[VERBOSE] First 300 characters of reply:\n";
      std::cerr << response.substr(0, 300) << "\n";
    }
    return false;
  }

  if (j.contains("error")) {
    std::cerr << "❌ HORIZONS API returned error: "
              << j["error"].get<std::string>() << "\n";
    return false;
  }

  if (!j.contains("result")) {
    std::cerr << "❌ HORIZONS JSON missing 'result' field\n";
    return false;
  }

  std::string ephemText = j["result"].get<std::string>();

  // Write output
  std::ofstream out(outputPath);
  if (!out) {
    std::cerr << "❌ Could not open output file: " << outputPath << "\n";
    return false;
  }

  out << ephemText;
  out.close();

  std::cout << "✅ HORIZONS ephemeris saved to: " << outputPath << "\n";
  return true;
}

/*******************
 * fetchHorizonsEphemerisPOST
 * @brief: Calls the HORIZONS File API using HTTP POST and writes raw ephemer
 * @param opts       - HORIZONS request options
 * @param outputPath - File where the ephemeris text (from JSON "result")
 * @param verbose    - Enable verbose output
 * @return true on success, false on failure
 *******************/
/****************************************************
 * POST-based Horizons fetch (recommended by NASA)
 ****************************************************/
bool fetchHorizonsEphemerisPOST(const HorizonsFetchOptions &opts,
                                const std::string &outputPath, bool verbose) {
  //   const std::string url = "https://ssd-api.jpl.nasa.gov/horizons_file.api";
  //   // GET API
  const std::string url =
      "https://ssd.jpl.nasa.gov/api/horizons.api"; // POST API

  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "❌ Failed to initialize libcurl\n";
    return false;
  }

  // Fix YYYY-MM-DD → YYYY-MM-DD 00:00
  auto fixDate = [](const std::string &s) {
    if (s.find(':') == std::string::npos)
      return s + " 00:00";
    return s;
  };

  std::string start = fixDate(opts.start_time);
  std::string stop = fixDate(opts.stop_time);

  // Build POST body (NASA requires form-urlencoded)
  std::ostringstream body;

  body << "format=json"
       << "&EPHEM_TYPE=VECTORS"
       << "&COMMAND='" << opts.command << "'"
       << "&CENTER='" << opts.center << "'"
       << "&START_TIME='" << start << "'"
       << "&STOP_TIME='" << stop << "'"
       << "&STEP_SIZE='" << opts.step_size << "'"
       << "&MAKE_EPHEM=YES";

  std::string postData = body.str();

  if (verbose) {
    std::cout << "\n[VERBOSE] POST → " << url << "\n";
    std::cout << "[VERBOSE] POST body:\n" << postData << "\n\n";
  }

  std::string response;

  // libcurl settings
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.size());

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToStringCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  curl_easy_setopt(curl, CURLOPT_USERAGENT, "orbit-sim/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    std::cerr << "❌ curl_easy_perform failed: " << curl_easy_strerror(res)
              << "\n";
    curl_easy_cleanup(curl);
    return false;
  }

  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  if (verbose) {
    std::cout << "[VERBOSE] HTTP status: " << httpCode << "\n";
  }

  curl_easy_cleanup(curl);

  if (httpCode != 200) {
    std::cerr << "❌ Horizons returned HTTP " << httpCode << "\n";
    return false;
  }

  // Optional debugging dump
  if (verbose) {
    std::ofstream dbg("horizons_debug.json");
    dbg << response;
    std::cout << "[VERBOSE] Raw response saved to horizons_debug.json\n";
  }

  // Parse JSON from the API
  json j;
  try {
    j = json::parse(response);
  } catch (const std::exception &e) {
    std::cerr << "❌ Failed to parse JSON: " << e.what() << "\n";
    return false;
  }

  if (!j.contains("result")) {
    std::cerr << "❌ JSON missing 'result' field\n";
    return false;
  }

  std::string text = j["result"].get<std::string>();

  // Save ephemeris text
  std::ofstream out(outputPath);
  if (!out) {
    std::cerr << "❌ Couldn't open output file: " << outputPath << "\n";
    return false;
  }

  out << text;

  std::cout << "✅ POST Horizons ephemeris saved to: " << outputPath << "\n";
  return true;
}
