/**
 * @file modbus_protocol.hpp
 * @author Diego Arcos Sapena
 * @brief Abstract base class for Modbus communication protocols.
 * @version a-1.0.2
 * @date 2024/07/26
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <mutex>
#include <vector>

#include "../Iprotocol.hpp"  // Main protocol interface

// Modbus function codes
#define MODBUS_READ_COILS 0x01
#define MODBUS_READ_DISCRETE_INPUTS 0x02
#define MODBUS_READ_HOLDING_REGISTERS 0x03
#define MODBUS_READ_INPUT_REGISTERS 0x04
#define MODBUS_WRITE_SINGLE_COIL 0x05
#define MODBUS_WRITE_SINGLE_REGISTER 0x06
#define MODBUS_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_WRITE_MULTIPLE_REGISTERS 0x10

/**
 * @class ModbusProtocol
 * @brief Implements the common Modbus logic (PDU creation) and defines a contract
 * for protocol-specific framing (ADU handling).
 */
class ModbusProtocol : public Protocol {
  public:
  /**
   * @brief Constructs a new ModbusProtocol object.
   * @param[in] channelPtr A shared pointer to a communication channel.
   * @param[in] bus_mutex A shared pointer to a mutex for bus synchronization at a protocol level.
   * @param[in] timeout_ms Timeout for Modbus operations in milliseconds.
   */
  ModbusProtocol(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex, uint32_t timeout_ms);

  /**
   * @brief Virtual destructor.
   */
  virtual ~ModbusProtocol() = default;

  /**
   * @brief Establishes a connection to the protocol device.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes connect() override;

  /**
   * @brief Disconnects from the protocol device.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes disconnect() override;

  /**
   * @brief Reads the state of a single bit, deciding whether to read a Coil or a Discrete Input.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] address The address of the bit to read (0-based offset).
   * @param[out] value Reference to a boolean variable to store the read bit value.
   * @param[in] hardware_access The I/O direction to determine the correct Modbus function.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes readBit(const std::string &address_on_channel, uint16_t address, bool &value,
                        uint8_t hardware_access) override;

  /**
   * @brief Reads multiple bits starting from a specified address.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] start_address The starting address of the bits to read.
   * @param[out] dest Pointer to an array where the read bit values will be stored.
   * @param[in] quantity The number of bits to read.
   * @param[in] hardware_access The I/O direction to determine the correct Modbus function.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes readBits(const std::string &address_on_channel, uint16_t start_address, bool *dest,
                         uint16_t quantity, uint8_t hardware_access) override;

  /**
   * @brief Writes the state of a single bit, which always maps to a Modbus Coil.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] address The address of the bit to write.
   * @param[in] value The boolean value to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes writeBit(const std::string &address_on_channel, uint16_t address,
                         bool value) override;

  /**
   * @brief Writes multiple bits starting from a specified address.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] start_address The starting address for the write operation.
   * @param[in] values Pointer to an array of boolean values to write.
   * @param[in] quantity The number of bits to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes writeBits(const std::string &address_on_channel, uint16_t start_address,
                          bool *values, uint16_t quantity) override;

  /**
   * @brief Reads the value of a single 16-bit register, deciding between Input or Holding register.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] address The address of the register to read.
   * @param[out] value Reference to a uint16_t variable to store the read register value.
   * @param[in] hardware_access The I/O direction to determine the correct Modbus function.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes readRegister(const std::string &address_on_channel, uint16_t address,
                             uint16_t &value, uint8_t hardware_access) override;

  /**
   * @brief Reads multiple 16-bit registers starting from a specified address.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] start_address The starting address of the registers to read.
   * @param[out] dest Pointer to an array where the read register values will be stored.
   * @param[in] quantity The number of registers to read.
   * @param[in] hardware_access The I/O direction to determine the correct Modbus function.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes readRegisters(const std::string &address_on_channel, uint16_t start_address,
                              uint16_t *dest, uint16_t quantity, uint8_t hardware_access) override;

  /**
   * @brief Writes a value to a single 16-bit register, which always maps to a Modbus Holding
   * Register.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] address The address of the register to write.
   * @param[in] value The uint16_t value to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes writeRegister(const std::string &address_on_channel, uint16_t address,
                              uint16_t value) override;

  /**
   * @brief Writes multiple 16-bit registers starting from a specified address.
   * @param[in] address_on_channel A string identifier for the specific device/slave on the channel.
   * @param[in] start_address The starting address for the write operation.
   * @param[in] values Pointer to an array of uint16_t values to write.
   * @param[in] quantity The number of registers to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes writeRegisters(const std::string &address_on_channel, uint16_t start_address,
                               uint16_t *values, uint16_t quantity) override;

  /**
   * @brief Performs a basic communication test based on the module's first I/O block.
   * @param address_on_channel A string identifier for the specific device/slave.
   * @param first_block The first IO_Block defined for the module, to determine what to read.
   * @return PlcErrorCodes::PLC_SUCCESS if the device responds correctly.
   */
  PlcErrorCodes pingDevice(std::string &address_on_channel, IO_Block &first_block) override;

  protected:
  /**
   * @brief Contract for child classes: they must implement how the entire frame (ADU) is sent and
   * received.
   * @param[in] unit_id The Modbus slave ID.
   * @param[in] pdu The Protocol Data Unit (Function Code + Data).
   * @param[out] response_pdu The response PDU received.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  virtual PlcErrorCodes sendAndReceiveAdu(uint8_t unit_id, const std::vector<uint8_t> &pdu,
                                          std::vector<uint8_t> &response_pdu) = 0;

  /**
   * Mutex for bus synchronization at a protocol level.
   * This mutex is used to ensure that only one module can access the bus at a time (while
   * communicating, exchanging messages).
   */
  std::shared_ptr<std::mutex> _bus_mutex;

  /**
   * @brief Timeout for Modbus operations in milliseconds.
   */
  uint32_t _timeout_ms;
};