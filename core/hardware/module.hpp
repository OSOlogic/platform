/**
 * @file module.hpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Module class (header)
 * @version a-1.0.0
 * @date 2024/08/26
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../common/errors.hpp"
#include "../communication/protocols/Iprotocol.hpp"
#include "IModule.hpp"
#include "plc.hpp"

/* Value, required and update flags combo */
template <typename T>
struct DataUnit {
  // Comes from hardware
  T value;
  bool updateValue;

  // Comes from database
  T required;
  bool updateRequired;
  std::chrono::steady_clock::time_point request_timestamp;  // Just for debugging
};
/**
 * @brief Typedef for bit basic unit
 */
using bitUnit = DataUnit<bool>;

/**
 * @brief Typedef for register basic unit
 */
using registerUnit = DataUnit<uint64_t>;

/* Struct to unify both bits and registers */
struct moduleValues {
  uint16_t value;
  uint8_t type;
};

/**
 * @brief A simple helper struct to hold data from the 'devices' table
 */
struct DeviceConfig {
  uint32_t module_id;
  std::string module_name;
  std::string channel;  // Changed from channel_type
  std::string protocol;
  std::string connection_string;
  uint16_t max_read_bit_block_size;
  uint16_t max_read_register_block_size;
  uint16_t max_write_bit_block_size;
  uint16_t max_write_register_block_size;
  std::string address;
  uint32_t model_id = 0;       // Default to 0 if NULL
  uint32_t timeout_ms = 1000;  // Default to 1000 ms
};

/**
 * @class Module
 * @brief Represents a PLC module with its properties and methods to interact with it.
 *
 * This class provides methods to initialize the module, read and write bits and registers,
 * manage the module's UUID, WDT, and other properties.
 */
class Module final : public IModule {
  public:
  /**
   * @brief Construct a new OsoLogicPLC Module object
   * @param[in] module_id Unique ID for this module.
   * @param[in] model_id Model ID for this module.
   * @param[in] module_name Descriptive name for this module.
   * @param[in] address_on_channel Address string used by the protocol to identify the device (e.g.,
   * Modbus Slave ID, IP address).
   * @param[in] _max_read_bit_block_size Maximum number of bits that can be read in a single block.
   * @param[in] _max_read_register_block_size Maximum number of registers that can be read in a
   * single block.
   * @param[in] _max_write_bit_block_size Maximum number of bits that can be written in a single
   * block.
   * @param[in] _max_write_register_block_size Maximum number of registers that can be written in a
   * single block.
   * @param[in] channel_type Type of channel (e.g., "modbus-rtu", "modbus-tcp", "spi",
   * "aggregated").
   * @param[in] protocol Protocol used for communication (e.g., "modbus", "custom-protocol").
   * @param[in] backend Smart pointer to protocol object.
   */
  Module(uint32_t module_id, uint32_t model_id, const std::string &module_name,
         const std::string &address_on_channel, uint16_t _max_read_bit_block_size,
         uint16_t _max_read_register_block_size, uint16_t _max_write_bit_block_size,
         uint16_t _max_write_register_block_size, const std::string &channel,
         const std::string &protocol, ProtocolPtr backend);

  // Lifecycle & Control
  /**
   * @brief Initialize module parameters;
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes initialize() override;

  /**
   * @brief @copydoc IModule::sync
   */
  PlcErrorCodes sync() override;

  /**
   * @brief Set module to a safe state;
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setSafeState() override;

  /**
   * @brief @copydoc IModule::freeModule
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

  // --- State & Status ---

  /**
   * @brief Gets the hardware UUID.
   * For an aggregated module, it could return 0 or a composite value.
   * @param[out] uuid The unique identifier.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getUuid(uint32_t &uuid) const;

  /**
   * @brief Performs a single attempt to connect, verify, and configure a module.
   *
   * This function is a "one-shot" tool used by tasks to establish a connection.
   * It loads the module's configuration from the database, attempts to communicate
   * with the physical hardware (e.g., connect, ping, read hardware IDs), and
   * prepares the module's in-memory structures. It does NOT contain a loop.
   * @return PlcErrorCodes::PLC_SUCCESS on a successful identification, or an error code on failure.
   */
  PlcErrorCodes identifyModule();

  /**
   * @brief Sets the I/O block structure that defines this module.
   * @param[in] blocks A vector of IO_Block structs.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes setIOBlocks(std::vector<IO_Block> &blocks);

  /**
   * @brief @copydoc IModule::getAllIoDefinitions
   */
  PlcErrorCodes getAllIoDefinitions(std::vector<IoDefinition> &out_defs) const override;

  /**
   * @brief Collects all values that have changed in memory and need to be synchronized to the
   * database. This optimization acquires the mutex only once to traverse all logical points.
   * @param[in] mode The current operation mode.
   * @param[in] force_all If true, returns all points behaving as if they had changed.
   * @param[out] changed_values A list of changed io_definition_id and their new values.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes collectChangedValues(OperationMode mode, bool force_all,
                                     std::vector<std::pair<uint32_t, uint64_t>> &changed_values);

  /**
   * @brief Get Module connection status.
   * @param[out] connected Module connection status.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes getConnected(uint8_t &connected) const override;

  /**
   * @brief Get module backend.
   * @param[out] backend Module backend.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes getBackend(ProtocolPtr &backend) const;

  /**
   * @brief Sets or replaces the backend protocol handler for this module.
   * @param[in] backend The new protocol handler.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes setBackend(ProtocolPtr backend);

  /**
   * @brief @copydoc IModule::setConnected
   */
  PlcErrorCodes setConnected(uint8_t connected) override;

  /**
   * @brief Set module UUID.
   * @param[in] uuid Unique identifier of the module.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setUuid(uint32_t uuid);

  /**
   * @brief Get force full sync flag.
   * @param[out] force_full_sync Force full sync flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes getForceFullSync(bool &force_full_sync) const;

  /**
   * @brief Set force full sync flag.
   * @param[in] force_full_sync Force full sync flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setForceFullSync(bool force_full_sync);

  /**
   * @brief Commands the module to perform a full data refresh.
   * This forces all I/O points to be re-sent to the database on the next sync cycle.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes forceFullSync() override;

  /**
   * @brief Get the physical address string of the module on its channel.
   * @param[out] address_on_channel The physical address of the module on its channel.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getAddressOnChannel(std::string &address_on_channel) const;

  /**
   * @brief Get the protocol used by this module.
   * @param[out] protocol The protocol string.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getProtocol(std::string &protocol) const;

  // --- In-Memory Data Accessors ---

  // --- High-Level Data Access API (for reading the module's in-memory state) ---
  // --- High-Level Control API (for writing required values to the module's memory) ---

  /**
   * @brief @copydoc IModule::getRequiredValueById
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

  // --- Atomic Update Fetchers ---

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
   * @brief Module mutex (thread safe).
   */
  mutable std::recursive_mutex _mutex{};

  /**
   * @brief Module UUID
   */
  uint32_t _uuid;

  /**
   * @brief Custom protocol to access module communications
   */
  ProtocolPtr _backend;

  /**
   * @brief Module connection status 0 = disconnected, 1 = disconnected and modified in DB, 2 =
   * connected, 3 = connected and modified in DB
   */
  std::atomic<uint8_t> _connected;

  /**
   * @brief Primary Key from the 'devices' table.
   */
  uint32_t _module_id;

  /**
   * @brief Foreign Key to 'model_config' table (the model ID).
   */
  uint32_t _model_id;

  /**
   * @brief Descriptive name for this module.
   */
  std::string _module_name;

  /**
   * @brief Maximum bit block size for communication when reading with this module.
   */
  uint16_t _max_read_bit_block_size;

  /**
   * @brief Maximum register block size for communication when reading with this module.
   */
  uint16_t _max_read_register_block_size;

  /**
   * @brief Maximum bit block size for communication when writing with this module.
   */
  uint16_t _max_write_bit_block_size;

  /**
   * @brief Maximum register block size for communication when writing with this module.
   */
  uint16_t _max_write_register_block_size;

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
   * @brief Force full sync with database in rtmirror table.
   * Useful when a module has just been connected and we want to ensure all data is synchronized.
   */
  std::atomic<bool> _force_full_sync;

  /**
   * @brief Complete I/O block structure for initial/full sync (ALL purposes).
   * Used when module first connects or when force_full_sync is true.
   */
  std::vector<IO_Block> _io_blocks_initial;

  /**
   * @brief Mode-optimized I/O block structure for continuous sync.
   * Built from definitions filtered by current OperationMode:
   * - EXECUTION mode: only 'standard' purpose definitions
   * - CONFIGURATION mode: only 'config' and 'secure_state' purpose definitions
   * Each block is optimally constructed from the filtered definitions.
   */
  std::vector<IO_Block> _io_blocks_sync;

  /**
   * @brief In-memory map for standard input bits.
   */
  std::map<uint16_t, bitUnit> _input_bits;

  /**
   * @brief In-memory map for standard output bits.
   */
  std::map<uint16_t, bitUnit> _output_bits;

  /**
   * @brief In-memory map for standard input registers.
   */
  std::map<uint16_t, registerUnit> _input_registers;

  /**
   * @brief In-memory map for standard output registers.
   */
  std::map<uint16_t, registerUnit> _output_registers;

  /**
   * @brief In-memory map for secure state bits.
   */
  std::map<uint16_t, bitUnit> _secure_state_bits;

  /**
   * @brief In-memory map for secure state registers.
   */
  std::map<uint16_t, registerUnit> _secure_state_registers;

  /**
   * @brief In-memory map for configuration bits of the module.
   */
  std::map<uint16_t, bitUnit> _config_bits;

  /**
   * @brief In-memory map for configuration registers of the module.
   */
  std::map<uint16_t, registerUnit> _config_registers;

  /**
   * @brief In-memory map for secure state logical address mapping (bits).
   * Maps standard logical address -> secure state logical address.
   */
  std::map<uint16_t, uint16_t> _secure_state_map_bits;

  /**
   * @brief In-memory map for secure state logical address mapping (registers).
   * Maps standard logical address -> secure state logical address.
   */
  std::map<uint16_t, uint16_t> _secure_state_map_registers;

  /**
   * @brief In-memory map for I/O definitions by their unique ID.
   */
  std::map<uint32_t, IoDefinition> _io_definition_map;

  /**
   * @brief Reads a block from hardware and updates the module's in-memory state. (HW -> MEM).
   * Readonly and writeread values in hardware
   * @param block The I/O block to be synchronized.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes _syncBlock_HW_to_Mem(const IO_Block &block);

  /**
   * @brief Writes required values from memory to the hardware for an output block. (MEM -> HW).
   * Writeread values in hardware
   * @param block The I/O block to be synchronized.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes _syncBlock_Mem_to_HW(const IO_Block &block);

  /**
   * @brief Builds optimized I/O blocks from a vector of sorted IoDefinitions.
   * Definitions MUST be pre-sorted by io_type, hardware_access, physical_address.
   * @param[in] sorted_definitions Pre-sorted vector of IoDefinitions.
   * @param[out] out_blocks Vector to be filled with the constructed IO_Block objects.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes _buildIOBlocks(const std::vector<IoDefinition> &sorted_definitions,
                               std::vector<IO_Block> &out_blocks);

  /**
   * @brief Template method to get a field from a container map.
   * @tparam T The type of the container's value.
   * @tparam FieldType The type of the field to get.
   * @param container The map container (accepts both const and non-const maps).
   * @param address The key address in the map.
   * @param member Pointer to the member field in the struct.
   * @param out Reference to store the output value.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or PlcErrorCodes::PLC_ADDRESS_NOT_FOUND if
   * address is not found.
   */
  template <typename T, typename FieldType>
  PlcErrorCodes getField(const std::map<uint16_t, T> &container, uint16_t address,
                         FieldType T::*member, FieldType &out) const;

  /**
   * @brief Template method to set a field in a container map.
   * @tparam T The type of the container's value.
   * @tparam FieldType The type of the field to set.
   * @param container The map container.
   * @param address The key address in the map.
   * @param member Pointer to the member field in the struct.
   * @param value The value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or PlcErrorCodes::PLC_ADDRESS_NOT_FOUND if
   * address is not found.
   */
  template <typename T, typename FieldType>
  PlcErrorCodes setField(std::map<uint16_t, T> &container, uint16_t address, FieldType T::*member,
                         FieldType value);

  /**
   * @brief Generic, thread-safe implementation to get a value if its update flag is set.
   * This template is the core of the atomic get-and-reset operations.
   * @tparam T The data type (bool or uint16_t).
   * @tparam ValueT The data type of the value itself (e.g., bool, uint16_t).
   * @param container The specific map to operate on (e.g., _input_bits, _output_registers).
   * @param address The logical address of the I/O point.
   * @param[out] out_value The retrieved value.
   * @param[out] has_changed True if an update was pending, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  template <typename T, typename ValueT>
  PlcErrorCodes getUpdateIfChanged(std::map<uint16_t, T> &container, uint16_t address,
                                   ValueT &out_value, bool &has_changed);
};

/**
 * @brief ModulePtr is a shared pointer type for Module.
 */
using ModulePtr = std::shared_ptr<Module>;
