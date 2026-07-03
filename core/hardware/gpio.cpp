/**
 * @file gpio.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief GPIO definitions and initialization code
 * @version a-1.0.0
 * @date 2024/08/19
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "gpio.hpp"

#include <wiringPi.h>

#include <mutex>

#include "../common/errors.hpp"

PlcErrorCodes plcGpioInit(void) {
  /* Init wiringPi library */
  if (wiringPiSetup()) {
    log_error("plcGpioInit", "Failed to initialize WiringPi library.",
              PlcErrorCodes::ERROR_WPSETUP);
    return PlcErrorCodes::ERROR_WPSETUP;
  }

  /* Set SPI pins */
  pinMode(PIN_SPI_G, OUTPUT);
  pinMode(PIN_SPI_CS0, OUTPUT);
  pinMode(PIN_SPI_CS1, OUTPUT);
  pinMode(PIN_SPI_CS2, OUTPUT);
  pinMode(PIN_SPI_MOSI, OUTPUT);
  pinMode(PIN_SPI_MISO, INPUT);
  pinMode(PIN_SPI_CLK, OUTPUT);

  /* Set LED pins */
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_CLK, OUTPUT);
  pinMode(PIN_LED_DATA, OUTPUT);
  pinMode(PIN_LED_STROBE, OUTPUT);

  /* Set other pins */
  pinMode(PIN_SYNC, OUTPUT);

  /* Log successful GPIO initialization */
  log_msg("[INFO] GPIO initialized successfully.");

  return PlcErrorCodes::PLC_SUCCESS;
}

void threadSafeDigitalWrite(uint8_t pin, uint8_t value) {
  std::lock_guard<std::mutex> lock(IOMutex);
  digitalWrite(pin, value);
}

int threadSafeDigitalRead(uint8_t pin) {
  std::lock_guard<std::mutex> lock(IOMutex);
  return digitalRead(pin);
}
