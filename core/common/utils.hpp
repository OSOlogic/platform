/**
 * @file utils.hpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Commonly use functions (header)
 * @version a-1.0.0
 * @date 2024/08/28
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <inttypes.h>

#include <string>
#include <vector>

#include "errors.hpp"

/**
 * @brief Get the current timestamp as a string.
 * @return {std::string} Current timestamp.
 */
std::string get_timestamp();

/**
 * @brief Split string in items separated by delimiter.
 * @param[in] input String to split.
 * @param[in] delimiter Delimiter.
 * @return Vector of std::string pieces.
 */
std::vector<std::string> split(const std::string& input, const std::string& delimiter);

/**
 * @brief Remove back and front spaces from string.
 * @param[in] input String to trim.
 * @return {std::string} Trimmed string.
 */
std::string trim(const std::string& input);

struct DbConfig {
  std::string host;
  std::string user;
  std::string password;
  std::string database;
};

/**
 * @brief Load database configuration from central config.json
 * @param[out] config Reference to store the loaded database configuration.
 * @return {PlcErrorCodes} PLC_SUCCESS on success, or an error code.
 */
PlcErrorCodes load_db_config(DbConfig& config);
