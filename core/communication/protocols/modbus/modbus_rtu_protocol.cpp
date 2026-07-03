/**
 * @file modbus_rtu_protocol.cpp
 * @author Diego Arcos Sapena
 * @brief Modbus RTU protocol implementation
 * @version a-1.1.0
 * @date 2024/08/01
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "modbus_rtu_protocol.hpp"

#include <chrono>
#include <cstring>
#include <vector>

#include "../../channels/rs485_channel.hpp"  // Required for getSerialFD
#include "../../channels/tcp_channel.hpp"    // Required for getSocketFD

ModbusRtuProtocol::ModbusRtuProtocol(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex,
                                     uint32_t timeout_ms)
    : ModbusProtocol(channelPtr, bus_mutex, timeout_ms) {}

uint16_t ModbusRtuProtocol::calculateCrc16(const uint8_t *buffer, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buffer[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

PlcErrorCodes ModbusRtuProtocol::sendAndReceiveAdu(uint8_t unit_id, const std::vector<uint8_t> &pdu,
                                                   std::vector<uint8_t> &response_pdu) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);
  if (!_channelPtr) {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Channel pointer is null.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // --- 1. Build and Send Request ---
  std::vector<uint8_t> request_adu;
  request_adu.push_back(unit_id);
  request_adu.insert(request_adu.end(), pdu.begin(), pdu.end());
  uint16_t crc = calculateCrc16(request_adu.data(), request_adu.size());
  request_adu.push_back(crc & 0xFF);
  request_adu.push_back((crc >> 8) & 0xFF);

  auto rtu_channel = std::dynamic_pointer_cast<RS485_Channel>(_channelPtr);
  if (rtu_channel) {
    rtu_channel->flush();
  }

  ssize_t bytes_written = 0;
  PlcErrorCodes rs = _channelPtr->write(request_adu.data(), request_adu.size(), bytes_written);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Channel write failed.", rs);
    return rs;
  }

  // --- 2. Robust Read Logic using select() ---
  std::vector<uint8_t> frame_buffer;
  frame_buffer.reserve(MAX_MODBUS_ADU_SIZE * 2);

  const int total_timeout_ms = _timeout_ms;
  const int inter_byte_timeout_ms = 20;  // Standard Modbus inter-frame delay

  auto start_time = std::chrono::steady_clock::now();

  int fd;

  // TODO: WHEN SPI IS IMPLEMENTED THROUGH KERNEL DRIVERS, PUT GETFD() IN ICHANNEL AND THEN AVOID
  // DOING THIS DYNAMIC_POINTER_CAST
  auto rs485_channel = std::dynamic_pointer_cast<RS485_Channel>(_channelPtr);
  auto tcp_channel = std::dynamic_pointer_cast<TCP_Channel>(_channelPtr);
  if (rs485_channel) {
    rs = rs485_channel->getSerialFD(fd);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Failed to get serial FD.", rs);
      return rs;
    }
    if (fd < 0) {
      log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Invalid serial file descriptor obtained.",
                PlcErrorCodes::ERROR_RS485_NOT_CONNECTED);
      return PlcErrorCodes::ERROR_RS485_NOT_CONNECTED;
    }
  } else if (tcp_channel) {
    rs = tcp_channel->getSocketFD(fd);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Failed to get socket FD.", rs);
      return rs;
    }
    if (fd < 0) {
      log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Invalid socket file descriptor obtained.",
                PlcErrorCodes::ERROR_TCP_NOT_CONNECTED);
      return PlcErrorCodes::ERROR_TCP_NOT_CONNECTED;
    }
  } else {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Channel is neither RS485 nor TCP.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                               start_time)
             .count() < total_timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = inter_byte_timeout_ms * 1000;

    int retval = select(fd + 1, &read_fds, NULL, NULL, &tv);

    if (retval == -1)  // select() error
    {
      log_error("ModbusRtuProtocol::sendAndReceiveAdu",
                "select() error: " + std::string(strerror(errno)),
                PlcErrorCodes::ERROR_RS485_READ_FAILED);
      return PlcErrorCodes::ERROR_RS485_READ_FAILED;
    } else if (retval > 0)  // Data is available to be read
    {
      uint8_t read_buf[MAX_MODBUS_ADU_SIZE];
      ssize_t bytes_read = 0;
      rs = _channelPtr->read(read_buf, sizeof(read_buf), bytes_read);
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Channel read failed.", rs);
        return rs;
      }

      if (bytes_read > 0) {
        frame_buffer.insert(frame_buffer.end(), read_buf, read_buf + bytes_read);
      }
    } else  // Timeout (retval == 0)
    {
      // If we received some data and then the inter-byte timeout was triggered,
      // we assume the frame is complete.
      if (!frame_buffer.empty()) {
        break;
      }
    }
  }

  // --- 3. Intelligently Extract Real Response, Discarding Echo ---
  std::vector<uint8_t> actual_response;
  bool found_echo = false;

  // Search for the last occurrence of the request ADU (echo)
  auto last_echo = frame_buffer.end();
  for (auto it = frame_buffer.begin(); it != frame_buffer.end(); ++it) {
    if (std::distance(it, frame_buffer.end()) >= request_adu.size()) {
      if (std::equal(request_adu.begin(), request_adu.end(), it)) {
        last_echo = it;
      }
    }
  }
  if (last_echo != frame_buffer.end()) {
    size_t echo_end = std::distance(frame_buffer.begin(), last_echo) + request_adu.size();
    if (echo_end < frame_buffer.size()) {
      actual_response.assign(frame_buffer.begin() + echo_end, frame_buffer.end());
      found_echo = true;
    }
  }

  if (!found_echo) {
    // Search for a valid Modbus frame (Unit ID + CRC)
    for (size_t i = 0; i + 4 <= frame_buffer.size(); ++i) {
      uint8_t uid = frame_buffer[i];
      if (uid == unit_id) {
        size_t len = frame_buffer.size() - i;
        if (len >= 4) {
          uint16_t crc_recv = (frame_buffer[i + len - 1] << 8) | frame_buffer[i + len - 2];
          uint16_t crc_calc = calculateCrc16(&frame_buffer[i], len - 2);
          if (crc_recv == crc_calc) {
            actual_response.assign(frame_buffer.begin() + i, frame_buffer.begin() + i + len);
            break;
          }
        }
      }
    }
    if (actual_response.empty()) {
      actual_response = frame_buffer;
    }
  }

  // --- 4. Validate the Extracted Response ---
  if (actual_response.empty()) {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu",
              "Timeout or only echo received. No valid response.",
              PlcErrorCodes::ERROR_MODBUS_TIMEOUT);
    return PlcErrorCodes::ERROR_MODBUS_TIMEOUT;
  }

  if (actual_response.size() < 4) {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu", "Modbus response too short.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  if (actual_response[0] != unit_id) {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu",
              "Incorrect Unit ID in response. Expected " + std::to_string(unit_id) + ", got " +
                  std::to_string(actual_response[0]),
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  uint16_t received_crc = (actual_response[actual_response.size() - 1] << 8) |
                          actual_response[actual_response.size() - 2];
  uint16_t calculated_crc = calculateCrc16(actual_response.data(), actual_response.size() - 2);

  if (received_crc != calculated_crc) {
    log_error("ModbusRtuProtocol::sendAndReceiveAdu", "CRC checksum error in response.",
              PlcErrorCodes::ERROR_MODBUS_CRC_ERROR);
    return PlcErrorCodes::ERROR_MODBUS_CRC_ERROR;
  }

  // --- 5. Extract PDU ---
  uint8_t function_code = actual_response[1];
  if ((function_code & 0x80) != 0) {
    uint8_t exception_code = actual_response.size() > 2 ? actual_response[2] : 0;
    log_error("ModbusRtuProtocol::sendAndReceiveAdu",
              "Modbus exception received. Code: " + std::to_string(exception_code),
              PlcErrorCodes::ERROR_MODBUS_EXCEPTION);
    return PlcErrorCodes::ERROR_MODBUS_EXCEPTION;
  }

  response_pdu.assign(actual_response.begin() + 1, actual_response.end() - 2);

  return PlcErrorCodes::PLC_SUCCESS;
}