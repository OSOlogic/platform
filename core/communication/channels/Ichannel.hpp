/**
 * @file channel.hpp
 * @author Diego Arcos Sapena
 * @brief Abstract base class for all physical communication channels.
 * @version a-1.0.0
 * @date 2024/07/11
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <sys/types.h>

#include <cstddef>
#include <memory>

#include "../../common/errors.hpp"

/**
 * @class Channel
 * @brief Abstract interface representing a generic communication channel (the "wire").
 *
 * This class defines the fundamental contract that all specific channel
 * implementations (like SPI, Serial, TCP) must adhere to.
 */
class Channel {
  public:
  /**
   * @brief Virtual destructor to ensure proper cleanup in derived classes.
   */
  virtual ~Channel() = default;

  /**
   * @brief Establishes a connection to the channel.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes connect() = 0;

  /**
   * @brief Terminates the connection to the channel.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes disconnect() = 0;

  /**
   * @brief Writes a block of bytes to the channel.
   * @param[in] buf Pointer to the data buffer to write.
   * @param[in] n Number of bytes to write.
   * @param[out] bytes_written The number of bytes that were actually written.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes write(const void *buf, size_t n, ssize_t &bytes_written) = 0;

  /**
   * @brief Reads a block of bytes from the channel.
   * @param[out] buf Pointer to the buffer where read data will be stored.
   * @param[in] n Number of bytes to read.
   * @param[out] bytes_read The number of bytes that were actually read.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  virtual PlcErrorCodes read(void *buf, size_t n, ssize_t &bytes_read) = 0;
};

/**
 * @brief A shared pointer to a Channel object. This is the primary way channels
 */
using ChannelPtr = std::shared_ptr<Channel>;