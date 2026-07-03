/**
 * @file modbus_tcp_protocol.hpp
 * @author Diego Arcos Sapena
 * @brief Modbus TCP protocol implementation (header).
 * @version a-2.0.0
 * @date 2024/07/26
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "modbus_protocol.hpp"

/**
 * @class ModbusTcpProtocol
 * @brief Implements the Modbus protocol over a TCP/IP channel.
 *
 * This class inherits the generic Modbus PDU (Protocol Data Unit) logic from
 * ModbusProtocol and is responsible for wrapping it in a Modbus TCP frame (ADU)
 * with an MBAP header for transmission.
 */
class ModbusTcpProtocol : public ModbusProtocol {
  public:
  /**
   * @brief Constructs a new ModbusTcpProtocol object.
   * @param[in] channelPtr A shared pointer to a communication channel (should be a TCP_Channel).
   * @param[in] bus_mutex A shared pointer to a mutex for bus synchronization at a protocol level.
   * @param[in] timeout_ms Timeout for Modbus operations in milliseconds.
   */
  ModbusTcpProtocol(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex,
                    uint32_t timeout_ms);

  /**
   * @brief Destructor.
   */
  ~ModbusTcpProtocol() = default;

  protected:
  /**
   * @brief Handles the transport-specific framing for Modbus TCP.
   *
   * This method takes a PDU, wraps it with an MBAP header to create an ADU,
   * sends it over the TCP channel, waits for the response, and extracts the
   * response PDU.
   * @param[in] unit_id The Modbus slave/unit ID.
   * @param[in] pdu The Protocol Data Unit (function code + data) to send.
   * @param[out] response_pdu The PDU received in the response.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes sendAndReceiveAdu(uint8_t unit_id, const std::vector<uint8_t> &pdu,
                                  std::vector<uint8_t> &response_pdu) override;

  private:
  /**
   * @brief Modbus TCP transaction ID, increments with each request.
   */
  uint16_t _transaction_id;

  /**
   * @brief Builds the Modbus Application Header (MBAP) for a TCP request.
   * @param[in] unit_id The Modbus slave/unit ID.
   * @param[in] pdu_length The length of the Protocol Data Unit (PDU) that follows.
   * @param[out] mbapHeader The MBAP header to be filled.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes buildMbapHeader(uint8_t unit_id, uint16_t pdu_length,
                                std::vector<uint8_t> &mbapHeader);
};