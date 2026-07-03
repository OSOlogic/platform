/**
 * @file utils.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Commonly use functions (code)
 * @version a-1.0.0
 * @date 2024/08/28
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "utils.hpp"

#include <inttypes.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>

// Function to get the current timestamp as a string
std::string get_timestamp() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  char buffer[100];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now_time));
  return std::string(buffer);
}

std::vector<std::string> split(const std::string& input, const std::string& delimiter) {
  /* Prepare result variable */
  std::vector<std::string> result;

  /* Calculate char* length */
  const int l = input.length() + 1;

  /* Cast input into char[] */
  char charInput[l];
  std::strcpy(charInput, input.c_str());

  /* Cast delimiter into char* */
  const char* charDelimiter = delimiter.c_str();

  /* Divide input in pieces separated by delimiter */
  char* pointer = strtok(charInput, charDelimiter);

  /* Iterate pieces until pointer is null */
  while (pointer != NULL) {
    /* Save piece in result vector*/
    result.push_back(std::string(pointer));

    /* Get next piece */
    pointer = strtok(NULL, charDelimiter);
  }

  return result;
}

#include <fstream>

std::string trim(const std::string& input) {
  /* avoid destroying input string */
  std::string aux = input;

  /* trim */
  aux.erase(std::remove_if(aux.begin(), aux.end(), ::isspace), aux.end());

  return aux;
}

PlcErrorCodes load_db_config(DbConfig& config) {
  // Reset config to avoid using previous values
  config.host = "";
  config.user = "";
  config.password = "";
  config.database = "";

  std::vector<std::string> paths = {"../../../../config/config.json",
                                    "../../../config/config.json",
                                    "../../config/config.json",
                                    "../config/config.json",
                                    "config/config.json",
                                    "/opt/PLC_OsoLogic/config/config.json"};

  bool file_found = false;
  for (const auto& path : paths) {
    std::ifstream file(path);
    if (file.is_open()) {
      file_found = true;
      std::string line;
      bool in_database_block = false;
      while (std::getline(file, line)) {
        if (line.find("\"database\":") != std::string::npos &&
            line.find("{") != std::string::npos) {
          in_database_block = true;
          continue;
        }

        if (in_database_block) {
          if (line.find("}") != std::string::npos) {
            in_database_block = false;
            continue;
          }

          if (line.find("\"host\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":")) + 1;
            size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos)
              config.host = line.substr(start, end - start);
          } else if (line.find("\"user\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":")) + 1;
            size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos)
              config.user = line.substr(start, end - start);
          } else if (line.find("\"password\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":")) + 1;
            size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos)
              config.password = line.substr(start, end - start);
          } else if (line.find("\"db_name\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":")) + 1;
            size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos)
              config.database = line.substr(start, end - start);
          }
        }
      }
      file.close();
      break;
    }
  }

  if (!file_found) {
    fprintf(stderr, "[ERROR] Config file not found in any of the expected paths.\n");
    return PlcErrorCodes::ERROR_NOT_FOUND;
  }

  // Verify all mandatory fields were loaded
  if (config.host.empty() || config.user.empty() || config.password.empty() ||
      config.database.empty()) {
    fprintf(stderr, "[ERROR] Config file found but mandatory fields are missing or empty.\n");
    return PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}
