/**
 * @file async_logger.hpp
 * @author Diego Arcos Sapena
 * @brief Asynchronous logger — lock-free enqueue, dedicated writer thread.
 * @version a-1.0.0
 * @date 2026/03/03
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 *
 * @details
 * All calls to log_msg(), log_error(), DEBUG_STREAM(), and TRACE_STREAM()
 * ultimately call AsyncLogger::enqueue(), which is a near-instant push onto
 * a queue. A dedicated background thread drains the queue and performs the
 * actual I/O (stderr + rotating file). This ensures that no sync thread ever
 * blocks on console or file I/O, especially while holding a mutex.
 *
 * Usage:
 *   AsyncLogger::start();   // once at application startup (before any logging)
 *   AsyncLogger::stop();    // once at application shutdown (flushes queue)
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class AsyncLogger {
  public:
  /**
   * @brief Starts the background writer thread.
   * Must be called once before any log output is expected.
   */
  static void start();

  /**
   * @brief Gracefully stops the logger: drains remaining entries, then joins.
   * Blocks until the queue is empty and the thread has exited.
   */
  static void stop();

  /**
   * @brief Enqueues a log entry for asynchronous output. Never blocks.
   * Safe to call from any thread, including threads holding mutexes.
   * @param msg The pre-formatted log string to write.
   */
  static void enqueue(std::string msg);

  private:
  /// The background writer loop.
  static void _worker();

  static std::queue<std::string> _queue;
  static std::mutex _queueMutex;
  static std::condition_variable _cv;
  static std::atomic<bool> _running;
  static std::thread _thread;
};
