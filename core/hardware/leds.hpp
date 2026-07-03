/**
 * @file leds.hpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief Leds header.
 * @version a-1.0.0
 * @date 2024/11/16
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <inttypes.h>
#include <stdio.h>

#include <memory>
#include <mutex>

#include "../common/errors.hpp"

#define LED_BLINK_TICKS 5

/**
 * @class Leds
 * @brief Implements the LED control for the PLC.
 *
 * This class provides methods to control the front LEDs, including writing values,
 * blinking, and managing the state of the LEDs.
 */
class Leds {
  public:
  /**
   * @brief Destroy the Leds object.
   */
  ~Leds();

  /**
   * @brief Write status to front LEDs where each led is 1 bit
   * @param[in] value Value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes writeLEDs(uint8_t value);

  /**
   * @brief Turn on or off the red LED.
   * @param[in] value Value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes writeRedLED(bool value);

  /**
   * @brief Turn on or off the green LED.
   * @param[in] value Value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes writeGreenLED(bool value);

  /**
   * @brief Set led to blink for {LED_BLINK_TICKS} cycles.
   * @param[in] value Value to set.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setLED(uint8_t index);

  /**
   * @brief Set led to blink for {t} cycles.
   * @param[in] value Value to set.
   * @param[in] t Number of cycles to blink.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes setLED(uint8_t index, size_t t);

  /**
   * @brief Advance 1 cycle in LEDs blink time.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes refreshLEDs(void);

  /**
   * @brief Get the pointer of Leds instance.
   */
  static PlcErrorCodes getInstance(std::shared_ptr<Leds> &instance_ref);

  private:
  /**
   * @brief Green LED Pin.
   */
  uint8_t _LED_GREEN;

  /**
   * @brief Red LED Pin.
   */
  uint8_t _LED_RED;

  /**
   * @brief LED clock pin.
   */
  uint8_t _LED_CLK;

  /**
   * @brief LED data pin.
   */
  uint8_t _LED_DATA;

  /**
   * @brief LED strobe pin.
   */
  uint8_t _LED_STROBE;

  /**
   * @brief Synchronization pin.
   */
  uint8_t _SYNC;

  /**
   * @brief LED blink counter
   */
  uint8_t _frontLEDsCounter[8];

  /**
   * @brief Single instance for Singleton pattern
   */
  static std::shared_ptr<Leds> _instance_ptr;

  /**
   * @brief Mutex for Singleton pattern. Avoid getting multiple
   * instances in multithreaded environment
   */
  static std::mutex _instanceMutex;

  /**
   * @brief Construct a new Leds object.
   * Private for Singleton pattern.
   */
  Leds();
};
