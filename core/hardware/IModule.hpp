/**
 * @file IModule.hpp
 * @author Diego Arcos Sapena
 * @brief Abstract interface for all PLC module objects.
 * @version 1.1.0
 * @date 2025/08/30
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../common/errors.hpp"

/**
 * @brief Represents the complete, static definition of a single I/O point,
 * reflecting the model_io_definition and module_io_config tables. This struct is the "passport"
 * for every I/O point in the system.
 */
struct IoDefinition {
  uint32_t io_definition_id;  // The unique primary key
  uint32_t fk_model_id;

  // --- Logical Attributes ---
  uint16_t logical_address;
  uint8_t io_type;  // 1: bit, 2: register
  uint8_t purpose;  // 1: standard, 2: secure_state, 3: config
  uint8_t register_count;
  double scale_factor;
  double offset;
  uint8_t endianess;  // 1: big, 2: little

  // --- Permission & Capability Model ---
  uint8_t hardware_access;  // 1: readonly (input), 2: readwrite (output)

  // --- Physical Access Attributes ---
  uint16_t physical_address;
  uint8_t access_method;  // 1: direct, 2: bitmask
  uint8_t bitmask_offset;
  std::string user_label;
};

/**
 * @brief Describes a physically contiguous block of I/O points that share
 * the same characteristics (type, purpose, access method). This is the
 * fundamental unit for efficient, low-level hardware communication.
 */
struct IO_Block {
  // --- Block Characteristics ---
  uint8_t io_type;          // 1: bit, 2: register
  uint8_t hardware_access;  // 1: input (read_only at a hardware level), 2: output (read_write at a
                            // hardware level)

  // --- Hardware Access Info ---
  uint16_t physical_start_address;  // The first physical address in the block
  uint16_t quantity;                // How many contiguous points are in this block

  // --- Link back to the full definitions ---
  // Contains all IoDefinition structs for the points within this block.
  // This allows us to map the data read from hardware back to the correct logical addresses.
  std::vector<IoDefinition> contained_definitions;
};

/**
 * @class IModule
 * @brief Defines the public API contract for any logical module within the PLC system.
 *
 * This pure abstract class is the core of the Composite design pattern, allowing
 * client code to interact with all module types in a uniform way.
 */
class IModule {
  public:
  /**
   * @brief Virtual destructor to ensure proper cleanup of derived classes.
   */
  virtual ~IModule() = default;

  // --- Lifecycle & Control ---

  /**
   * @brief Performs the main initialization logic for the module.
   * For a PhysicalModule, this involves hardware identification (ping, UUID check).
   * For an AggregatorModule, this involves building its internal translation map.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes initialize() = 0;

  /**
   * @brief Triggers one full synchronization cycle for the module.
   * If physical, it communicates with hardware. If virtual, it orchestrates its children.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes sync() = 0;

  /**
   * @brief Commands the module and all its children (if any) to enter a safe state.
   * This typically involves setting all outputs to a predefined non-hazardous value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSafeState() = 0;

  /**
   * @brief Resets the module's internal state to an uninitialized/disconnected status.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes freeModule() = 0;

  // --- Identity & Configuration ---

  /**
   * @brief Gets the unique ID of the module from the database.
   * @param[out] moduleId The integer module ID.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getModuleId(uint32_t &moduleId) const = 0;

  /**
   * @brief Gets the descriptive name of the module.
   * @param[out] name The string name of the module.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getModuleName(std::string &name) const = 0;

  /**
   * @brief Gets the model ID of the module.
   * @param[out] modelId The integer model ID.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getModelId(uint32_t &modelId) const = 0;

  /**
   * @brief Get the type of channel this module is connected to.
   * @param[out] channel The channel type string.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getChannel(std::string &channel) const = 0;

  /**
   * @brief Gets all logical I/O definitions exposed by this module.
   * @details For a physical module, these are its hardware points.
   * For a virtual module, these are its aggregated virtual points.
   * @param[out] out_defs The vector to be filled with the IoDefinition structs.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getAllIoDefinitions(std::vector<IoDefinition> &out_defs) const = 0;

  // --- State & Status ---

  /**
   * @brief Checks if the module is considered connected and operational.
   * For an aggregated module, this means that all its children are connected.
   * @param[out] connected The connection status.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConnected(uint8_t &connected) const = 0;

  /**
   * @brief Set connection status.
   * @param[out] connected 0 = disconnected, 1 = disconnected and modified in DB, 2 = connected, 3 =
   * connected and modified in DB
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  virtual PlcErrorCodes setConnected(uint8_t connected) = 0;

  /**
   * @brief Commands the module and all its children (if any) to perform a full data refresh.
   * This forces all I/O points to be re-sent to the database on the next sync cycle.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes forceFullSync() = 0;

  // --- In-Memory Data Accessors ---

  // --- High-Level Data Access API (for reading the module's in-memory state) ---
  // --- High-Level Control API (for writing required values to the module's memory) ---

  /**
   * @brief Sets the 'required' value for a standard output point by its io_definition_id.
   * @param io_definition_id The unique identifier for the I/O definition.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setRequiredValueById(uint32_t io_definition_id, uint64_t value) = 0;

  /**
   * @brief Sets multiple 'required' values in a single operation.
   * This allows derived classes to acquire a lock once for all updates.
   * @param required_values A vector of pairs containing io_definition_id and value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setAllRequiredValues(
      const std::vector<std::pair<uint32_t, uint64_t>> &required_values) = 0;

  /**
   * @brief Gets the last-known value of a standard input bit from the memory buffer.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getInputBitValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the value of a standard input bit in the memory buffer.
   * @param address The logical address of the bit.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setInputBitValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the update flag for a standard input bit.
   * @details This flag indicates that the bit's 'value' has changed and needs to be synchronized
   * with the database.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getInputBitUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a standard input bit.
   * @param address The logical address of the bit.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setInputBitUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the last-known value of a standard input register from the memory buffer.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getInputRegisterValue(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the value of a standard input register in the memory buffer.
   * @param address The logical address of the register.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setInputRegisterValue(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the update flag for a standard input register.
   * @details This flag indicates that the register's 'value' has changed and needs to be
   * synchronized with the database.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getInputRegisterUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a standard input register.
   * @param address The logical address of the register.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setInputRegisterUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the last-known value of a standard output bit from the memory buffer.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputBitValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the value of a standard output bit in the memory buffer.
   * @param address The logical address of the bit.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputBitValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'required' value for a standard output bit.
   * @details This is the desired value that the system wants to write to the hardware.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the required value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputBitRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'required' value for a standard output bit.
   * @param address The logical address of the bit.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputBitRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the update flag for a standard output bit's value.
   * @details This flag indicates that the bit's 'value' (read from hardware) has changed and needs
   * to be synced to the database.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputBitUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a standard output bit's value.
   * @param address The logical address of the bit.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputBitUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'update required' flag for a standard output bit.
   * @details This flag indicates a new 'required' value has been set and must be written to
   * hardware on the next sync cycle.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputBitUpdateRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'update required' flag for a standard output bit.
   * @param address The logical address of the bit.
   * @param value The new state for the 'update required' flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputBitUpdateRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the last-known value of a standard output register from the memory buffer.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputRegisterValue(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the value of a standard output register in the memory buffer.
   * @param address The logical address of the register.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputRegisterValue(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the 'required' value for a standard output register.
   * @details This is the desired value that the system wants to write to the hardware.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the required value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputRegisterRequired(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the 'required' value for a standard output register.
   * @param address The logical address of the register.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputRegisterRequired(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the update flag for a standard output register's value.
   * @details This flag indicates that the register's 'value' (read from hardware) has changed and
   * needs to be synced to the database.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputRegisterUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a standard output register's value.
   * @param address The logical address of the register.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputRegisterUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'update required' flag for a standard output register.
   * @details This flag indicates a new 'required' value has been set and must be written to
   * hardware on the next sync cycle.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getOutputRegisterUpdateRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'update required' flag for a standard output register.
   * @param address The logical address of the register.
   * @param value The new state for the 'update required' flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setOutputRegisterUpdateRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the last-known value of a secure state bit from the memory buffer.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateBitValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the value of a secure state bit in the memory buffer.
   * @param address The logical address of the bit.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateBitValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'required' value for a secure state bit.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the required value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateBitRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'required' value for a secure state bit.
   * @param address The logical address of the bit.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateBitRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the update flag for a secure state bit's value.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateBitUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a secure state bit's value.
   * @param address The logical address of the bit.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateBitUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'update required' flag for a secure state bit.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateBitUpdateRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'update required' flag for a secure state bit.
   * @param address The logical address of the bit.
   * @param value The new state for the 'update required' flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateBitUpdateRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the last-known value of a secure state register from the memory buffer.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateRegisterValue(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the value of a secure state register in the memory buffer.
   * @param address The logical address of the register.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateRegisterValue(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the 'required' value for a secure state register.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the required value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateRegisterRequired(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the 'required' value for a secure state register.
   * @param address The logical address of the register.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateRegisterRequired(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the update flag for a secure state register's value.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateRegisterUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a secure state register's value.
   * @param address The logical address of the register.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateRegisterUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'update required' flag for a secure state register.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getSecureStateRegisterUpdateRequired(uint16_t address,
                                                             bool &value) const = 0;

  /**
   * @brief Sets the 'update required' flag for a secure state register.
   * @param address The logical address of the register.
   * @param value The new state for the 'update required' flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setSecureStateRegisterUpdateRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the value of a configuration bit.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigBitValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the value of a configuration bit.
   * @param address The logical address of the bit.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigBitValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'required' value for a configuration bit.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the required value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigBitRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'required' value for a configuration bit.
   * @param address The logical address of the bit.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigBitRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the update flag for a configuration bit's value.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigBitUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a configuration bit's value.
   * @param address The logical address of the bit.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigBitUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'update required' flag for a configuration bit.
   * @param address The logical address of the bit.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigBitUpdateRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'update required' flag for a configuration bit.
   * @param address The logical address of the bit.
   * @param value The new state for the 'update required' flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigBitUpdateRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the value of a configuration register.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the read value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigRegisterValue(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the value of a configuration register.
   * @param address The logical address of the register.
   * @param value The new value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigRegisterValue(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the 'required' value for a configuration register.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the required value.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigRegisterRequired(uint16_t address, uint64_t &value) const = 0;

  /**
   * @brief Sets the 'required' value for a configuration register.
   * @param address The logical address of the register.
   * @param value The desired value to be written to hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigRegisterRequired(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Gets the update flag for a configuration register's value.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigRegisterUpdateValue(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the update flag for a configuration register's value.
   * @param address The logical address of the register.
   * @param value The new state for the update flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigRegisterUpdateValue(uint16_t address, bool value) = 0;

  /**
   * @brief Gets the 'update required' flag for a configuration register.
   * @param address The logical address of the register.
   * @param[out] value The variable to store the flag's state.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes getConfigRegisterUpdateRequired(uint16_t address, bool &value) const = 0;

  /**
   * @brief Sets the 'update required' flag for a configuration register.
   * @param address The logical address of the register.
   * @param value The new state for the 'update required' flag.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes setConfigRegisterUpdateRequired(uint16_t address, bool value) = 0;

  /**
   * @brief Atomically requests a write for a standard output bit.
   * This sets the 'required' value and the 'updateRequired' flag.
   * @param address The logical address of the bit.
   * @param value The desired value to write to the hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes requestOutputBit(uint16_t address, bool value) = 0;

  /**
   * @brief Atomically requests a write for a standard output register.
   * @param address The logical address of the register.
   * @param value The desired value to write to the hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes requestOutputRegister(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Atomically requests a write for a secure state bit.
   * @param address The logical address of the bit.
   * @param value The desired value to write to the hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes requestSecureStateBit(uint16_t address, bool value) = 0;

  /**
   * @brief Atomically requests a write for a secure state register.
   * @param address The logical address of the register.
   * @param value The desired value to write to the hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes requestSecureStateRegister(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Atomically requests a write for a configuration bit.
   * @param address The logical address of the bit.
   * @param value The desired value to write to the hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes requestConfigBit(uint16_t address, bool value) = 0;

  /**
   * @brief Atomically requests a write for a configuration register.
   * @param address The logical address of the register.
   * @param value The desired value to write to the hardware.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes requestConfigRegister(uint16_t address, uint64_t value) = 0;

  /**
   * @brief Atomically checks for an update, retrieves the new value if available,
   * and resets the update flag.
   * This method ensures that a value change is read and acknowledged in a single,
   * thread-safe operation.
   * * @param address The logical address of the bit.
   * @param[out] out_value The current value of the bit.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getInputBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                   bool &has_changed) = 0;

  /**
   * @brief Atomically checks for an update, retrieves the new value if available,
   * and resets the update flag.
   * * @param address The logical address of the bit.
   * @param[out] out_value The current value of the bit.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getOutputBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                    bool &has_changed) = 0;

  /**
   * @brief Atomically checks for an update, retrieves the new value if available,
   * and resets the update flag.
   * * @param address The logical address of the register.
   * @param[out] out_value The current value of the register.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getInputRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                        bool &has_changed) = 0;

  /**
   * @brief Atomically checks for an update, retrieves the new value if available,
   * and resets the update flag.
   * * @param address The logical address of the register.
   * @param[out] out_value The current value of the register.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getOutputRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                         bool &has_changed) = 0;

  /**
   * @brief Atomically checks for a secure state update, retrieves the new value if available,
   * and resets the update flag.
   * * @param address The logical address of the bit.
   * @param[out] out_value The current value of the bit.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getSecureStateBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                         bool &has_changed) = 0;

  /**
   * @brief Atomically checks for a secure state update, retrieves the new value if available,
   * and resets the update flag.
   * * @param address The logical address of the register.
   * @param[out] out_value The current value of the register.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getSecureStateRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                              bool &has_changed) = 0;

  /**
   * @brief Atomically checks for a configuration update, retrieves the new value if available,
   * and resets the update flag.
   * @param address The logical address of the bit.
   * @param[out] out_value The current value of the bit.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes getConfigBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                    bool &has_changed) = 0;

  /**
   * @brief Atomically checks for a configuration update, retrieves the new value if available,
   * and resets the update flag.
   * @param address The logical address of the register.
   * @param[out] out_value The current value of the register.
   * @param[out] has_changed True if an update was pending and retrieved, false otherwise.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on
   */
  virtual PlcErrorCodes getConfigRegisterUpdateIfChanged(uint16_t address, uint64_t &out_value,
                                                         bool &has_changed) = 0;
};

/**
 * @brief A shared pointer to an IModule object. This is the primary way modules
 * should be referenced and passed throughout the system to manage ownership.
 */
using IModulePtr = std::shared_ptr<IModule>;