/**
 * @file async_logger.cpp
 * @author Diego Arcos Sapena
 * @brief Asynchronous logger implementation.
 * @version a-1.0.0
 * @date 2026/03/03
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "async_logger.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "errors.hpp"  // For LOG_FILE_RUTE, MAX_LOG_FILE_SIZE

namespace fs = std::filesystem;

// --- Static member definitions ---
std::queue<std::string> AsyncLogger::_queue;
std::mutex AsyncLogger::_queueMutex;
std::condition_variable AsyncLogger::_cv;
std::atomic<bool> AsyncLogger::_running{false};
std::thread AsyncLogger::_thread;

// ---------------------------------------------------------------------------

void AsyncLogger::start() {
  if (_running.exchange(true)) return;  // Already started

  _thread = std::thread(&AsyncLogger::_worker);
}

void AsyncLogger::stop() {
  _running.store(false, std::memory_order_release);
  _cv.notify_one();
  if (_thread.joinable()) _thread.join();
}

void AsyncLogger::enqueue(std::string msg) {
  {
    std::lock_guard<std::mutex> lock(_queueMutex);
    _queue.push(std::move(msg));
  }
  _cv.notify_one();
}

// ---------------------------------------------------------------------------

void AsyncLogger::_worker() {
  while (true) {
    std::string entry;

    // --- Wait for work or stop signal ---
    {
      std::unique_lock<std::mutex> lock(_queueMutex);
      _cv.wait(lock, [] {
        return !AsyncLogger::_queue.empty() ||
               !AsyncLogger::_running.load(std::memory_order_acquire);
      });

      // Drain any remaining entries even after stop() is called
      if (_queue.empty()) break;  // _running == false and queue is empty → exit

      entry = std::move(_queue.front());
      _queue.pop();
    }

    // --- Perform the actual I/O (outside the lock) ---

    // 1. Console output
    std::cerr << entry << '\n';

    // 2. Rotating file output
    try {
      const std::string filename = LOG_FILE_RUTE;

      if (fs::exists(filename) && fs::file_size(filename) >= MAX_LOG_FILE_SIZE) {
        std::string old_filename = filename + ".old";
        if (fs::exists(old_filename)) fs::remove(old_filename);
        fs::rename(filename, old_filename);
      }

      std::ofstream log_file(filename, std::ios::app);
      if (log_file.is_open()) {
        log_file << entry << '\n';
      }
    } catch (const fs::filesystem_error &e) {
      std::cerr << "[ASYNC LOGGER] Filesystem error: " << e.what() << '\n';
    } catch (const std::exception &e) {
      std::cerr << "[ASYNC LOGGER] Unexpected error: " << e.what() << '\n';
    }
  }
}
