/**
 * @file leds.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief Leds class. In charge of managing leds and leds synchronization with peripheral modules
 * @version a-1.0.0
 * @date 2024/11/16
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "leds.hpp"

#include <stdio.h>
#include <wiringPi.h>

#include <memory>
#include <mutex>

#include "../common/errors.hpp"
#include "gpio.hpp"

/* PRIVATE FUNCTIONS */

// Initialize instance pointer and mutex
std::shared_ptr<Leds> Leds::_instance_ptr = nullptr;
std::mutex Leds::_instanceMutex{};

Leds::Leds()
    : _LED_GREEN(PIN_LED_GREEN),
      _LED_RED(PIN_LED_RED),
      _LED_CLK(PIN_LED_CLK),
      _LED_DATA(PIN_LED_DATA),
      _LED_STROBE(PIN_LED_STROBE),
      _SYNC(PIN_SYNC),
      _frontLEDsCounter{0} {}  // frontLEDsCounter{0, 0, 0, 0, 0, 0, 0, 0}

Leds::~Leds() {}

/* PUBLIC FUNCTIONS */
PlcErrorCodes Leds::getInstance(std::shared_ptr<Leds> &instance_ref) {
  // Ensure mutual exclusion in a multithreaded environment
  std::lock_guard<std::mutex> lock(_instanceMutex);

  // If the instance has not been created yet, create it
  if (!_instance_ptr) {
    _instance_ptr = std::shared_ptr<Leds>(new Leds());
  }

  // Assign the instance to the reference passed by the caller
  instance_ref = _instance_ptr;

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Leds::writeLEDs(uint8_t value) {
  /* Reroute leds */
  uint8_t route[8] = {3, 2, 7, 1, 6, 0, 5, 4};
  uint8_t x = 0;

  // Ensure no out-of-bounds access
  for (int i = 0; i < 8; i++) {
    if (value & (1 << i)) {
      x |= (1 << route[i]);
    }
  }
  value = x;
  value ^= 255;  // Invert bits

  /* Set lines to 0 */
  threadSafeDigitalWrite(_LED_CLK, LOW);
  threadSafeDigitalWrite(_LED_STROBE, LOW);

  /* Set each LED */
  for (uint32_t i = 0; i < 8; i++) {
    /* Set LED i */
    threadSafeDigitalWrite(_LED_DATA, (value & 0x80) ? HIGH : LOW);

    /* TICK CLOCK */
    threadSafeDigitalWrite(_LED_CLK, HIGH);
    threadSafeDigitalWrite(_LED_CLK, LOW);

    /* Shift to next LED */
    value <<= 1;
  }

  /* TICK STROBE */
  threadSafeDigitalWrite(_LED_STROBE, HIGH);
  threadSafeDigitalWrite(_LED_STROBE, LOW);

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Leds::writeRedLED(bool value) {
  threadSafeDigitalWrite(_LED_RED, value);

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Leds::writeGreenLED(bool value) {
  threadSafeDigitalWrite(_LED_GREEN, value);

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Leds::setLED(uint8_t index) {
  _frontLEDsCounter[index & 7] = 2000;

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Leds::setLED(uint8_t index, size_t t) {
  _frontLEDsCounter[index & 7] = t;

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Leds::refreshLEDs(void) {
  uint8_t v = 0;

  /* Iterate current counter. Remove 1 and get Byte */
  for (uint32_t i = 0; i < 8; i++) {
    if (_frontLEDsCounter[i]) {
      _frontLEDsCounter[i]--;
      v |= 1 << i;
    }
  }

  /* Write new values */
  writeLEDs(v);

  /* Return success */
  return PlcErrorCodes::PLC_SUCCESS;
}
