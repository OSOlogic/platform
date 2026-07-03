/**
 * @file modbus_protocol.cpp
 * @author Diego Arcos Sapena
 * @brief Shared implementation for Modbus protocols.
 * @version a-2.0.0
 * @date 2024/07/26
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "modbus_protocol.hpp"

ModbusProtocol::ModbusProtocol(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex,
                               uint32_t timeout_ms)
    : Protocol(channelPtr), _bus_mutex(bus_mutex), _timeout_ms(timeout_ms) {}

PlcErrorCodes ModbusProtocol::connect() {
  if (!_channelPtr) {
    log_error("ModbusProtocol::connect", "Channel pointer is null.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  return _channelPtr->connect();
}

PlcErrorCodes ModbusProtocol::disconnect() {
  if (!_channelPtr) {
    log_error("ModbusProtocol::disconnect", "Channel pointer is null.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }
  return _channelPtr->disconnect();
}

PlcErrorCodes ModbusProtocol::readBit(const std::string &address_on_channel, uint16_t address,
                                      bool &value, uint8_t hardware_access) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::readBit", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint8_t function_code = (hardware_access == 1) ? MODBUS_READ_DISCRETE_INPUTS : MODBUS_READ_COILS;

  std::vector<uint8_t> pdu = {
      function_code, static_cast<uint8_t>((address >> 8) & 0xFF),
      static_cast<uint8_t>(address & 0xFF), 0x00, 0x01  // Quantity of bits: 1
  };

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::readBit", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu.size() < 3 || response_pdu[0] != function_code) {
    log_error("ModbusProtocol::readBit", "Malformed response PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }
  value = (response_pdu[2] & 0x01) != 0;

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::readBits(const std::string &address_on_channel,
                                       uint16_t start_address, bool *dest, uint16_t quantity,
                                       uint8_t hardware_access) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::readBits", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint8_t function_code = (hardware_access == 1) ? MODBUS_READ_DISCRETE_INPUTS : MODBUS_READ_COILS;
  std::vector<uint8_t> pdu = {function_code, static_cast<uint8_t>((start_address >> 8) & 0xFF),
                              static_cast<uint8_t>(start_address & 0xFF),
                              static_cast<uint8_t>((quantity >> 8) & 0xFF),
                              static_cast<uint8_t>(quantity & 0xFF)};

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) return rs;

  if (response_pdu.size() < 2 || response_pdu[0] != function_code) {
    log_error("ModbusProtocol::readBits", "Malformed response PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  uint8_t byte_count = response_pdu[1];
  if (response_pdu.size() != 2 + byte_count) {
    log_error("ModbusProtocol::readBits", "Response PDU size does not match expected byte count.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  for (int i = 0; i < quantity; ++i) {
    int byte_index = i / 8;
    int bit_index = i % 8;
    dest[i] = (response_pdu[2 + byte_index] >> bit_index) & 0x01;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::writeBit(const std::string &address_on_channel, uint16_t address,
                                       bool value) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::writeBit", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint16_t modbus_value = value ? 0xFF00 : 0x0000;
  std::vector<uint8_t> pdu = {MODBUS_WRITE_SINGLE_COIL, static_cast<uint8_t>((address >> 8) & 0xFF),
                              static_cast<uint8_t>(address & 0xFF),
                              static_cast<uint8_t>((modbus_value >> 8) & 0xFF),
                              static_cast<uint8_t>(modbus_value & 0xFF)};

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::writeBit", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu != pdu) {
    log_error("ModbusProtocol::writeBit", "Response PDU does not match request PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::writeBits(const std::string &address_on_channel,
                                        uint16_t start_address, bool *values, uint16_t quantity) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::writeBits", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint8_t byte_count = (quantity + 7) / 8;
  std::vector<uint8_t> pdu;
  pdu.reserve(6 + byte_count);
  pdu.push_back(MODBUS_WRITE_MULTIPLE_COILS);
  pdu.push_back((start_address >> 8) & 0xFF);
  pdu.push_back(start_address & 0xFF);
  pdu.push_back((quantity >> 8) & 0xFF);
  pdu.push_back(quantity & 0xFF);
  pdu.push_back(byte_count);

  for (int i = 0; i < byte_count; ++i) {
    uint8_t byte = 0;
    for (int j = 0; j < 8; ++j) {
      int bit_index = i * 8 + j;
      if (bit_index < quantity && values[bit_index]) {
        byte |= (1 << j);
      }
    }
    pdu.push_back(byte);
  }

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::writeBits", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu.size() < 5 || response_pdu[0] != MODBUS_WRITE_MULTIPLE_COILS) {
    log_error("ModbusProtocol::writeBits", "Malformed response PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::readRegister(const std::string &address_on_channel, uint16_t address,
                                           uint16_t &value, uint8_t hardware_access) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::readRegister", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint8_t function_code =
      (hardware_access == 1) ? MODBUS_READ_INPUT_REGISTERS : MODBUS_READ_HOLDING_REGISTERS;

  std::vector<uint8_t> pdu = {
      function_code, static_cast<uint8_t>((address >> 8) & 0xFF),
      static_cast<uint8_t>(address & 0xFF), 0x00, 0x01  // Quantity of registers: 1
  };

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::readRegister", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu.size() < 4 || response_pdu[0] != function_code || response_pdu[1] != 2) {
    log_error("ModbusProtocol::readRegister", "Malformed response PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  value = (response_pdu[2] << 8) | response_pdu[3];
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::readRegisters(const std::string &address_on_channel,
                                            uint16_t start_address, uint16_t *dest,
                                            uint16_t quantity, uint8_t hardware_access) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::readRegisters", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint8_t function_code =
      (hardware_access == 1) ? MODBUS_READ_INPUT_REGISTERS : MODBUS_READ_HOLDING_REGISTERS;

  std::vector<uint8_t> pdu = {function_code, static_cast<uint8_t>((start_address >> 8) & 0xFF),
                              static_cast<uint8_t>(start_address & 0xFF),
                              static_cast<uint8_t>((quantity >> 8) & 0xFF),
                              static_cast<uint8_t>(quantity & 0xFF)};

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::readRegisters", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu.size() < 2 || response_pdu[0] != function_code) {
    log_error("ModbusProtocol::readRegisters", "Malformed response PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  uint8_t byte_count = response_pdu[1];
  if (byte_count != quantity * 2 || response_pdu.size() != 2 + byte_count) {
    log_error("ModbusProtocol::readRegisters",
              "Response PDU size does not match expected byte count.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  for (uint16_t i = 0; i < quantity; ++i) {
    dest[i] = (response_pdu[2 + i * 2] << 8) | response_pdu[3 + i * 2];
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::writeRegister(const std::string &address_on_channel, uint16_t address,
                                            uint16_t value) {
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::writeRegister", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  std::vector<uint8_t> pdu = {
      MODBUS_WRITE_SINGLE_REGISTER, static_cast<uint8_t>((address >> 8) & 0xFF),
      static_cast<uint8_t>(address & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF),
      static_cast<uint8_t>(value & 0xFF)};

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::writeRegister", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu != pdu) {
    log_error("ModbusProtocol::writeRegister", "Response PDU does not match request PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::writeRegisters(const std::string &address_on_channel,
                                             uint16_t start_address, uint16_t *values,
                                             uint16_t quantity) {
  // To avoid unused parameter warning
  uint8_t unit_id;
  try {
    unit_id = static_cast<uint8_t>(std::stoi(address_on_channel));
  } catch (const std::exception &) {
    log_error("ModbusProtocol::writeRegisters", "Invalid unit ID format.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  uint8_t byte_count = quantity * 2;
  std::vector<uint8_t> pdu;
  pdu.reserve(6 + byte_count);
  pdu.push_back(MODBUS_WRITE_MULTIPLE_REGISTERS);
  pdu.push_back((start_address >> 8) & 0xFF);
  pdu.push_back(start_address & 0xFF);
  pdu.push_back((quantity >> 8) & 0xFF);
  pdu.push_back(quantity & 0xFF);
  pdu.push_back(byte_count);

  for (int i = 0; i < quantity; ++i) {
    pdu.push_back((values[i] >> 8) & 0xFF);
    pdu.push_back(values[i] & 0xFF);
  }

  std::vector<uint8_t> response_pdu;
  PlcErrorCodes rs = sendAndReceiveAdu(unit_id, pdu, response_pdu);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusProtocol::writeRegisters", "Failed to send and receive ADU.", rs);
    return rs;
  }

  if (response_pdu.size() < 5 || response_pdu[0] != MODBUS_WRITE_MULTIPLE_REGISTERS) {
    log_error("ModbusProtocol::writeRegisters", "Malformed response PDU.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusProtocol::pingDevice(std::string &address_on_channel, IO_Block &first_block) {
  // Intelligent ping: reads the first point of the first I/O block.
  if (first_block.io_type == 1) {
    bool dummy_value;
    uint8_t direction = first_block.hardware_access;
    // Read the first bit of the block
    return readBit(address_on_channel, first_block.physical_start_address, dummy_value, direction);
  } else {  // "register"
    uint16_t dummy_value;
    uint8_t direction = first_block.hardware_access;
    // Read the first register of the block
    return readRegister(address_on_channel, first_block.physical_start_address, dummy_value,
                        direction);
  }
}