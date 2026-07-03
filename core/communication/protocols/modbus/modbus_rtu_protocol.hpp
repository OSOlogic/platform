/**
 * @file modbus_rtu_protocol.hpp
 * @author Diego Arcos Sapena
 * @brief Modbus RTU protocol implementation (header).
 * @version a-1.0.1
 * @date 2024/07/30
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "../../../common/errors.hpp"
#include "modbus_protocol.hpp"

/**
 * @class ModbusRtuProtocol
 * @brief Final implementation of the Modbus RTU protocol.
 *
 * This class handles the specifics of the Modbus RTU framing, which includes
 * adding a unit ID (slave address) and a CRC-16 checksum to the PDU (Protocol Data Unit)
 * to create a complete ADU (Application Data Unit).
 */
class ModbusRtuProtocol final : public ModbusProtocol {
  public:
  /**
   * @brief Constructs a new ModbusRtuProtocol object.
   * @param[in] channelPtr A shared pointer to the communication channel (typically RS485_Channel).
   * @param[in] bus_mutex A shared pointer to a single mutex for thread-safe access to the RS485
   * bus.
   * @param[in] timeout_ms Timeout for Modbus operations in milliseconds.
   */
  explicit ModbusRtuProtocol(ChannelPtr channelPtr, std::shared_ptr<std::mutex> bus_mutex,
                             uint32_t timeout_ms);

  /**
   * @brief Default destructor.
   */
  ~ModbusRtuProtocol() override = default;

  /**
   * @brief Sends a Modbus RTU request and waits for the response.
   *
   * This method builds the full Modbus RTU frame (ADU) by prepending the unit ID
   * and appending the CRC-16 checksum to the PDU. It then sends the frame,
   * reads the response, validates its integrity (Unit ID, CRC, exceptions),
   * and extracts the response PDU.
   *
   * @param[in] unit_id The Modbus slave ID to address.
   * @param[in] pdu The Protocol Data Unit (Function Code + Data) to send.
   * @param[out] response_pdu The PDU received from the slave in response.
   * @return PlcErrorCodes::PLC_SUCCESS on a successful transaction, or an error code on failure.
   */
  PlcErrorCodes sendAndReceiveAdu(uint8_t unit_id, const std::vector<uint8_t> &pdu,
                                  std::vector<uint8_t> &response_pdu) override;

  private:
  /**
   * @brief The maximum size of a Modbus RTU Application Data Unit (ADU).
   * (1 byte Unit ID + 253 bytes PDU + 2 bytes CRC = 256).
   */
  static constexpr size_t MAX_MODBUS_ADU_SIZE = 256;

  /**
   * @brief Calculates the Modbus CRC-16 checksum for a given buffer.
   * @param[in] buffer Pointer to the data for which to calculate the CRC.
   * @param[in] len The length of the data in bytes.
   * @return The calculated 16-bit CRC value.
   */
  uint16_t calculateCrc16(const uint8_t *buffer, size_t len);
};
