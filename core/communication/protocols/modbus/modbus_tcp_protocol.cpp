/**
 * @file modbus_tcp_protocol.cpp
 * @author Diego Arcos Sapena
 * @brief Modbus TCP protocol implementation (code).
 * @version a-2.0.0
 * @date 2024/07/26
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "modbus_tcp_protocol.hpp"

#include <sys/select.h>
#include <sys/time.h>

#include <vector>

#include "../../channels/tcp_channel.hpp"  // Required for getSocketFD

ModbusTcpProtocol::ModbusTcpProtocol(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex,
                                     uint32_t timeout_ms)
    : ModbusProtocol(channelPtr, bus_mutex, timeout_ms), _transaction_id(0) {}

PlcErrorCodes ModbusTcpProtocol::buildMbapHeader(uint8_t unit_id, uint16_t pdu_length,
                                                 std::vector<uint8_t> &mbapHeader) {
  _transaction_id++;
  uint16_t message_length = pdu_length + 1;  // Length = PDU length + Unit ID (1 byte)

  mbapHeader = {
      static_cast<uint8_t>((_transaction_id >> 8) & 0xFF),  // Transaction ID High
      static_cast<uint8_t>(_transaction_id & 0xFF),         // Transaction ID Low
      0x00,
      0x00,                                                // Protocol ID (0 for Modbus)
      static_cast<uint8_t>((message_length >> 8) & 0xFF),  // Length High
      static_cast<uint8_t>(message_length & 0xFF),         // Length Low
      unit_id                                              // Unit ID (Slave ID)
  };

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes ModbusTcpProtocol::sendAndReceiveAdu(uint8_t unit_id, const std::vector<uint8_t> &pdu,
                                                   std::vector<uint8_t> &response_pdu) {
  std::lock_guard<std::mutex> lock(*_bus_mutex);

  if (!_channelPtr) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Channel pointer is null.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // --- 1. Build and Send the Application Data Unit (ADU = MBAP + PDU) ---
  std::vector<uint8_t> mbap_header;
  PlcErrorCodes rs = buildMbapHeader(unit_id, pdu.size(), mbap_header);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Failed to build MBAP header.", rs);
    return rs;
  }

  std::vector<uint8_t> request_adu = mbap_header;
  request_adu.insert(request_adu.end(), pdu.begin(), pdu.end());

  ssize_t bytes_written = 0;
  rs = _channelPtr->write(request_adu.data(), request_adu.size(), bytes_written);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Channel write failed.", rs);
    return rs;
  }
  if (bytes_written != request_adu.size()) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Incomplete write to channel.",
              PlcErrorCodes::ERROR_TCP_WRITE_FAILED);
    return PlcErrorCodes::ERROR_TCP_WRITE_FAILED;
  }

  // --- 2. Wait for Response using select() ---
  auto tcp_channel = std::dynamic_pointer_cast<TCP_Channel>(_channelPtr);
  if (!tcp_channel) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Channel is not a valid TCP_Channel.",
              PlcErrorCodes::ERROR_INVALID_ARGUMENT);
    return PlcErrorCodes::ERROR_INVALID_ARGUMENT;
  }

  int sock_fd = -1;
  rs = tcp_channel->getSocketFD(sock_fd);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Failed to get socket FD.", rs);
    return rs;
  }
  if (sock_fd < 0) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Invalid socket file descriptor obtained.",
              PlcErrorCodes::ERROR_TCP_NOT_CONNECTED);
    return PlcErrorCodes::ERROR_TCP_NOT_CONNECTED;
  }

  fd_set read_fds;
  FD_ZERO(&read_fds);          // Clean the set
  FD_SET(sock_fd, &read_fds);  // Add the socket file descriptor to the set

  struct timeval timeout;
  timeout.tv_sec = _timeout_ms / 1000;
  timeout.tv_usec = (_timeout_ms % 1000) * 1000;  // Microseconds

  int select_res = select(sock_fd + 1, &read_fds, NULL, NULL,
                          &timeout);  // Check if the socket is ready for reading
  if (select_res <= 0) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu",
              "select() failed or timed out waiting for response.",
              PlcErrorCodes::ERROR_TCP_READ_FAILED);
    return PlcErrorCodes::ERROR_TCP_READ_FAILED;
  }

  // --- 3. Read and Parse the Response ADU ---
  std::vector<uint8_t> response_header(7);
  ssize_t bytes_read = 0;
  rs = _channelPtr->read(response_header.data(), 7,
                         bytes_read);  // Extract the MBAP header (7 bytes)
  if (rs != PlcErrorCodes::PLC_SUCCESS || bytes_read != 7) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Failed to read response header.",
              PlcErrorCodes::ERROR_TCP_READ_FAILED);
    return PlcErrorCodes::ERROR_TCP_READ_FAILED;
  }

  if (response_header[0] != mbap_header[0] || response_header[1] != mbap_header[1]) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Transaction ID mismatch.",
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  uint16_t response_body_length = (response_header[4] << 8 | response_header[5]);
  if (response_body_length < 2) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu",
              "Invalid response length in header: " + std::to_string(response_body_length),
              PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED);
    return PlcErrorCodes::ERROR_MODBUS_RESPONSE_MALFORMED;
  }

  uint16_t pdu_len = response_body_length - 1;
  response_pdu.resize(pdu_len);
  rs = _channelPtr->read(response_pdu.data(), pdu_len,
                         bytes_read);  // Read the PDU part of the response (Modbus)
  if (rs != PlcErrorCodes::PLC_SUCCESS || bytes_read != pdu_len) {
    log_error("ModbusTcpProtocol::sendAndReceiveAdu", "Incomplete response PDU read.",
              PlcErrorCodes::ERROR_TCP_READ_FAILED);
    return PlcErrorCodes::ERROR_TCP_READ_FAILED;
  }

  if (response_pdu[0] > 0x80) {
    uint8_t exception_code = response_pdu.size() > 1 ? response_pdu[1] : 0;
    log_error("ModbusTcpProtocol::sendAndReceiveAdu",
              "Modbus exception received. Code: " + std::to_string(exception_code),
              PlcErrorCodes::ERROR_MODBUS_EXCEPTION);
    return PlcErrorCodes::ERROR_MODBUS_EXCEPTION;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}