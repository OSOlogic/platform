/**
 * @file timer.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief Timer tasks
 * @version a-1.0.0
 * @date 2024/11/13
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include <memory>

#include "../hardware/leds.hpp"
#include "tasks.hpp"

std::thread timerThread;

void timerTask() {
  for (;;) {
    std::shared_ptr<Leds> leds;
    PlcErrorCodes rs = Leds::getInstance(leds);

    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("ModuleName::FunctionName", "Failed to get Leds instance.", rs);
    }
    rs = leds->refreshLEDs();
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("timerTask", "Failed to refresh LEDs.", rs);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

std::thread startTimerTask() {
  return std::thread(timerTask);
}
