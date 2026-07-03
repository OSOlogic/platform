/**
 * @file errors.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Hardware errors definitions (code)
 * @version a-1.0.0
 * @date 2024/08/21
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "errors.hpp"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <string>

#include "async_logger.hpp"
#include "utils.hpp"

// log_msg: non-blocking enqueue. The AsyncLogger thread does the actual I/O.
void log_msg(const std::string &log_entry) {
  AsyncLogger::enqueue(log_entry);
}

// Variables for error logging rate-limiting
static std::mutex s_log_mutex;
static std::map<std::string, std::chrono::steady_clock::time_point> s_last_log_time;
constexpr int LOG_COOLDOWN_MS = 2000;

// Function to log errors to both console and log file (with rate-limiting)
void log_error(const std::string &function_name, const std::string &message,
               PlcErrorCodes error_code) {
  // Create a key combining the origin and error type
  std::string error_key = function_name + "_" + std::to_string(static_cast<int32_t>(error_code));

  {
    std::lock_guard<std::mutex> lock(s_log_mutex);
    auto now = std::chrono::steady_clock::now();
    
    auto it = s_last_log_time.find(error_key);
    if (it != s_last_log_time.end()) {
        auto time_since_last_log = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (time_since_last_log < LOG_COOLDOWN_MS) {
            return; // Block print: too soon for the same function and error code
        }
    }
    // Update the record
    s_last_log_time[error_key] = now;
  }

  std::string timestamp = get_timestamp();

  std::stringstream ss;
  ss << "[" << timestamp << "] Error in " << function_name << ": " << message << " (Error code: 0x"
     << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
     << static_cast<uint16_t>(std::abs(static_cast<int32_t>(error_code))) << ")";

  log_msg(ss.str());
}

inline bool operator<(PlcErrorCodes lhs, PlcErrorCodes rhs) {
  return static_cast<int32_t>(lhs) < static_cast<int32_t>(rhs);
}
