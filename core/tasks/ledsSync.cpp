/**
 * @file ledsSync.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief LEDs synchronization task
 * @version a-1.0.0
 * @date 2024/11/13
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include <wiringPi.h>

#include <chrono>
#include <memory>
#include <thread>

#include "../hardware/gpio.hpp"
#include "../hardware/leds.hpp"
#include "tasks.hpp"

void ledsSyncTask() {
  unsigned int divisor = 0;
  uint8_t valor = 0;
  for (;;) {
    if (++divisor >= 1024) {
      divisor = 0;
      valor = valor ? 0 : 1;
      // Signal for synchronizing with peripherals
      threadSafeDigitalWrite(PIN_SYNC, valor);
    }
    std::shared_ptr<Leds> leds;
    PlcErrorCodes rs = Leds::getInstance(leds);

    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("ModuleName::FunctionName", "Failed to get Leds instance.", rs);
    }
    leds->writeGreenLED((divisor & 512) ? 1 : 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

std::thread startLedsSyncTask() {
  return std::thread(ledsSyncTask);
}
