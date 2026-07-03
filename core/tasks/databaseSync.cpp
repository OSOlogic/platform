/**
 * @file databaseSync.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief Database synchronization task
 * @version a-1.0.0
 * @date 2024/11/22
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include <chrono>
#include <string>
#include <thread>

#include "../common/debug.hpp"
#include "../common/errors.hpp"
#include "../database/database.hpp"
#include "../hardware/aggregator_module.hpp"
#include "../hardware/module.hpp"
#include "../hardware/plc.hpp"
#include "tasks.hpp"

/**
 * @brief Synchronizes the connection state and other properties of a PLC module with the database.
 * @param module A shared pointer to the PLC module to synchronize.
 * @param module A shared pointer to the PLC module to synchronize.
 * @param database A shared pointer to the PLC database instance.
 * @param mode The current operation mode of the PLC.
 * @return PlcErrorCodes::PLC_SUCCESS on success, or an error code on failure.
 */
PlcErrorCodes sync_module_states(std::shared_ptr<IModule> module,
                                 std::shared_ptr<PLC_Database> database, OperationMode mode);

void databaseSyncTask(std::shared_ptr<OsoLogicPLC> plc) {
  std::shared_ptr<PLC_Database> database;
  PlcErrorCodes rs;

  rs = PLC_Database::getInstance(database);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("databaseSyncTask", "Failed to get database instance. Task cannot run.", rs);
    return;
  }

  rs = database->cleanup_rtmirror();
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("databaseSyncTask",
              "Initial database cleanup failed. This may indicate inconsistent data.", rs);
    return;
  }

  const long desired_rate_us = 10000;  // 10ms cycle time

  // Get the reverse map once before the loop for maximum efficiency.
  std::map<std::pair<int32_t, uint32_t>, std::vector<AggregatedTarget>> reverse_map;
  rs = plc->getReverseMap(reverse_map);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("databaseSyncTask", "Failed to get PLC reverse_map", rs);
  }

  while (true) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<IModulePtr> modules;
    rs = plc->getModules(modules);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("databaseSyncTask", "Failed to get PLC modules. Skipping cycle.", rs);
      continue;
    }

    // --- 1. TRANSACTION AND CONNECTION STATE SYNC ---
    if ((rs = database->set_autocommit(false)) != PlcErrorCodes::PLC_SUCCESS) {
      log_error("databaseSyncTask", "Failed to disable autocommit for databaseSyncTask", rs);
      continue;
    }

    std::vector<DbUpdateInstruction> all_updates_list;
    bool error_in_cycle = false;

    // Get operation mode for mode-aware required value filtering
    OperationMode mode;
    if ((rs = plc->getOperationMode(mode)) != PlcErrorCodes::PLC_SUCCESS) {
      log_error("databaseSyncTask", "Failed to get operation mode.", rs);
      error_in_cycle = true;
    }

    // --- 2.2 DATABASE TO MEMORY SYNC (BATCHED) ---
    // Fetch required values filtered by current mode's purpose
    std::map<uint32_t, std::vector<std::pair<uint32_t, uint64_t>>> all_required_values;
    auto start_phase1 = std::chrono::high_resolution_clock::now();  // TIMING START PHASE 1

    if (!error_in_cycle && (rs = database->get_all_required_values(all_required_values, mode)) !=
                               PlcErrorCodes::PLC_SUCCESS) {
      log_error("databaseSyncTask", "Failed to get all required values.", rs);
      error_in_cycle = true;
    }

    std::vector<uint32_t> modules_to_clear_required;

    for (auto &module : modules) {
      if (error_in_cycle) {
        break;
      }
      uint32_t module_id;
      if ((rs = module->getModuleId(module_id)) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("databaseSyncTask",
                  "Failed to get module_id for a module. Skipping sync for this module.", rs);
        error_in_cycle = true;
        continue;
      }

      // mode already obtained before the loop

      uint8_t connected_before = 0;
      if ((rs = module->getConnected(connected_before)) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("databaseSyncTask",
                  "Failed to get pre-sync connected status for module " + std::to_string(module_id),
                  rs);
        error_in_cycle = true;
        continue;
      }

      if ((rs = sync_module_states(module, database, mode)) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("databaseSyncTask",
                  "Error syncing module connection state for module " + std::to_string(module_id),
                  rs);
        error_in_cycle = true;
        continue;
      }

      uint8_t connected = 0;
      if ((rs = module->getConnected(connected)) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("databaseSyncTask",
                  "Failed to get connected status for module " + std::to_string(module_id), rs);
        error_in_cycle = true;
        continue;
      }

      if (connected >= 2) {
        // --- Handle aggregator reconnection: collect all values from children ---
        if (connected_before == 2) {
          auto aggregator = std::dynamic_pointer_cast<AggregatorModule>(module);
          if (aggregator) {
            // Aggregator just reconnected: collect all current values from children
            std::vector<DbUpdateInstruction> aggregator_updates;
            if ((rs = aggregator->collectAllCurrentValues(mode, aggregator_updates)) !=
                PlcErrorCodes::PLC_SUCCESS) {
              log_error("databaseSyncTask",
                        "Failed to collect values for aggregator " + std::to_string(module_id), rs);
              error_in_cycle = true;
              continue;
            }

            // Add all aggregator updates to the main list
            for (const auto &update : aggregator_updates) {
              all_updates_list.push_back(update);
            }

            DEBUG_STREAM("Collected " << aggregator_updates.size()
                                      << " values for reconnected aggregator " << module_id);
          }
        }

        // MEMORY TO DATABASE SYNC only scanning physical modules. Their corresponding aggregated
        // aggregator parents files in database will be updated from this scan. Readonly and
        // writeread values in hardware
        auto physical_module = std::dynamic_pointer_cast<Module>(module);
        if (physical_module) {
          // --- 2. DATA SYNC IF MODULE IS CONNECTED ---

          // --- 2.1 MEMORY TO DATABASE SYNC (Write updates from memory to DB) ---

          // Get all changed values in a single locked operation
          bool force_all = (connected_before == 2);
          std::vector<std::pair<uint32_t, uint64_t>> changed_values;

          if ((rs = physical_module->collectChangedValues(mode, force_all, changed_values)) !=
              PlcErrorCodes::PLC_SUCCESS) {
            log_error("databaseSyncTask",
                      "Failed to collect changed values for module " + std::to_string(module_id),
                      rs);
            error_in_cycle = true;
            continue;
          }

          for (const auto &change : changed_values) {
            uint32_t io_def_id = change.first;
            uint64_t current_val_bits = change.second;

            all_updates_list.push_back({module_id, io_def_id, current_val_bits});

            // Add update instructions for any aggregated points mapped to this physical point
            auto key = std::make_pair(module_id, io_def_id);
            auto it_rev = reverse_map.find(key);
            if (it_rev != reverse_map.end()) {
              for (const auto &target : it_rev->second) {
                TRACE_STREAM("[MEM->DB] -> Aggregated Module "
                             << target.aggregated_module_id
                             << " io_def=" << target.aggregated_io_definition_id
                             << " value=" << current_val_bits);
                all_updates_list.push_back({target.aggregated_module_id,
                                            target.aggregated_io_definition_id, current_val_bits});
              }
            }
          }
        }

        // --- PROCESS BATCHED REQUIRED VALUES FOR THIS MODULE ---
        auto it_req = all_required_values.find(module_id);
        if (it_req != all_required_values.end()) {
          bool module_update_success = true;

          if ((rs = module->setAllRequiredValues(it_req->second)) != PlcErrorCodes::PLC_SUCCESS) {
            log_error("databaseSyncTask",
                      "Failed to set all required values for module " + std::to_string(module_id),
                      rs);
            error_in_cycle = true;
            module_update_success = false;
          }

          if (module_update_success) {
            modules_to_clear_required.push_back(module_id);
          }
        }
      }
    }

    // --- 2.3 CLEAN UP REQUIRED VALUES in DB (BATCHED) ---
    if (!error_in_cycle && !modules_to_clear_required.empty()) {
      if ((rs = database->update_required_values_to_null_batch(modules_to_clear_required)) !=
          PlcErrorCodes::PLC_SUCCESS) {
        error_in_cycle = true;
        log_error("databaseSyncTask", "Failed to batch clean required values.", rs);
      } else {
        auto end_phase1 = std::chrono::high_resolution_clock::now();  // TIMING END PHASE 1
        auto duration_phase1 =
            std::chrono::duration_cast<std::chrono::microseconds>(end_phase1 - start_phase1)
                .count();
        // Count total changes
        size_t total_changes = 0;
        for (auto m_id : modules_to_clear_required)
          total_changes += all_required_values[m_id].size();
        DEBUG_STREAM("[TIMING] PHASE 1 (DB->MEM): Detected " << total_changes << " changes. Took "
                                                             << duration_phase1 << " us.");
      }
    }

    // --- 3. EXECUTE ALL DB UPDATES AND FINALIZE TRANSACTION ---
    if (!error_in_cycle) {
      // If there are updates, send them all in a single request
      if (!all_updates_list.empty()) {
        auto start_phase4 = std::chrono::high_resolution_clock::now();  // TIMING START PHASE 4
        rs = database->batch_update_rtmirror_values(all_updates_list);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          error_in_cycle = true;
          log_error("databaseSyncTask", "Failed during batch update execution.", rs);
        } else {
          auto end_phase4 = std::chrono::high_resolution_clock::now();  // TIMING END PHASE 4
          auto duration_phase4 =
              std::chrono::duration_cast<std::chrono::microseconds>(end_phase4 - start_phase4)
                  .count();
          DEBUG_STREAM("[TIMING] PHASE 4 (MEM->DB): Batch updating "
                       << all_updates_list.size() << " values. Took " << duration_phase4 << " us.");
        }
      }
    }

    if (error_in_cycle) {
      database->rollback();
      log_error("databaseSyncTask", "Transaction rolled back due to error during cycle.",
                PlcErrorCodes::PLC_SUCCESS);
    } else {
      if ((rs = database->commit()) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("databaseSyncTask", "Failed to commit transaction.", rs);
      }
    }

    if ((rs = database->set_autocommit(true)) != PlcErrorCodes::PLC_SUCCESS) {
      log_error("databaseSyncTask", "Failed to re-enable autocommit.", rs);
    }

    // --- Rate Limiting ---
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

#if defined(DEBUG) || defined(TRACE)
    // Record cycle stats (work time only, before sleep)
    g_stats_database.record(elapsed_time);
    DEBUG_STREAM("DATABASE SYNC TASK: Cycle time " << elapsed_time << " us.");
#endif
    if (desired_rate_us > elapsed_time) {
      std::this_thread::sleep_for(std::chrono::microseconds(desired_rate_us - elapsed_time));
    }
  }
}

std::thread startDatabaseSyncTask(std::shared_ptr<OsoLogicPLC> plc) {
  return std::thread(databaseSyncTask, plc);
}

PlcErrorCodes sync_module_states(std::shared_ptr<IModule> module,
                                 std::shared_ptr<PLC_Database> database, OperationMode mode) {
  uint32_t module_id;
  uint8_t connected;
  uint32_t model_id;
  uint32_t uuid;
  uint16_t starts;
  uint8_t version;
  uint16_t wdt_value;
  PlcErrorCodes rs;

  rs = module->getModuleId(module_id);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("sync_module_states", "Failed to get module_id from module.", rs);
    return rs;
  }
  rs = module->getModelId(model_id);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("sync_module_states", "Failed to get model_id from module.", rs);
    return rs;
  }

  rs = module->getConnected(connected);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("sync_module_states",
              "Failed to get connection status for module " + std::to_string(module_id), rs);
    return rs;
  }

  // B. Logic to handle connection state
  if (connected == 2) {
    DEBUG_STREAM("DATABASE: Module " << static_cast<int>(module_id) << " has just been connected");

    // 1. Update the connection status and runtime info in the 'devices' table
    rs = database->update_is_connected(module_id, true);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error(
          "sync_module_states",
          "Failed to update is_connected state to true for module " + std::to_string(module_id),
          rs);
      return rs;
    }

    // 2. Insert the primary keys into the runtime state tables (inserts FILTERED I/O definitions)
    rs = database->insert_pk(module_id, model_id, mode);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error(
          "sync_module_states",
          "Failed to insert in runtime tables for connected module " + std::to_string(module_id),
          rs);
      return rs;
    }

    // 3. Mark the module as connected and with the DB updated
    rs = module->setConnected(3);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("sync_module_states",
                "Failed to update connection status to '3' for module " + std::to_string(module_id),
                rs);
      return rs;
    }
  } else if (connected == 0) {
    DEBUG_STREAM("DATABASE: Module " << static_cast<int>(module_id)
                                     << " has just been disconnected");

    // The decision to delete is made here, based on the connection state.
    rs = database->delete_data(module_id);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      // If deletion fails, we should not transition to a stable state, so we can retry.
      return rs;
    }

    // Update the connection status in the 'devices' table
    rs = database->update_is_connected(module_id, false);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error(
          "sync_module_states",
          "Failed to update is_connected state to false for module " + std::to_string(module_id),
          rs);
      return rs;
    }

    // Mark the module as disconnected and with the DB updated
    rs = module->setConnected(1);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("sync_module_states",
                "Failed to update connection status to '1' for module " + std::to_string(module_id),
                rs);
      return rs;
    }
  }

  // Nothing changed (connected == 1 or 3), no action needed
  return PlcErrorCodes::PLC_SUCCESS;
}