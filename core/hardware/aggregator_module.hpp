/**
 * @file aggregator_module.hpp
 * @author Diego Arcos Sapena
 * @brief Defines the AggregatorModule, an aggregated module that acts as a logical view over other
 * modules.
 * @version a-2.1.0
 * @date 2025/08/31
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "IModule.hpp"

// Forward declarations to avoid circular includes
enum class OperationMode;
struct DbUpdateInstruction;

/**
 * @struct MappingTarget
 * @brief A helper struct to store the redirection target for an aggregated address.
 */
struct MappingTarget {
  IModulePtr module;
  uint16_t address;
};

/**
 * @struct AggregatedMappingEntry
 * @brief Represents a single I/O mapping from an aggregated model point to a child model I/O point.
 *        The mapping is defined at the MODEL level using slot indexes, not module instance IDs.
 *        The child_slot_index references a slot from the aggregated_model_children table.
 */
struct AggregatedMappingEntry {
  uint32_t map_id;
  uint32_t fk_aggregated_io_definition_id;
  uint8_t child_slot_index;  // Slot index referencing aggregated_model_children
  uint16_t child_logical_address;
  uint32_t fk_child_io_definition_id;
};

/**
 * @class AggregatorModule
 * @brief Implements the IModule interface for an aggregated module.
 *
 * This class does not communicate directly with hardware. Instead, it holds a map
 * that translates its own aggregated I/O addresses to the logical addresses of its
 * child modules. All read/write operations are redirected to the appropriate child.
 */
class AggregatorModule : public IModule {
  public:
  /**
   * @brief Constructs a new Aggregator Module object.
   * @param module_id The unique ID for this aggregated module.
   * @param model_id The model ID for this aggregated module.
   * @param module_name The descriptive name for this aggregated module.
   * @param address_on_channel The address string used by the protocol to identify the device (e.g.,
   * IP address).
   * @param channel_type The type of channel (e.g., "modbus-rtu", "modbus-tcp", "spi",
   * "aggregated").
   * @param protocol The protocol used for communication (e.g., "modbus", "custom-protocol").
   * @param child_modules A vector of shared pointers to the modules this aggregator will manage.
   * @param raw_mappings Initial list of mapping rules to populate the translation tables.
   */
  AggregatorModule(uint32_t id, uint32_t model, const std::string &name, const std::string &address,
                   const std::string &channel, const std::string &protocol,
                   const std::vector<IModulePtr> &children,
                   const std::vector<AggregatedMappingEntry> &raw_mappings);

  /**
   * @brief Virtual destructor.
   */
  virtual ~AggregatorModule() = default;

  // --- IModule Interface Implementation ---

  /**
   * @brief Initializes the aggregator by building its internal translation tables.
   * This method must be called after construction and requires the raw aggregated mapping data from
   * the database.
   * @param raw_mappings A vector of AggregatedMappingEntry structs defining the I/O redirection.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes initialize() override;

  /**
   * @brief The sync operation for an aggregator does nothing.
   * Synchronization is the responsibility of the underlying physical modules.
   * @return PlcErrorCodes::PLC_SUCCESS always.
   */
  PlcErrorCodes sync() override;

  /**
   * @brief Finds the direct child module and address that an aggregated point maps to.
   * @param aggregated_address The logical address within this aggregated module.
   * @param purpose The purpose of the I/O point (1=standard, 2=secure_state).
   * @param[out] out_target The struct to be filled with the child module pointer and address.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code if the mapping is not found.
   */
  PlcErrorCodes findChildMapping(uint16_t aggregated_address, uint8_t purpose,
                                 MappingTarget &out_target);

  /**
   * @brief Commands all child modules to enter their safe state.
   * @return PlcErrorCodes::PLC_SUCCESS if all children succeed.
   */
  PlcErrorCodes setSafeState() override;

  /**
   * @brief Resets the internal state of the aggregator.
   * This clears the mapping tables.
   * @return PlcErrorCodes::PLC_SUCCESS always.
   */
  PlcErrorCodes freeModule() override;

  // --- Identity & Configuration Getters ---

  /**
   * @brief @copydoc IModule::getModuleId
   */
  PlcErrorCodes getModuleId(uint32_t &moduleId) const override;

  /**
   * @brief @copydoc IModule::getModuleName
   */
  PlcErrorCodes getModuleName(std::string &name) const override;

  /**
   * @brief @copydoc IModule::getModelId
   */
  PlcErrorCodes getModelId(uint32_t &modelId) const override;

  /**
   * @brief @copydoc IModule::getChannel
   */
  PlcErrorCodes getChannel(std::string &channel) const override;

  /**
   * @brief @copydoc IModule::getAllIoDefinitions
   */
  PlcErrorCodes getAllIoDefinitions(std::vector<IoDefinition> &out_defs) const override;

  /**
   * @brief Finds the mapping rule (from raw_mappings) for a given aggregated I/O definition ID.
   * @param io_definition_id The ID of the I/O point on the aggregated module.
   * @param out_mapping Reference to store the found mapping entry.
   * @return PLC_SUCCESS or ERROR_NOT_FOUND.
   */
  PlcErrorCodes findChildMappingById(uint32_t io_definition_id,
                                     AggregatedMappingEntry &out_mapping);

  /**
   * @brief Gets a child module by its slot index.
   * @param slot_index The slot index (0, 1, 2, ...) matching aggregated_model_children.
   * @param out_child Reference to store the child module pointer.
   * @return PLC_SUCCESS or ERROR_MODULE_NOT_FOUND.
   */
  PlcErrorCodes getChildBySlotIndex(uint8_t slot_index, IModulePtr &out_child);

  // --- State & Status Getters ---

  /**
   * @brief Checks if the module is considered connected, this means that all its children are
   * connected.
   * @param[out] connected The connection status.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getConnected(uint8_t &connected) const override;

  /**
   * @brief @copydoc IModule::setConnected
   */
  PlcErrorCodes setConnected(uint8_t connected) override;

  /**
   * @brief Commands the module and all its children to perform a full data refresh.
   * This forces all I/O points to be re-sent to the database on the next sync cycle.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes forceFullSync() override;

  /**
   * @brief Collects all current values from child modules and returns them as database update
   * instructions.
   *
   * This method iterates over all I/O definitions for this aggregator and reads the current
   * value from the mapped child module's memory using existing redirection methods.
   * It does NOT check for changes or modify update flags - it simply returns all current values.
   *
   * @param mode The current operation mode for filtering by purpose.
   * @param[out] out_updates A vector to be filled with the update instructions for all I/O points.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes collectAllCurrentValues(OperationMode mode,
                                        std::vector<DbUpdateInstruction> &out_updates);

  // --- In-Memory Data Accessors (Redirected to Children) ---

  // --- High-Level Data Access API (for reading the module's in-memory state) ---
  // --- High-Level Control API (for writing required values to the module's memory) ---

  /**
   * @brief Sets a required value for one of the aggregator's aggregated I/O points.
   * This function finds the corresponding child module and redirects the call to it.
   *
   * @param io_definition_id The unique ID of the AGGREGATED I/O point.
   * @param value The value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on successful redirection.
   */
  PlcErrorCodes setRequiredValueById(uint32_t io_definition_id, uint64_t value) override;

  PlcErrorCodes setAllRequiredValues(
      const std::vector<std::pair<uint32_t, uint64_t>> &required_values) override;

  /**
   * @brief @copydoc IModule::getInputBitValue
   */
  PlcErrorCodes getInputBitValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setInputBitValue
   */
  PlcErrorCodes setInputBitValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getInputBitUpdateValue
   */
  PlcErrorCodes getInputBitUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setInputBitUpdateValue
   */
  PlcErrorCodes setInputBitUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getInputRegisterValue
   */
  PlcErrorCodes getInputRegisterValue(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setInputRegisterValue
   */
  PlcErrorCodes setInputRegisterValue(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getInputRegisterUpdateValue
   */
  PlcErrorCodes getInputRegisterUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setInputRegisterUpdateValue
   */
  PlcErrorCodes setInputRegisterUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getOutputBitValue
   */
  PlcErrorCodes getOutputBitValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setOutputBitValue
   */
  PlcErrorCodes setOutputBitValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getOutputBitRequired
   */
  PlcErrorCodes getOutputBitRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setOutputBitRequired
   */
  PlcErrorCodes setOutputBitRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getOutputBitUpdateValue
   */
  PlcErrorCodes getOutputBitUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setOutputBitUpdateValue
   */
  PlcErrorCodes setOutputBitUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getOutputBitUpdateRequired
   */
  PlcErrorCodes getOutputBitUpdateRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setOutputBitUpdateRequired
   */
  PlcErrorCodes setOutputBitUpdateRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getOutputRegisterValue
   */
  PlcErrorCodes getOutputRegisterValue(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setOutputRegisterValue
   */
  PlcErrorCodes setOutputRegisterValue(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getOutputRegisterRequired
   */
  PlcErrorCodes getOutputRegisterRequired(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setOutputRegisterRequired
   */
  PlcErrorCodes setOutputRegisterRequired(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getOutputRegisterUpdateValue
   */
  PlcErrorCodes getOutputRegisterUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setOutputRegisterUpdateValue
   */
  PlcErrorCodes setOutputRegisterUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getOutputRegisterUpdateRequired
   */
  PlcErrorCodes getOutputRegisterUpdateRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setOutputRegisterUpdateRequired
   */
  PlcErrorCodes setOutputRegisterUpdateRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getSecureStateBitValue
   */
  PlcErrorCodes getSecureStateBitValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateBitValue
   */
  PlcErrorCodes setSecureStateBitValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getSecureStateBitRequired
   */
  PlcErrorCodes getSecureStateBitRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateBitRequired
   */
  PlcErrorCodes setSecureStateBitRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getSecureStateBitUpdateValue
   */
  PlcErrorCodes getSecureStateBitUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateBitUpdateValue
   */
  PlcErrorCodes setSecureStateBitUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getSecureStateBitUpdateRequired
   */
  PlcErrorCodes getSecureStateBitUpdateRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateBitUpdateRequired
   */
  PlcErrorCodes setSecureStateBitUpdateRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getSecureStateRegisterValue
   */
  PlcErrorCodes getSecureStateRegisterValue(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateRegisterValue
   */
  PlcErrorCodes setSecureStateRegisterValue(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getSecureStateRegisterRequired
   */
  PlcErrorCodes getSecureStateRegisterRequired(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateRegisterRequired
   */
  PlcErrorCodes setSecureStateRegisterRequired(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getSecureStateRegisterUpdateValue
   */
  PlcErrorCodes getSecureStateRegisterUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateRegisterUpdateValue
   */
  PlcErrorCodes setSecureStateRegisterUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getSecureStateRegisterUpdateRequired
   */
  PlcErrorCodes getSecureStateRegisterUpdateRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setSecureStateRegisterUpdateRequired
   */
  PlcErrorCodes setSecureStateRegisterUpdateRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getConfigBitValue
   */
  PlcErrorCodes getConfigBitValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setConfigBitValue
   */
  PlcErrorCodes setConfigBitValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getConfigBitRequired
   */
  PlcErrorCodes getConfigBitRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setConfigBitRequired
   */
  PlcErrorCodes setConfigBitRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getConfigBitUpdateValue
   */
  PlcErrorCodes getConfigBitUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setConfigBitUpdateValue
   */
  PlcErrorCodes setConfigBitUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getConfigBitUpdateRequired
   */
  PlcErrorCodes getConfigBitUpdateRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setConfigBitUpdateRequired
   */
  PlcErrorCodes setConfigBitUpdateRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getConfigRegisterValue
   */
  PlcErrorCodes getConfigRegisterValue(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setConfigRegisterValue
   */
  PlcErrorCodes setConfigRegisterValue(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getConfigRegisterRequired
   */
  PlcErrorCodes getConfigRegisterRequired(uint16_t address, uint64_t &value) const override;

  /**
   * @brief @copydoc IModule::setConfigRegisterRequired
   */
  PlcErrorCodes setConfigRegisterRequired(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::getConfigRegisterUpdateValue
   */
  PlcErrorCodes getConfigRegisterUpdateValue(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setConfigRegisterUpdateValue
   */
  PlcErrorCodes setConfigRegisterUpdateValue(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::getConfigRegisterUpdateRequired
   */
  PlcErrorCodes getConfigRegisterUpdateRequired(uint16_t address, bool &value) const override;

  /**
   * @brief @copydoc IModule::setConfigRegisterUpdateRequired
   */
  PlcErrorCodes setConfigRegisterUpdateRequired(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::requestOutputBit
   */
  PlcErrorCodes requestOutputBit(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::requestOutputRegister
   */
  PlcErrorCodes requestOutputRegister(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::requestSecureStateBit
   */
  PlcErrorCodes requestSecureStateBit(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::requestSecureStateRegister
   */
  PlcErrorCodes requestSecureStateRegister(uint16_t address, uint64_t value) override;

  /**
   * @brief @copydoc IModule::requestConfigBit
   */
  PlcErrorCodes requestConfigBit(uint16_t address, bool value) override;

  /**
   * @brief @copydoc IModule::requestConfigRegister
   */
  PlcErrorCodes requestConfigRegister(uint16_t address, uint64_t value) override;

  // --- Atomic Update Fetchers (Redirected) ---

  /**
   * @brief @copydoc IModule::getInputBitUpdateIfChanged
   */
  PlcErrorCodes getInputBitUpdateIfChanged(uint16_t address, bool &out_value,
                                           bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getOutputBitUpdateIfChanged
   */
  PlcErrorCodes getOutputBitUpdateIfChanged(uint16_t address, bool &out_value,
                                            bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getInputRegisterUpdateIfChanged
   */
  PlcErrorCodes getInputRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getOutputRegisterUpdateIfChanged
   */
  PlcErrorCodes getOutputRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                 bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getSecureStateBitUpdateIfChanged
   */
  PlcErrorCodes getSecureStateBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                 bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getSecureStateRegisterUpdateIfChanged
   */
  PlcErrorCodes getSecureStateRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                      bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getConfigBitUpdateIfChanged
   */
  PlcErrorCodes getConfigBitUpdateIfChanged(uint16_t address, bool &out_value,
                                            bool &has_changed) override;

  /**
   * @brief @copydoc IModule::getConfigRegisterUpdateIfChanged
   */
  PlcErrorCodes getConfigRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                 bool &has_changed) override;

  private:
  /**
   * @brief A recursive mutex to ensure thread-safe access to the module's state.
   */
  mutable std::recursive_mutex _mutex;

  // --- Module Identity ---

  /**
   * @brief Primary Key from the 'devices' table.
   */
  uint32_t _module_id;

  /**
   * @brief Foreign Key to 'model_config' table (the model ID).
   */
  uint32_t _model_id;

  /**
   * @brief The physical address of the module on its communication channel (e.g., SPI slot, Modbus
   * Slave ID).
   */
  std::string _address_on_channel;

  /**
   * @brief The type of channel this module is connected to (e.g., 'spi', 'rs485', 'tcp').
   */
  std::string _channel;

  /**
   * @brief The communication protocol used by this module (e.g., 'borrell-spi', 'modbus-rtu').
   */
  std::string _protocol;

  /**
   * @brief Descriptive name of the module from the 'devices' table.
   */

  std::string _module_name;

  /**
   * @brief Module connection status 0 = disconnected, 1 = disconnected and modified in DB, 2 =
   * connected, 3 = connected and modified in DB
   */
  mutable uint8_t _connected;

  /**
   * @brief The modules of which this aggregator is composed.
   */
  std::vector<IModulePtr> _child_modules;

  /**
   * @brief A copy of the raw mappings loaded from the database.
   */
  std::vector<AggregatedMappingEntry> _raw_mappings;

  // --- Translation Maps ---

  /**
   * @brief The internal mapping tables for redirecting aggregated inputs bits to child modules.
   */
  std::map<uint16_t, MappingTarget> _input_bit_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated output bits to child modules.
   */
  std::map<uint16_t, MappingTarget> _output_bit_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated secure state bits to child
   * modules.
   */
  std::map<uint16_t, MappingTarget> _secure_state_bit_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated input registers to child modules.
   */
  std::map<uint16_t, MappingTarget> _input_register_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated output registers to child
   * modules.
   */
  std::map<uint16_t, MappingTarget> _output_register_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated secure state registers to child
   * modules.
   */
  std::map<uint16_t, MappingTarget> _secure_state_register_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated configuration bits to child
   * modules.
   */
  std::map<uint16_t, MappingTarget> _config_bit_map;

  /**
   * @brief The internal mapping tables for redirecting aggregated configuration registers to child
   * modules.
   */
  std::map<uint16_t, MappingTarget> _config_register_map;

  /**
   * @brief In-memory map for I/O definitions by their unique ID.
   */
  std::map<uint32_t, IoDefinition> _io_definition_map;

  /**
   * @brief Generic template method to redirect a 'get' call to the correct child module.
   * @tparam T The type of the value to get (bool or uint16_t).
   * @param address The aggregated address to look up.
   * @param map The specific translation map to use.
   * @param[out] value The variable to store the result.
   * @param func A lambda function representing the specific get method to call on the child module.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  template <typename T, typename Func>
  PlcErrorCodes _getRedirect(uint16_t address, const std::map<uint16_t, MappingTarget> &map,
                             T &value, Func func) const;

  /**
   * @brief Generic template method to redirect a 'set' call to the correct child module.
   * @tparam T The type of the value to set (bool or uint16_t).
   * @param address The aggregated address to look up.
   * @param map The specific translation map to use.
   * @param value The value to set.
   * @param func A lambda function representing the specific set method to call on the child module.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  template <typename T, typename Func>
  PlcErrorCodes _setRedirect(uint16_t address, const std::map<uint16_t, MappingTarget> &map,
                             T value, Func func);
};