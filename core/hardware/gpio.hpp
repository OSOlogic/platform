/**
 * @file gpio.hpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief GPIO definitions and initialization header
 * @version a-1.0.0
 * @date 2024/08/19
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once
#include <mutex>

#include "../common/errors.hpp"

/* SPI PINS */

// Data transmission
#define PIN_SPI_G 15

// Chip Selection
#define PIN_SPI_CS0 5
#define PIN_SPI_CS1 7
#define PIN_SPI_CS2 8

// Master Output Slave Input
#define PIN_SPI_MOSI 11

// Master Input Slave Output
#define PIN_SPI_MISO 12

// Clock
#define PIN_SPI_CLK 14

/* LED PINS */

// GREEN / RED
#define PIN_LED_GREEN 16
#define PIN_LED_RED 13

// 2x4 FRONTAL LED MATRIX
#define PIN_LED_CLK 18
#define PIN_LED_DATA 19
#define PIN_LED_STROBE 17

/* OTHER PINS */

/* SYNCHRONIZATION PIN */
#define PIN_SYNC 20

/**
 * @brief Mutex for thread safe IO.
 */
static std::mutex IOMutex{};

/* FUNCTIONS */

/**
 * @brief Initializes all needed PINs.
 * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
 */
PlcErrorCodes plcGpioInit(void);

/**
 * @brief Makes digitalWrite from wiringpi thread safe.
 * @param[in] pin Pin to write.
 * @param[in] value Value to set.
 */
void threadSafeDigitalWrite(uint8_t pin, uint8_t value);

/**
 * @brief Makes digitalRead from wiringpi thread safe.
 * @param[in] pin Pin to read.
 * @return int value read.
 */
int threadSafeDigitalRead(uint8_t pin);
