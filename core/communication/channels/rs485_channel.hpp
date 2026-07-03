/**
 * @file rs485_channel.hpp
 * @author Diego Arcos Sapena
 * @brief RS485 communication channel (header)
 * @version a-1.0.1
 * @date 2024/07/30
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */
#pragma once

#include <termios.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>

#include <mutex>
#include <string>

#include "../../common/errors.hpp"
#include "Ichannel.hpp"

/**
 * @class RS485_Channel
 * @brief Final implementation of a Channel for RS485 serial communication.
 *
 * This class acts as a low-level driver, managing the specific hardware
 * characteristics of a half-duplex RS485 bus. Its key driver-level
 * responsibilities include:
 *
 * - **Bus Turnaround Management:** After a write operation, the class
 * implements a calculated delay (`nanosleep`) to ensure the bus has
 * stabilized and other devices have time to respond. This is critical
 * for half-duplex communication.
 *
 *
 * These functions are software emulations of behaviors often handled by
 * advanced hardware controllers, providing a robust and transparent
 * communication interface to the upper layers of the application.
 */
class RS485_Channel final : public Channel {
  public:
  /**
   * @brief Constructs a new RS485 Channel object.
   * @param[in] device_path The filesystem path to the serial device (e.g., "/dev/ttyS0").
   * @param[in] baudrate The communication speed (e.g., 9600, 19200).
   */
  RS485_Channel(const std::string& device_path, int32_t baudrate, char parity, uint8_t stop_bits,
                uint8_t data_bits);

  /**
   * @brief Destroys the RS485 Channel object, ensuring disconnection.
   */
  ~RS485_Channel() override;

  /**
   * @brief Opens and configures the serial port for communication.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes connect() override;

  /**
   * @brief Doesn't close the serial port file descriptor because it's a shared bus. Doesn't do
   * nothing
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes disconnect() override;

  /**
   * @brief Writes a block of bytes to the serial port.
   * @param[in] buf Pointer to the data buffer to write.
   * @param[in] n Number of bytes to write.
   * @param[out] bytes_written The number of bytes actually written.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes write(const void* buf, size_t n, ssize_t& bytes_written) override;

  /**
   * @brief Reads a block of bytes from the serial port with timeout logic.
   * @param[out] buf Pointer to the buffer where read data will be stored.
   * @param[in] n Maximum number of bytes to read.
   * @param[out] bytes_read The number of bytes actually read.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes read(void* buf, size_t n, ssize_t& bytes_read) override;

  /**
   * @brief Flushes the serial port's read and write buffers.
   */
  PlcErrorCodes flush();

  /**
   * @brief Gets the underlying socket file descriptor.
   * @param[out] serial_fd Reference to an integer where the descriptor will be stored.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getSerialFD(int& _serial_fd);

  private:
  /**
   * @brief Applies the serial port configuration (parity, data bits, stop bits).
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
   */
  PlcErrorCodes _applySerialConfig();

  /**
   * @brief The filesystem path to the serial device (e.g., "/dev/ttyS0").
   */
  std::string _device_path;

  /**
   * @brief The communication speed in bauds (e.g., 9600, 19200).
   */
  int _baudrate;

  /**
   * @brief Serial port configuration parity
   */
  char _parity;

  /**
   * @brief Serial port configuration parameters
   */
  uint8_t _stop_bits;

  /**
   * @brief Serial port configuration data bits
   */
  uint8_t _data_bits;

  /**
   * @brief The file descriptor for the open serial port. -1 if not connected.
   */
  int _serial_fd;

  /**
   * @brief Mutex for thread-safe access to the channel's resources.
   */
  std::mutex _mutex;

  /**
   * @brief The total time in milliseconds to wait for a response before timing out.
   */
  static constexpr int READ_TOTAL_TIMEOUT_MS = 500;

  /**
   * @brief The interval in microseconds to wait between polling for available data.
   */
  static constexpr int POLL_INTERVAL_US = 1000;
};
