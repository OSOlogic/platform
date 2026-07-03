/**
 * @file spi_protocol.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Communication protocol over SPI (code)
 * @version a-1.0.0
 * @date 2024/08/19
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "spi_protocol.hpp"

#include <cstring>

#include "../../../common/debug.hpp"
#include "../../../common/errors.hpp"
#include "../../channels/spi_channel.hpp"

/* PUBLIC FUNCTIONS */

ProtocolSPIV0::ProtocolSPIV0(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex)
    : Protocol(channelPtr), _bus_mutex(bus_mutex) {
  // Ensure buffer lengths are initialized
  _requestBufferLength = 0;
  _dataBufferLength = 0;
  _responseBufferLength = 0;

  // Initialize buffers to zero
  std::memset(_requestBuffer, 0, sizeof(_requestBuffer));
  std::memset(_dataBuffer, 0, sizeof(_dataBuffer));
  std::memset(_responseBuffer, 0, sizeof(_responseBuffer));

  /* Set function codes */
  _functions = {/* Get version must always be the same */
                /* {PLC_FUNCTION_GET_MODULE_VERSION,        // MISSING IN FIRMWARE. MUST NOT
                 CONFLICT WITH GET MODULE INFO (0x00) PLC_FUNCTION_GET_MODULE_VERSION_CODE}, */
                /* Other functions may change */
                {PLC_FUNCTION_GET_MODULE_INFO, 0x00},
                {PLC_FUNCTION_READ_SINGLE_BIT, 0x10},
                {PLC_FUNCTION_WRITE_SINGLE_BIT, 0x11},
                {PLC_FUNCTION_READ_SINGLE_REGISTER, 0x20},
                {PLC_FUNCTION_WRITE_SINGLE_REGISTER, 0x21},
                {PLC_FUNCTION_READ_MULTIPLE_BITS, 0x30},
                {PLC_FUNCTION_WRITE_MULTIPLE_BITS, 0x31},
                {PLC_FUNCTION_READ_MULTIPLE_REGISTERS, 0x40},
                {PLC_FUNCTION_WRITE_MULTIPLE_REGISTERS, 0x41},
                {PLC_FUNCTION_READ_UUID, 0x01},
                {PLC_FUNCTION_SET_UUID, 0x02},
                {PLC_FUNCTION_SET_WDT, 0x03},
                {PLC_FUNCTION_READ_WDT, 0x04}};

  /* Create inverse function map */
  for (auto it = _functions.begin(); it != _functions.end(); it++) {
    _iFunctions[it->second] = it->first;
  }

  /* Set status codes */
  _status = {{static_cast<int>(PlcErrorCodes::PLC_SUCCESS), 0x00},
             {static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_WRONG_CHECKSUM), 0x01},
             {static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_WRONG_LENGTH), 0x02},
             {static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_INVALID_ADDRESS), 0x03},
             {static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_INVALID_FUNCTION), 0x10}};

  /* Create inverse status codes map */
  for (auto it = _status.begin(); it != _status.end(); it++) {
    _iStatus[it->second] = it->first;
  }
}

// --- Interface Method Implementations ---

// This is the core pattern for all public methods:
// 1. Cast the generic channel to the specific SPI_Channel we need.
// 2. Start the transaction for the specific slot.
// 3. Perform the protocol logic (send/receive/check).
// 4. Always stop the transaction, even on error.

// Implementations of the 4 mandatory interface methods
PlcErrorCodes ProtocolSPIV0::connect() {
  // The real SPI connection is handled per-transaction (start/stop) in the Channel layer.
  // Here, the high-level call simply delegates.
  if (!_channelPtr) {
    log_error("SPIV0::connect", "Channel is a null pointer.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  return _channelPtr->connect();
}

PlcErrorCodes ProtocolSPIV0::disconnect() {
  // The real SPI connection is handled per-transaction (start/stop) in the Channel layer.
  // Here, the high-level call simply delegates.
  if (!_channelPtr) {
    log_error("SPIV0::disconnect", "Channel is a null pointer.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  return _channelPtr->disconnect();
}

PlcErrorCodes ProtocolSPIV0::getModuleInfo(const std::string &address_on_channel, uint8_t *values) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::getModuleInfo", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Ensure the values pointer is not null
  if (values == nullptr) {
    log_error("ProtocolSPIV0::getModuleInfo", "Null pointer provided for values.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::getModuleInfo", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  // Start endPoint connection
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::getModuleInfo",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Send request
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_GET_MODULE_INFO], nullptr, 0, slot,
                    _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::getModuleInfo",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Receive response
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::getModuleInfo",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Check if response is correct
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::getModuleInfo",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }
  // Process result
  std::memcpy(values, &_responseBuffer[3], _responseBuffer[2]);

  // End slot connection
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::getModuleInfo",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::readUuid(const std::string &address_on_channel, uint32_t &uuid) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  uuid = 0;
  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::readUuid", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::readUuid", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  // Start slot connection
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readUuid",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Send request with function code for UUID
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_READ_UUID], nullptr, 0, slot,
                    _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readUuid",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Receive response
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readUuid",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Check if response is correct
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readUuid",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Process UUID from response
  std::memcpy(&uuid, &_responseBuffer[3], sizeof(uint32_t));

  // End slot connection
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readUuid",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::setUuid(const std::string &address_on_channel, uint32_t uuid) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::setUuid", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::setUuid", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  // Start slot connection
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setUuid",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Prepare data (UUID as 4 bytes)
  _dataBufferLength = 0;
  std::memcpy(_dataBuffer, &uuid, sizeof(uint32_t));
  _dataBufferLength += sizeof(uint32_t);

  // Send request with function code for set UUID
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_SET_UUID], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setUuid",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Receive response
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setUuid",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Check if response is correct
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setUuid",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // End slot connection
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setUuid",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::readWDT(const std::string &address_on_channel, uint16_t &wdtValue) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::readWDT", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Convert slot string to uint8_t
  uint8_t slot;

  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::readWDT", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  DEBUG_STREAM("[SPI] READ WDT: slot " << static_cast<int>(slot));

  // Start slot connection
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readWDT",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Send request with function code for readWDT (no payload)
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_READ_WDT], nullptr, 0, slot,
                    _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readWDT",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Receive response
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readWDT",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Check response
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readWDT",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Validate that the data length received is indeed 4 bytes for a uint32_t
  if (_responseBuffer[2] != sizeof(uint16_t)) {
    log_error(
        "ProtocolSPIV0::readWDT",
        "Invalid data length in response. Expected 4, got " + std::to_string(_responseBuffer[2]),
        PlcErrorCodes::ERROR_RESPONSE_WRONG_DATA_LENGTH);
    spi_channel->stop(slot);
    return PlcErrorCodes::ERROR_RESPONSE_WRONG_DATA_LENGTH;
  }

  // Extract WDT value from response
  std::memcpy(&wdtValue, &_responseBuffer[3], sizeof(uint16_t));

  // End slot connection
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readWDT",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::setWDT(const std::string &address_on_channel, uint16_t timeoutMs) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::setWDT", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::setWDT", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  // Start slot connection
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setWDT",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Prepare data (2 bytes for WDT value)
  _dataBufferLength = 0;
  std::memcpy(_dataBuffer, &timeoutMs, sizeof(uint16_t));
  _dataBufferLength += sizeof(uint16_t);

  // Send request with function code for setWDT
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_SET_WDT], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setWDT",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Receive response
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setWDT",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // Check response
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setWDT",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  // End slot connection
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::setWDT",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::readBit(const std::string &address_on_channel, uint16_t address,
                                     bool &value, uint8_t hardware_access) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  (void)hardware_access;  // It tells the compiler that hardware_access is intentionally unused

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::readBit", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::readBit", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBit",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => BIT ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(address);

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_READ_SINGLE_BIT], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBit",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBit",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBit",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Process result */
  value = _responseBuffer[3] ? true : false;

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBit",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::writeBit(const std::string &address_on_channel, uint16_t address,
                                      bool value) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::writeBit", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::writeBit", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBit",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => BIT ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(address);

  // DATA[1] => BIT VALUE
  _dataBuffer[_dataBufferLength++] = value ? 0xFF : 0x00;

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_WRITE_SINGLE_BIT], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBit",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBit",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBit",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBit",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::readRegister(const std::string &address_on_channel, uint16_t address,
                                          uint16_t &value, uint8_t hardware_access) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  (void)hardware_access;  // It tells the compiler that hardware_access is intentionally unused

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::readRegister", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::readRegister", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegister",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => REGISTER ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(address);

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_READ_SINGLE_REGISTER], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegister",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegister",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegister",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Process result */
  value = (_responseBuffer[4] << 8) | _responseBuffer[3];

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegister",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::writeRegister(const std::string &address_on_channel, uint16_t address,
                                           uint16_t value) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::writeRegister", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::writeRegister", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegister",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => REGISTER ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(address);

  // DATA[1] => REGISTER VALUE [LOW]
  _dataBuffer[_dataBufferLength++] = (value & 0x00FF);

  // DATA[2] => REGISTER VALUE [HIGH]
  _dataBuffer[_dataBufferLength++] = (value >> 8);

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_WRITE_SINGLE_REGISTER], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegister",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegister",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegister",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegister",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::readBits(const std::string &address_on_channel, uint16_t start_address,
                                      bool *values, uint16_t quantity, uint8_t hardware_access) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  (void)hardware_access;  // It tells the compiler that hardware_access is intentionally unused

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::readBits", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Ensure the values pointer is valid
  if (values == nullptr) {
    log_error("ProtocolSPIV0::readBits", "Null pointer provided for values.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::readBits", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBits",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => FIRST BIT ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(start_address);

  // DATA[1] => NUMBER OF BITS TO READ
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(quantity);

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_READ_MULTIPLE_BITS], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBits",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBits",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBits",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Process result */
  int32_t read = 0;
  // Iterate response data length (Bytes)
  for (uint32_t i = 0; i < _responseBuffer[2]; i++) {
    // Iterate bits inside each Byte (1 bit = 1 bit status)
    for (uint32_t j = 0; j < 8 && read < quantity; j++) {
      values[read++] = (_responseBuffer[i + 3] & (1 << j)) ? true : false;
    }
  }

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readBits",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::writeBits(const std::string &address_on_channel, uint16_t address,
                                       bool *values, uint16_t size) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::writeBits", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Ensure the values pointer is valid
  if (values == nullptr) {
    log_error("ProtocolSPIV0::writeBits", "Null pointer provided for values.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::writeBits", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBits",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => FIRST BIT ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(address);

  // DATA[1] => NUMBER OF BITS TO WRITE
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(size);

  /* Calculate number of bytes needed to hold bits status */
  int32_t needed = size / 8 + (size % 8 ? 1 : 0);

  /* Copy bool array as Bytes into data (8 Bools = 1 Byte) */
  int32_t written = 0;
  for (uint32_t i = 0; i < needed; i++) {
    uint8_t byte = 0;
    for (uint32_t j = 0; j < 8 && written < size; j++) {
      if (values[i * 8 + j]) {
        byte |= (1 << j);
      }
      written++;
    }
    _dataBuffer[_dataBufferLength++] = byte;
  }

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_WRITE_MULTIPLE_BITS], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBits",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBits",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBits",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeBits",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::readRegisters(const std::string &address_on_channel,
                                           uint16_t start_address, uint16_t *values,
                                           uint16_t quantity, uint8_t hardware_access) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  (void)hardware_access;  // It tells the compiler that hardware_access is intentionally unused

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::readRegisters", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Ensure the values pointer is valid
  if (values == nullptr) {
    log_error("ProtocolSPIV0::readRegisters", "Null pointer provided for values.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::readRegisters", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegisters",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => FIRST REGISTER ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(start_address);
  // DATA[1] => NUMBER OF REGISTERS TO READ
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(quantity);

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_READ_MULTIPLE_REGISTERS], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegisters",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegisters",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegisters",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Process result */
  for (uint32_t i = 0; i < quantity; i++) {
    values[i] = (_responseBuffer[4 + (i * 2)] << 8) | _responseBuffer[3 + (i * 2)];
  }

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::readRegisters",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::writeRegisters(const std::string &address_on_channel, uint16_t address,
                                            uint16_t *values, uint16_t size) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::writeRegisters", "Invalid channel type.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Ensure the values pointer is valid
  if (values == nullptr) {
    log_error("ProtocolSPIV0::writeRegisters", "Null pointer provided for values.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Convert slot string to uint8_t
  uint8_t slot;
  try {
    slot = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &e) {
    log_error("ProtocolSPIV0::writeRegisters", "Invalid slot address format: " + address_on_channel,
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  /* Start slot connection */
  PlcErrorCodes rs = spi_channel->start(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegisters",
              "Failed to start communication with slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Prepare data */
  _dataBufferLength = 0;

  // DATA[0] => REGISTER ADDRESS
  _dataBuffer[_dataBufferLength++] = static_cast<uint8_t>(address);

  //_dataBuffer[_dataBufferLength++] = size; No needed

  // Fill data buffer with register values
  for (uint32_t i = 0; i < size; i++) {
    // DATA[X] => REGISTER[i] VALUE [LOW]
    _dataBuffer[_dataBufferLength++] = (values[i] & 0x00FF);

    // DATA[X+1] => REGISTER[i] VALUE [HIGH]
    _dataBuffer[_dataBufferLength++] = (values[i] >> 8);
  }

  /* Send request */
  rs = _sendRequest(_requestBuffer, _functions[PLC_FUNCTION_WRITE_MULTIPLE_REGISTERS], _dataBuffer,
                    _dataBufferLength, slot, _requestBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegisters",
              "Failed to send request to slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Receive response */
  rs = _recvResponse(_responseBuffer, slot, _responseBufferLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegisters",
              "Failed to receive response from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* Check if response is correct */
  rs = _checkResponse(_responseBuffer, _responseBufferLength, _requestBuffer);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegisters",
              "Invalid response received from slot " + std::to_string(slot) + ".", rs);
    spi_channel->stop(slot);
    return rs;
  }

  /* End slot connection */
  rs = spi_channel->stop(slot);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::writeRegisters",
              "Failed to stop communication with slot " + std::to_string(slot) + ".", rs);
    return rs;
  }

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::pingDevice(std::string &address_on_channel, IO_Block &first_block) {
  // std::lock_guard<std::mutex> lock(*_bus_mutex); Not necessary because getModuleInfo mutexes the
  // channel internally For SPI, getModuleInfo is the most reliable "ping" as it verifies the model.
  // We can ignore the first_block parameter.
  (void)first_block;  // Mark as intentionally unused
  uint8_t dummy_values[5] = {0};
  return getModuleInfo(address_on_channel, dummy_values);
}

/* PRIVATE FUNCTIONS */

PlcErrorCodes ProtocolSPIV0::_sendRequest(uint8_t *req, uint8_t function, uint8_t *data,
                                          size_t length, uint8_t slot, ssize_t &bytesWritten) {
  // Ensure the request pointer is valid
  if (req == nullptr) {
    log_error("ProtocolSPIV0::_sendRequest", "Null pointer provided for request buffer.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::_sendRequest", "Invalid channel type. Expected SPI_Channel.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  size_t reqLen = 0;

  /* Add header to request */
  req[reqLen++] = function;
  req[reqLen++] = length;
  uint8_t checksum = function + length;

  /* Add data to request */
  for (uint32_t i = 0; i < length; i++) {
    req[reqLen++] = data[i];
    checksum += data[i];
  }

  /* Add checksum to request */
  req[reqLen++] = checksum;

  bytesWritten = 0;
  /* Send request */

#ifdef DEBUG
  {
    std::string hexStr = "";
    char bufStr[10];
    for (size_t i = 0; i < reqLen; i++) {
      snprintf(bufStr, sizeof(bufStr), "%02X ", req[i]);
      hexStr += bufStr;
    }
    DEBUG_STREAM("--- [SPI PROTOCOL] SENDING REQUEST ---");
    DEBUG_STREAM("-> Fnc: " << static_cast<int>(function) << " | Slot: " << static_cast<int>(slot));
    DEBUG_STREAM("-> Raw (" << reqLen << " b): " << hexStr);
    DEBUG_STREAM("--------------------------------------");
  }
#endif

  /* Send request by delegating to the SPI channel's write method */
  PlcErrorCodes rs = spi_channel->write(req, reqLen, bytesWritten);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::_sendRequest", "Failed to write request to bus.", rs);
    return rs;
  }
  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::_recvResponse(uint8_t *res, uint8_t slot, ssize_t &bytesRead_acc) {
  // Ensure the response buffer pointer is valid
  if (res == nullptr) {
    log_error("ProtocolSPIV0::_recvResponse", "Null pointer provided for response buffer.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // Ensure the channel is a valid SPI_Channel
  auto spi_channel = std::dynamic_pointer_cast<SPI_Channel>(_channelPtr);
  if (!spi_channel) {
    log_error("SPIV0::_recvResponse", "Invalid channel type. Expected SPI_Channel.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  ssize_t bytesRead = 0;
  bytesRead_acc = 0;

  /* Read header (3 bytes) {[Function][ErrorCode][DataLength]} */
  PlcErrorCodes rs = spi_channel->read(&res[bytesRead], 3, bytesRead);
  if (rs != PlcErrorCodes::PLC_SUCCESS || bytesRead != 3) {
    log_error("ProtocolSPIV0::_recvResponse",
              "Failed to read response header or incomplete header received.",
              (rs == PlcErrorCodes::PLC_SUCCESS) ? PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH : rs);
    return (rs == PlcErrorCodes::PLC_SUCCESS) ? PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH : rs;
  }
  bytesRead_acc += bytesRead;

  /* Read response data */
  rs = spi_channel->read(&res[bytesRead_acc], res[2], bytesRead);
  if (rs != PlcErrorCodes::PLC_SUCCESS || bytesRead < (res[2])) {
    log_error("ProtocolSPIV0::_recvResponse",
              "Failed to read response data or incomplete data received.",
              (rs == PlcErrorCodes::PLC_SUCCESS) ? PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH : rs);
    return (rs == PlcErrorCodes::PLC_SUCCESS) ? PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH : rs;
  }
  bytesRead_acc += bytesRead;

  /* Read checksum */
  rs = spi_channel->read(&res[bytesRead_acc], 1, bytesRead);
  if (rs != PlcErrorCodes::PLC_SUCCESS || bytesRead != 1) {
    log_error("ProtocolSPIV0::_recvResponse",
              "Failed to read response checksum or incomplete checksum received.",
              (rs == PlcErrorCodes::PLC_SUCCESS) ? PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH : rs);
    return (rs == PlcErrorCodes::PLC_SUCCESS) ? PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH : rs;
  }
  bytesRead_acc += bytesRead;

#ifdef DEBUG
  {
    std::string hexStr = "";
    char bufStr[10];
    for (size_t i = 0; i < bytesRead_acc; i++) {
      snprintf(bufStr, sizeof(bufStr), "%02X ", res[i]);
      hexStr += bufStr;
    }
    DEBUG_STREAM("--- [SPI PROTOCOL] COMPLETED READ RESPONSE ---");
    DEBUG_STREAM("<- Slot: " << static_cast<int>(slot));
    DEBUG_STREAM("<- Raw (" << bytesRead_acc << " b): " << hexStr);
    DEBUG_STREAM("----------------------------------------------");
  }
#endif

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::_checkResponse(uint8_t *res, size_t length, uint8_t *req) {
  // Ensure response and request pointers are valid
  if (res == nullptr || req == nullptr) {
    log_error("ProtocolSPIV0::_checkResponse",
              "Null pointer provided for response or request buffer.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  /* Length must be minimum 4 (header + checksum) */
  if (length < 4) {
    log_error("ProtocolSPIV0::_checkResponse", "Invalid response length. Length is too short.",
              PlcErrorCodes::ERROR_RESPONSE_INVALID_MESSAGE);
    return PlcErrorCodes::ERROR_RESPONSE_INVALID_MESSAGE;
  }

  /* Check if function is the same as the requested */
  if (req[0] != res[0]) {
    log_error("ProtocolSPIV0::_checkResponse", "Response function does not match request function.",
              PlcErrorCodes::ERROR_RESPONSE_WRONG_FUNCTION);
    return PlcErrorCodes::ERROR_RESPONSE_WRONG_FUNCTION;
  }

  /* Check if there is an error code */
  auto it_status = _iStatus.find(res[1]);
  if (it_status == _iStatus.end()) {
    log_error(
        "ProtocolSPIV0::_checkResponse",
        "Unknown error code in response from hardware: " + std::to_string(static_cast<int>(res[1])),
        PlcErrorCodes::ERROR_RESPONSE_INVALID_ERROR_CODE);
    return PlcErrorCodes::ERROR_RESPONSE_INVALID_ERROR_CODE;
  }

  switch (it_status->second) {
    case static_cast<int>(PlcErrorCodes::PLC_SUCCESS):
      break;  // No error, continue processing

    case static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_WRONG_CHECKSUM):
      log_error("ProtocolSPIV0::_checkResponse", "Response indicates wrong checksum.",
                PlcErrorCodes::ERROR_REQUEST_WRONG_CHECKSUM);
      return PlcErrorCodes::ERROR_REQUEST_WRONG_CHECKSUM;

    case static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_WRONG_LENGTH):
      log_error("ProtocolSPIV0::_checkResponse", "Response indicates wrong request length.",
                PlcErrorCodes::ERROR_REQUEST_WRONG_LENGTH);
      return PlcErrorCodes::ERROR_REQUEST_WRONG_LENGTH;

    case static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_INVALID_ADDRESS):
      log_error("ProtocolSPIV0::_checkResponse", "Response indicates invalid request address.",
                PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
      return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;

    case static_cast<int>(PlcErrorCodes::ERROR_SPI_RESPONSE_INVALID_FUNCTION):
      log_error("ProtocolSPIV0::_checkResponse", "Response indicates invalid request function.",
                PlcErrorCodes::ERROR_REQUEST_INVALID_FUNCTION);
      return PlcErrorCodes::ERROR_REQUEST_INVALID_FUNCTION;

    default:
      log_error("ProtocolSPIV0::_checkResponse", "Unknown error code in response.",
                PlcErrorCodes::ERROR_RESPONSE_INVALID_ERROR_CODE);
      return PlcErrorCodes::ERROR_RESPONSE_INVALID_ERROR_CODE;
  }

  /* Check if length is correct (should be data[2] + 4) */
  if (length != (res[2] + 4)) {
    log_error("ProtocolSPIV0::_checkResponse", "Response length does not match expected length.",
              PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH);
    return PlcErrorCodes::ERROR_RESPONSE_WRONG_LENGTH;
  }

  /* Calculate expected data length depending on request */
  size_t expectedDataLength = 0;
  PlcErrorCodes rs = _calculateResponseDataLength(req, expectedDataLength);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ProtocolSPIV0::_checkResponse", "Failed to calculate expected response data length.",
              rs);
    return rs;
  }

  /* Check if computed length is correct */
  if (expectedDataLength != res[2]) {
    TRACE_STREAM("[SPI] Real data length: " << static_cast<int>(res[2])
                                            << ", Expected: " << expectedDataLength);
    TRACE_STREAM("[SPI] Response Dump: Fx=" << static_cast<int>(res[0])
                                            << ", Err=" << static_cast<int>(res[1])
                                            << ", Len=" << static_cast<int>(res[2]));

    log_error("ProtocolSPIV0::_checkResponse",
              "Response data length does not match expected data length. Expected: " +
                  std::to_string(expectedDataLength) + ", Got: " + std::to_string(res[2]),
              PlcErrorCodes::ERROR_RESPONSE_WRONG_DATA_LENGTH);
    return PlcErrorCodes::ERROR_RESPONSE_WRONG_DATA_LENGTH;
  }

  /* Calculate checksum */
  uint8_t checksum = 0;
  for (uint32_t i = 0; i < length - 1; i++) {
    checksum += res[i];
  }

  /* Check if checksum is correct */
  if (checksum != res[length - 1]) {
    log_error("ProtocolSPIV0::_checkResponse", "Response checksum verification failed.",
              PlcErrorCodes::ERROR_RESPONSE_WRONG_CHECKSUM);
    return PlcErrorCodes::ERROR_RESPONSE_WRONG_CHECKSUM;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ProtocolSPIV0::_calculateResponseDataLength(uint8_t *req, size_t &length) {
  // Ensure request buffer is valid
  if (req == nullptr) {
    log_error("ProtocolSPIV0::_calculateResponseDataLength",
              "Null pointer provided for request buffer.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  length = 0;

  auto it_func = _iFunctions.find(req[0]);
  if (it_func == _iFunctions.end()) {
    log_error("ProtocolSPIV0::_calculateResponseDataLength",
              "Invalid function code in request: " + std::to_string(static_cast<int>(req[0])),
              PlcErrorCodes::ERROR_RESPONSE_INVALID_FUNCTION);
    return PlcErrorCodes::ERROR_RESPONSE_INVALID_FUNCTION;
  }

  switch (it_func->second) {
    case PLC_FUNCTION_GET_MODULE_INFO: {
      /* Data length will be 6 [ModuleId, NumOfBits, NumOfRegs, version, NumStarts (uint16_t)]*/
      length += 6;
      break;
    }
    case PLC_FUNCTION_READ_SINGLE_BIT: {
      /* Data length will be 1 (1 bit stored as 1 Byte) */
      length += 1;
      break;
    }
    case PLC_FUNCTION_READ_SINGLE_REGISTER: {
      /* Data length will be 2 (1 register stored as 2 Byte) */
      length += 2;
      break;
    }
    case PLC_FUNCTION_READ_MULTIPLE_BITS: {
      /* Data length will be number of bits / 8 as every bit */
      /* will be stored as 1 bit (+1 is not multiple of 8) */
      int numOfBits = req[3] / 8 + (req[3] % 8 ? 1 : 0);
      length += numOfBits;
      break;
    }
    case PLC_FUNCTION_READ_MULTIPLE_REGISTERS: {
      /* Data length will be number of registers x2 as every register */
      /* will be stored as 2 bytes */
      length += (req[3] * 2);
      break;
    }
    case PLC_FUNCTION_WRITE_SINGLE_BIT:
    case PLC_FUNCTION_WRITE_SINGLE_REGISTER:
    case PLC_FUNCTION_WRITE_MULTIPLE_BITS:
    case PLC_FUNCTION_WRITE_MULTIPLE_REGISTERS: {
      /* No data returned, so data length will be 0 */
      length += 0;
      break;
    }
    case PLC_FUNCTION_READ_UUID: {
      length += 4;
      break;
    }
    case PLC_FUNCTION_SET_UUID: {
      length += 0;
      break;
    }
    case PLC_FUNCTION_READ_WDT: {
      length += 2;
      break;
    }
    case PLC_FUNCTION_SET_WDT: {
      length += 0;
      break;
    }
    /* FUNCTION CODE DOES NOT EXIST */
    default:
      log_error("ProtocolSPIV0::_calculateResponseDataLength", "Invalid function code in request.",
                PlcErrorCodes::ERROR_RESPONSE_INVALID_FUNCTION);
      return PlcErrorCodes::ERROR_RESPONSE_INVALID_FUNCTION;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}
