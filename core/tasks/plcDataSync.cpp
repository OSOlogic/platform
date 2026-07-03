/**
 * @file plcDataSync.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief PLC data synchronization task
 * @version a-1.0.0
 * @date 2024/11/22
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include <chrono>
#include <thread>

#include "../common/debug.hpp"
#include "../common/errors.hpp"
#include "../hardware/IModule.hpp"
#include "tasks.hpp"

void internalModulesSyncTask(const std::vector<IModulePtr>& modules) {
  const long desired_rate_us = 10000;  // 300ms cycle time

  while (true) {
    auto start_time = std::chrono::high_resolution_clock::now();

    for (auto& module : modules) {
#if defined(DEBUG) || defined(TRACE)
      auto start_mod = std::chrono::high_resolution_clock::now();
#endif
      // The sync() method will handle both identification and data exchange.
      PlcErrorCodes sync_rs = module->sync();
#if defined(DEBUG) || defined(TRACE)
      auto end_mod = std::chrono::high_resolution_clock::now();
      auto elapsed_mod =
          std::chrono::duration_cast<std::chrono::microseconds>(end_mod - start_mod).count();

      uint32_t mid;
      module->getModuleId(mid);
      DEBUG_STREAM("Module " << mid << " sync took " << elapsed_mod << " us.");
#endif

      // If a communication error occurs, the module's internal state will be
      // reset by the sync() method itself, triggering a re-identification
      // attempt on the next cycle.
      if (sync_rs != PlcErrorCodes::PLC_SUCCESS) {
        uint32_t module_id = 0;
        module->getModuleId(module_id);
        log_error("internalModulesSyncTask",
                  "Communication error during sync for module " + std::to_string(module_id) + ".",
                  sync_rs);
      }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

#if defined(DEBUG) || defined(TRACE)
    // Record cycle stats (work time only, before sleep)
    g_stats_hw_internal.record(elapsed_time);
    DEBUG_STREAM("PLC INTERNAL SYNC TASK: Cycle time " << elapsed_time << " us.");
#endif

    if (desired_rate_us > elapsed_time) {
      std::this_thread::sleep_for(std::chrono::microseconds(desired_rate_us - elapsed_time));
    }
  }
}

void externalModuleSyncTask(IModulePtr module) {
  const long desired_rate_us = 10000;  // 300ms cycle time

  // Variables for the progressive reconnection delay
  const long INITIAL_RECONNECT_DELAY_S = 2;  // Start with 2 seconds
  const long MAX_RECONNECT_DELAY_S = 30;     // Cap at 30 seconds
  long reconnect_delay_s = INITIAL_RECONNECT_DELAY_S;

  while (true) {
    uint8_t connected = 0;
    module->getConnected(connected);

    if (connected < 2) {
      // Attempt to connect/initialize by calling sync().
      // If not connected, sync() will internally call the identification logic.
      PlcErrorCodes rs = module->sync();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        // --- RECONNECTION LOGIC ---
        uint32_t module_id;
        rs = module->getModuleId(module_id);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("externalModuleSyncTask", "Failed to get module ID for disconnected module.",
                    rs);
        }
        log_error("externalModuleSyncTask",
                  "Connection failed for module " + std::to_string(module_id) + ". Retrying in " +
                      std::to_string(reconnect_delay_s) + " seconds...",
                  rs);

        // 2. Wait for the current delay period.
        std::this_thread::sleep_for(std::chrono::seconds(reconnect_delay_s));

        // 3. Increase the delay for the NEXT attempt, capping it at the maximum.
        reconnect_delay_s *= 2;  // Double the delay
        if (reconnect_delay_s > MAX_RECONNECT_DELAY_S) {
          reconnect_delay_s = MAX_RECONNECT_DELAY_S;
        }
      } else {
        // --- Connection Succeeded ---
        // If we successfully connected, reset the delay for future disconnections.
        reconnect_delay_s = INITIAL_RECONNECT_DELAY_S;
      }
    } else  // Module is connected, proceed with normal synchronization.
    {
      auto start_time = std::chrono::high_resolution_clock::now();

      PlcErrorCodes rs = module->sync();  // Perform a data sync cycle.

      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        // An error here means communication was lost while connected.
        // The sync() method itself will have already called freeModule() internally.
        // The next loop iteration will detect is_connected = false and trigger the reconnection
        // logic.
        uint32_t module_id;
        rs = module->getModuleId(module_id);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("externalModuleSyncTask", "Failed to get module ID for a connected module.",
                    rs);
        }

        log_error("externalModuleSyncTask",
                  "Communication error during sync for module " + std::to_string(module_id) + ".",
                  rs);

        // Reset the delay so the first retry is fast.
        reconnect_delay_s = INITIAL_RECONNECT_DELAY_S;
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      auto elapsed_time =
          std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

#if defined(DEBUG) || defined(TRACE)
      // Record cycle stats (work time only, before sleep)
      uint32_t module_id;
      if (module->getModuleId(module_id) == PlcErrorCodes::PLC_SUCCESS) {
        g_stats_hw_external.record(module_id, elapsed_time);
        DEBUG_STREAM("PLC EXTERNAL SYNC TASK: Cycle time " << elapsed_time << " us for module "
                                                           << module_id);
      }
#endif
      if (desired_rate_us > elapsed_time) {
        std::this_thread::sleep_for(std::chrono::microseconds(desired_rate_us - elapsed_time));
      }
    }
  }
}
