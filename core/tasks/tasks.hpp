/**
 * @file tasks.hpp
 * @author Diego Arcos Sapena
 * @brief Declarations for the main application tasks and thread launchers.
 * @version d-1.0.0
 * @date 2024/07/29
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once

#include <atomic>
#include <climits>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../hardware/plc.hpp"

// ---------------------------------------------------------------------------
// Cycle time statistics — updated from worker threads, read from SIGINT handler
// ---------------------------------------------------------------------------

/**
 * @brief Thread-safe accumulator for cycle time statistics.
 *
 * All fields use relaxed atomics — the only guarantee needed is that
 * individual 64-bit stores/loads are atomic (no ordering between fields).
 * The SIGINT handler reads them after the OS has delivered the signal,
 * which introduces an implicit memory barrier on all common architectures.
 */
struct CycleStats {
  std::atomic<long long> total_us{0};        ///< Accumulated cycle time in microseconds
  std::atomic<long long> count{0};           ///< Number of completed cycles
  std::atomic<long long> min_us{LLONG_MAX};  ///< Minimum cycle time observed
  std::atomic<long long> max_us{0};          ///< Maximum cycle time observed

#if defined(DEBUG) || defined(TRACE)
  /// Record one cycle of duration `elapsed_us` microseconds.
  void record(long long elapsed_us) {
    total_us.fetch_add(elapsed_us, std::memory_order_relaxed);
    count.fetch_add(1, std::memory_order_relaxed);

    // Update min — CAS loop
    long long cur_min = min_us.load(std::memory_order_relaxed);
    while (elapsed_us < cur_min &&
           !min_us.compare_exchange_weak(cur_min, elapsed_us, std::memory_order_relaxed)) {
    }

    // Update max — CAS loop
    long long cur_max = max_us.load(std::memory_order_relaxed);
    while (elapsed_us > cur_max &&
           !max_us.compare_exchange_weak(cur_max, elapsed_us, std::memory_order_relaxed)) {
    }
  }

  /// Print a formatted summary to stdout.
  void print(const std::string &label) const {
    long long n = count.load(std::memory_order_relaxed);
    long long tot = total_us.load(std::memory_order_relaxed);
    long long mn = min_us.load(std::memory_order_relaxed);
    long long mx = max_us.load(std::memory_order_relaxed);

    long long avg = (n > 0) ? (tot / n) : 0;
    if (mn == LLONG_MAX) mn = 0;  // No cycles recorded

    std::cout << "\n  [" << label << "]\n"
              << "    Cycles   : " << n << "\n"
              << "    Avg      : " << avg << " us  (" << (avg / 1000.0) << " ms)\n"
              << "    Min      : " << mn << " us  (" << (mn / 1000.0) << " ms)\n"
              << "    Max      : " << mx << " us  (" << (mx / 1000.0) << " ms)\n"
              << "    Total    : " << tot << " us  (" << (tot / 1000000.0) << " s)\n";
  }
#else
  void record(long long) {}
  void print(const std::string &) const {}
#endif
};

class ExternalModuleStats {
  public:
#if defined(DEBUG) || defined(TRACE)
  void record(uint32_t module_id, long long elapsed_us) {
    std::shared_ptr<CycleStats> stats;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      auto it = m_stats.find(module_id);
      if (it == m_stats.end()) {
        stats = std::make_shared<CycleStats>();
        m_stats[module_id] = stats;
      } else {
        stats = it->second;
      }
    }
    stats->record(elapsed_us);
  }

  void printAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stats.empty()) {
      std::cout << "\n  [Hardware Sync  — external network modules]\n"
                << "    <No external modules running>\n";
    }
    for (auto &pair : m_stats) {
      pair.second->print("Hardware Sync  — external network module ID: " +
                         std::to_string(pair.first));
    }
  }
#else
  void record(uint32_t, long long) {}
  void printAll() {}
#endif

  private:
  std::mutex m_mutex;
  std::map<uint32_t, std::shared_ptr<CycleStats>> m_stats;
};

/// Global stats instances — one per task type.
extern CycleStats g_stats_hw_internal;           ///< internalModulesSyncTask
extern ExternalModuleStats g_stats_hw_external;  ///< externalModuleSyncTask (tracks each external
                                                 ///< module separately)
extern CycleStats g_stats_database;              ///< databaseSyncTask

// --- Main Task Functions ---

/**
 * @brief Task for continuously synchronizing internal (e.g., SPI) modules.
 * @details This task runs in a fast loop, attempting to sync all internal
 * modules. SPI identification is very fast, so no progressive delay is needed
 * on failure.
 * @param modules A vector of shared pointers to the internal modules to be
 * synced.
 */
void internalModulesSyncTask(const std::vector<IModulePtr> &modules);

/**
 * @brief Task for continuously synchronizing a single external (e.g., Modbus)
 * module.
 * @details This task runs in its own thread to prevent a slow or disconnected
 * module from blocking others. It implements a progressive backoff delay for
 * reconnection attempts.
 * @param module A shared pointer to the external module to be synced.
 */
void externalModuleSyncTask(IModulePtr module);

/**
 * @brief Synchronizes the in-memory state of all modules with the database.
 * This task runs in its own thread and handles all database read/write
 * operations.
 * @param[in] plc A shared pointer to the main PLC object containing all
 * modules.
 */
void databaseSyncTask(std::shared_ptr<OsoLogicPLC> plc);

/**
 * @brief Task for managing system timers.
 */
void timerTask();

/**
 * @brief Task for synchronizing the state of hardware LEDs.
 */
void ledsSyncTask();

// --- Thread Launchers ---

/**
 * @brief Launches a thread that executes databaseSyncTask.
 * @param[in] plc A shared pointer to the main PLC object.
 * @return The new thread object.
 */
std::thread startDatabaseSyncTask(std::shared_ptr<OsoLogicPLC> plc);

/**
 * @brief Launches a thread that executes ledsSyncTask.
 * @return The new thread object.
 */
std::thread startLedsSyncTask();

/**
 * @brief Launches a thread that executes timerTask.
 * @return The new thread object.
 */
std::thread startTimerTask();