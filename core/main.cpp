/**
 * @file main.cpp
 * @author Diego Arcos Sapena
 * @brief Main PLC application entry point
 * @version a-1.0.0
 * @date 2024/11/23
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include <pthread.h>
#include <unistd.h>
#include <wiringPi.h>

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "common/async_logger.hpp"
#include "communication/channels/rs485_channel.hpp"
#include "communication/channels/spi_channel.hpp"
#include "communication/channels/tcp_channel.hpp"
#include "communication/protocols/Iprotocol.hpp"
#include "communication/protocols/modbus/modbus_rtu_protocol.hpp"
#include "communication/protocols/modbus/modbus_tcp_protocol.hpp"
#include "communication/protocols/spi/spi_protocol.hpp"
#include "database/database.hpp"
#include "hardware/IModule.hpp"
#include "hardware/aggregator_module.hpp"
#include "hardware/gpio.hpp"
#include "hardware/module.hpp"
#include "hardware/plc.hpp"
#include "tasks/tasks.hpp"

/**
 * @brief Dedicated signal-watcher thread: waits for SIGINT (Ctrl+C) using
 *        sigwait(), prints cycle-time statistics, then terminates the process.
 *
 * This is the correct pattern for multi-threaded Linux applications:
 *   1. Block the signal in ALL threads (done in main() before spawning any
 * thread).
 *   2. Let ONE dedicated thread unblock and wait for it with sigwait().
 * This way no async-signal-safety issues arise, and external libraries like
 * wiringPi cannot steal the handler because the signal is masked at the OS
 * level.
 */
static void SignalWatcherThread() {
  // Build the set of signals this thread will wait for.
  sigset_t wait_set;
  sigemptyset(&wait_set);
  sigaddset(&wait_set, SIGINT);
  sigaddset(&wait_set, SIGTERM);

  int caught_signal = 0;
  // sigwait() is a blocking, cancellation-safe call.
  // It returns when one of the signals in wait_set is delivered to the process.
  sigwait(&wait_set, &caught_signal);

#if defined(DEBUG) || defined(TRACE)
  // ---- signal received ----
  std::cout << "\n\n================================================\n"
            << " CYCLE TIME STATISTICS (signal " << caught_signal << " — final report)\n"
            << "================================================\n";

  g_stats_hw_internal.print("Hardware Sync  — internal SPI modules");
  g_stats_hw_external.printAll();
  g_stats_database.print("Database Sync  — memory <-> database");

  std::cout << "\n================================================\n" << std::endl;
#endif

  // Flush remaining log entries before exit
  AsyncLogger::stop();
  _exit(0);
}

/**
 * @brief Main function.
 * @param[in] argc Number of parameters from console.
 * @param[in] argv Pointer to parameters from console.
 * @return int Execution result value.
 */
int main(int argc, char** argv) {
  // -----------------------------------------------------------------------
  // SIGNAL SETUP — must be done BEFORE any thread is created.
  //
  // Strategy: block SIGINT and SIGTERM in the main thread. Because threads
  // inherit the signal mask of their creator, ALL subsequent threads will
  // also have these signals blocked. Then we launch one dedicated watcher
  // thread that unblocks them via sigwait(). This survives wiringPi and any
  // other library that installs its own handlers.
  // -----------------------------------------------------------------------
  sigset_t block_set;
  sigemptyset(&block_set);
  sigaddset(&block_set, SIGINT);
  sigaddset(&block_set, SIGTERM);
  // Block in the calling thread (inherited by all child threads).
  pthread_sigmask(SIG_BLOCK, &block_set, nullptr);

  // Launch the watcher thread now, while the mask is already in effect.
  // It will call sigwait() internally and never return until the signal fires.
  std::thread watcher_thread(SignalWatcherThread);
  watcher_thread.detach();  // We don't join it; _exit() terminates everything.

  PlcErrorCodes rs;
  std::thread databaseSyncThread;
  // Start async logger FIRST — before any log call.
  AsyncLogger::start();

  log_msg("[INFO] PLC OSOlogic Linux - version 1.00");

  log_msg("[INFO] Initializing GPIO");
  rs = plcGpioInit();
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Failed to initialize GPIO.", rs);
    AsyncLogger::stop();
    return -1;
  }

  log_msg("[INFO] Starting leds sync thread");
  std::thread ledsSyncThread = startLedsSyncTask();

  log_msg("[INFO] Starting timer thread");
  std::thread timerThread = startTimerTask();

  log_msg("[INFO] Connecting to database...");
  std::shared_ptr<PLC_Database> database;
  rs = PLC_Database::getInstance(database);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Failed to connect to the database. Cannot continue.", rs);
    AsyncLogger::stop();
    return -1;
  }

  std::vector<DeviceConfig> deviceConfigs;
  log_msg("[INFO] Retrieving device configurations from database...");
  rs = database->getDeviceConfigurations(
      deviceConfigs);  // Fetches device configurations from the database
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main",
              "Failed to retrieve device configurations from database. Cannot "
              "continue.",
              rs);
    AsyncLogger::stop();
    return -1;
  }

  log_msg("[INFO] Loading global PLC settings...");
  PLC_Config plc_config;  // Create config struct with default values
  rs = database->get_plc_config(plc_config);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Could not load PLC settings from database.", rs);
    AsyncLogger::stop();
    return -1;
  }
  log_msg("[INFO] RS-485 Baud Rate: " + std::to_string(plc_config.rs485_baudrate));

  // Set the operation mode globally on the PLC class (static)
  rs = OsoLogicPLC::setOperationMode(plc_config.operation_mode);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Could not set operation mode.", rs);
    AsyncLogger::stop();
    return -1;
  }
  log_msg("[INFO] Operation Mode set to: " +
          std::string(plc_config.operation_mode == OperationMode::EXECUTION ? "EXECUTION"
                                                                            : "CONFIGURATION"));

  // Map to store communication channels and mutexes (rs485) by type and
  // connection string. This prevents creating multiple channel instances for
  // the same physical bus/connection.
  std::map<std::pair<std::string, std::string>, ChannelPtr> active_channels;
  std::map<std::pair<std::string, std::string>, std::shared_ptr<std::mutex>> bus_mutexes;
  auto spi_bus_mutex = std::make_shared<std::mutex>();

  log_msg("[INFO] Creating PLC modules based on database configuration...");

  // Vectors to manage creation
  std::vector<IModulePtr> created_modules;
  std::vector<DeviceConfig> physical_configs;
  std::vector<DeviceConfig> aggregated_configs_pending;

  // --- STEP 1: Separate configurations into physical and aggregated ---
  for (const auto& config : deviceConfigs) {
    if (config.channel == "aggregated") {
      aggregated_configs_pending.push_back(config);
      log_msg("[INFO]   Aggregated config found: " + config.module_name);
    } else {
      physical_configs.push_back(config);
    }
  }
  // --- STEP 2: Create all physical modules ---
  for (const auto& config : physical_configs) {
    ChannelPtr channel = nullptr;
    ProtocolPtr protocol_handler = nullptr;

    // --- Determine Channel and Protocol based on DB configuration ---
    // 1. Logic for SPI Modules (Internal)
    if (config.channel == "spi" && config.protocol == "borrell-spi") {
      // Create SPI channel only once if it hasn't been created yet for this
      // connection string Assuming 'embedded-spi' is a common connection string
      // for the single SPI bus.
      std::pair<std::string, std::string> channel_key = {
          config.channel, config.connection_string};  // Key for map: {channel, connection_string}
      if (active_channels.find(channel_key) ==
          active_channels.end())  // If channel not found in map
      {
        // Initialize a new SPI_Channel with predefined GPIO pins and delays
        channel = std::make_shared<SPI_Channel>(
            PIN_SPI_G, PIN_SPI_CS0, PIN_SPI_CS1, PIN_SPI_CS2, PIN_SPI_MOSI, PIN_SPI_MISO,
            PIN_SPI_CLK, SPI_DEFAULT_DELAY, SPI_DEFAULT_DELAY_START, SPI_DEFAULT_DELAY_RW);
        active_channels[channel_key] = channel;  // Store the new channel in the map
      } else {
        channel = active_channels[channel_key];  // Reuse existing channel
      }
      // Create a ProtocolSPIV0 handler for the SPI channel
      protocol_handler = std::make_shared<ProtocolSPIV0>(channel, spi_bus_mutex);
    }
    // 2. Logic for RS-485 internal bus Modbus RTU Modules (External)
    else if (config.channel == "rs485" && config.protocol == "modbus-rtu") {
      uint32_t baud_rate = plc_config.rs485_baudrate;  // Use baudrate from config
      // Reuse the channel and mutex if another module is on the same serial
      // port (bus)
      std::pair<std::string, std::string> channel_key = {config.channel, config.connection_string};
      if (active_channels.find(channel_key) == active_channels.end()) {
        // Create the RS485 channel. This version assumes a system-level driver
        // is handling the transmission direction control, just like the C code.
        channel = std::make_shared<RS485_Channel>(
            config.connection_string, baud_rate, plc_config.rs485_parity, plc_config.rs485_bit_stop,
            plc_config.rs485_data_bits);
        active_channels[channel_key] = channel;
        bus_mutexes[channel_key] = std::make_shared<std::mutex>();
      } else {
        channel = active_channels[channel_key];
      }
      auto bus_mutex = bus_mutexes[channel_key];

      // Create the Modbus RTU protocol handler
      protocol_handler = std::make_shared<ModbusRtuProtocol>(channel, bus_mutex, config.timeout_ms);
    }
    // 4. Logic for RS-485 Modbus RTU Over TCP Modules (External)
    else if (config.channel == "tcp" && config.protocol == "modbus-rtu") {
      // --- Parse IP and Port from the connection_string ---
      size_t colon_pos = config.connection_string.find(":");
      if (colon_pos == std::string::npos) {
        log_error("main",
                  "Invalid Modbus RTU over TCP connection string for '" + config.module_name +
                      "'. Expected IP:Port.",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
        continue;
      }

      std::string ip_address = config.connection_string.substr(0, colon_pos);
      int port = 0;
      try {
        port = std::stoi(config.connection_string.substr(colon_pos + 1));
      } catch (const std::exception& e) {
        log_error("main", "Invalid port in connection string for '" + config.module_name + "'.",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
        continue;
      }

      // --- Reuse or create the communication channel ---
      std::pair<std::string, std::string> channel_key = {config.channel, config.connection_string};
      if (active_channels.find(channel_key) == active_channels.end()) {
        // Use TCP_Channel, our generic TCP channel implementation.
        channel = std::make_shared<TCP_Channel>(ip_address, port);
        active_channels[channel_key] = channel;
        bus_mutexes[channel_key] = std::make_shared<std::mutex>();
      } else {
        channel = active_channels[channel_key];
      }
      auto bus_mutex = bus_mutexes[channel_key];

      protocol_handler = std::make_shared<ModbusRtuProtocol>(channel, bus_mutex, config.timeout_ms);
    }
    // 5. Logic for TCP Modbus TCP/IP Modules (External)
    else if (config.channel == "tcp" && config.protocol == "modbus-tcp") {
      // --- Parse IP and Port from the connection_string ---
      size_t colon_pos = config.connection_string.find(":");
      if (colon_pos == std::string::npos) {
        log_error("main",
                  "Invalid Modbus TCP connection string for '" + config.module_name +
                      "'. Expected IP:Port.",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
        continue;
      }

      std::string ip_address = config.connection_string.substr(0, colon_pos);
      int port = 0;
      try {
        port = std::stoi(config.connection_string.substr(colon_pos + 1));
      } catch (const std::exception& e) {
        log_error("main", "Invalid port in connection string for '" + config.module_name + "'.",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
        continue;
      }

      // --- Reuse or create the communication channel ---
      std::pair<std::string, std::string> channel_key = {config.channel, config.connection_string};
      if (active_channels.find(channel_key) == active_channels.end()) {
        // Use TCP_Channel, our generic TCP channel implementation.
        channel = std::make_shared<TCP_Channel>(ip_address, port);
        active_channels[channel_key] = channel;
        bus_mutexes[channel_key] = std::make_shared<std::mutex>();
      } else {
        channel = active_channels[channel_key];
      }
      auto bus_mutex = bus_mutexes[channel_key];
      protocol_handler = std::make_shared<ModbusTcpProtocol>(channel, bus_mutex, config.timeout_ms);
    }
    // Handle unsupported combinations
    else {
      log_error("main",
                "Unsupported channel/protocol combination for device '" + config.module_name +
                    "': " + config.channel + "/" + config.protocol + ". Skipping.",
                PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
      continue;  // Skip this device
    }

    // --- Create Module and Assign Configuration from DeviceConfig ---
    if (protocol_handler)  // Proceed only if a protocol handler was successfully
                           // created/identified
    {
      auto module = std::make_shared<Module>(
          config.module_id, config.model_id, config.module_name, config.address,
          config.max_read_bit_block_size, config.max_read_register_block_size,
          config.max_write_bit_block_size, config.max_write_register_block_size, config.channel,
          config.protocol,
          protocol_handler);  // Create the module with its protocol handler

      if (config.address.empty()) {
        log_error("main",
                  "Empty channel address for device '" + config.module_name +
                      "'. Skipping module initialization.",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
        continue;
      }

      created_modules.push_back(module);
    }
  }

  log_msg("[INFO] Initializing all PHYSICAL modules...");  // Needed for virtual
                                                           // modules that depend
                                                           // on physical ones
  for (const auto& module : created_modules) {
    std::string channel;
    PlcErrorCodes rs = module->getChannel(channel);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get channel for module.", rs);
      continue;
    }

    if (channel != "aggregated") {
      if ((rs = module->initialize()) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("main", "Failed to initialize physical module with ID ", rs);
      }
    }
  }

  // --- STEP 3: Iterative loop to create aggregated modules ---
  while (!aggregated_configs_pending.empty()) {
    bool progress_made_this_iteration = false;

    for (auto it = aggregated_configs_pending.begin(); it != aggregated_configs_pending.end();
         /* no increment here */) {
      const auto& config = *it;
      bool can_create_module = true;  // Flag to track if creation is possible in this iteration

      // --- 1. Parse Child IDs from Connection String (with error handling) ---
      size_t colon_pos = config.connection_string.find(":");
      if (colon_pos == std::string::npos) {
        log_error("main",
                  "Invalid aggregated connection string for '" + config.module_name +
                      "'. Expected format: type:id1;id2;...",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
        it = aggregated_configs_pending.erase(it);  // Remove invalid config
        continue;
      }

      std::string ids_string = config.connection_string.substr(colon_pos + 1);
      std::vector<int32_t> child_module_ids;
      std::string current_id_str;
      for (char c : ids_string) {
        if (c == ';' || c == ',') {
          if (!current_id_str.empty()) {
            try {
              child_module_ids.push_back(std::stoi(current_id_str));
            } catch (const std::exception& e) {
              log_error("main",
                        "Invalid child module ID in aggregated connection string: '" +
                            current_id_str + "' for module '" + config.module_name + "'",
                        PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
              can_create_module = false;
              break;
            }
            current_id_str.clear();
          }
        } else {
          current_id_str += c;
        }
      }
      if (can_create_module && !current_id_str.empty()) {
        try {
          child_module_ids.push_back(std::stoi(current_id_str));
        } catch (const std::exception& e) {
          log_error("main",
                    "Invalid child module ID in aggregated connection string: '" + current_id_str +
                        "' for module '" + config.module_name + "'",
                    PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
          can_create_module = false;
        }
      }

      if (!can_create_module) {                     // If parsing failed
        it = aggregated_configs_pending.erase(it);  // Remove invalid config
        continue;
      }

      // --- 2. Check if all children have been created ---
      std::vector<IModulePtr> child_modules_for_aggregator;
      bool all_children_found = true;
      for (int32_t id : child_module_ids) {
        bool found = false;
        for (const auto& module_ptr : created_modules) {
          uint32_t current_id;
          module_ptr->getModuleId(current_id);
          if (current_id == static_cast<uint32_t>(id)) {
            child_modules_for_aggregator.push_back(module_ptr);
            found = true;
            break;
          }
        }
        if (!found) {
          all_children_found = false;
          break;
        }
      }

      // --- 3. If dependencies are met, validate and create the aggregated
      // module ---
      if (all_children_found) {
        // --- 3a. Validate child modules match the model definition ---
        std::vector<uint32_t> expected_child_model_ids;
        rs = database->getAggregatedModelChildren(config.model_id, expected_child_model_ids);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("main",
                    "Failed to get aggregated_model_children for model " +
                        std::to_string(config.model_id) + " ('" + config.module_name +
                        "'). Skipping.",
                    rs);
          it = aggregated_configs_pending.erase(it);
          continue;
        }

        if (expected_child_model_ids.size() != child_modules_for_aggregator.size()) {
          log_error("main",
                    "Model " + std::to_string(config.model_id) + " ('" + config.module_name +
                        "') expects " + std::to_string(expected_child_model_ids.size()) +
                        " children but connection_string provides " +
                        std::to_string(child_modules_for_aggregator.size()) + ". Skipping.",
                    PlcErrorCodes::ERROR_CONFIGURATION_INVALID_VALUE);
          it = aggregated_configs_pending.erase(it);
          continue;
        }

        bool model_mismatch = false;
        for (size_t slot = 0; slot < expected_child_model_ids.size(); ++slot) {
          uint32_t actual_child_model_id;
          child_modules_for_aggregator[slot]->getModelId(actual_child_model_id);
          if (actual_child_model_id != expected_child_model_ids[slot]) {
            uint32_t actual_child_id;
            child_modules_for_aggregator[slot]->getModuleId(actual_child_id);
            log_error("main",
                      "Model mismatch at slot " + std::to_string(slot) + " for '" +
                          config.module_name +
                          "': expected model_id=" + std::to_string(expected_child_model_ids[slot]) +
                          " but child module " + std::to_string(actual_child_id) +
                          " has model_id=" + std::to_string(actual_child_model_id),
                      PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL);
            model_mismatch = true;
          }
        }
        if (model_mismatch) {
          log_error("main",
                    "Aggregated module '" + config.module_name +
                        "' has child model mismatches. Skipping.",
                    PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL);
          it = aggregated_configs_pending.erase(it);
          continue;
        }

        // --- 3b. Create the aggregated module ---
        log_msg("[INFO]   Creating aggregated module: " + config.module_name +
                " (dependencies met, validation OK)");

        std::vector<AggregatedMappingEntry> mappings;
        rs = database->getAggregatedIoMap(config.model_id, mappings);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("main",
                    "Failed to get aggregated I/O map for '" + config.module_name + "'. Skipping.",
                    rs);
          it = aggregated_configs_pending.erase(it);
          continue;
        }

        auto module = std::make_shared<AggregatorModule>(
            config.module_id, config.model_id, config.module_name, config.address, config.channel,
            config.protocol, child_modules_for_aggregator, mappings);

        created_modules.push_back(module);

        it = aggregated_configs_pending.erase(it);
        progress_made_this_iteration = true;
      } else {
        ++it;  // Cannot be created yet, try next one
      }
    }

    // --- 4. Check for circular dependencies or unresolvable errors ---
    if (!progress_made_this_iteration && !aggregated_configs_pending.empty()) {
      log_error("main",
                "Circular dependency or missing module ID detected. Cannot "
                "create the following aggregated modules:",
                PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL);
      for (const auto& config : aggregated_configs_pending) {
        log_error("main",
                  "  - " + config.module_name + " (ID: " + std::to_string(config.module_id) + ")",
                  PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL);
      }
      break;  // Exit the loop to avoid an infinite loop
    }
  }

  log_msg("[INFO] Initializing all AGGREGATED modules...");
  for (const auto& module : created_modules) {
    std::string channel;
    PlcErrorCodes rs = module->getChannel(channel);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get channel for module.", rs);
      continue;
    }

    if (channel == "aggregated") {
      if ((rs = module->initialize()) != PlcErrorCodes::PLC_SUCCESS) {
        log_error("main", "Failed to initialize aggregated module with ID ", rs);
      }
    }
  }
  rs = OsoLogicPLC::initializeInstance(created_modules);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Failed to initialize PLC instance.", rs);
    AsyncLogger::stop();
    return -1;
  }
  OsoLogicPLCPtr plc;
  rs = OsoLogicPLC::getInstance(plc);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Failed to get PLC instance.", rs);
    AsyncLogger::stop();
    return -1;
  }

  log_msg("[INFO] Building hierarchical reverse map for aggregated modules...");
  plc->buildReverseMap();
  log_msg("[INFO] Reverse map built successfully.");
  log_msg("[INFO] PLC initialization process completed.");

#ifdef DEBUG
  std::vector<IModulePtr> debug_modules;
  rs = plc->getModules(debug_modules);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("main", "Failed to get PLC modules for debug display.", rs);
  }

  DEBUG_STREAM("----------------------------");
  DEBUG_STREAM("MODULE INFORMATION (After Initialization):");
  DEBUG_STREAM("----------------------------");

  for (auto& module : debug_modules) {
    uint32_t module_id = 0;
    uint32_t model_id = 0;
    uint8_t connected = 0;
    std::string channel;
    std::vector<IoDefinition> io_defs;

    rs = module->getModuleId(module_id);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get module ID for module.", rs);
      exit(1);
    }
    rs = module->getModelId(model_id);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get model ID for module.", rs);
      exit(1);
    }
    rs = module->getChannel(channel);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get channel for module.", rs);
      exit(1);
    }
    rs = module->getConnected(connected);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get connected status for module.", rs);
      exit(1);
    }
    rs = module->getAllIoDefinitions(io_defs);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("main", "Failed to get IO definitions for module.", rs);
      exit(1);
    }

    DEBUG_STREAM("Module ID   : " << module_id);
    DEBUG_STREAM("  Model ID  : " << model_id);
    DEBUG_STREAM("  Channel   : " << channel);
    DEBUG_STREAM("  Connected : " << static_cast<int>(connected));
    DEBUG_STREAM("  IO Points : " << io_defs.size());
    DEBUG_STREAM("----------------------------");
  }
#endif

  // Separate all modules into internal and external for task management
  // Vectors to hold the separated modules
  std::vector<IModulePtr> internal_modules;  // For fast, local SPI
  std::vector<IModulePtr> external_modules;  // For slower, network-based protocols (TCP, etc.)

  for (const auto& module : created_modules) {
    std::string channel;
    module->getChannel(channel);

    // IMPORTANT FILTER! Only non-aggregated modules have hardware
    // synchronization tasks.
    if (channel != "aggregated") {
      if (channel == "spi") {
        internal_modules.push_back(module);
      } else {
        external_modules.push_back(module);
      }
    }
  }

  log_msg("[INFO] Total modules created: " + std::to_string(created_modules.size()));
  log_msg("[INFO] Non-aggregated modules (with tasks): " +
          std::to_string(internal_modules.size() + external_modules.size()));
  log_msg(
      "[INFO]   Aggregated (Aggregators): " +
      std::to_string(created_modules.size() - internal_modules.size() - external_modules.size()));
  log_msg("[INFO]   Internal modules (SPI): " + std::to_string(internal_modules.size()));
  log_msg("[INFO]   External modules (Network): " + std::to_string(external_modules.size()));

  // --- Launch Threads ---
  log_msg("[INFO] Starting all tasks only for non aggregator modules...");

  std::vector<std::thread> task_threads;

  // The database sync task needs access to all modules
  // 1 thread and 1 connection to database for simplicity. There is no need for
  // using a pool of threads due to the small dimensity of this scope
  task_threads.emplace_back(databaseSyncTask, plc);

  // The SPI sync task gets only the internal modules for a fast, dedicated
  // polling loop
  if (!internal_modules.empty()) {
    task_threads.emplace_back(internalModulesSyncTask, internal_modules);
  }

  // Launch one dedicated thread for each external network module (TCP, etc.)
  for (auto& module : external_modules) {
    task_threads.emplace_back(externalModuleSyncTask, module);
  }

  log_msg("[INFO] All " + std::to_string(task_threads.size()) +
          " tasks launched. System is running.");

  // Wait for all threads to complete (they won't in this case, but it's good
  // practice)
  for (auto& t : task_threads) {
    t.join();
  }

  return 0;
}
