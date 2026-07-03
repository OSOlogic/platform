/**
 * @file spi_protocol.hpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Communication protocol over SPI (header)
 * @version a-1.0.0
 * @date 2024/08/19
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <inttypes.h>

#include <map>
#include <mutex>

#include "../../../common/errors.hpp"
#include "../Iprotocol.hpp"

/* BUFFERS */

#define PLC_REQUEST_BUFFER_SIZE 1024
#define PLC_RESPONSE_BUFFER_SIZE 1024
#define PLC_DATA_BUFFER_SIZE 1024

/* FUNCIONS */

#define PLC_FUNCTION_GET_MODULE_INFO 0x01
#define PLC_FUNCTION_READ_SINGLE_BIT 0x02
#define PLC_FUNCTION_WRITE_SINGLE_BIT 0x03
#define PLC_FUNCTION_READ_SINGLE_REGISTER 0x04
#define PLC_FUNCTION_WRITE_SINGLE_REGISTER 0x05
#define PLC_FUNCTION_READ_MULTIPLE_BITS 0x06
#define PLC_FUNCTION_WRITE_MULTIPLE_BITS 0x07
#define PLC_FUNCTION_READ_MULTIPLE_REGISTERS 0x08
#define PLC_FUNCTION_WRITE_MULTIPLE_REGISTERS 0x09
// #define PLC_FUNCTION_GET_MODULE_VERSION          0x0A
#define PLC_FUNCTION_READ_UUID 0x0A
#define PLC_FUNCTION_SET_UUID 0x0B
#define PLC_FUNCTION_READ_WDT 0x0C
#define PLC_FUNCTION_SET_WDT 0x0D

/**
 * @brief Definition of protocol over SPI used to control PLC modules
 */
class ProtocolSPIV0 final : public Protocol {
  public:
  /**
   * @brief Construct a new ProtocolSPIV0 object.
   * @param channel A shared pointer to an object that implements the Channel interface.
   * @param bus_mutex A shared pointer to a mutex for bus synchronization at a protocol level.
   */
  explicit ProtocolSPIV0(ChannelPtr channel, std::shared_ptr<std::mutex> bus_mutex);

  /**
   * @brief Destroys the ProtocolSPIV0 object.
   */
  ~ProtocolSPIV0() override = default;

  /**
   * @brief Establishes a logical connection for the protocol.
   * For SPI, this typically involves checking the underlying channel's readiness,
   * but the actual bus transaction start/stop is handled per command.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes connect() override;

  /**
   * @brief Terminates a logical connection for the protocol.
   * For SPI, this typically involves releasing any held resources by the underlying channel.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes disconnect() override;

  /**
   * @brief Read single bit.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] address Bit address.
   * @param[out] value Bit value.
   * @param hardware_access The I/O direction (input/output) to determine the correct function in
   * lower layers. Not used in SPI.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes readBit(const std::string &address_on_channel, uint16_t address, bool &value,
                        uint8_t hardware_access) override;

  /**
   * @brief Write single bit.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] address Bit address.
   * @param[in] value Value to set the bit to.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes writeBit(const std::string &address_on_channel, uint16_t address,
                         bool value) override;

  /**
   * @brief Read single register.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] address Register address.
   * @param[out] value Register value.
   * @param hardware_access The I/O direction (input/output) to determine the correct function in
   * lower layers. Not used in SPI.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes readRegister(const std::string &address_on_channel, uint16_t address,
                             uint16_t &value, uint8_t hardware_access) override;

  /**
   * @brief Write single register.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] address Register address.
   * @param[in] value Value to set the register to.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes writeRegister(const std::string &address_on_channel, uint16_t address,
                              uint16_t value) override;

  /**
   * @brief Read multiple bits.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] start_address First bit address.
   * @param[in] values Bit values to read.
   * @param[in] quantity Number of bits to read.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes readBits(const std::string &address_on_channel, uint16_t start_address,
                         bool *values, uint16_t quantity, uint8_t hardware_access) override;

  /**
   * @brief Write multiple bits.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] start_address First bit address.
   * @param[in] values Bit values to write.
   * @param[in] quantity Number of bits to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes writeBits(const std::string &address_on_channel, uint16_t start_address,
                          bool *values, uint16_t quantity) override;

  /**
   * @brief Read multiple registers.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] start_address First register address.
   * @param[in] values Register values to read.
   * @param[in] quantity Number of registers to read.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes readRegisters(const std::string &address_on_channel, uint16_t start_address,
                              uint16_t *values, uint16_t quantity,
                              uint8_t hardware_access) override;

  /**
   * @brief Write multiple registers.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] start_address First register address.
   * @param[in] values Register values to write.
   * @param[in] quantity Number of registers to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
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

  // SPIcific functions

  /**
   * @brief Get the Module Information
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[out] values Results as [type, bits, registers].
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes getModuleInfo(const std::string &address_on_channel, uint8_t *values);

  /**
   * @brief Get the UUID of the module
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[out] uuid Module UUID.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes readUuid(const std::string &address_on_channel, uint32_t &uuid);

  /**
   * @brief Set the UUID of the module.
   * @note This function is used to set the UUID of the module. It should not be used by a normal
   * user.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] uuid new value for the module UUID.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setUuid(const std::string &address_on_channel, uint32_t uuid);

  /**
   * @brief Read the WDT of the module in ms.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[out] wdtValue value in ms for the module WDT.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes readWDT(const std::string &address_on_channel, uint16_t &wdtValue);

  /**
   * @brief Set the WDT of the module.
   * @param[in] address_on_channel The physical address of the module on its channel (e.g., SPI
   * slot).
   * @param[in] timeoutMs new value in ms for the module WDT.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setWDT(const std::string &address_on_channel, uint16_t timeoutMs);

  private:
  /**
   * @brief Send request to slave nodes
   * @param[out] req Request sent
   * @param[in] function Function ID
   * @param[in] data Data to send
   * @param[in] length Data length
   * @param[in] slot Slot id. At the bus level: channel.
   * @param[out] bytesWritten Number of bytes written
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes _sendRequest(uint8_t *req, uint8_t function, uint8_t *data, size_t length,
                             uint8_t slot, ssize_t &bytesWritten);

  /**
   * @brief Receive response from slave nodes
   * @param[out] res Response received
   * @param[in] slot Slot id. At the bus level: channel.
   * @param[out] bytesRead Number of bytes read
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes _recvResponse(uint8_t *res, uint8_t slot, ssize_t &bytesRead);

  /**
   * @brief Check if response is valid
   * @param[in] res Response data
   * @param[in] length Response length
   * @param[in] req Request data
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes _checkResponse(uint8_t *res, size_t length, uint8_t *req);

  /**
   * @brief Calculates the expected data length of the response.
   * @param[in] req Request message
   * @param[out] length Length or negative value for error.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes _calculateResponseDataLength(uint8_t *req, size_t &length);

  /**
   * @brief Request buffer
   */
  uint8_t _requestBuffer[PLC_REQUEST_BUFFER_SIZE];

  /**
   * @brief Length of data stored in the request buffer
   */
  ssize_t _requestBufferLength;

  /**
   * @brief Request data buffer
   */
  uint8_t _dataBuffer[PLC_DATA_BUFFER_SIZE];

  /**
   * @brief Length of data stored in the request data buffer
   */
  ssize_t _dataBufferLength;

  /**
   * @brief Response buffer
   */
  uint8_t _responseBuffer[PLC_RESPONSE_BUFFER_SIZE];

  /**
   * @brief Length of data stored in the response buffer
   */
  ssize_t _responseBufferLength;

  /**
   * @brief Function codes map
   */
  std::map<int, int> _functions;

  /**
   * @brief Function codes inverse map
   */
  std::map<int, int> _iFunctions;

  /**
   * @brief Response status codes
   */
  std::map<int, int> _status;

  /**
   * @brief Response status codes
   */
  std::map<int, int> _iStatus;

  /**
   * @brief Mutex for bus synchronization at a protocol level.
   * This mutex is used to ensure that only one module can access the bus at a time (while
   * communicating, exchanging messages).
   */
  std::shared_ptr<std::mutex> _bus_mutex;
};
