/**
 * @file database.cpp
 * @author Diego Arcos Sapena
 * @brief PLC_Database class (code)
 * @version a-1.0.0
 * @date 2024/11/23
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "database.hpp"

#include <mariadb/mysql.h>

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../common/debug.hpp"
#include "../common/errors.hpp"
#include "../common/utils.hpp"
#include "../hardware/module.hpp"

// Static variables initialization
std::shared_ptr<PLC_Database> PLC_Database::_instance_ptr = nullptr;
std::recursive_mutex PLC_Database::_database_mutex;

// Private constructor for Singleton.
PLC_Database::PLC_Database()
    : _mysql_connection(mysql_init(NULL)),
      _timeout(PLC_DATABASE_TIMEOUT_SECONDS),
      _connected(false) {
  connect();
}

// Get instance
PlcErrorCodes PLC_Database::getInstance(std::shared_ptr<PLC_Database> &instance_ref) {
  DEBUG_STREAM("[DB] Thread entering getInstance()");
  // Ensure mutual exclusion in a multithreaded environment
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  // If the instance has not been created yet, create it
  if (!_instance_ptr) {
    _instance_ptr = std::shared_ptr<PLC_Database>(new PLC_Database());
  }

  // Assign the instance to the reference passed by the caller
  instance_ref = _instance_ptr;

  return PlcErrorCodes::PLC_SUCCESS;
}

PLC_Database::~PLC_Database(void) {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  /* FORCE to close connection if possible */
  if (_connected) {
    /* Set connection flag to false */
    _connected = false;

    /* Close mysql object connection */
    mysql_close(_mysql_connection);
  }
}

// Disable or enable autocommit
PlcErrorCodes PLC_Database::set_autocommit(bool enable) {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering set_autocommit");
#endif
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  if (mysql_autocommit(_mysql_connection, enable ? 1 : 0)) {
    log_error("PLC_Database::set_autocommit",
              "Failed to " + std::string(enable ? "enable" : "disable") + " autocommit. " +
                  mysql_error(_mysql_connection),
              PlcErrorCodes::ERROR_DATABASE_TRANSACTION);
    return PlcErrorCodes::ERROR_DATABASE_TRANSACTION;
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

// Commit the current transaction
PlcErrorCodes PLC_Database::commit() {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering commit()");
#endif
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  if (mysql_commit(_mysql_connection)) {
    log_error("PLC_Database::commit",
              "Failed to commit transaction. " + std::string(mysql_error(_mysql_connection)),
              PlcErrorCodes::ERROR_DATABASE_COMMIT);
    return PlcErrorCodes::ERROR_DATABASE_COMMIT;
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

// Rollback the current transaction
PlcErrorCodes PLC_Database::rollback() {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering rollback()");
#endif
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  if (mysql_rollback(_mysql_connection)) {
    log_error("PLC_Database::rollback",
              "Failed to rollback transaction. " + std::string(mysql_error(_mysql_connection)),
              PlcErrorCodes::ERROR_DATABASE_ROLLBACK);
    return PlcErrorCodes::ERROR_DATABASE_ROLLBACK;
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

// Connect to the database
PlcErrorCodes PLC_Database::connect() {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering connect()");
#endif
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  if (_mysql_connection == nullptr) {
    log_error("PLC_Database::connect",
              "Error creating MySQL object. " + std::string(mysql_error(_mysql_connection)),
              PlcErrorCodes::ERROR_DATABASE_NULL);
    return PlcErrorCodes::ERROR_DATABASE_NULL;
  }

  DbConfig config;
  PlcErrorCodes config_rs = load_db_config(config);
  if (config_rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("PLC_Database::connect", "Failed to load database configuration from config.json",
              config_rs);
    return config_rs;
  }

  if (mysql_real_connect(_mysql_connection, config.host.c_str(), config.user.c_str(),
                         config.password.c_str(), config.database.c_str(), 0, nullptr,
                         0) == nullptr) {
    log_error("PLC_Database::connect",
              "Failed connecting to database. " + std::string(mysql_error(_mysql_connection)),
              PlcErrorCodes::ERROR_DATABASE_CONNECT);
    return PlcErrorCodes::ERROR_DATABASE_CONNECT;
  }

  /* Set connection timeout */
  mysql_options(_mysql_connection, MYSQL_OPT_CONNECT_TIMEOUT, &_timeout);

  /* Set connection flag to true */
  _connected = true;
  log_msg("[INFO] Database connected successfully.");

  /* Return success code */
  return PlcErrorCodes::PLC_SUCCESS;
}

// Disconnect from the database
PlcErrorCodes PLC_Database::disconnect() {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering disconnect()");
#endif

  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  /* FORCE to close connection if possible */
  if (_connected) {
    /* Set connection flag to false */
    _connected = false;

    /* Close MySQL object connection */
    mysql_close(_mysql_connection);

    log_msg("[INFO] Database disconnected successfully.");
    return PlcErrorCodes::PLC_SUCCESS;
  }

  log_error("PLC_Database::disconnect",
            "Database already disconnected. " + std::string(mysql_error(_mysql_connection)),
            PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
  return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
}

PlcErrorCodes PLC_Database::getDeviceConfigurations(
    std::vector<DeviceConfig> &out_device_configs) const {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering getDeviceConfigurations()");
#endif

  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  out_device_configs.clear();

  if (!_connected) {
    log_error("PLC_Database::getDeviceConfigurations",
              "Cannot retrieve device configurations. Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Define query template (static query, no parameters)
  // devices contains the instance configuration
  // model_config contains the hardware definition (including protocol)
  std::string query =
      "SELECT "
      "d.module_id, d.module_name, d.channel, mc.protocol, "
      "d.connection_string, d.address_on_channel, d.fk_model_id, "
      "mc.max_read_bit_block_size, "
      "mc.max_read_register_block_size, "
      "mc.max_write_bit_block_size, "
      "mc.max_write_register_block_size, "
      "COALESCE(d.timeout_ms, mc.default_timeout_ms) as timeout_ms "
      "FROM devices d "
      "LEFT JOIN model_config mc ON d.fk_model_id = mc.model_id "
      "ORDER BY d.address_on_channel ASC;";

  // 2. Initialize and Prepare statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::getDeviceConfigurations", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::getDeviceConfigurations",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. No parameters to bind (static query)

  // 4. Execute statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::getDeviceConfigurations",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 5. Buffer result set
  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::getDeviceConfigurations",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  // 6. Bind output columns
  MYSQL_BIND results[12];
  memset(results, 0, sizeof(results));
  my_bool is_null[12];

  int32_t module_id, model_id;
  uint16_t max_read_bit, max_read_reg, max_write_bit, max_write_reg;
  uint32_t timeout_ms;

  // Buffers for variable-length string data
  char module_name_buf[101], channel_buf[51], protocol_buf[51], conn_string_buf[256],
      address_buf[51];
  unsigned long name_len, chan_len, proto_len, conn_len, addr_len;

  results[0].buffer_type = MYSQL_TYPE_LONG;
  results[0].buffer = &module_id;
  results[0].is_unsigned = false;
  results[0].is_null = &is_null[0];
  results[1].buffer_type = MYSQL_TYPE_STRING;
  results[1].buffer = module_name_buf;
  results[1].buffer_length = sizeof(module_name_buf);
  results[1].length = &name_len;
  results[1].is_null = &is_null[1];
  results[2].buffer_type = MYSQL_TYPE_STRING;
  results[2].buffer = channel_buf;
  results[2].buffer_length = sizeof(channel_buf);
  results[2].length = &chan_len;
  results[2].is_null = &is_null[2];
  results[3].buffer_type = MYSQL_TYPE_STRING;
  results[3].buffer = protocol_buf;
  results[3].buffer_length = sizeof(protocol_buf);
  results[3].length = &proto_len;
  results[3].is_null = &is_null[3];
  results[4].buffer_type = MYSQL_TYPE_STRING;
  results[4].buffer = conn_string_buf;
  results[4].buffer_length = sizeof(conn_string_buf);
  results[4].length = &conn_len;
  results[4].is_null = &is_null[4];
  results[5].buffer_type = MYSQL_TYPE_STRING;
  results[5].buffer = address_buf;
  results[5].buffer_length = sizeof(address_buf);
  results[5].length = &addr_len;
  results[5].is_null = &is_null[5];
  results[6].buffer_type = MYSQL_TYPE_LONG;
  results[6].buffer = &model_id;
  results[6].is_unsigned = false;
  results[6].is_null = &is_null[6];
  results[7].buffer_type = MYSQL_TYPE_SHORT;
  results[7].buffer = &max_read_bit;
  results[7].is_unsigned = true;
  results[7].is_null = &is_null[7];
  results[8].buffer_type = MYSQL_TYPE_SHORT;
  results[8].buffer = &max_read_reg;
  results[8].is_unsigned = true;
  results[8].is_null = &is_null[8];
  results[9].buffer_type = MYSQL_TYPE_SHORT;
  results[9].buffer = &max_write_bit;
  results[9].is_unsigned = true;
  results[9].is_null = &is_null[9];
  results[10].buffer_type = MYSQL_TYPE_SHORT;
  results[10].buffer = &max_write_reg;
  results[10].is_unsigned = true;
  results[10].is_null = &is_null[10];
  results[11].buffer_type = MYSQL_TYPE_LONG;
  results[11].buffer = &timeout_ms;
  results[11].is_unsigned = true;
  results[11].is_null = &is_null[11];
  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::getDeviceConfigurations",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 7. Fetch rows
  while (mysql_stmt_fetch(stmt) == 0)  // 0 means success, a row was fetched
  {
    DeviceConfig config;
    config.module_id = is_null[0] ? -1 : module_id;
    config.module_name = is_null[1] ? "" : std::string(module_name_buf, name_len);
    config.channel = is_null[2] ? "" : std::string(channel_buf, chan_len);
    config.protocol = is_null[3] ? "" : std::string(protocol_buf, proto_len);
    config.connection_string = is_null[4] ? "" : std::string(conn_string_buf, conn_len);
    config.address = is_null[5] ? "" : std::string(address_buf, addr_len);
    config.model_id = is_null[6] ? -1 : model_id;
    config.max_read_bit_block_size = is_null[7] ? 0 : max_read_bit;
    config.max_read_register_block_size = is_null[8] ? 0 : max_read_reg;
    config.max_write_bit_block_size = is_null[9] ? 0 : max_write_bit;
    config.max_write_register_block_size = is_null[10] ? 0 : max_write_reg;
    config.timeout_ms = is_null[11] ? 1000 : timeout_ms;

    out_device_configs.push_back(config);
  }

  // 8. Close statement
  mysql_stmt_close(stmt);

  log_msg("[INFO] Retrieved " + std::to_string(out_device_configs.size()) +
          " device configurations from database.");
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::delete_data(uint32_t module_id) {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering delete_data(module_id=" << module_id << ")");
#endif

  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  DEBUG_STREAM("[DB] Deleting data for module_id: " << module_id);
  if (!_connected) {
    log_error("PLC_Database::delete_data",
              "Database is disconnected. " + std::string(mysql_error(_mysql_connection)),
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Define query template
  std::string query = "DELETE FROM rtmirror WHERE fk_module_id = ?";

  // 2. Initialize statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::delete_data", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. Prepare statement
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::delete_data",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 4. Bind parameter
  MYSQL_BIND param[1];
  memset(param, 0, sizeof(param));  // Clear bind structure

  param[0].buffer_type = MYSQL_TYPE_LONG;  // Data type for int32_t
  param[0].buffer = (void *)&module_id;    // Pointer to the data
  param[0].is_unsigned = false;
  param[0].is_null = 0;

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::delete_data",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 5. Execute statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::delete_data",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 6. Close statement
  mysql_stmt_close(stmt);

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::update_is_connected(uint32_t module_id, bool is_connected_state) {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering update_is_connected(module_id=" << module_id << ", connected="
                                                              << is_connected_state << ")");
#endif
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  if (!_connected) {
    log_error("PLC_Database::update_is_connected", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Define query template - devices table now only contains status
  std::string query =
      "UPDATE device_status SET is_connected = ?, last_seen = NOW() WHERE fk_module_id = ?";

  // 2. Initialize statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::update_is_connected", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. Prepare statement
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::update_is_connected",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 4. Bind parameters
  MYSQL_BIND param[2];
  memset(param, 0, sizeof(param));  // Clear bind structure

  // Convert bool to tinyint for binding
  my_bool is_connected_tiny = is_connected_state ? 1 : 0;

  // Bind param 1: is_connected (bool as tinyint)
  param[0].buffer_type = MYSQL_TYPE_TINY;
  param[0].buffer = (void *)&is_connected_tiny;
  param[0].is_unsigned = false;
  param[0].is_null = 0;

  // Bind param 2: module_id (int32_t)
  param[1].buffer_type = MYSQL_TYPE_LONG;
  param[1].buffer = (void *)&module_id;
  param[1].is_unsigned = false;
  param[1].is_null = 0;

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::update_is_connected",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 5. Execute statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::update_is_connected",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 6. Close statement
  mysql_stmt_close(stmt);

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::insert_pk(int32_t fk_module_id, int32_t fk_model_id,
                                      OperationMode mode) {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  if (!_connected) {
    log_error("PLC_Database::insert_pk", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  MYSQL_STMT *stmt = nullptr;
  MYSQL_BIND param[2];

  // --- Prepare shared parameters ---
  memset(param, 0, sizeof(param));

  // Param 1: fk_module_id
  param[0].buffer_type = MYSQL_TYPE_LONG;
  param[0].buffer = (void *)&fk_module_id;
  param[0].is_unsigned = false;

  // Param 2: fk_model_id
  param[1].buffer_type = MYSQL_TYPE_LONG;
  param[1].buffer = (void *)&fk_model_id;
  param[1].is_unsigned = false;

  // --- Build purpose filter based on OperationMode ---
  std::string purpose_filter;
  if (mode == OperationMode::EXECUTION) {
    purpose_filter = " AND mid.purpose = 1";  // standard only
  } else                                      // CONFIGURATION
  {
    purpose_filter = " AND mid.purpose IN (2, 3)";  // secure_state or config
  }

  // --- Query: Insert FILTERED I/O definitions into rtmirror ---
  std::string query_rt =
      "INSERT IGNORE INTO rtmirror (fk_module_id, fk_io_definition_id) "
      "SELECT cfg.fk_module_id, cfg.fk_io_definition_id "
      "FROM module_io_config cfg "
      "JOIN model_io_definition mid ON cfg.fk_io_definition_id = mid.io_definition_id "
      "WHERE cfg.fk_module_id = ? "
      "  AND mid.fk_model_id = ?" +
      purpose_filter + ";";

  stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::insert_pk", "mysql_stmt_init() failed for rtmirror",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  if (mysql_stmt_prepare(stmt, query_rt.c_str(), query_rt.length())) {
    log_error("PLC_Database::insert_pk",
              "mysql_stmt_prepare() failed for rtmirror: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::insert_pk",
              "mysql_stmt_bind_param() failed for rtmirror: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::insert_pk",
              "mysql_stmt_execute() failed for rtmirror: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  mysql_stmt_close(stmt);  // Close the first statement

  DEBUG_STREAM("[DB] PKs inserted into rtmirror with filtered values for module_id "
               << fk_module_id);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::batch_update_rtmirror_values(
    const std::vector<DbUpdateInstruction> &updates) {
  // Lock the mutex to ensure thread safety
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  if (updates.empty()) {
    return PlcErrorCodes::PLC_SUCCESS;
  }

  if (!_connected) {
    log_error("PLC_Database::batch_update_rtmirror_values", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Initialize the statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::batch_update_rtmirror_values", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 2. Dynamically construct the query string with placeholders
  // Pattern: INSERT INTO table (cols) VALUES (?,?,?), (?,?,?) ... ON DUPLICATE KEY UPDATE ...
  std::string query = "INSERT INTO rtmirror (fk_module_id, fk_io_definition_id, value) VALUES ";

  // Reserve memory to avoid reallocations (approx 8 chars for "(?,?,?)," per item)
  query.reserve(150 + (updates.size() * 8));

  for (size_t i = 0; i < updates.size(); ++i) {
    query += (i == 0 ? "(?,?,?)" : ",(?,?,?)");
  }

  // The magic part: if the primary key (module_id + io_def_id) exists, update the value.
  // If it does not exist, insert it.
  query += " ON DUPLICATE KEY UPDATE value = VALUES(value), timestamp = NOW()";

  // 3. Prepare the statement
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::batch_update_rtmirror_values",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 4. Configure the Bind parameters
  // We need 3 parameters per update instruction
  std::vector<MYSQL_BIND> bind_params(updates.size() * 3);
  std::memset(bind_params.data(), 0, bind_params.size() * sizeof(MYSQL_BIND));

  size_t bind_idx = 0;
  for (const auto &up : updates) {
    // Param 1: fk_module_id (int32_t -> LONG)
    bind_params[bind_idx].buffer_type = MYSQL_TYPE_LONG;
    bind_params[bind_idx].buffer = (void *)&up.module_id;
    bind_params[bind_idx].is_unsigned = false;
    bind_idx++;

    // Param 2: fk_io_definition_id (uint32_t -> LONG UNSIGNED)
    bind_params[bind_idx].buffer_type = MYSQL_TYPE_LONG;
    bind_params[bind_idx].buffer = (void *)&up.io_definition_id;
    bind_params[bind_idx].is_unsigned = true;
    bind_idx++;

    // Param 3: value (uint64_t -> LONGLONG UNSIGNED)
    bind_params[bind_idx].buffer_type = MYSQL_TYPE_LONGLONG;
    bind_params[bind_idx].buffer = (void *)&up.value;
    bind_params[bind_idx].is_unsigned = true;
    bind_idx++;
  }

  // 5. Bind the parameters to the statement
  if (mysql_stmt_bind_param(stmt, bind_params.data())) {
    log_error("PLC_Database::batch_update_rtmirror_values",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 6. Execute the statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::batch_update_rtmirror_values",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

#ifdef TRACE
  uint64_t affected = mysql_stmt_affected_rows(stmt);
  TRACE_STREAM("[DB BATCH] Executed " << updates.size() << " updates, affected_rows=" << affected);
#endif

  // 7. Clean up
  mysql_stmt_close(stmt);

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::get_all_required_values(
    std::map<uint32_t, std::vector<std::pair<uint32_t, uint64_t>>> &out_values,
    OperationMode mode) const {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  out_values.clear();

  if (!_connected) {
    log_error("PLC_Database::get_all_required_values", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Build purpose filter based on OperationMode
  std::string purpose_filter;
  if (mode == OperationMode::EXECUTION) {
    purpose_filter = " AND model_io_definition.purpose = 1";  // standard only
  } else                                                      // CONFIGURATION
  {
    purpose_filter = " AND model_io_definition.purpose IN (2, 3)";  // secure_state or config
  }

  // 2. Define query with mode-aware filtering
  // It gets all points for ALL modules where a write has been requested.
  std::string query =
      "SELECT fk_module_id, fk_io_definition_id, required_value"
      " FROM rtmirror"
      " JOIN model_io_definition ON rtmirror.fk_io_definition_id = "
      "model_io_definition.io_definition_id"
      " WHERE required_value IS NOT NULL"
      " AND value != required_value"
      " AND model_io_definition.hardware_access = 2"  // Only readwrite definitions
      + purpose_filter + ";";

  // 2. Initialize statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::get_all_required_values", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. Prepare statement
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::get_all_required_values",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 4. Execute statement (no params)
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::get_all_required_values",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 5. Bind output results
  uint32_t mod_id;
  uint32_t io_def_id;
  uint64_t req_value;

  MYSQL_BIND results[3];
  memset(results, 0, sizeof(results));

  // Bind result 1: fk_module_id (uint32_t)
  results[0].buffer_type = MYSQL_TYPE_LONG;
  results[0].buffer = (void *)&mod_id;
  results[0].is_unsigned = true;

  // Bind result 2: fk_io_definition_id (uint32_t)
  results[1].buffer_type = MYSQL_TYPE_LONG;
  results[1].buffer = (void *)&io_def_id;
  results[1].is_unsigned = true;

  // Bind result 3: required_value (uint64_t)
  results[2].buffer_type = MYSQL_TYPE_LONGLONG;
  results[2].buffer = (void *)&req_value;
  results[2].is_unsigned = true;

  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::get_all_required_values",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 6. Store result to buffer rows
  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::get_all_required_values",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  // 7. Fetch rows
  while (mysql_stmt_fetch(stmt) == 0)  // 0 means success
  {
    out_values[mod_id].push_back({io_def_id, req_value});
  }

  // 8. Close statement
  mysql_stmt_close(stmt);

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::update_required_values_to_null_batch(
    const std::vector<uint32_t> &module_ids) {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  if (module_ids.empty()) {
    return PlcErrorCodes::PLC_SUCCESS;
  }

  if (!_connected) {
    log_error("PLC_Database::update_required_values_to_null_batch", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // Construct query: UPDATE rtmirror SET required_value = NULL WHERE fk_module_id IN (?,?,?...)
  std::string query =
      "UPDATE rtmirror SET required_value = NULL, net_required_value = NULL WHERE fk_module_id IN "
      "(";
  for (size_t i = 0; i < module_ids.size(); ++i) {
    query += (i == 0 ? "?" : ",?");
  }
  query += ")";

  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::update_required_values_to_null_batch", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::update_required_values_to_null_batch",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // Bind parameters
  std::vector<MYSQL_BIND> bind_params(module_ids.size());
  std::memset(bind_params.data(), 0, bind_params.size() * sizeof(MYSQL_BIND));

  for (size_t i = 0; i < module_ids.size(); ++i) {
    bind_params[i].buffer_type = MYSQL_TYPE_LONG;
    bind_params[i].buffer = (void *)&module_ids[i];
    bind_params[i].is_unsigned = true;
  }

  if (mysql_stmt_bind_param(stmt, bind_params.data())) {
    log_error("PLC_Database::update_required_values_to_null_batch",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::update_required_values_to_null_batch",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  mysql_stmt_close(stmt);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::get_plc_config(PLC_Config &out_config) const {
#ifdef DEBUG
  DEBUG_STREAM("[DB] Entering get_plc_config()");
#endif

  std::lock_guard<std::recursive_mutex> lock(_database_mutex);

  if (!_connected) {
    log_error("PLC_Database::get_plc_config", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Define query template
  // 1. Define query template
  std::string query =
      "SELECT rs485_baud_rate, rs485_parity, rs485_stop_bits, rs485_data_bits, operation_mode + 0 "
      "FROM plc_settings LIMIT 1;";

  // 2. Initialize and Prepare statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::get_plc_config", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::get_plc_config",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. Execute statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::get_plc_config",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 4. Buffer result set
  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::get_plc_config",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  // 5. Bind output columns
  MYSQL_BIND results[5];
  memset(results, 0, sizeof(results));
  my_bool is_null[5];

  uint32_t baudrate;
  char parity_buffer[2];  // Buffer for single char + null terminator
  unsigned long parity_length = 0;
  uint8_t stop_bits;
  uint8_t data_bits;
  uint32_t op_mode_int;

  results[0].buffer_type = MYSQL_TYPE_LONG;  // rs485_baud_rate (uint32_t)
  results[0].buffer = &baudrate;
  results[0].is_unsigned = true;
  results[0].is_null = &is_null[0];

  results[1].buffer_type = MYSQL_TYPE_STRING;  // rs485_parity (CHAR(1))
  results[1].buffer = parity_buffer;
  results[1].buffer_length = sizeof(parity_buffer);
  results[1].length = &parity_length;
  results[1].is_null = &is_null[1];

  results[2].buffer_type = MYSQL_TYPE_TINY;  // rs485_stop_bits (uint8_t)
  results[2].buffer = &stop_bits;
  results[2].is_unsigned = true;
  results[2].is_null = &is_null[2];

  results[3].buffer_type = MYSQL_TYPE_TINY;  // rs485_data_bits (uint8_t)
  results[3].buffer = &data_bits;
  results[3].is_unsigned = true;
  results[3].is_null = &is_null[3];

  results[4].buffer_type = MYSQL_TYPE_LONG;  // operation_mode (uint32_t)
  results[4].buffer = &op_mode_int;
  results[4].is_unsigned = true;
  results[4].is_null = &is_null[4];

  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::get_plc_config",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 6. Fetch row
  if (mysql_stmt_fetch(stmt) == 0)  // 0 means success, a row was fetched
  {
    if (!is_null[0] && !is_null[1] && !is_null[2] && !is_null[3] && parity_length > 0) {
      out_config.rs485_baudrate = baudrate;
      out_config.rs485_parity = parity_buffer[0];
      out_config.rs485_bit_stop = stop_bits;
      out_config.rs485_data_bits = data_bits;

      if (!is_null[4]) {
        if (op_mode_int == 1)
          out_config.operation_mode = OperationMode::EXECUTION;
        else if (op_mode_int == 2)
          out_config.operation_mode = OperationMode::CONFIGURATION;
        else
          out_config.operation_mode = OperationMode::EXECUTION;  // Default
      } else {
        out_config.operation_mode = OperationMode::EXECUTION;
      }
    } else {
      // Handle case where row was found but contained NULLs
      log_error("PLC_Database::get_plc_config", "Data in 'plc_config' table contains NULL values.",
                PlcErrorCodes::ERROR_DATABASE_FETCH);
    }
  }
  // Note: No 'else' needed, if fetch fails (no row found), we just return SUCCESS with default
  // config

  // 7. Close statement
  mysql_stmt_close(stmt);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::cleanup_rtmirror() {
  DEBUG_STREAM("[DB] Thread entering cleanup_rtmirror()");

  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  DEBUG_STREAM("[DB] Starting cleanup of orphaned runtime data...");
  if (!_connected) {
    log_error("PLC_Database::cleanup_rtmirror", "Database is disconnected. Cannot perform cleanup.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  PlcErrorCodes rs_autocommit_off = set_autocommit(false);
  if (rs_autocommit_off != PlcErrorCodes::PLC_SUCCESS) {
    log_error("PLC_Database::cleanup_rtmirror", "Failed to disable autocommit for cleanup.",
              rs_autocommit_off);
    return rs_autocommit_off;
  }

  bool cleanup_error = false;

  // Full reset: Delete ALL rows from rtmirror (not just orphaned)
  std::string query_rtmirror = "DELETE FROM rtmirror;";
  if (mysql_query(_mysql_connection, query_rtmirror.c_str())) {
    log_error("PLC_Database::cleanup_rtmirror",
              "Failed to clear rtmirror table: " + std::string(mysql_error(_mysql_connection)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    cleanup_error = true;
  } else {
    DEBUG_STREAM("[DB] rtmirror table cleared successfully.");
  }

  if (cleanup_error) {
    log_error("PLC_Database::cleanup_rtmirror", "Cleanup transaction rolled back due to error.",
              PlcErrorCodes::PLC_SUCCESS);
    rollback();
  } else {
    commit();
  }

  PlcErrorCodes rs_autocommit_on = set_autocommit(true);
  if (rs_autocommit_on != PlcErrorCodes::PLC_SUCCESS) {
    log_error("PLC_Database::cleanup_rtmirror", "Failed to re-enable autocommit after cleanup.",
              rs_autocommit_on);
    return rs_autocommit_on;
  }

  return cleanup_error ? PlcErrorCodes::ERROR_DATABASE_EXECUTE : PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::getIoDefinitions(uint32_t model_id, uint32_t module_id,
                                             std::vector<IoDefinition> &out_defs) const {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  out_defs.clear();

  if (!_connected) {
    log_error("PLC_Database::getIoDefinitions", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Define query template
  // JOIN 'model_io_definition' (mid) with 'module_io_config' (cfg)
  // to combine the hardware definition with the user instance configuration.
  // We retrieve ALL definitions regardless of purpose/mode. Sync filtering happens at runtime.
  std::string query =
      "SELECT "
      "mid.io_definition_id, "
      "mid.fk_model_id, "
      "mid.logical_address, "
      "mid.physical_address, "
      "CASE mid.io_type WHEN 'bit' THEN 1 WHEN 'register' THEN 2 ELSE 0 END AS io_type, "
      "CASE mid.purpose WHEN 'standard' THEN 1 WHEN 'secure_state' THEN 2 WHEN 'config' THEN 3 "
      "ELSE 0 END AS purpose, "
      "mid.register_count, "
      "cfg.scale_factor, "
      "cfg.offset, "
      "CASE mid.endianess WHEN 'big' THEN 1 WHEN 'little' THEN 2 ELSE 0 END AS endianess, "
      "CASE mid.hardware_access WHEN 'readonly' THEN 1 WHEN 'readwrite' THEN 2 ELSE 0 END AS "
      "hardware_access, "
      "CASE mid.access_method WHEN 'direct' THEN 1 WHEN 'bitmask' THEN 2 ELSE 0 END AS "
      "access_method, "
      "mid.bitmask_offset, "
      "cfg.user_label "
      "FROM model_io_definition mid "
      "JOIN module_io_config cfg "
      "ON mid.io_definition_id = cfg.fk_io_definition_id AND cfg.fk_module_id = ? "
      "WHERE mid.fk_model_id = ?;";

  // 2. Initialize and Prepare statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::getIoDefinitions", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::getIoDefinitions",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. Bind input parameters
  // Order matches the '?' in query: 1st is cfg.fk_module_id, 2nd is mid.fk_model_id
  MYSQL_BIND param[2];
  memset(param, 0, sizeof(param));

  param[0].buffer_type = MYSQL_TYPE_LONG;  // module_id (uint32_t)
  param[0].buffer = (void *)&module_id;
  param[0].is_unsigned = true;

  param[1].buffer_type = MYSQL_TYPE_LONG;  // model_id (uint32_t)
  param[1].buffer = (void *)&model_id;
  param[1].is_unsigned = true;

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::getIoDefinitions",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 4. Execute statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::getIoDefinitions",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 5. Buffer result set
  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::getIoDefinitions",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  // 6. Bind output columns
  MYSQL_BIND results[14];
  memset(results, 0, sizeof(results));

  uint32_t io_def_id, fk_model_id;
  uint16_t logical_addr, physical_addr;
  uint8_t io_type, purpose, register_count, endianess, hardware_access, access_method,
      bitmask_offset;

  double scale_factor;
  double offset;

  char user_label_buffer[101];
  unsigned long user_label_length = 0;
  my_bool is_null[14];

  results[0].buffer_type = MYSQL_TYPE_LONG;
  results[0].buffer = &io_def_id;
  results[0].is_unsigned = true;
  results[0].is_null = &is_null[0];
  results[1].buffer_type = MYSQL_TYPE_LONG;
  results[1].buffer = &fk_model_id;
  results[1].is_unsigned = true;
  results[1].is_null = &is_null[1];
  results[2].buffer_type = MYSQL_TYPE_SHORT;
  results[2].buffer = &logical_addr;
  results[2].is_unsigned = true;
  results[2].is_null = &is_null[2];
  results[3].buffer_type = MYSQL_TYPE_SHORT;
  results[3].buffer = &physical_addr;
  results[3].is_unsigned = true;
  results[3].is_null = &is_null[3];
  results[4].buffer_type = MYSQL_TYPE_TINY;
  results[4].buffer = &io_type;
  results[4].is_unsigned = true;
  results[4].is_null = &is_null[4];
  results[5].buffer_type = MYSQL_TYPE_TINY;
  results[5].buffer = &purpose;
  results[5].is_unsigned = true;
  results[5].is_null = &is_null[5];
  results[6].buffer_type = MYSQL_TYPE_TINY;
  results[6].buffer = &register_count;
  results[6].is_unsigned = true;
  results[6].is_null = &is_null[6];
  results[7].buffer_type = MYSQL_TYPE_DOUBLE;
  results[7].buffer = &scale_factor;
  results[7].is_null = &is_null[7];
  results[8].buffer_type = MYSQL_TYPE_DOUBLE;
  results[8].buffer = &offset;
  results[8].is_null = &is_null[8];
  results[9].buffer_type = MYSQL_TYPE_TINY;
  results[9].buffer = &endianess;
  results[9].is_unsigned = true;
  results[9].is_null = &is_null[9];
  results[10].buffer_type = MYSQL_TYPE_TINY;
  results[10].buffer = &hardware_access;
  results[10].is_unsigned = true;
  results[10].is_null = &is_null[10];
  results[11].buffer_type = MYSQL_TYPE_TINY;
  results[11].buffer = &access_method;
  results[11].is_unsigned = true;
  results[11].is_null = &is_null[11];
  results[12].buffer_type = MYSQL_TYPE_TINY;
  results[12].buffer = &bitmask_offset;
  results[12].is_unsigned = true;
  results[12].is_null = &is_null[12];
  results[13].buffer_type = MYSQL_TYPE_STRING;
  results[13].buffer = user_label_buffer;
  results[13].buffer_length = sizeof(user_label_buffer);
  results[13].is_null = &is_null[13];
  results[13].length = &user_label_length;

  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::getIoDefinitions",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 7. Fetch rows
  while (mysql_stmt_fetch(stmt) == 0)  // 0 means success, a row was fetched
  {
    IoDefinition def;
    def.io_definition_id = is_null[0] ? 0 : io_def_id;
    def.fk_model_id = is_null[1] ? 0 : fk_model_id;
    def.logical_address = is_null[2] ? 0 : logical_addr;
    def.physical_address = is_null[3] ? 0 : physical_addr;
    def.io_type = is_null[4] ? 0 : io_type;
    def.purpose = is_null[5] ? 0 : purpose;
    def.register_count = is_null[6] ? 0 : register_count;

    // Use defaults if values are NULL (e.g. no config entry yet)
    def.scale_factor = is_null[7] ? 1.0 : scale_factor;
    def.offset = is_null[8] ? 0.0 : offset;

    def.endianess = is_null[9] ? 0 : endianess;
    def.hardware_access = is_null[10] ? 0 : hardware_access;
    def.access_method = is_null[11] ? 0 : access_method;
    def.bitmask_offset = is_null[12] ? 0 : bitmask_offset;
    def.user_label =
        is_null[13] ? "No label defined" : std::string(user_label_buffer, user_label_length);

    out_defs.push_back(def);
  }

  // 8. Close statement
  mysql_stmt_close(stmt);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::getAggregatedIoMap(
    uint32_t aggregated_model_id, std::vector<AggregatedMappingEntry> &out_mappings) const {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  out_mappings.clear();

  if (!_connected) {
    log_error("PLC_Database::getAggregatedIoMap", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  // 1. Define query template
  // Now queries by model_id directly (model-level mapping).
  // child_slot_index references a slot in aggregated_model_children.
  std::string query =
      "SELECT "
      " vmap.map_id, "
      " vmap.fk_aggregated_io_definition_id, "
      " vmap.child_slot_index, "
      " c_def.logical_address, "
      " vmap.fk_child_io_definition_id "
      "FROM aggregated_io_map AS vmap "
      "JOIN model_io_definition AS v_def ON vmap.fk_aggregated_io_definition_id = "
      "v_def.io_definition_id "
      "JOIN model_io_definition AS c_def ON vmap.fk_child_io_definition_id = "
      "c_def.io_definition_id "
      "WHERE v_def.fk_model_id = ?;";

  // 2. Initialize and Prepare statement
  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::getAggregatedIoMap", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::getAggregatedIoMap",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  // 3. Bind input parameter
  MYSQL_BIND param[1];
  memset(param, 0, sizeof(param));

  param[0].buffer_type = MYSQL_TYPE_LONG;  // aggregated_model_id (uint32_t)
  param[0].buffer = (void *)&aggregated_model_id;
  param[0].is_unsigned = true;

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::getAggregatedIoMap",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 4. Execute statement
  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::getAggregatedIoMap",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  // 5. Buffer result set
  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::getAggregatedIoMap",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  // 6. Bind output columns
  MYSQL_BIND results[5];
  memset(results, 0, sizeof(results));
  my_bool is_null[5];

  AggregatedMappingEntry entry;

  results[0].buffer_type = MYSQL_TYPE_LONG;  // map_id (uint32_t)
  results[0].buffer = &entry.map_id;
  results[0].is_unsigned = true;
  results[0].is_null = &is_null[0];

  results[1].buffer_type = MYSQL_TYPE_LONG;  // fk_aggregated_io_definition_id (uint32_t)
  results[1].buffer = &entry.fk_aggregated_io_definition_id;
  results[1].is_unsigned = true;
  results[1].is_null = &is_null[1];

  results[2].buffer_type = MYSQL_TYPE_TINY;  // child_slot_index (uint8_t)
  results[2].buffer = &entry.child_slot_index;
  results[2].is_unsigned = true;
  results[2].is_null = &is_null[2];

  results[3].buffer_type = MYSQL_TYPE_SHORT;  // child_logical_address (uint16_t)
  results[3].buffer = &entry.child_logical_address;
  results[3].is_unsigned = true;
  results[3].is_null = &is_null[3];

  results[4].buffer_type = MYSQL_TYPE_LONG;  // fk_child_io_definition_id (uint32_t)
  results[4].buffer = &entry.fk_child_io_definition_id;
  results[4].is_unsigned = true;
  results[4].is_null = &is_null[4];

  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::getAggregatedIoMap",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  // 7. Fetch rows
  while (mysql_stmt_fetch(stmt) == 0)  // 0 means success, a row was fetched
  {
    // Create a copy of the entry struct populated by fetch
    AggregatedMappingEntry fetched_entry = entry;

    // Handle potential NULLs by setting to 0 (matches original logic)
    if (is_null[0]) fetched_entry.map_id = 0;
    if (is_null[1]) fetched_entry.fk_aggregated_io_definition_id = 0;
    if (is_null[2]) fetched_entry.child_slot_index = 0;
    if (is_null[3]) fetched_entry.child_logical_address = 0;
    if (is_null[4]) fetched_entry.fk_child_io_definition_id = 0;

    out_mappings.push_back(fetched_entry);
  }

  // 8. Close statement
  mysql_stmt_close(stmt);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::getAggregatedModelChildren(
    uint32_t aggregated_model_id, std::vector<uint32_t> &out_child_model_ids) const {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  out_child_model_ids.clear();

  if (!_connected) {
    log_error("PLC_Database::getAggregatedModelChildren", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  std::string query =
      "SELECT fk_child_model_id "
      "FROM aggregated_model_children "
      "WHERE fk_aggregated_model_id = ? "
      "ORDER BY slot_index ASC;";

  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::getAggregatedModelChildren", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::getAggregatedModelChildren",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  MYSQL_BIND param[1];
  memset(param, 0, sizeof(param));
  param[0].buffer_type = MYSQL_TYPE_LONG;
  param[0].buffer = (void *)&aggregated_model_id;
  param[0].is_unsigned = true;

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::getAggregatedModelChildren",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::getAggregatedModelChildren",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::getAggregatedModelChildren",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  MYSQL_BIND results[1];
  memset(results, 0, sizeof(results));
  uint32_t child_model_id;
  my_bool is_null;

  results[0].buffer_type = MYSQL_TYPE_LONG;
  results[0].buffer = &child_model_id;
  results[0].is_unsigned = true;
  results[0].is_null = &is_null;

  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::getAggregatedModelChildren",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  while (mysql_stmt_fetch(stmt) == 0) {
    if (!is_null) {
      out_child_model_ids.push_back(child_model_id);
    }
  }

  mysql_stmt_close(stmt);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes PLC_Database::getSecureStateMapping(uint32_t model_id,
                                                  std::map<uint32_t, uint32_t> &out_mapping) const {
  std::lock_guard<std::recursive_mutex> lock(_database_mutex);
  out_mapping.clear();

  if (!_connected) {
    log_error("PLC_Database::getSecureStateMapping", "Database is disconnected.",
              PlcErrorCodes::ERROR_DATABASE_DISCONNECTED);
    return PlcErrorCodes::ERROR_DATABASE_DISCONNECTED;
  }

  std::string query =
      "SELECT fk_standard_io_definition_id, fk_secure_state_io_definition_id FROM "
      "model_secure_state_mapping WHERE fk_model_id = ?;";

  MYSQL_STMT *stmt = mysql_stmt_init(_mysql_connection);
  if (!stmt) {
    log_error("PLC_Database::getSecureStateMapping", "mysql_stmt_init() failed",
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    log_error("PLC_Database::getSecureStateMapping",
              "mysql_stmt_prepare() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_PREPARE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_PREPARE;
  }

  MYSQL_BIND param[1];
  memset(param, 0, sizeof(param));
  param[0].buffer_type = MYSQL_TYPE_LONG;
  param[0].buffer = (void *)&model_id;
  param[0].is_unsigned = true;

  if (mysql_stmt_bind_param(stmt, param)) {
    log_error("PLC_Database::getSecureStateMapping",
              "mysql_stmt_bind_param() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  if (mysql_stmt_execute(stmt)) {
    log_error("PLC_Database::getSecureStateMapping",
              "mysql_stmt_execute() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_EXECUTE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_EXECUTE;
  }

  if (mysql_stmt_store_result(stmt)) {
    log_error("PLC_Database::getSecureStateMapping",
              "mysql_stmt_store_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_STORE);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_STORE;
  }

  MYSQL_BIND results[2];
  memset(results, 0, sizeof(results));
  uint32_t standard_id;
  uint32_t secure_id;
  my_bool is_null[2];

  results[0].buffer_type = MYSQL_TYPE_LONG;
  results[0].buffer = &standard_id;
  results[0].is_unsigned = true;
  results[0].is_null = &is_null[0];

  results[1].buffer_type = MYSQL_TYPE_LONG;
  results[1].buffer = &secure_id;
  results[1].is_unsigned = true;
  results[1].is_null = &is_null[1];

  if (mysql_stmt_bind_result(stmt, results)) {
    log_error("PLC_Database::getSecureStateMapping",
              "mysql_stmt_bind_result() failed: " + std::string(mysql_stmt_error(stmt)),
              PlcErrorCodes::ERROR_DATABASE_BIND);
    mysql_stmt_close(stmt);
    return PlcErrorCodes::ERROR_DATABASE_BIND;
  }

  while (mysql_stmt_fetch(stmt) == 0) {
    if (!is_null[0] && !is_null[1]) {
      out_mapping[standard_id] = secure_id;
    }
  }

  mysql_stmt_close(stmt);
  return PlcErrorCodes::PLC_SUCCESS;
}
