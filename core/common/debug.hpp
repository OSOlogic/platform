/**
 * @file debug.hpp
 * @author Diego Arcos Sapena
 * @brief Debug and trace macros for conditional logging
 * @version a-1.0.0
 * @date 2026/01/29
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 *
 * Usage:
 *   - Compile with `make`       -> No debug, no trace output
 *   - Compile with `make debug` -> Debug output enabled (timing, technical info)
 *   - Compile with `make trace` -> Trace output enabled (data flow traceability)
 *
 * DEBUG_STREAM: Timing and technical info (only with make debug)
 * TRACE_STREAM: Data flow traceability with high resolution (only with make trace)
 *
 * Examples:
 *   DEBUG_STREAM("Cycle time: " << elapsed << " us.");
 *   TRACE_STREAM("[HW->MEM] Module " << id << " bit " << addr << ": " << old_val << " -> " <<
 * new_val);
 */

#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <iostream>
#include <sstream>

#include "errors.hpp"
#include "utils.hpp"

// DEBUG macros - only compiled with -DDEBUG (make debug)
#ifdef DEBUG
#define DEBUG_PRINT(msg)                                  \
  do {                                                    \
    std::stringstream _ss;                                \
    _ss << "[" << get_timestamp() << "] [DEBUG] " << msg; \
    log_msg(_ss.str());                                   \
  } while (0)

#define DEBUG_STREAM(stream_expr)                                 \
  do {                                                            \
    std::stringstream _ss;                                        \
    _ss << "[" << get_timestamp() << "] [DEBUG] " << stream_expr; \
    log_msg(_ss.str());                                           \
  } while (0)
#else
#define DEBUG_PRINT(msg) ((void)0)
#define DEBUG_STREAM(stream_expr) ((void)0)
#endif

// TRACE macros - only compiled with -DTRACE (make trace)
#ifdef TRACE
#define TRACE_STREAM(stream_expr)                                 \
  do {                                                            \
    std::stringstream _ss;                                        \
    _ss << "[" << get_timestamp() << "] [TRACE] " << stream_expr; \
    log_msg(_ss.str());                                           \
  } while (0)
#else
#define TRACE_STREAM(stream_expr) ((void)0)
#endif

#endif  // DEBUG_HPP
