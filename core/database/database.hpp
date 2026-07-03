/**
 * @file database.hpp
 * @author Diego Arcos Sapena
 * @brief PLC_Database class (header)
 * @version a-1.0.0
 * @date 2024/11/23
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <mariadb/mysql.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "../common/errors.hpp"
#include "../hardware/aggregator_module.hpp"
#include "../hardware/module.hpp"
#include "../hardware/plc.hpp"

#define PLC_DATABASE_TIMEOUT_SECONDS 10

// A struct to group the necessary information for a single database update.
struct DbUpdateInstruction {
  uint32_t module_id;
  uint32_t io_definition_id;
  uint64_t value;
};

/**
 * @brief PLC_Database class for managing database operations.
 *
 * This class provides methods to connect to the database, execute queries,
 * and manage device configurations.
 */
class PLC_Database {
  public:
  /**
   * @brief Get the pointer of the instance of the database
   * @param[out] instance_ref Reference to the pointer of the instance
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  static PlcErrorCodes getInstance(std::shared_ptr<PLC_Database> &instance_ref);

  /**
   * @brief PLC_Database class destructor.
   */
  ~PLC_Database(void);

  /**
   * @brief Enables or disables autocommit mode for the current database connection.
   *
   * @param[in] enable Set to true to enable autocommit, false to disable.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes set_autocommit(bool enable);

  /**
   * @brief Commits the current transaction.
   *
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes commit();

  /**
   * @brief Rolls back the current transaction.
   *
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes rollback();

  /**
   * @brief Starts database connection.
   * @par None.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes connect(void);

  /**
   * @brief End database connection.
   * @par None.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes disconnect(void);

  /**
   * @brief Retrieves device configurations from the 'devices' table in the database.
   * This function only reads data; it does not initialize hardware or PLC objects.
   *
   * @param[out] out_device_configs Reference to a vector where the retrieved configurations will be
   * stored.
   * @return PlcErrorCodes Error code or PLC_SUCCESS if the operation was successful.
   */
  PlcErrorCodes getDeviceConfigurations(std::vector<DeviceConfig> &out_device_configs) const;

  /**
   * @brief Delete module-related data in rtmirror. For one module
   * @param[in] module_id The unique identifier of the module.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes delete_data(uint32_t module_id);

  /**
   * @brief Updates the connection state of a module in the 'devices' table.
   * @param[in] module_id The unique identifier of the module.
   * @param[in] is_connected_state True to set as connected, false to set as disconnected.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes update_is_connected(uint32_t module_id, bool is_connected_state);

  /**
   * @brief Inserts the primary keys into rtmirror when a new card is inserted.
   * Inserts ALL I/O definitions for the module, regardless of operation mode.
   * @param[in] fk_module_id The unique identifier for the module from the 'devices' table.
   * @param[in] fk_model_id The foreign key to the 'model_config' table.
   * @param[in] mode The operation mode for the module.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes insert_pk(int32_t fk_module_id, int32_t fk_model_id, OperationMode mode);

  /**
   * @brief Updates multiple values in rtmirror in a single SQL query.
   */
  PlcErrorCodes batch_update_rtmirror_values(const std::vector<DbUpdateInstruction> &updates);

  /**
   * @brief Retrieves all I/O points for ALL modules that have a pending required_value.
   * Filters by purpose based on OperationMode:
   * - EXECUTION: only standard (purpose=1)
   * - CONFIGURATION: only secure_state and config (purpose=2,3)
   *
   * @param out_values A map where key is module_id and value is a vector of {io_definition_id,
   * required_value}.
   * @param mode The current operation mode for purpose filtering.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes get_all_required_values(
      std::map<uint32_t, std::vector<std::pair<uint32_t, uint64_t>>> &out_values,
      OperationMode mode) const;

  /**
   * @brief Clears the required_value field for all points of MULTIPLE modules where the
   * requested value has been successfully written.
   *
   * @param module_ids A vector of module IDs to clean up.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes update_required_values_to_null_batch(const std::vector<uint32_t> &module_ids);

  /**
   * @brief Retrieves all global settings from the 'plc_settings' table
   * and populates a PLC_Config struct.
   *
   * @param[out] out_config Reference to a PLC_Config struct to be filled with the settings.
   * The struct should be pre-initialized with default values.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes get_plc_config(PLC_Config &out_config) const;

  /**
   * @brief Performs an initial cleanup of orphaned runtime data in MEMORY tables.
   * This function deletes entries in rtmirror.
   * that do not have a corresponding module_id in the 'devices' table. In case an entry of devices
   * table is deleted while system is off Designed to be called once at application startup.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes cleanup_rtmirror();

  /**
   * @brief Retrieves ALL I/O definitions for a given model from the 'model_io_definitions' table.
   * Gets all definitions regardless of purpose/mode. Also retrieves the user_label from
   * module_io_config.
   * @param[in] model_id The unique identifier of the model.
   * @param[in] module_id The unique identifier of the module.
   * @param[out] out_defs A vector to be filled with the I/O definitions.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes getIoDefinitions(uint32_t model_id, uint32_t module_id,
                                 std::vector<IoDefinition> &out_defs) const;

  /**
   * @brief Retrieves the I/O mapping for an aggregated model from the 'aggregated_io_map' table.
   *        The mapping is defined at the model level, using child_slot_index references.
   * @param[in] aggregated_model_id The model_id of the aggregated model.
   * @param[out] out_mappings A vector to be filled with the mapping entries.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes getAggregatedIoMap(uint32_t aggregated_model_id,
                                   std::vector<AggregatedMappingEntry> &out_mappings) const;

  /**
   * @brief Retrieves the expected child model IDs for an aggregated model from
   * 'aggregated_model_children'. Results are ordered by slot_index. Used to validate that child
   * module instances match the model-level definition at startup.
   * @param[in] aggregated_model_id The model_id of the aggregated model.
   * @param[out] out_child_model_ids A vector of expected child model_ids, ordered by slot_index.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes getAggregatedModelChildren(uint32_t aggregated_model_id,
                                           std::vector<uint32_t> &out_child_model_ids) const;

  /**
   * @brief Retrieves the secure state mapping for a given model.
   * @param[in] model_id The unique identifier of the model.
   * @param[out] out_mapping A map where key is standard io_definition_id and value is secure_state
   * io_definition_id.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes getSecureStateMapping(uint32_t model_id,
                                      std::map<uint32_t, uint32_t> &out_mapping) const;

  private:
  /**
   * @brief PLC_Database class constructor. Private for Signleton
   */
  PLC_Database();

  /**
   * @brief Mysql controller
   */
  MYSQL *_mysql_connection;

  /**
   * @brief Mysql connection timeout
   */
  int _timeout;

  /**
   * @brief PLC_Database ip
   */
  const char *_ip;

  /**
   * @brief PLC_Database user
   */
  const char *_user;

  /**
   * @brief PLC_Database password
   */
  const char *_password;

  /**
   * @brief PLC_Database name
   */
  const char *_name;

  /**
   * @brief Started flag
   */
  bool _connected;

  // Shared static instance
  /**
   * @brief Static pointer to the instance of the database
   */
  static std::shared_ptr<PLC_Database> _instance_ptr;

  /**
   * @brief Mutex static for thread safety in the getInstance method and others
   */
  static std::recursive_mutex _database_mutex;
};
