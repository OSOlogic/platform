/**
 * @file spi_channel.hpp
 * @author Diego Arcos Sapena
 * @brief Bus communication (GPIO to SPI and LEDS) (header)
 * @version a-1.0.0
 * @date 2024/08/19
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <inttypes.h>
#include <stdio.h>

#include <memory>

#include "../../common/errors.hpp"
#include "Ichannel.hpp"

#define SPI_DEFAULT_DELAY_RW 500
#define SPI_DEFAULT_DELAY_START 150
#define SPI_DEFAULT_DELAY 200

#define LED_BLINK_TICKS 5

/**
 * @class SPI_Channel
 * @brief Implements a GPIO-based SPI communication channel.
 *
 * This class provides methods to start and stop SPI communication, write and read data,
 * and manage the SPI bus using GPIO pins. This channel is inteligent because it handles the
 * "session" of the bus through the start and stop methods, making the protocol functions atomic and
 * thread-safe, avoiding race conditions between two or more threads trying to access the bus at the
 * same time.
 */
class SPI_Channel final : public Channel {
  public:
  /**
   * @brief Construct a new OsoLogicPLC Bus object.
   * @param[in] _SPI_G G signal pin.
   * @param[in] _SPI_CS0 Chip enable 0 signal pin.
   * @param[in] _SPI_CS1 Chip enable 1 signal pin.
   * @param[in] _SPI_CS2 Chip enable 2 signal pin.
   * @param[in] _SPI_MOSI MOSI signal pin.
   * @param[in] _SPI_MISO MISO signal pin.
   * @param[in] _SPI_CLK SPI clock signal pin.
   * @param[in] _SPI_DELAY Delay between SPI clock ticks.
   * @param[in] _SPI_DELAY_START Delay on slot start.
   * @param[in] _SPI_DELAY_RW Delay between Bytes writed on the bus.
   */
  SPI_Channel(uint8_t _SPI_G, uint8_t _SPI_CS0, uint8_t _SPI_CS1, uint8_t _SPI_CS2,
              uint8_t _SPI_MOSI, uint8_t _SPI_MISO, uint8_t _SPI_CLK, size_t _SPI_DELAY,
              size_t _SPI_DELAY_START, size_t _SPI_DELAY_RW);

  /**
   * @brief Destroy the OsoLogicPLC Bus object.
   */
  ~SPI_Channel();

  /* SPI functions */

  /**
   * @brief Start SPI communication with given slot.
   * @param[in] slot slot to start communication with and lock_id
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes start(uint8_t slot);

  /**
   * @brief Stops the transaction for a specific slot, releasing the bus.
   * @param slot The slot number that owns the lock.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes stop(uint8_t slot);

  // --- Implementation of the Channel contract ---
  // These methods provide a generic, standard interface for communication.

  /**
   * @brief Establishes a connection to the channel. For this GPIO-based implementation,
   * this method performs no action, as connection is handled by start().
   * @return Always returns PlcErrorCodes::PLC_SUCCESS (0).
   */
  PlcErrorCodes connect() override;

  /**
   * @brief Ends the connection to the channel. Performs no action for this implementation.
   * @return Always returns PlcErrorCodes::PLC_SUCCESS (0).
   */
  PlcErrorCodes disconnect() override;

  /**
   * @brief Writes a block of bytes to the bus.
   * @param buf Pointer to the data buffer to write.
   * @param n Number of bytes to write.
   * @param[out] bytes_written The number of bytes that were actually written.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes write(const void *buf, size_t n, ssize_t &bytes_written) override;

  /**
   * @brief Reads a block of bytes from the bus.
   * @param[out] buf Pointer to the buffer where data will be stored.
   * @param n Number of bytes to read.
   * @param[out] bytes_read The number of bytes that were actually read.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes read(void *buf, size_t n, ssize_t &bytes_read) override;

  private:
  // Low-level byte-wise operations, specific to this implementation
  /**
   * @brief Writes a single byte to the bus.
   * @param data The byte to write.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes _write_byte(uint8_t data);
  /**
   * @brief Reads a single byte from the bus.
   * @param[out] data Reference to store the read byte.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes _read_byte(uint8_t &data);
  /**
   * @brief Waits for a specified time in milliseconds.
   * @param t Time to wait in milliseconds.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or a specific error code on failure.
   */
  PlcErrorCodes _wait(size_t t);
  /**
   * @brief Bus is bussy pin.
   */
  uint8_t _SPI_G;

  /**
   * @brief Chip selection pin 0.
   */
  uint8_t _SPI_CS0;

  /**
   * @brief Chip selection pin 1.
   */
  uint8_t _SPI_CS1;

  /**
   * @brief Chip selection pin 2.
   */
  uint8_t _SPI_CS2;

  /**
   * @brief Master Output Slave Input Pin.
   */
  uint8_t _SPI_MOSI;

  /**
   * @brief Master Input Slave Output Pin.
   */
  uint8_t _SPI_MISO;

  /**
   * @brief SPI Clock Pin.
   */
  uint8_t _SPI_CLK;

  /**
   * @brief SPI default delay (in cpu cycles)
   */
  size_t _SPI_DELAY;

  /**
   * @brief SPI read/write delay (in cpu cycles)
   */
  size_t _SPI_DELAY_RW;

  /**
   * @brief SPI start delay (in cpu cycles)
   */
  size_t _SPI_DELAY_START;

  /**
   * @brief LED blink counter
   */
  uint8_t _frontLEDsCounter[8];
};

using SPIChannelPtr = std::shared_ptr<SPI_Channel>;
