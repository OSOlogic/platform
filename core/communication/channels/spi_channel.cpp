/**
 * @file bus.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Bus communication (code)
 * @version a-1.0.0
 * @date 2024/08/19
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "spi_channel.hpp"

#include <inttypes.h>
#include <stdio.h>
#include <wiringPi.h>

#include <memory>
#include <string>

#include "../../common/debug.hpp"
#include "../../common/errors.hpp"
#include "../../hardware/gpio.hpp"
#include "../../hardware/leds.hpp"

/* PUBLIC FUNCTIONS */

SPI_Channel::SPI_Channel(uint8_t _SPI_G, uint8_t _SPI_CS0, uint8_t _SPI_CS1, uint8_t _SPI_CS2,
                         uint8_t _SPI_MOSI, uint8_t _SPI_MISO, uint8_t _SPI_CLK, size_t _SPI_DELAY,
                         size_t _SPI_DELAY_START, size_t _SPI_DELAY_RW)
    : _SPI_G(_SPI_G),
      _SPI_CS0(_SPI_CS0),
      _SPI_CS1(_SPI_CS1),
      _SPI_CS2(_SPI_CS2),
      _SPI_MOSI(_SPI_MOSI),
      _SPI_MISO(_SPI_MISO),
      _SPI_CLK(_SPI_CLK),
      _SPI_DELAY(_SPI_DELAY),
      _SPI_DELAY_START(_SPI_DELAY_START),
      _SPI_DELAY_RW(_SPI_DELAY_RW),
      _frontLEDsCounter{0} {}

SPI_Channel::~SPI_Channel() {}

// --- Channel Interface Implementation ---

PlcErrorCodes SPI_Channel::connect() {
  // For this GPIO-based SPI, no general connection is needed.
  // It's handled per-transaction by start()/stop().
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes SPI_Channel::disconnect() {
  // Nothing to disconnect at a general level.
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes SPI_Channel::write(const void *buf, size_t n, ssize_t &bytes_written) {
  bytes_written = 0;
  if (buf == nullptr) {
    log_error("SPI_Channel::write", "Null pointer provided for buffer.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  const auto *data = static_cast<const uint8_t *>(buf);

#ifdef DEBUG
  {
    std::string hexStr = "";
    char strBuf[10];
    for (size_t i = 0; i < n; i++) {
      snprintf(strBuf, sizeof(strBuf), "%02X ", data[i]);
      hexStr += strBuf;
    }
    DEBUG_STREAM("[SPI CHANNEL] WRITE (" << n << " b): " << hexStr);
  }
#endif

  for (size_t i = 0; i < n; ++i) {
    PlcErrorCodes rs = _write_byte(data[i]);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      // Log is done inside _write_byte if it fails
      return rs;
    }
    bytes_written++;
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes SPI_Channel::read(void *buf, size_t n, ssize_t &bytes_read) {
  bytes_read = 0;
  if (buf == nullptr) {
    log_error("SPI_Channel::read", "Null pointer provided for buffer.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  auto *data = static_cast<uint8_t *>(buf);
  for (size_t i = 0; i < n; ++i) {
    PlcErrorCodes rs = _read_byte(data[i]);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      return rs;
    }
    bytes_read++;
  }

#ifdef DEBUG
  {
    std::string hexStr = "";
    char strBuf[10];
    for (size_t i = 0; i < n; i++) {
      snprintf(strBuf, sizeof(strBuf), "%02X ", data[i]);
      hexStr += strBuf;
    }
    DEBUG_STREAM("[SPI CHANNEL] READ  (" << n << " b): " << hexStr);
  }
#endif

  return PlcErrorCodes::PLC_SUCCESS;
}

// --- SPI-Specific Method Implementations ---

PlcErrorCodes SPI_Channel::start(uint8_t slot) {
  /* Set module LED to indicate communication */
  std::shared_ptr<Leds> leds;
  PlcErrorCodes rs = Leds::getInstance(leds);

  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("ModuleName::FunctionName", "Failed to get Leds instance.", rs);
    return rs;
  }
  leds->setLED(slot);

  /* Set CLOCK to LOW */
  threadSafeDigitalWrite(_SPI_CLK, LOW);
  _wait(_SPI_DELAY_START);

  /* Set slot ID into pinout [CSV2 CSV1 CSV0] (e.g., 5 => 101) */
  threadSafeDigitalWrite(_SPI_CS0, (slot & 1) ? HIGH : LOW);
  threadSafeDigitalWrite(_SPI_CS1, (slot & 2) ? HIGH : LOW);
  threadSafeDigitalWrite(_SPI_CS2, (slot & 4) ? HIGH : LOW);

  _wait(_SPI_DELAY_START * 2);

  /* Set G to HIGH - Mark slot as busy */
  threadSafeDigitalWrite(_SPI_G, HIGH);

  _wait(_SPI_DELAY_START);

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes SPI_Channel::stop(uint8_t slot) {
  /* G to 0 - Mark slot as free */
  threadSafeDigitalWrite(_SPI_G, LOW);

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes SPI_Channel::_write_byte(uint8_t data) {
  for (uint32_t i = 0; i < 8; i++) {
    threadSafeDigitalWrite(_SPI_CLK, LOW);
    _wait(_SPI_DELAY);
    threadSafeDigitalWrite(_SPI_MOSI, (data & 0x80) ? HIGH : LOW);
    threadSafeDigitalWrite(_SPI_CLK, HIGH);
    _wait(_SPI_DELAY);
    data <<= 1;
  }

  threadSafeDigitalWrite(_SPI_CLK, LOW);
  _wait(_SPI_DELAY);
  _wait(_SPI_DELAY_RW);

  return PlcErrorCodes::PLC_SUCCESS;
}

// Private helper method for reading a single byte
PlcErrorCodes SPI_Channel::_read_byte(uint8_t &data) {
  data = 0;

  _wait(_SPI_DELAY_RW);
  threadSafeDigitalWrite(_SPI_MOSI, HIGH);
  threadSafeDigitalWrite(_SPI_CLK, LOW);
  _wait(_SPI_DELAY);

  for (uint32_t i = 0; i < 8; i++) {
    threadSafeDigitalWrite(_SPI_CLK, HIGH);
    _wait(_SPI_DELAY);
    data <<= 1;
    if (threadSafeDigitalRead(_SPI_MISO)) {
      data |= 1;
    }
    threadSafeDigitalWrite(_SPI_CLK, LOW);
    _wait(_SPI_DELAY);
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes SPI_Channel::_wait(size_t t) {
  // TODO - CHECK BETTER WAY OF WAITING OR FIX CPU FRECUENCY
  while (t--) {
    __asm volatile("nop");
  }

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}
