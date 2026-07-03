/**
 * @file Iprotocol.hpp
 * @author Diego Arcos Sapena
 * @brief Abstract base class for all communication protocols.
 * @version a-1.0.0
 * @date 2024/07/11
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <cstdint>
#include <memory>

#include "../../common/errors.hpp"
#include "../../hardware/IModule.hpp"
#include "../channels/Ichannel.hpp"

/**
 * @class Protocol
 * @brief Abstract interface defining the contract for all communication protocols.
 *
 * This class provides a set of pure virtual functions that represent high-level
 * operations (e.g., "read a bit", "get module info"). Concrete classes like
 * ProtocolSPIV0 or ProtocolModbusRTU will implement this
 * interface, translating these generic commands into protocol-specific byte frames.
 * It depends on the abstract Channel interface for physical data transport.
 */
class Protocol {
  public:
  /**
   * @brief Constructs a new OsoLogicPLC Protocol object.
   * @param[in] channel A shared pointer to an object that implements the Channel interface.
   */
  Protocol(ChannelPtr channelPtr);

  /**
   * @brief Virtual destructor to ensure proper cleanup of derived classes.
   */
  virtual ~Protocol() = default;

  // --- PURE VIRTUAL FUNCTIONS (The Core Contract) ---
  // All derived protocol classes MUST implement these fundamental methods.

  /**
   * @brief Establishes a connection to the protocol device.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes connect() = 0;

  /**
   * @brief Disconnects from the protocol device.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes disconnect() = 0;

  /**
   * @brief Reads the state of a single bit from a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel
   * (e.g., Modbus Slave ID, IP address).
   * @param address The address of the bit within the device (0-based offset).
   * @param value Reference to a boolean variable to store the read bit value.
   * @param hardware_access The I/O direction (input/output) to determine the correct function in
   * lower layers.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes readBit(const std::string &address_on_channel, uint16_t address,
                                bool &value, uint8_t hardware_access) = 0;

  /**
   * @brief Reads multiple bits starting from a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param start_address The starting address of the bits to read (0-based offset).
   * @param dest Pointer to an array where the read bit values will be stored.
   * @param quantity The number of bits to read.
   * @param hardware_access The I/O direction (input/output) to determine the correct function in
   * lower layers.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes readBits(const std::string &address_on_channel, uint16_t start_address,
                                 bool *dest, uint16_t quantity, uint8_t hardware_access) = 0;

  /**
   * @brief Writes the state of a single bit to a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param address The address of the bit within the device (0-based offset).
   * @param value The boolean value to write (true for ON, false for OFF).
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes writeBit(const std::string &address_on_channel, uint16_t address,
                                 bool value) = 0;

  /**
   * @brief Writes multiple bits starting from a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param start_address The starting address of the bits to write (0-based offset).
   * @param values Pointer to an array containing the boolean values to write.
   * @param quantity The number of bits to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes writeBits(const std::string &address_on_channel, uint16_t start_address,
                                  bool *values, uint16_t quantity) = 0;

  /**
   * @brief Reads the value of a single 16-bit register from a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param address The address of the register within the device (0-based offset).
   * @param value Reference to a uint16_t variable to store the read register value.
   * @param hardware_access The I/O direction (input/output) to determine the correct function in
   * lower layers.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes readRegister(const std::string &address_on_channel, uint16_t address,
                                     uint16_t &value, uint8_t hardware_access) = 0;

  /**
   * @brief Reads multiple registers starting from a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param start_address The starting address of the registers to read (0-based offset).
   * @param dest Pointer to an array where the read register values will be stored.
   * @param quantity The number of registers to read.
   * @param hardware_access The I/O direction (input/output) to determine the correct function in
   * lower layers.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes readRegisters(const std::string &address_on_channel, uint16_t start_address,
                                      uint16_t *dest, uint16_t quantity,
                                      uint8_t hardware_access) = 0;

  /**
   * @brief Writes a value to a single 16-bit register at a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param address The address of the register within the device (0-based offset).
   * @param value The uint16_t value to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes writeRegister(const std::string &address_on_channel, uint16_t address,
                                      uint16_t value) = 0;

  /**
   * @brief Writes multiple registers starting from a specified address.
   * @param address_on_channel A string identifier for the specific device/slave on the channel.
   * @param start_address The starting address of the registers to write (0-based offset).
   * @param values Pointer to an array containing the values to write.
   * @param quantity The number of registers to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  virtual PlcErrorCodes writeRegisters(const std::string &address_on_channel,
                                       uint16_t start_address, uint16_t *values,
                                       uint16_t quantity) = 0;

  /**
   * @brief Performs a basic communication test based on the module's first I/O block.
   * @param address_on_channel A string identifier for the specific device/slave.
   * @param first_block The first IO_Block defined for the module, to determine what to read.
   * @return PlcErrorCodes::PLC_SUCCESS if the device responds correctly.
   */
  virtual PlcErrorCodes pingDevice(std::string &address_on_channel, IO_Block &first_block) = 0;

  protected:
  /**
   * @brief A shared pointer to the underlying communication channel.
   */
  ChannelPtr _channelPtr;
};

/**
 * @brief ProtocolPtr is a shared pointer type for Protocol.
 */
using ProtocolPtr = std::shared_ptr<Protocol>;
