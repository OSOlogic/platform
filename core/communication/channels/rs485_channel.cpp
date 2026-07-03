/**
 * @file rs485_channel.cpp
 * @author Diego Arcos Sapena
 * @brief RS485 communication channel (code)
 * @version a-1.0.1
 * @date 2025/08/30
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "rs485_channel.hpp"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>  // ADDED: Header for tcdrain
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>

#include <cstring>

#include "../../common/debug.hpp"

RS485_Channel::RS485_Channel(const std::string &device_path, int32_t baudrate, char parity,
                             uint8_t stop_bits, uint8_t data_bits)
    : _device_path(device_path),
      _baudrate(baudrate),
      _parity(std::toupper(parity)),  // Uppercase for consistency
      _stop_bits(stop_bits),
      _data_bits(data_bits),
      _serial_fd(-1) {
  if (!(_parity == 'N' || _parity == 'E' || _parity == 'O')) {
    log_error("RS485_Channel",
              "Invalid parity received: " + std::string(1, _parity) + ". Using 'N'.",
              PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
    _parity = 'N';
  }
  if (!(_stop_bits == 1 || _stop_bits == 2)) {
    log_error("RS485_Channel",
              "Invalid stop_bits received: " + std::to_string(_stop_bits) + ". Using 1.",
              PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
    _stop_bits = 1;
  }
  if (!(_data_bits == 7 || _data_bits == 8)) {
    log_error("RS485_Channel",
              "Invalid data_bits received: " + std::to_string(_data_bits) + ". Using 8.",
              PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
    _data_bits = 8;
  }
}

RS485_Channel::~RS485_Channel() {
  disconnect();
}

PlcErrorCodes RS485_Channel::connect() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_serial_fd != -1) {
    DEBUG_STREAM("[RS485_Channel] Already connected to " << _device_path << ":" << _baudrate << " ["
                                                         << _data_bits << _parity << _stop_bits
                                                         << "]");
    return PlcErrorCodes::PLC_SUCCESS;
  }
  _serial_fd = serialOpen(_device_path.c_str(), _baudrate);
  if (_serial_fd < 0) {
    log_error("RS485_Channel::connect",
              "Could not open serial device: " + _device_path + ": " + strerror(errno),
              PlcErrorCodes::ERROR_RS485_OPEN_FAILED);
    return PlcErrorCodes::ERROR_RS485_OPEN_FAILED;
  }

  // Apply serial port configuration
  PlcErrorCodes config_rs = _applySerialConfig();
  if (config_rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("rs485_Channel::connect",
              "Failed to apply serial configuration [" + std::to_string(_data_bits) + _parity +
                  std::to_string(_stop_bits) + "] to " + _device_path,
              config_rs);
    serialClose(_serial_fd);
    _serial_fd = -1;
    return config_rs;
  }
  DEBUG_STREAM("[RS485_Channel] Connected to " << _device_path << " at " << _baudrate << " bauds.");
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes RS485_Channel::disconnect() {
  std::lock_guard<std::mutex> lock(_mutex);

  if (_serial_fd != -1) {
    serialClose(_serial_fd);
    _serial_fd = -1;
    DEBUG_STREAM("[RS485_Channel] Disconnected.");
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes RS485_Channel::write(const void *buf, size_t n, ssize_t &bytes_written) {
  std::lock_guard<std::mutex> lock(_mutex);

  bytes_written = 0;
  if (_serial_fd < 0) {
    log_error("RS485_Channel::write", "Serial port not open.",
              PlcErrorCodes::ERROR_RS485_NOT_CONNECTED);
    return PlcErrorCodes::ERROR_RS485_NOT_CONNECTED;
  }
  if (buf == nullptr) {
    log_error("RS485_Channel::write", "Null buffer provided.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  ssize_t result = ::write(_serial_fd, buf, n);
  if (result < 0) {
    log_error("RS485_Channel::write",
              "Failed to write to serial port: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_RS485_WRITE_FAILED);
    return PlcErrorCodes::ERROR_RS485_WRITE_FAILED;
  }
  bytes_written = result;

  // --- Synchronize with the hardware ---
  // Wait until all bytes in the OS output buffer have been transmitted.
  // This is a precise way to ensure the message is physically sent before
  // proceeding, replacing the less reliable nanosleep() logic.
  if (tcdrain(_serial_fd) == -1) {
    log_error("RS485_Channel::write", "tcdrain failed: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_RS485_WRITE_FAILED);
    return PlcErrorCodes::ERROR_RS485_WRITE_FAILED;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

// --- Read method modification ---
PlcErrorCodes RS485_Channel::read(void *buf, size_t n, ssize_t &bytes_read) {
  std::lock_guard<std::mutex> lock(_mutex);

  bytes_read = 0;
  if (_serial_fd < 0) {
    log_error("RS485_Channel::read", "Serial port not open.",
              PlcErrorCodes::ERROR_RS485_NOT_CONNECTED);
    return PlcErrorCodes::ERROR_RS485_NOT_CONNECTED;
  }
  if (buf == nullptr) {
    log_error("RS485_Channel::read", "Null buffer provided.", PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  // This is a simple, low-level, non-blocking read. It tries to read
  // what's available and returns immediately. The protocol layer will be
  // responsible for calling this in a loop to assemble a full frame.
  int flags = fcntl(_serial_fd, F_GETFL, 0);
  fcntl(_serial_fd, F_SETFL, flags | O_NONBLOCK);

  ssize_t result = ::read(_serial_fd, buf, n);

  if (result < 0) {
    // EAGAIN (or EWOULDBLOCK) means no data is available right now. This is not
    // an error.
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      bytes_read = 0;
      return PlcErrorCodes::PLC_SUCCESS;
    }
    // Any other errno indicates a true read error.
    log_error("RS485_Channel::read",
              "Failed to read from serial port: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_RS485_READ_FAILED);
    return PlcErrorCodes::ERROR_RS485_READ_FAILED;
  }

  bytes_read = result;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes RS485_Channel::flush() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_serial_fd != -1) {
    serialFlush(_serial_fd);
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes RS485_Channel::getSerialFD(int &serial_fd) {
  std::lock_guard<std::mutex> lock(_mutex);
  serial_fd = this->_serial_fd;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes RS485_Channel::_applySerialConfig() {
  if (_serial_fd < 0) {
    return PlcErrorCodes::ERROR_RS485_NOT_CONNECTED;
  }

  struct termios options;
  if (tcgetattr(_serial_fd, &options) != 0) {
    log_error("rs485_Channel::_applySerialConfig",
              "tcgetattr failed: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_RS485_CONFIG_FAILED);
    return PlcErrorCodes::ERROR_RS485_CONFIG_FAILED;
  }

  // --- Modify flags based on members ---

  // 1. Configure Data Bits (using _data_bits)
  options.c_cflag &= ~CSIZE;  // Clear size bits
  switch (_data_bits) {
    case 8:
      options.c_cflag |= CS8;
      break;
    case 7:
      options.c_cflag |= CS7;
      break;
    default:
      log_error("rs485_Channel::_applySerialConfig",
                "Internal error: Invalid data_bits value: " + std::to_string(_data_bits),
                PlcErrorCodes::ERROR_RS485_CONFIG_FAILED);
      return PlcErrorCodes::ERROR_RS485_CONFIG_FAILED;  // Internal error if
                                                        // constructor validation
                                                        // failed
  }

  // 2. Configure Parity (using _parity)
  switch (_parity) {  // _parity is already uppercase from the constructor
    case 'N':         // No Parity
      options.c_cflag &= ~PARENB;
      options.c_iflag &= ~INPCK;
      break;
    case 'E':  // Even Parity
      options.c_cflag |= PARENB;
      options.c_cflag &= ~PARODD;
      options.c_iflag |= INPCK;
      break;
    case 'O':  // Odd Parity
      options.c_cflag |= PARENB;
      options.c_cflag |= PARODD;
      options.c_iflag |= INPCK;
      break;
    default:
      log_error("rs485_Channel::_applySerialConfig",
                "Internal error: Invalid parity value: " + std::string(1, _parity),
                PlcErrorCodes::ERROR_RS485_CONFIG_FAILED);
      return PlcErrorCodes::ERROR_RS485_CONFIG_FAILED;  // Internal error
  }

  // 3. Configure Stop Bits (using _stop_bits)
  switch (_stop_bits) {
    case 1:
      options.c_cflag &= ~CSTOPB;
      break;  // 1 stop bit
    case 2:
      options.c_cflag |= CSTOPB;
      break;  // 2 stop bits
    default:
      log_error("rs485_Channel::_applySerialConfig",
                "Internal error: Invalid stop_bits value: " + std::to_string(_stop_bits),
                PlcErrorCodes::ERROR_RS485_CONFIG_FAILED);
      return PlcErrorCodes::ERROR_RS485_CONFIG_FAILED;  // Internal error
  }

  // --- Apply the new configuration ---
  if (tcsetattr(_serial_fd, TCSANOW, &options) != 0) {
    log_error("rs485_Channel::_applySerialConfig",
              "tcsetattr failed: " + std::string(strerror(errno)),
              PlcErrorCodes::ERROR_RS485_CONFIG_FAILED);
    return PlcErrorCodes::ERROR_RS485_CONFIG_FAILED;
  }

  tcflush(_serial_fd, TCIOFLUSH);  // Flush buffers
  return PlcErrorCodes::PLC_SUCCESS;
}
