/**
 * @file protocol.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Jorge Martín Morant
 * @brief Communication protocol over SPI (code)
 * @version a-1.0.0
 * @date 2024/08/28
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "module.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <tuple>

#include "../common/debug.hpp"
#include "../common/errors.hpp"
#include "../communication/protocols/spi/spi_protocol.hpp"
#include "../database/database.hpp"
#include "../hardware/plc.hpp"

Module::Module(uint32_t module_id, uint32_t model_id, const std::string& module_name,
               const std::string& address_on_channel, uint16_t _max_read_bit_block_size,
               uint16_t _max_read_register_block_size, uint16_t _max_write_bit_block_size,
               uint16_t _max_write_register_block_size, const std::string& channel,
               const std::string& protocol, ProtocolPtr backend)
    : _module_id(module_id),
      _model_id(model_id),
      _module_name(module_name),
      _address_on_channel(address_on_channel),
      _channel(channel),
      _protocol(protocol),
      _backend(backend),
      _max_read_bit_block_size(_max_read_bit_block_size),
      _max_read_register_block_size(_max_read_register_block_size),
      _max_write_bit_block_size(_max_write_bit_block_size),
      _max_write_register_block_size(_max_write_register_block_size),
      _uuid(0),
      _connected(0),
      _force_full_sync(false) {}

PlcErrorCodes Module::initialize() {
  PlcErrorCodes rs;

  // --- READ THE FULL I/O DEFINITION FROM THE MASTER TABLE ---

  std::shared_ptr<PLC_Database> database;
  rs = PLC_Database::getInstance(database);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::identifyModule", "Failed to get database instance for model config.", rs);
    rs = freeModule();
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::identifyModule",
                "Failed to free module after database instance error for "
                "module_id: " +
                    std::to_string(_module_id) + ".",
                rs);
    }
    return rs;
  }
  /* Use lock_guard to manage mutex automatically */
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  std::vector<IoDefinition> io_definitions;

  // Get ALL I/O definitions regardless of mode (mode filtering happens at sync
  // time)
  if ((rs = database->getIoDefinitions(_model_id, _module_id, io_definitions)) !=
      PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::initialize",
              "Failed to get I/O definitions for model_id " + std::to_string(_model_id), rs);
    return rs;
  }

  // --- Build the COMPLETE I/O block structure for initial/full sync ---
  // This contains ALL purposes and is used when module connects or
  // force_full_sync is true.
  _io_blocks_initial.clear();
  _io_blocks_sync.clear();

  if (!io_definitions.empty()) {
    // Sort ALL definitions for _io_blocks_initial
    std::sort(io_definitions.begin(), io_definitions.end(),
              [](const IoDefinition& a, const IoDefinition& b) {
                if (a.io_type != b.io_type) return a.io_type < b.io_type;
                if (a.hardware_access != b.hardware_access)
                  return a.hardware_access < b.hardware_access;
                if (a.physical_address != b.physical_address)
                  return a.physical_address < b.physical_address;
                if (a.access_method == 2) return a.bitmask_offset < b.bitmask_offset;
                return a.logical_address < b.logical_address;
              });

    // Build initial blocks from all definitions
    rs = _buildIOBlocks(io_definitions, _io_blocks_initial);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::initialize", "Failed to build initial I/O blocks.", rs);
      return rs;
    }
  }

  // --- Build the MODE-FILTERED I/O block structure for sync cycles ---
  OperationMode mode;
  rs = OsoLogicPLC::getOperationMode(mode);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::initialize", "Failed to get operation mode.", rs);
    return rs;
  }

  // Filter definitions by purpose based on current mode
  std::vector<IoDefinition> sync_definitions;
  for (const auto& def : io_definitions) {
    bool should_include = (mode == OperationMode::EXECUTION)
                              ? (def.purpose == 1)                       // standard only
                              : (def.purpose == 2 || def.purpose == 3);  // secure_state or config

    if (should_include) {
      sync_definitions.push_back(def);
    }
  }

  if (!sync_definitions.empty()) {
    // Sort and build sync blocks
    std::sort(sync_definitions.begin(), sync_definitions.end(),
              [](const IoDefinition& a, const IoDefinition& b) {
                if (a.io_type != b.io_type) return a.io_type < b.io_type;
                if (a.hardware_access != b.hardware_access)
                  return a.hardware_access < b.hardware_access;
                if (a.physical_address != b.physical_address)
                  return a.physical_address < b.physical_address;
                if (a.access_method == 2) return a.bitmask_offset < b.bitmask_offset;
                return a.logical_address < b.logical_address;
              });

    rs = _buildIOBlocks(sync_definitions, _io_blocks_sync);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::initialize", "Failed to build sync I/O blocks.", rs);
      return rs;
    }
  }

  DEBUG_STREAM("[INIT] Module ID: " << _module_id << " Initial Blocks (Total: "
                                    << _io_blocks_initial.size() << "):");
  int b_idx = 0;
  for (const auto& b : _io_blocks_initial) {
    DEBUG_STREAM("  Block " << ++b_idx << ": " << "Type=" << (int)b.io_type << " "
                            << "Acc=" << (int)b.hardware_access << " " << "Qty=" << b.quantity
                            << " " << "Start=" << b.physical_start_address);
  }

  DEBUG_STREAM("[INIT] Module ID: "
               << _module_id << " Sync Blocks (Mode: "
               << (mode == OperationMode::EXECUTION ? "EXECUTION" : "CONFIGURATION")
               << ", Total: " << _io_blocks_sync.size() << "):");
  b_idx = 0;
  for (const auto& b : _io_blocks_sync) {
    DEBUG_STREAM("  Block " << ++b_idx << ": " << "Type=" << (int)b.io_type << " "
                            << "Acc=" << (int)b.hardware_access << " " << "Qty=" << b.quantity
                            << " " << "Start=" << b.physical_start_address);
  }

  // --- Populate the io_definition_map ---
  _io_definition_map.clear();
  for (const auto& def : io_definitions) {
    _io_definition_map[def.io_definition_id] = def;
  }

  // --- LOAD SECURE STATE MAPPING ---
  std::map<uint32_t, uint32_t> secure_mapping_ids;
  if ((rs = database->getSecureStateMapping(_model_id, secure_mapping_ids)) !=
      PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::initialize",
              "Failed to get secure state mapping for model_id " + std::to_string(_model_id), rs);
    return rs;
  }

  _secure_state_map_bits.clear();
  _secure_state_map_registers.clear();

  for (const auto& pair : secure_mapping_ids) {
    uint32_t standard_id = pair.first;
    uint32_t secure_id = pair.second;

    if (_io_definition_map.find(standard_id) != _io_definition_map.end() &&
        _io_definition_map.find(secure_id) != _io_definition_map.end()) {
      const auto& std_def = _io_definition_map[standard_id];
      const auto& sec_def = _io_definition_map[secure_id];

      if (std_def.io_type == 1)  // Bit
      {
        _secure_state_map_bits[std_def.logical_address] = sec_def.logical_address;
      } else  // Register
      {
        _secure_state_map_registers[std_def.logical_address] = sec_def.logical_address;
      }
    }
  }
  // Clear and initialize the data maps based on the I/O blocks
  _input_bits.clear();
  _output_bits.clear();
  _input_registers.clear();
  _output_registers.clear();
  _secure_state_bits.clear();
  _secure_state_registers.clear();
  _config_registers.clear();
  _config_bits.clear();

  for (const auto& def : io_definitions) {
    if (def.purpose == 1) {    // standard
      if (def.io_type == 1) {  // bit
        if (def.hardware_access == 1) {
          _input_bits[def.logical_address] = {};
        } else {
          _output_bits[def.logical_address] = {};
        }
      } else {  // register
        if (def.hardware_access == 1) {
          _input_registers[def.logical_address] = {};
        } else {
          _output_registers[def.logical_address] = {};
        }
      }
    } else if (def.purpose == 2) {  // secure_state
      if (def.io_type == 1) {
        _secure_state_bits[def.logical_address] = {};
      } else {
        _secure_state_registers[def.logical_address] = {};
      }
    } else if (def.purpose == 3) {  // config
      if (def.io_type == 1) {
        _config_bits[def.logical_address] = {};
      } else {
        _config_registers[def.logical_address] = {};
      }
    }
  }

  //_connected = 2;

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::identifyModule() {
  PlcErrorCodes rs;

  std::string current_address_on_channel;
  std::string current_channel;
  std::string current_protocol;
  ProtocolPtr current_backend;
  uint8_t current_connected_state;
  std::vector<IO_Block> current_io_blocks;
  int32_t current_module_id;

  // Check if the backend pointer itself is null before attempting to
  // dereference it for any protocol call.
  {
    std::unique_lock<std::recursive_mutex> lock(_mutex);

    current_address_on_channel = _address_on_channel;
    current_channel = _channel;
    current_protocol = _protocol;
    current_backend = _backend;
    current_connected_state = _connected;
    current_io_blocks = _io_blocks_initial;
    current_module_id = _module_id;

    if (!current_backend) {
      log_error("Module::identifyModule",
                "Backend protocol pointer is null for module_id: " +
                    std::to_string(current_module_id) + ". Cannot communicate.",
                PlcErrorCodes::ERROR_NULL_POINTER);
      if (current_connected_state == 2 || current_connected_state == 3) {
        rs = freeModule();
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::identifyModule",
                    "Failed to free module after null backend for module_id: " +
                        std::to_string(current_module_id) + ".",
                    rs);
        }
      }
      return PlcErrorCodes::ERROR_NULL_POINTER;
    }
  }

  // Get the database instance to access model configuration.
  std::shared_ptr<PLC_Database> database;
  rs = PLC_Database::getInstance(database);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::identifyModule", "Failed to get database instance for model config.", rs);
    rs = freeModule();
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::identifyModule",
                "Failed to free module after database instance error for "
                "module_id: " +
                    std::to_string(current_module_id) + ".",
                rs);
    }
    return rs;
  }

  // *******************************************************************
  // CONDITIONAL LOGIC: Specific for internal SPI modules (borrell-spi)
  // *******************************************************************
  if (current_channel == "spi" && current_protocol == "borrell-spi") {
    // Get basic module information (model, version, starts) from SPI hardware
    auto backend = std::dynamic_pointer_cast<ProtocolSPIV0>(current_backend);
    if (!backend) {
      log_error("Module::identifyModule",
                "Backend is not a valid ProtocolSPIV0 instance for module_id: " +
                    std::to_string(current_module_id) + ". Cannot communicate.",
                PlcErrorCodes::ERROR_NULL_POINTER);
      if (current_connected_state == 2 || current_connected_state == 3) {
        rs = freeModule();
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::identifyModule",
                    "Failed to free module after backend cast error for module_id: " +
                        std::to_string(current_module_id) + ".",
                    rs);
          return rs;
        }
      }
      return PlcErrorCodes::ERROR_NULL_POINTER;
    }

    // Validate hardware model ID against configured model ID
    uint8_t module_info[6] = {0};
    rs = backend->getModuleInfo(current_address_on_channel, module_info);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::identifyModule",
                "Failed to read basic info from SPI device at " + current_address_on_channel +
                    " (module_id: " + std::to_string(current_module_id) + ").",
                rs);
      if (current_connected_state == 2 || current_connected_state == 3) {
        if (freeModule() != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::identifyModule",
                    "Failed to free module after info read error for module_id: " +
                        std::to_string(current_module_id) + ".",
                    rs);
        }
      }
      return rs;
    }

    uint32_t hardware_model_id = module_info[0];
    if (hardware_model_id != static_cast<uint32_t>(_model_id)) {
      log_error("Module::identifyModule",
                "Hardware Model ID mismatch for module " + std::to_string(current_module_id) +
                    "! Expected: " + std::to_string(_model_id) +
                    ", but hardware reported: " + std::to_string(hardware_model_id),
                PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL);

      if (current_connected_state == 2 || current_connected_state == 3) {
        if (freeModule() != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::identifyModule",
                    "Failed to free module after model mismatch for module_id: " +
                        std::to_string(current_module_id) + ".",
                    PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL);
        }
      }
      return PlcErrorCodes::ERROR_CONFIGURATION_INVALID_MODEL;
    }

    // Read UUID from SPI hardware
    uint32_t hardware_uuid = 0;
    DEBUG_STREAM("current_address_on_channel: " << std::stoi(current_address_on_channel));
    rs = backend->readUuid(current_address_on_channel, hardware_uuid);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::identifyModule",
                "Failed to read UUID from module at SPI address " + current_address_on_channel +
                    " (module_id: " + std::to_string(current_module_id) + ").",
                rs);
      rs = freeModule();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::identifyModule",
                  "Failed to free module after readUuid error for module_id: " +
                      std::to_string(current_module_id) + ".",
                  rs);
      }
      return rs;
    }
    {
      std::unique_lock<std::recursive_mutex> lock(_mutex);
      uint64_t hardware_uuid_64 = hardware_uuid;
      memcpy(&_config_registers[101].value, &hardware_uuid_64,
             sizeof(_config_registers[101].value));

      DEBUG_STREAM("Module at module_id " << current_module_id
                                          << ": UUID read from hardware: " << hardware_uuid);

      if (_uuid == 0 || _uuid != hardware_uuid) {
        DEBUG_STREAM("Module at module_id "
                     << current_module_id
                     << ": Hardware identity changed or first identification. "
                        "Updating UUID to "
                     << hardware_uuid << ". Triggering data cleanup.");
        rs = freeModule();
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::identifyModule",
                    "Failed to free module after hardware identity change for "
                    "module_id: " +
                        std::to_string(current_module_id) + ".",
                    rs);
          return rs;
        }
        _uuid = hardware_uuid;  // Update internal UUID to the new hardware UUID.
                                // return PlcErrorCodes::PLC_SUCCESS;
      } else {                  // Case 2: No hardware change detected (same UUID)
        DEBUG_STREAM("Module at module_id "
                     << current_module_id << ": No hardware change detected. UUID: "
                     << hardware_uuid << ". Continuing with current configuration.");
        // identity_changed = false; // Identity is stable.
      }
    }
  }

  // =========================================================================
  // COMMON LOGIC FOR ALL MODULE TYPES
  // =========================================================================

  // ATTEMPT TO CONNECT BEFORE DOING ANYTHING ELSE
  rs = current_backend->connect();
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::identifyModule",
              "Failed to connect to external module at " + current_address_on_channel, rs);
    if (current_connected_state == 2 || current_connected_state == 3) {
      rs = freeModule();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::identifyModule",
                  "Failed to free module after connection error for module_id: " +
                      std::to_string(current_module_id) + ".",
                  rs);
      }
    }
    return rs;
  }

  // PERFORM INTELLIGENT PING TO VERIFY COMMUNICATION
  // We use the first I/O block to perform a representative read operation.
  if ((rs = current_backend->pingDevice(current_address_on_channel, current_io_blocks.front())) !=
      PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::identifyModule",
              "Ping failed for module " + std::to_string(current_module_id) +
                  " At address on channel " + current_address_on_channel +
                  ". The device is not responding.",
              rs);
    if (current_connected_state == 2 || current_connected_state == 3) {
      rs = freeModule();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::identifyModule",
                  "Failed to free module after ping error for module_id: " +
                      std::to_string(current_module_id) + ".",
                  rs);
      }
    }
    return rs;  // This failure will trigger the retry logic
  }

  // Final update of `_connected` state.
  // If it was previously disconnected (0 or 1) and all checks passed, it's now
  // truly connected (2). If it was already connected (2 or 3) and all checks
  // passed, it remains connected.
  {
    std::unique_lock<std::recursive_mutex> lock(_mutex);
    if (_connected == 0 || _connected == 1) {
      //_connected = 2;          // Physically connected, needs DB update.
      _force_full_sync = true;  // Force full sync with database in rtmirror table.
    }

    // =========================================================================
    // INFORMATION LOGGING AND RETURN
    // =========================================================================
    DEBUG_STREAM("====[ MODULE INFO DEVICE ID "
                 << static_cast<int>(current_module_id) << " (Channel: " << current_channel
                 << ", Address on channel: " << current_address_on_channel << ") ]====");
    DEBUG_STREAM("Configured Model ID:     " << static_cast<int>(_model_id));
    DEBUG_STREAM("Hardware UUID:           " << static_cast<unsigned int>(_uuid));

    // --- New I/O Block Information Logging ---
    if (current_io_blocks.empty()) {
      DEBUG_STREAM("I/O Block Configuration: No I/O blocks defined for this model.");
    } else {
      DEBUG_STREAM("I/O Block Configuration:");
      int block_count = 1;
      for (const auto& block : current_io_blocks) {
        DEBUG_STREAM("  - Block " << block_count++ << ": Type: " << std::to_string(block.io_type)
                                  << ", Hardware access: " << std::to_string(block.hardware_access)
                                  << ", Qty: " << block.quantity
                                  << ", StartAddr: " << block.physical_start_address);
      }
    }

    DEBUG_STREAM("-----------------------------------------");
    DEBUG_STREAM("Internal Connected State: " << static_cast<int>(current_connected_state));
    DEBUG_STREAM("=========================================");
  }
  return PlcErrorCodes::PLC_SUCCESS;  // Returns success if identification and
                                      // configuration were correct
}

PlcErrorCodes Module::_syncBlock_HW_to_Mem(const IO_Block& block) {
  PlcErrorCodes rs;

  if (block.io_type == 1)  // Bit operations
  {
    auto buffer = std::make_unique<bool[]>(block.quantity);
    auto start_phase3 = std::chrono::high_resolution_clock::now();  // TIMING START PHASE 3
    rs = _backend->readBits(_address_on_channel, block.physical_start_address, buffer.get(),
                            block.quantity, block.hardware_access);
    auto end_phase3 = std::chrono::high_resolution_clock::now();  // TIMING END PHASE 3
    long duration_phase3 =
        std::chrono::duration_cast<std::chrono::microseconds>(end_phase3 - start_phase3).count();

    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::_syncBlock_HW_to_Mem",
                "Failed to read bits from module at address " + _address_on_channel, rs);
      return rs;
    }
    {
      std::unique_lock<std::recursive_mutex> lock(_mutex);
      bool timing_printed = false;
      for (uint16_t i = 0; i < block.quantity; ++i) {
        const auto& def = block.contained_definitions[i];
        uint16_t logical_addr = def.logical_address;
        bool current_val = false;

        // Get current value by calling the specific accessor
        if (def.purpose == 1) {  // standard
          rs = (block.hardware_access == 1) ? getInputBitValue(logical_addr, current_val)
                                            : getOutputBitValue(logical_addr, current_val);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_HW_to_Mem",
                      "Failed to get current bit value for logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
        } else if (def.purpose == 2) {  // secure_state
          rs = getSecureStateBitValue(logical_addr, current_val);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_HW_to_Mem",
                      "Failed to get current secure state bit value for "
                      "logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
        } else if (def.purpose == 3) {  // config
          rs = getConfigBitValue(logical_addr, current_val);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_HW_to_Mem",
                      "Failed to get current config bit value for logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
        }

        // If value changed or full sync is forced, update memory
        if (current_val != buffer[i] || _force_full_sync) {
          if (!timing_printed) {
            DEBUG_STREAM("[TIMING] PHASE 3 (HW->MEM): Read Bits HW took "
                         << duration_phase3 << " us. Change detected.");
            timing_printed = true;
          }
          if (def.purpose == 1) {              // standard
            if (block.hardware_access == 1) {  // input
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " INPUT_BIT addr " << logical_addr
                                               << ": " << current_val << " -> " << buffer[i]);
              rs = setInputBitValue(logical_addr, buffer[i]);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set input bit value for logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " INPUT_BIT addr " << logical_addr
                                               << " update_flag = true");
              rs = setInputBitUpdateValue(logical_addr, true);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set input bit update flag for logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
            } else {  // output
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " OUTPUT_BIT addr " << logical_addr
                                               << ": " << current_val << " -> " << buffer[i]);
              rs = setOutputBitValue(logical_addr, buffer[i]);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set output bit value for logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " OUTPUT_BIT addr " << logical_addr
                                               << " update_flag = true");
              rs = setOutputBitUpdateValue(logical_addr, true);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set output bit update flag for logical "
                          "address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
            }
          } else if (def.purpose == 2) {  // secure_state
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " SECURE_STATE_BIT addr "
                                             << logical_addr << ": " << current_val << " -> "
                                             << buffer[i]);
            rs = setSecureStateBitValue(logical_addr, buffer[i]);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set secure state bit value for logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " SECURE_STATE_BIT addr "
                                             << logical_addr << " update_flag = true");
            rs = setSecureStateBitUpdateValue(logical_addr, true);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set secure state bit update flag for "
                        "logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
          } else if (def.purpose == 3) {  // config
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " CONFIG_BIT addr " << logical_addr
                                             << ": " << current_val << " -> " << buffer[i]);
            rs = setConfigBitValue(logical_addr, buffer[i]);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set config bit value for logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " CONFIG_BIT addr " << logical_addr
                                             << " update_flag = true");
            rs = setConfigBitUpdateValue(logical_addr, true);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set config bit update flag for logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
          }
        }
      }
    }
  } else  // Register operations
  {
    auto buffer = std::make_unique<uint16_t[]>(block.quantity);
    long duration_phase3 = 0;  // TIMING VARIABLE

    // Special handling for SPI modules
    auto spi_backend = std::dynamic_pointer_cast<ProtocolSPIV0>(_backend);
    if (spi_backend && block.contained_definitions[0].purpose == 3 &&
        block.contained_definitions[0].logical_address >= 100 &&
        block.contained_definitions[0].logical_address <= 106) {
      // WDT
      if (block.contained_definitions[0].logical_address ==
          100)  // WDT is at logical address 100 in config
      {
        uint16_t wdt_value = 0;
        rs = spi_backend->readWDT(_address_on_channel, wdt_value);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::_syncBlock_Mem_to_HW",
                    "Failed to read WDT on SPI module at address " + _address_on_channel, rs);
          return rs;
        }
        buffer[0] = wdt_value;  // emulates that the WDT is in the first
                                // register. In fact, it is in E2PROM
      }

      else if (block.contained_definitions[0].logical_address == 101) {
        // UUID
        uint32_t uuid_value = 0;
        rs = spi_backend->readUuid(_address_on_channel, uuid_value);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::_syncBlock_Mem_to_HW",
                    "Failed to read UUID on SPI module at address " + _address_on_channel, rs);
          return rs;
        }
        buffer[0] |= uuid_value >> 16;       // emulates that the UUID_low is in the second
                                             // register. In fact, it is in E2PROM
        buffer[1] |= (uuid_value & 0xFFFF);  // emulates that the UUID_high is in the
                                             // second register. In fact, it is in E2PROM
      }

      else if (block.contained_definitions[0].logical_address == 102) {
        // Starts
        uint16_t starts_value;
        uint8_t module_info[6] = {0};  // model, num_bits, num_regs, fw_version, starts (uint16_t)
        rs = spi_backend->getModuleInfo(_address_on_channel, module_info);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::_syncBlock_Mem_to_HW",
                    "Failed to get Starts on SPI module at address " + _address_on_channel, rs);
          return rs;
        }
        memcpy(&starts_value, &module_info[4], sizeof(starts_value));
        buffer[0] = starts_value;  // emulates that the Starts is in the third
                                   // register. In fact, it is in E2PROM
      }

      else if (block.contained_definitions[0].logical_address == 103) {
        // Firmware Version
        uint8_t module_info[6] = {0};
        rs = spi_backend->getModuleInfo(_address_on_channel, module_info);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error(
              "Module::_syncBlock_Mem_to_HW",
              "Failed to get Firmware Version on SPI module at address " + _address_on_channel, rs);
          return rs;
        }
        buffer[0] = module_info[3];  // emulates that the Firmware Version is in the
                                     // fourth register. In fact, it is in E2PROM
      }

      else if (block.contained_definitions[0].logical_address == 104) {
        // Model ID
        uint8_t module_info[6] = {0};
        rs = spi_backend->getModuleInfo(_address_on_channel, module_info);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::_syncBlock_Mem_to_HW",
                    "Failed to get Model ID on SPI module at address " + _address_on_channel, rs);
          return rs;
        }
        buffer[0] = module_info[0];  // emulates that the Model ID is in the
                                     // fifth register
      }

      else if (block.contained_definitions[0].logical_address == 105) {
        // Number of bits
        uint8_t module_info[6] = {0};
        rs = spi_backend->getModuleInfo(_address_on_channel, module_info);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::_syncBlock_Mem_to_HW",
                    "Failed to get Number of bits on SPI module at address " + _address_on_channel,
                    rs);
          return rs;
        }
        buffer[0] = module_info[1];  // emulates that the Number of bits is in
                                     // the sixth register
      }

      else if (block.contained_definitions[0].logical_address == 106) {
        // Number of registers
        uint8_t module_info[6] = {0};
        rs = spi_backend->getModuleInfo(_address_on_channel, module_info);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error(
              "Module::_syncBlock_Mem_to_HW",
              "Failed to get Number of registers on SPI module at address " + _address_on_channel,
              rs);
          return rs;
        }
        buffer[0] = module_info[2];  // emulates that the Number of registers is
                                     // in the seventh register
      }
    } else {
      auto start_phase3 = std::chrono::high_resolution_clock::now();  // TIMING START PHASE 3
      rs = _backend->readRegisters(_address_on_channel, block.physical_start_address, buffer.get(),
                                   block.quantity, block.hardware_access);
      auto end_phase3 = std::chrono::high_resolution_clock::now();  // TIMING END PHASE 3
      duration_phase3 =
          std::chrono::duration_cast<std::chrono::microseconds>(end_phase3 - start_phase3).count();

      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::_syncBlock_HW_to_Mem",
                  "Failed to read registers from module with id: " + std::to_string(_module_id),
                  rs);
        return rs;
      }
    }

    {
      std::unique_lock<std::recursive_mutex> lock(_mutex);
      bool timing_printed = false;
      const uint16_t* buffer_ptr = buffer.get();
      for (const auto& def : block.contained_definitions) {
        uint64_t new_value = 0;  // Value to store in internal memory

        // Calculate the index in the buffer based on the definition's physical
        // address
        uint16_t buffer_index = def.physical_address - block.physical_start_address;

        // Bounds check (ensure the index is within the buffer)
        if (buffer_index >= block.quantity) {
          // Log message translated
          log_error("Module::_syncBlock_HW_to_Mem",
                    "Buffer overflow detected for register definition (index check).",
                    PlcErrorCodes::ERROR_OVERFLOW);
          return PlcErrorCodes::ERROR_OVERFLOW;
        }

        if (def.access_method == 1)  // 1 = Direct Register
        {
          // Assemble the value (might use multiple registers from the buffer)
          uint64_t bit_pattern = 0;
          // Bounds check for the end of this register definition
          if (buffer_index + def.register_count > block.quantity) {
            // Log message translated
            log_error("Module::_syncBlock_HW_to_Mem", "Buffer overflow on multi-register read.",
                      PlcErrorCodes::ERROR_OVERFLOW);
            return PlcErrorCodes::ERROR_OVERFLOW;
          }

          // Assemble according to Endianness
          if (def.endianess == 2)  // Little-Endian
          {
            for (uint8_t i = 0; i < def.register_count; ++i) {
              bit_pattern |= static_cast<uint64_t>(buffer[buffer_index + i]) << (16 * i);
            }
          } else  // Big-Endian (or default)
          {
            for (uint8_t i = 0; i < def.register_count; ++i) {
              bit_pattern |= static_cast<uint64_t>(buffer[buffer_index + i])
                             << (16 * (def.register_count - 1 - i));
            }
          }
          new_value = bit_pattern;
        } else  // 2 = Bitmask Register
        {
          // Extract the specific bit from the corresponding register in the
          // buffer
          uint16_t reg_val = buffer[buffer_index];  // Reads only one register

          // Using 'def.offset' as the bit_offset, based on DB analysis
          uint8_t bit_offset = def.bitmask_offset;
          new_value = (reg_val >> bit_offset) & 1;  // Final value is 0 or 1
        }

        uint64_t current_val = 0;
        const uint16_t logical_addr = def.logical_address;

        // Get current value by calling the specific accessor
        if (def.purpose == 1) {  // standard
          rs = (block.hardware_access == 1) ? getInputRegisterValue(logical_addr, current_val)
                                            : getOutputRegisterValue(logical_addr, current_val);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_HW_to_Mem",
                      "Failed to get current register value for logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
        } else if (def.purpose == 2) {  // secure_state
          rs = getSecureStateRegisterValue(logical_addr, current_val);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_HW_to_Mem",
                      "Failed to get current secure state register value for "
                      "logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
        } else if (def.purpose == 3) {  // config
          rs = getConfigRegisterValue(logical_addr, current_val);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_HW_to_Mem",
                      "Failed to get current config register value for logical "
                      "address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
        }

        // If value changed or full sync is forced, update memory
        if (current_val != new_value || _force_full_sync) {
          if (!timing_printed && duration_phase3 > 0) {
            DEBUG_STREAM("[TIMING] PHASE 3 (HW->MEM): Read Registers HW took "
                         << duration_phase3 << " us. Change detected.");
            timing_printed = true;
          }
          if (def.purpose == 1) {              // standard
            if (block.hardware_access == 1) {  // input
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " INPUT_REG addr " << logical_addr
                                               << ": " << current_val << " -> " << new_value);
              rs = setInputRegisterValue(logical_addr, new_value);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set input register value for logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " INPUT_REG addr " << logical_addr
                                               << " update_flag = true");
              rs = setInputRegisterUpdateValue(logical_addr, true);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set input register update flag for "
                          "logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
            } else {  // output
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " OUTPUT_REG addr " << logical_addr
                                               << ": " << current_val << " -> " << new_value);
              rs = setOutputRegisterValue(logical_addr, new_value);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set output register value for logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
              TRACE_STREAM("[HW->MEM] Module " << _module_id << " OUTPUT_REG addr " << logical_addr
                                               << " update_flag = true");
              rs = setOutputRegisterUpdateValue(logical_addr, true);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_HW_to_Mem",
                          "Failed to set output register update flag for "
                          "logical address " +
                              std::to_string(logical_addr),
                          rs);
                return rs;
              }
            }
          } else if (def.purpose == 2) {  // secure_state
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " SECURE_STATE_REG addr "
                                             << logical_addr << ": " << current_val << " -> "
                                             << new_value);
            rs = setSecureStateRegisterValue(logical_addr, new_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set secure state register value for logical "
                        "address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " SECURE_STATE_REG addr "
                                             << logical_addr << " update_flag = true");
            rs = setSecureStateRegisterUpdateValue(logical_addr, true);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set secure state register update flag for "
                        "logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
          } else if (def.purpose == 3) {  // config
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " CONFIG_REG addr " << logical_addr
                                             << ": " << current_val << " -> " << new_value);
            rs = setConfigRegisterValue(logical_addr, new_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set config register value for logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[HW->MEM] Module " << _module_id << " CONFIG_REG addr " << logical_addr
                                             << " update_flag = true");
            rs = setConfigRegisterUpdateValue(logical_addr, true);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_HW_to_Mem",
                        "Failed to set config register update flag for logical "
                        "address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
          }
        }
        buffer_ptr += def.register_count;  // Move buffer pointer forward by the number of
                                           // registers this definition used
      }
    }
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::_syncBlock_Mem_to_HW(const IO_Block& block) {
  if (block.hardware_access != 2) {
    return PlcErrorCodes::PLC_SUCCESS;  // Only output blocks are written to
                                        // hardware
  }

  PlcErrorCodes rs;

  if (block.io_type == 1)  // bit
  {
    bool block_needs_update = false;
    auto buffer = std::make_unique<bool[]>(block.quantity);
    std::vector<std::tuple<uint16_t, uint8_t, bool>>
        addresses_to_clear_flags;  // Save logical addresses, purposes, and
                                   // written values to clear update required
                                   // flags after write

    {
      std::unique_lock<std::recursive_mutex> lock(_mutex);
      // 1. Prepare the buffer of values to write
      for (uint16_t i = 0; i < block.quantity; ++i) {
        const auto& def = block.contained_definitions[i];
        uint16_t logical_addr = def.logical_address;
        bool needs_update = false;
        bool required_value = false;
        bool current_value = false;

        // Find the correct map and get the value and the flag
        if (def.purpose == 1) {  // standard
          rs = getOutputBitUpdateRequired(logical_addr, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check output bit update required for logical "
                      "address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
          if (needs_update) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " OUTPUT_BIT addr " << logical_addr
                                             << " marked for hardware update.");
            rs = getOutputBitRequired(logical_addr, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required output bit value for logical "
                        "address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " OUTPUT_BIT addr " << logical_addr
                                             << " value to write: " << required_value);
            block_needs_update = true;
            buffer[i] = required_value;
            addresses_to_clear_flags.push_back(
                {logical_addr, def.purpose,
                 required_value});  // Mark this address to clear the flag later
          } else {
            rs = getOutputBitValue(logical_addr, current_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get current output bit value for logical "
                        "address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }

            buffer[i] = current_value;
          }
        } else if (def.purpose == 2) {  // secure_state
          rs = getSecureStateBitUpdateRequired(logical_addr, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check secure state bit update required for "
                      "logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
          if (needs_update) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " SECURE_STATE_BIT addr "
                                             << logical_addr << " marked for hardware update.");
            rs = getSecureStateBitRequired(logical_addr, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required secure state bit value for "
                        "logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " SECURE_STATE_BIT addr "
                                             << logical_addr
                                             << " value to write: " << required_value);
            buffer[i] = required_value;

            block_needs_update = true;
            addresses_to_clear_flags.push_back({logical_addr, def.purpose, required_value});
          } else {
            rs = getSecureStateBitValue(logical_addr, current_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get current secure state bit value for "
                        "logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }

            buffer[i] = current_value;
          }
        } else if (def.purpose == 3) {  // config
          rs = getConfigBitUpdateRequired(logical_addr, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check config bit update required for logical "
                      "address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }
          if (needs_update) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " CONFIG_BIT addr " << logical_addr
                                             << " marked for hardware update.");
            rs = getConfigBitRequired(logical_addr, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required config bit value for logical "
                        "address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " CONFIG_BIT addr " << logical_addr
                                             << " value to write: " << required_value);
            buffer[i] = required_value;

            block_needs_update = true;
            addresses_to_clear_flags.push_back({logical_addr, def.purpose, required_value});
          } else {
            rs = getConfigBitValue(logical_addr, current_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get current config bit value for logical "
                        "address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }

            buffer[i] = current_value;
          }
        }
      }
    }

    // 2. Write the entire block if any bit needs update
    if (block_needs_update) {
      // --- NETWORK I/O (WITHOUT LOCK) ---
      auto start_phase2 = std::chrono::high_resolution_clock::now();  // TIMING START PHASE 2
      rs = _backend->writeBits(_address_on_channel, block.physical_start_address, buffer.get(),
                               block.quantity);
      auto end_phase2 = std::chrono::high_resolution_clock::now();  // TIMING END PHASE 2
      auto duration_phase2 =
          std::chrono::duration_cast<std::chrono::microseconds>(end_phase2 - start_phase2).count();
      DEBUG_STREAM("[TIMING] PHASE 2 (MEM->HW): Writing Bit Block to HW. Took " << duration_phase2
                                                                                << " us.");
      TRACE_STREAM("[MEM->HW] Module "
                   << _module_id << " submitting BITS block to hardware (phys_addr="
                   << block.physical_start_address << ", qty=" << block.quantity << ")");
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error(
            "Module::_syncBlock_Mem_to_HW",
            "Failed to write bit block starting at " + std::to_string(block.physical_start_address),
            rs);
        return rs;
      }

      {
        std::unique_lock<std::recursive_mutex> lock(_mutex);

        // 3. Clear the update required flags for the bits that were written
        for (const auto& [logical_addr_to_clear, purpose, written_val] : addresses_to_clear_flags) {
          // --- SMART FLAG CLEANING (WITH LOCK) ---

          bool required_val = false;  // This is the value currently in the 'required' buffer

          // Get the 'required' value (which may have been changed by the DB
          // thread).
          if (purpose == 1) {
            rs = getOutputBitRequired(logical_addr_to_clear, required_val);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required output bit value for flag "
                        "clear check. Addr: " +
                            std::to_string(logical_addr_to_clear),
                        rs);
              return rs;
            }
          } else if (purpose == 2) {
            rs = getSecureStateBitRequired(logical_addr_to_clear, required_val);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required secure state bit value for "
                        "flag clear check. Addr: " +
                            std::to_string(logical_addr_to_clear),
                        rs);
              return rs;
            }
          } else if (purpose == 3) {
            rs = getConfigBitRequired(logical_addr_to_clear, required_val);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required config bit value for flag "
                        "clear check. Addr: " +
                            std::to_string(logical_addr_to_clear),
                        rs);
              return rs;
            }
          }

          // ONLY clear the flag if the 'required' value hasn't changed
          // while module were busy with the network I/O.
          if (written_val == required_val) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " BIT flag cleared for addr "
                                             << logical_addr_to_clear
                                             << ". Hardware matches Memory.");
            // The value in memory matches what it just wrote, so it's safe to
            // clear the flag.
            if (purpose == 1) {
              rs = setOutputBitUpdateRequired(logical_addr_to_clear, false);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Failed to clear output bit update required for "
                          "logical address " +
                              std::to_string(logical_addr_to_clear),
                          rs);
                return rs;
              }
            } else if (purpose == 2) {
              rs = setSecureStateBitUpdateRequired(logical_addr_to_clear, false);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Failed to clear secure state bit update required "
                          "for logical address " +
                              std::to_string(logical_addr_to_clear),
                          rs);
                return rs;
              }
            } else if (purpose == 3) {
              rs = setConfigBitUpdateRequired(logical_addr_to_clear, false);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Failed to clear config bit update required for "
                          "logical address " +
                              std::to_string(logical_addr_to_clear),
                          rs);
                return rs;
              }
            }
          } else {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " BIT flag REMAINS for addr "
                                             << logical_addr_to_clear
                                             << ". Value changed during write.");
          }
        }
      }
    }
  }

  else  // Register operations (io_type == 2)
  {
    // --- 1. DETECTION AND HANDLING OF SPECIAL CASES (SPI WDT/UUID) ---
    // This logic assumes that the WDT and UUID registers are in their own
    // dedicated IO_Blocks, which is true if their physical_address
    // in the DB is 0 or NULL and they are not contiguous with other registers.

    if (!block.contained_definitions.empty()) {
      const auto& first_def = block.contained_definitions.front();
      auto spi_backend = std::dynamic_pointer_cast<ProtocolSPIV0>(_backend);

      // Check if this block is a config register and we have an SPI backend
      if (spi_backend && first_def.purpose == 3) {
        uint16_t logical_addr = first_def.logical_address;
        bool needs_update = false;
        uint64_t required_value = 0;
        {
          std::unique_lock<std::recursive_mutex> lock(_mutex);
          // Check if this specific register needs an update
          rs = getConfigRegisterUpdateRequired(logical_addr, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check config update required for logical address " +
                          std::to_string(logical_addr),
                      rs);
            return rs;
          }

          if (needs_update) {
            rs = getConfigRegisterRequired(logical_addr, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get config required value for logical address " +
                            std::to_string(logical_addr),
                        rs);
              return rs;
            }
          }
        }

        if (needs_update) {
          // CASE 1: This is the UUID block (logical_address = 101 in
          // model_io_definition)
          if (logical_addr == 101) {
            // --- NETWORK I/O (WITHOUT LOCK) ---
            rs = spi_backend->setUuid(_address_on_channel, static_cast<uint32_t>(required_value));
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW", "Failed set SPI UUID", rs);
              return rs;
            }

            // --- SMART FLAG CLEANING (WITH LOCK) ---
            {
              std::unique_lock<std::recursive_mutex> lock(_mutex);

              uint64_t new_required_val_in_mem = 0;
              rs = getConfigRegisterRequired(logical_addr, new_required_val_in_mem);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Failed get required UUID value for flag check", rs);
                return rs;
              }

              // Only clear the flag if the required value hasn't changed while
              // we were busy.
              if (required_value == new_required_val_in_mem) {
                rs = setConfigRegisterUpdateRequired(logical_addr,
                                                     false);  // Clear the flag
                if (rs != PlcErrorCodes::PLC_SUCCESS) {
                  log_error("Module::_syncBlock_Mem_to_HW", "Failed clear SPI UUID flag", rs);
                  return rs;
                }
              }
              // else: A new value was set. Do nothing. Leave the flag as
              // 'true'.
            }
            return PlcErrorCodes::PLC_SUCCESS;
          }

          // CASE 2: This is the WDT block (logical_address = 100 in
          // model_io_definition)
          if (logical_addr == 100) {
            TRACE_STREAM("[MEM->HW] WDT sync for module " << _module_id << " addr " << logical_addr
                                                          << " value=" << required_value << " ms");

            // --- NETWORK I/O (WITHOUT LOCK) ---
            rs = spi_backend->setWDT(_address_on_channel, static_cast<uint16_t>(required_value));
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW", "Failed set SPI WDT", rs);
              return rs;
            }

            // --- SMART FLAG CLEANING (WITH LOCK) ---
            {
              std::unique_lock<std::recursive_mutex> lock(_mutex);

              uint64_t new_required_val_in_mem = 0;
              rs = getConfigRegisterRequired(logical_addr, new_required_val_in_mem);
              if (rs != PlcErrorCodes::PLC_SUCCESS) {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Failed get required WDT value for flag check", rs);
                return rs;
              }

              // Only clear the flag if the required value hasn't changed.
              if (required_value == new_required_val_in_mem) {
                rs = setConfigRegisterUpdateRequired(logical_addr,
                                                     false);  // Clear the flag
                if (rs != PlcErrorCodes::PLC_SUCCESS) {
                  log_error("Module::_syncBlock_Mem_to_HW", "Failed clear SPI WDT flag", rs);
                  return rs;
                }
              }
              // else: A new value was set. Do nothing. Leave the flag as
              // 'true'.
            }
            return PlcErrorCodes::PLC_SUCCESS;
          }
        }
      }
    }

    // --- 2. BLOCK WRITE LOGIC (For all other register blocks) ---
    bool block_needs_update = false;
    auto buffer = std::make_unique<uint16_t[]>(block.quantity);
    std::vector<std::tuple<uint16_t, uint8_t, uint64_t>>
        addresses_to_clear_flags;  // logical_addr, purpose, written_val

    auto buffer_ptr = buffer.get();
    const auto buffer_end = buffer.get() + block.quantity;
    uint16_t accumulated_bitmask_val = 0;
    {
      std::unique_lock<std::recursive_mutex> lock(_mutex);
      // Loop through definitions, not through physical registers
      for (uint16_t i = 0; i < block.contained_definitions.size(); ++i) {
        const auto& def = block.contained_definitions[i];

        // --- 2.1 Get the value for this definition (handles purpose, update
        // flags, etc.) ---
        uint64_t value_to_write = 0;  // For DIRECT, this is the final value. For
                                      // BITMASK, this is 0 or 1.
        uint64_t required_value = 0;
        uint64_t current_value = 0;
        bool needs_update = false;

        if (def.purpose == 1) {  // standard
          rs = getOutputRegisterUpdateRequired(def.logical_address, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check output register update required for "
                      "logical address " +
                          std::to_string(def.logical_address),
                      rs);
            return rs;
          }
          if (needs_update) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " OUTPUT_REG addr "
                                             << def.logical_address
                                             << " marked for hardware update.");
            rs = getOutputRegisterRequired(def.logical_address, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get output register required for logical "
                        "address " +
                            std::to_string(def.logical_address),
                        rs);
              return rs;
            }
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " OUTPUT_REG addr "
                                             << def.logical_address
                                             << " value to write: " << required_value);
            value_to_write = required_value;
            block_needs_update = true;  // Mark the whole block for writing
            addresses_to_clear_flags.push_back({def.logical_address, def.purpose, required_value});
          } else {
            rs = getOutputRegisterValue(def.logical_address, current_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get output register value for logical address " +
                            std::to_string(def.logical_address),
                        rs);
              return rs;
            }
            value_to_write = current_value;
          }
        } else if (def.purpose == 2) {  // secure_state
          rs = getSecureStateRegisterUpdateRequired(def.logical_address, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check secure state register update required "
                      "for logical address " +
                          std::to_string(def.logical_address),
                      rs);
            return rs;
          }
          if (needs_update) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " SECURE_STATE_REG addr "
                                             << def.logical_address
                                             << " marked for hardware update.");
            rs = getSecureStateRegisterRequired(def.logical_address, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get secure state register required for "
                        "logical address " +
                            std::to_string(def.logical_address),
                        rs);
              return rs;
            }
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " SECURE_STATE_REG addr "
                                             << def.logical_address
                                             << " value to write: " << required_value);
            value_to_write = required_value;
            block_needs_update = true;
            addresses_to_clear_flags.push_back({def.logical_address, def.purpose, required_value});
          } else {
            rs = getSecureStateRegisterValue(def.logical_address, current_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get secure state register value for logical "
                        "address " +
                            std::to_string(def.logical_address),
                        rs);
              return rs;
            }
            value_to_write = current_value;
          }
        } else if (def.purpose == 3) {  // config
          rs = getConfigRegisterUpdateRequired(def.logical_address, needs_update);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("Module::_syncBlock_Mem_to_HW",
                      "Failed to check config register update required for "
                      "logical address " +
                          std::to_string(def.logical_address),
                      rs);
            return rs;
          }
          if (needs_update) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " CONFIG_REG addr "
                                             << def.logical_address
                                             << " marked for hardware update.");
            rs = getConfigRegisterRequired(def.logical_address, required_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get config register required for logical "
                        "address " +
                            std::to_string(def.logical_address),
                        rs);
              return rs;
            }
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " CONFIG_REG addr "
                                             << def.logical_address
                                             << " value to write: " << required_value);
            value_to_write = required_value;
            block_needs_update = true;
            addresses_to_clear_flags.push_back({def.logical_address, def.purpose, required_value});
          } else {
            rs = getConfigRegisterValue(def.logical_address, current_value);
            if (rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get config register value for logical address " +
                            std::to_string(def.logical_address),
                        rs);
              return rs;
            }
            value_to_write = current_value;
          }
        }

        // --- 2.2 Place the value into the buffer ---

        if (buffer_ptr >= buffer_end) {
          log_error("Module::_syncBlock_Mem_to_HW",
                    "Buffer overflow detected before processing definition " +
                        std::to_string(def.io_definition_id),
                    PlcErrorCodes::ERROR_OVERFLOW);
          return PlcErrorCodes::ERROR_OVERFLOW;
        }

        if (def.access_method == 1)  // --- DIRECT ---
        {
          // If the previous definition was a bitmask, its group just ended.
          // Write it.
          if (i > 0 && block.contained_definitions[i - 1].access_method == 2) {
            *buffer_ptr = accumulated_bitmask_val;
            buffer_ptr++;
            accumulated_bitmask_val = 0;  // Reset accumulator

            if (buffer_ptr >= buffer_end)  // Check bounds after advancing
            {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Buffer overflow after flushing bitmask group.",
                        PlcErrorCodes::ERROR_OVERFLOW);
              return PlcErrorCodes::ERROR_OVERFLOW;
            }
          }

          // Now, process the current DIRECT definition
          if (def.endianess == 2)  // Little-Endian
          {
            for (int j = 0; j < def.register_count; ++j) {
              if (buffer_ptr + j < buffer_end) {
                buffer_ptr[j] = (value_to_write >> (16 * j)) & 0xFFFF;
              } else {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Buffer overflow on direct (little-endian) write.",
                          PlcErrorCodes::ERROR_OVERFLOW);
                return PlcErrorCodes::ERROR_OVERFLOW;
              }
            }
          } else  // Big-Endian
          {
            for (int j = 0; j < def.register_count; ++j) {
              if (buffer_ptr + j < buffer_end) {
                buffer_ptr[j] = (value_to_write >> (16 * (def.register_count - 1 - j))) & 0xFFFF;
              } else {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Buffer overflow on direct (big-endian) write.",
                          PlcErrorCodes::ERROR_OVERFLOW);
                return PlcErrorCodes::ERROR_OVERFLOW;
              }
            }
          }
          buffer_ptr += def.register_count;  // Move buffer pointer
        } else                               // --- BITMASK ---
        {
          // Check if this is a new bitmask group
          if (i == 0 ||                                                   // First item in block
              (block.contained_definitions[i - 1].access_method == 1) ||  // Previous was DIRECT
              (block.contained_definitions[i - 1].physical_address !=
               def.physical_address))  // Previous was BITMASK but different
                                       // address
          {
            // If the previous item was a bitmask (and we're starting a new
            // one), it must flush the old one.
            if (i > 0 && block.contained_definitions[i - 1].access_method == 2) {
              *buffer_ptr = accumulated_bitmask_val;
              buffer_ptr++;

              if (buffer_ptr >= buffer_end)  // Check bounds after advancing
              {
                log_error("Module::_syncBlock_Mem_to_HW",
                          "Buffer overflow flushing bitmask group before new one.",
                          PlcErrorCodes::ERROR_OVERFLOW);
                return PlcErrorCodes::ERROR_OVERFLOW;
              }
            }
            accumulated_bitmask_val = 0;  // Reset for the new group
          }

          // Add the current bit (value_to_write is 0 or 1) to the accumulator
          if (value_to_write) {
            accumulated_bitmask_val |= (1 << def.bitmask_offset);
          }
        }
      }

      // --- 2.3 After the loop, check if the last definition was a bitmask ---
      if (!block.contained_definitions.empty() &&
          block.contained_definitions.back().access_method == 2) {
        if (buffer_ptr >= buffer_end) {
          log_error("Module::_syncBlock_Mem_to_HW", "Buffer overflow on final bitmask flush.",
                    PlcErrorCodes::ERROR_OVERFLOW);
          return PlcErrorCodes::ERROR_OVERFLOW;
        }
        // The loop finished, so we must write the final accumulated value.
        *buffer_ptr = accumulated_bitmask_val;
        buffer_ptr++;
      }
    }

    // --- 3. Write the entire block if anything changed ---
    if (block_needs_update) {
      // --- NETWORK I/O (WITHOUT LOCK) ---
      auto start_phase2 = std::chrono::high_resolution_clock::now();  // TIMING START PHASE 2
      rs = _backend->writeRegisters(_address_on_channel, block.physical_start_address, buffer.get(),
                                    block.quantity);
      auto end_phase2 = std::chrono::high_resolution_clock::now();  // TIMING END PHASE 2
      auto duration_phase2 =
          std::chrono::duration_cast<std::chrono::microseconds>(end_phase2 - start_phase2).count();
      TRACE_STREAM("[TIMING] PHASE 2 (MEM->HW): Writing Register Block to HW. Took "
                   << duration_phase2 << " us.");
      TRACE_STREAM("[MEM->HW] Module "
                   << _module_id << " submitting REGISTER block to hardware (phys_addr="
                   << block.physical_start_address << ", qty=" << block.quantity << ")");
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::_syncBlock_Mem_to_HW",
                  "Failed to write register block starting at " +
                      std::to_string(block.physical_start_address),
                  rs);
        return rs;
      }

      // --- SMART FLAG CLEANING (WITH LOCK) ---
      {
        std::unique_lock<std::recursive_mutex> lock(_mutex);
        // Clear all flags for the values that were just written
        for (const auto& [logical_addr_to_clear, purpose, written_val] : addresses_to_clear_flags) {
          // --- START: Smart Flag Clearing Logic ---
          uint64_t required_val = 0;  // This is the value currently in the 'required' buffer
          PlcErrorCodes get_rs;

          // Get the 'required' value
          if (purpose == 1) {
            get_rs = getOutputRegisterRequired(logical_addr_to_clear, required_val);
            if (get_rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required output register value for flag "
                        "clear check. Addr: " +
                            std::to_string(logical_addr_to_clear),
                        get_rs);
              return get_rs;
            }
          } else if (purpose == 2) {
            get_rs = getSecureStateRegisterRequired(logical_addr_to_clear, required_val);
            if (get_rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required secure state register value "
                        "for flag clear check. Addr: " +
                            std::to_string(logical_addr_to_clear),
                        get_rs);
              return get_rs;
            }
          } else if (purpose == 3) {
            get_rs = getConfigRegisterRequired(logical_addr_to_clear, required_val);
            if (get_rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to get required config register value for flag "
                        "clear check. Addr: " +
                            std::to_string(logical_addr_to_clear),
                        get_rs);
              return get_rs;
            }
          }

          // ONLY clear the flag if the 'required' value hasn't changed
          if (written_val == required_val) {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " REG flag cleared for addr "
                                             << logical_addr_to_clear
                                             << ". Hardware matches Memory.");
            PlcErrorCodes clear_rs = PlcErrorCodes::PLC_SUCCESS;
            if (purpose == 1)
              clear_rs = setOutputRegisterUpdateRequired(logical_addr_to_clear, false);
            else if (purpose == 2)
              clear_rs = setSecureStateRegisterUpdateRequired(logical_addr_to_clear, false);
            else if (purpose == 3)
              clear_rs = setConfigRegisterUpdateRequired(logical_addr_to_clear, false);

            if (clear_rs != PlcErrorCodes::PLC_SUCCESS) {
              log_error("Module::_syncBlock_Mem_to_HW",
                        "Failed to clear register update required for logical "
                        "address " +
                            std::to_string(logical_addr_to_clear),
                        clear_rs);
              return clear_rs;  // Return on the first failure
            }
          } else {
            TRACE_STREAM("[MEM->HW] Module " << _module_id << " BIT flag REMAINS for addr "
                                             << logical_addr_to_clear
                                             << ". Value changed during write.");
          }
        }
      }
    }
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

// --- Public Method Implementations ---

PlcErrorCodes Module::sync() {
  PlcErrorCodes rs;

  // --- INITIALIZATION SEQUENCE ---
  // This logic handles the first connection or reconnection after a
  // communication error.
  if (_connected < 2) {
    rs = identifyModule();
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::sync", "Error identifying module id: " + std::to_string(_module_id), rs);
      return rs;  // Identification failed, will retry on the next cycle.
    }
    // After successful identification, perform the first hardware read for ALL
    // blocks. This ensures the memory is populated with valid data before the
    // module is marked as "connected".
    std::vector<IO_Block> blocks;
    {
      std::unique_lock<std::recursive_mutex> lock(_mutex);
      blocks = _io_blocks_initial;  // Use FULL block structure for initial sync
    }
    for (const auto& block : blocks) {
      PlcErrorCodes sync_rs = _syncBlock_HW_to_Mem(block);
      if (sync_rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::sync",
                  "Error during initial HW->Memory sync for module " + std::to_string(_module_id),
                  sync_rs);

        PlcErrorCodes free_rs = freeModule();
        if (free_rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::sync",
                    "Failed to free module after initial sync error for module_id: " +
                        std::to_string(_module_id) + ".",
                    free_rs);
        }

        return sync_rs;
      }
    }

    // Only after a successful identification AND a successful first data read,
    // do we declare the module as ready for the database sync task.
    _connected = 2;

    // Return immediately to allow the database task to process this new state.
    return PlcErrorCodes::PLC_SUCCESS;
  }

  // --- ONGOING IDENTITY VERIFICATION for connected SPI modules ---
  // This check is to detect a "ghost" hot-swap that occurred without a
  // communication error.
  /*auto spi_backend = std::dynamic_pointer_cast<ProtocolSPIV0>(_backend);
  if (spi_backend)
  {
      uint32_t current_hardware_uuid = 0;
      rs = spi_backend->readUuid(_address_on_channel, current_hardware_uuid);
      if (rs == PlcErrorCodes::PLC_SUCCESS)
      {
          std::unique_lock<std::recursive_mutex> lock(_mutex);
          if (_uuid != current_hardware_uuid)
          {
              log_error("Module::sync", "Hot-swap detected! In-memory UUID: " +
  std::to_string(_uuid) + ", hardware UUID: " +
  std::to_string(current_hardware_uuid) + ". Forcing re-identification.",
  PlcErrorCodes::PLC_SUCCESS); rs = freeModule(); // Force a full reset. if (rs
  != PlcErrorCodes::PLC_SUCCESS)
              {
                  log_error("Module::sync", "Failed to free module after
  hot-swap detection for module_id: " + std::to_string(_module_id) + ".", rs);
                  return rs;
              }
              return PlcErrorCodes::ERROR_MODULE_UUID_CHANGED; // Return an
  error to signal a state change.
          }
      }
      else
      {
          log_error("Module::sync", "Failed to verify UUID on connected module "
  + std::to_string(_module_id) + ". Resetting.", rs); rs = freeModule(); if (rs
  != PlcErrorCodes::PLC_SUCCESS)
          {
              log_error("Module::sync", "Failed to free module after UUID
  verification error for module_id: " + std::to_string(_module_id) + ".", rs);
          }
          return rs;
      }
  }*/

  // --- NORMAL SYNCHRONIZATION LOGIC (for stable, connected cycles) ---
  // Select block vector: use FULL blocks when force_full_sync is true,
  // otherwise use the pre-filtered SYNC blocks for efficiency.
  const std::vector<IO_Block>& blocks = _force_full_sync ? _io_blocks_initial : _io_blocks_sync;

  for (const auto& block : blocks) {
    // (No per-cycle filtering needed - blocks are already pre-filtered in
    // initialize())
    rs = _syncBlock_HW_to_Mem(block);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("Module::sync",
                "Error during HW->Memory sync for module " + std::to_string(_module_id) +
                    ". Resetting module.",
                rs);
      rs = freeModule();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::sync",
                  "Failed to free module after HW->Memory sync error for "
                  "module_id: " +
                      std::to_string(_module_id) + ".",
                  rs);
      }
      return rs;
    }

    // Phase 2: Sync from Memory to Hardware (for output blocks)
    if (block.hardware_access == 2)  // 2 = output
    {
      rs = _syncBlock_Mem_to_HW(block);
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::sync",
                  "Error during Memory->HW sync for module " + std::to_string(_module_id) +
                      ". Resetting module.",
                  rs);
        rs = freeModule();
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("Module::sync",
                    "Failed to free module after Memory->HW sync error for "
                    "module_id: " +
                        std::to_string(_module_id) + ".",
                    rs);
        }
        return rs;
      }
    }
  }

  if (_force_full_sync) {
    rs = setForceFullSync(false);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error(
          "Module::sync",
          "Failed to clear force_full_sync flag for module_id: " + std::to_string(_module_id) + ".",
          rs);
      return rs;
    }
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getBackend(ProtocolPtr& backend) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  if (!_backend) {
    log_error("Module::getBackend", "Attempted to get backend but it is null.",
              PlcErrorCodes::ERROR_NULL_POINTER);
    return PlcErrorCodes::ERROR_NULL_POINTER;
  }

  backend = _backend;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::freeModule() {
  auto channel = _channel;
  auto protocol = _protocol;
  {
    std::unique_lock<std::recursive_mutex> lock(_mutex);
    channel = _channel;
    protocol = _protocol;
  }

  PlcErrorCodes rs;
  // Only disconnect if channel is tcp and protocol is modbus tcp
  if (channel == "tcp" && protocol == "modbus-tcp") {
    if (_backend) {
      rs = _backend->disconnect();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("Module::freeModule",
                  "Failed to disconnect backend for module_id: " + std::to_string(_module_id), rs);
        return rs;
      }
    } else {
      log_error("Module::freeModule",
                "Attempted to disconnect backend but it is null for module_id: " +
                    std::to_string(_module_id),
                PlcErrorCodes::ERROR_NULL_POINTER);
      return PlcErrorCodes::ERROR_NULL_POINTER;
    }
  }
  //_backend->disconnect(); // This would make others modules connected to the
  // same shared channel (e.g. rs-485 or modbus rtu over tcp) to stop
  // communicating
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  // Reset module attributes
  _uuid = 0;

  //_backend = nullptr; If you do that, it will be a seg fault in the next
  // iteration of identify_card()
  _connected = 0;
  _force_full_sync = false;
  for (auto& pair : _input_bits) {
    pair.second = {};
  }
  for (auto& pair : _output_bits) {
    pair.second = {};
  }
  for (auto& pair : _input_registers) {
    pair.second = {};
  }
  for (auto& pair : _output_registers) {
    pair.second = {};
  }
  for (auto& pair : _secure_state_bits) {
    pair.second = {};
  }
  for (auto& pair : _secure_state_registers) {
    pair.second = {};
  }
  for (auto& pair : _config_bits) {
    pair.second = {};
  }
  for (auto& pair : _config_registers) {
    pair.second = {};
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setSafeState() {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  // --- Apply Safe State to all OUTPUT BITS ---
  // Iterate through all defined output bits for this module.
  for (auto& output_bit_pair : _output_bits) {
    uint16_t address = output_bit_pair.first;
    auto& output_bit_unit = output_bit_pair.second;

    // Determine secure state logical address
    uint16_t secure_address = address;  // Default to direct mapping if not found in map
    auto map_it = _secure_state_map_bits.find(address);
    if (map_it != _secure_state_map_bits.end()) {
      secure_address = map_it->second;
    }

    // Find the corresponding secure state value for this address.
    auto it = _secure_state_bits.find(secure_address);
    if (it != _secure_state_bits.end()) {
      // A specific secure state is defined for this bit. Use its last known
      // value. The '.value' of a secure_state_bit holds the configured safe
      // value.
      output_bit_unit.required = it->second.value;
    } else {
      // If no specific secure state is defined for this output, default to OFF.
      output_bit_unit.required = false;
    }

    // Flag this output to be written to the hardware on the next sync cycle.
    output_bit_unit.updateRequired = true;
  }

  // --- Apply Safe State to all OUTPUT REGISTERS ---
  // Iterate through all defined output registers for this module.
  for (auto& output_reg_pair : _output_registers) {
    uint16_t address = output_reg_pair.first;
    auto& output_reg_unit = output_reg_pair.second;

    // Determine secure state logical address
    uint16_t secure_address = address;  // Default to direct mapping if not found in map
    auto map_it = _secure_state_map_registers.find(address);
    if (map_it != _secure_state_map_registers.end()) {
      secure_address = map_it->second;
    }

    // Find the corresponding secure state value for this address.
    auto it = _secure_state_registers.find(secure_address);
    if (it != _secure_state_registers.end()) {
      // A specific secure state is defined for this register. Use its value.
      output_reg_unit.required = it->second.value;
    } else {
      // If no specific secure state defined, default to 0.
      output_reg_unit.required = 0;
    }

    // Flag this output to be written to the hardware on the next sync cycle.
    output_reg_unit.updateRequired = true;
  }

  TRACE_STREAM("Module::setSafeState Safe state triggered for module_id "
               << std::to_string(_module_id));

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setConnected(uint8_t connected) {
  _connected.store(connected, std::memory_order_release);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getUuid(uint32_t& uuid) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  uuid = _uuid;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setUuid(uint32_t uuid) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  _uuid = uuid;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getConnected(uint8_t& connected) const {
  connected = _connected.load(std::memory_order_acquire);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getForceFullSync(bool& force_full_sync) const {
  force_full_sync = _force_full_sync.load(std::memory_order_acquire);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setForceFullSync(bool force_full_sync) {
  _force_full_sync.store(force_full_sync, std::memory_order_release);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::forceFullSync() {
  _force_full_sync.store(true, std::memory_order_release);
  DEBUG_STREAM(" Full data sync has been forced for module_id " << std::to_string(_module_id));
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getModuleId(uint32_t& module_id) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  module_id = _module_id;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getModuleName(std::string& name) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  name = _module_name;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getModelId(uint32_t& model_id) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  model_id = _model_id;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getAddressOnChannel(std::string& address_on_channel) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  address_on_channel = _address_on_channel;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getChannel(std::string& channel) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  channel = _channel;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getProtocol(std::string& protocol) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  protocol = _protocol;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setIOBlocks(std::vector<IO_Block>& blocks) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  _io_blocks_initial = blocks;
  // Note: _io_blocks_continuous is not updated here. It's built in initialize()
  // based on OperationMode.
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::getAllIoDefinitions(std::vector<IoDefinition>& out_defs) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  out_defs.clear();
  // Reserve space for efficiency if the map is large
  out_defs.reserve(_io_definition_map.size());

  // Iterate through the internal map containing all definitions for this module
  for (const auto& pair : _io_definition_map) {
    out_defs.push_back(pair.second);  // Add the IoDefinition struct to the output vector
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::collectChangedValues(
    OperationMode mode, bool force_all,
    std::vector<std::pair<uint32_t, uint64_t>>& changed_values) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  // Iterate through all mapped I/O definitions for this module
  for (const auto& pair : _io_definition_map) {
    const auto& def = pair.second;

    // Filter by operation mode: strictly ensure we only send updates relevant
    // to the current mode
    if (mode == OperationMode::EXECUTION && def.purpose != 1) continue;
    if (mode == OperationMode::CONFIGURATION && (def.purpose != 2 && def.purpose != 3)) continue;

    bool needs_update = false;
    uint64_t current_val_bits = 0;
    uint16_t addr = def.logical_address;

    if (def.io_type == 1)  // bit
    {
      if (def.purpose == 1 && def.hardware_access == 1) {
        auto it = _input_bits.find(addr);
        if (it != _input_bits.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = static_cast<uint64_t>(it->second.value);
          it->second.updateValue = false;
        }
      } else if (def.purpose == 1 && def.hardware_access == 2) {
        auto it = _output_bits.find(addr);
        if (it != _output_bits.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = static_cast<uint64_t>(it->second.value);
          it->second.updateValue = false;
        }
      } else if (def.purpose == 2) {
        auto it = _secure_state_bits.find(addr);
        if (it != _secure_state_bits.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = static_cast<uint64_t>(it->second.value);
          it->second.updateValue = false;
        }
      } else if (def.purpose == 3) {
        auto it = _config_bits.find(addr);
        if (it != _config_bits.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = static_cast<uint64_t>(it->second.value);
          it->second.updateValue = false;
        }
      }
    } else  // register
    {
      if (def.purpose == 1 && def.hardware_access == 1) {
        auto it = _input_registers.find(addr);
        if (it != _input_registers.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = it->second.value;
          it->second.updateValue = false;
        }
      } else if (def.purpose == 1 && def.hardware_access == 2) {
        auto it = _output_registers.find(addr);
        if (it != _output_registers.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = it->second.value;
          it->second.updateValue = false;
        }
      } else if (def.purpose == 2) {
        auto it = _secure_state_registers.find(addr);
        if (it != _secure_state_registers.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = it->second.value;
          it->second.updateValue = false;
        }
      } else if (def.purpose == 3) {
        auto it = _config_registers.find(addr);
        if (it != _config_registers.end() && (it->second.updateValue || force_all)) {
          needs_update = true;
          current_val_bits = it->second.value;
          it->second.updateValue = false;
        }
      }
    }

    if (needs_update) {
      std::string type_str = (def.io_type == 1) ? "BIT" : "REG";
      std::string purpose_str = (def.purpose == 1) ? "STD" : ((def.purpose == 2) ? "SEC" : "CFG");
      std::string access_str = (def.hardware_access == 1) ? "IN" : "OUT";

      TRACE_STREAM("[MEM->DB] Module " << _module_id << " " << type_str << "_" << purpose_str << "_"
                                       << access_str << " io_def=" << def.io_definition_id
                                       << " addr=" << addr << " value=" << current_val_bits);

      changed_values.push_back({def.io_definition_id, current_val_bits});
    }
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setBackend(ProtocolPtr backend) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  _backend = backend;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setRequiredValueById(uint32_t io_definition_id, uint64_t value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  TRACE_STREAM("[DB->MEM] setRequiredValueById io_def=" << io_definition_id << " value=" << value
                                                        << " module=" << _module_id);
  auto it = _io_definition_map.find(io_definition_id);
  if (it == _io_definition_map.end()) {
    return PlcErrorCodes::ERROR_NOT_FOUND;  // Definition not found for this
                                            // module
  }

  const auto& def = it->second;
  if (def.hardware_access != 2)  // Not a writable value
  {
    log_error("Module::setRequiredValueById",
              "Attempted to set a non-writable (hardware_access != 2) IO "
              "definition with id: " +
                  std::to_string(io_definition_id) + " for module_id " +
                  std::to_string(this->_module_id),
              PlcErrorCodes::ERROR_INVALID_HARDWARE_ACCESS);
    return PlcErrorCodes::ERROR_NOT_FOUND;  // Definition is not an output
  }

  // Based on the definition's properties, call the correct internal setter
  auto timestamp = std::chrono::steady_clock::now();
  if (def.purpose == 1)  // standard
  {
    if (def.io_type == 1)  // bit
    {
      auto map_it = _output_bits.find(def.logical_address);
      if (map_it != _output_bits.end()) {
        map_it->second.request_timestamp = timestamp;
      }
      return requestOutputBit(def.logical_address, static_cast<bool>(value));
    } else  // register
    {
      auto map_it = _output_registers.find(def.logical_address);
      if (map_it != _output_registers.end()) {
        map_it->second.request_timestamp = timestamp;
      }
      return requestOutputRegister(def.logical_address, value);
    }
  } else if (def.purpose == 2)  // secure_state
  {
    if (def.io_type == 1)  // bit
    {
      auto map_it = _secure_state_bits.find(def.logical_address);
      if (map_it != _secure_state_bits.end()) {
        map_it->second.request_timestamp = timestamp;
      }
      return requestSecureStateBit(def.logical_address, static_cast<bool>(value));
    } else  // register
    {
      auto map_it = _secure_state_registers.find(def.logical_address);
      if (map_it != _secure_state_registers.end()) {
        map_it->second.request_timestamp = timestamp;
      }
      return requestSecureStateRegister(def.logical_address, value);
    }
  } else if (def.purpose == 3)  // configuration
  {
    if (def.io_type == 1)  // bit
    {
      auto map_it = _config_bits.find(def.logical_address);
      if (map_it != _config_bits.end()) {
        map_it->second.request_timestamp = timestamp;
      }
      return requestConfigBit(def.logical_address, static_cast<bool>(value));
    } else  // register
    {
      auto map_it = _config_registers.find(def.logical_address);
      if (map_it != _config_registers.end()) {
        map_it->second.request_timestamp = timestamp;
      }
      return requestConfigRegister(def.logical_address, value);
    }
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::setAllRequiredValues(
    const std::vector<std::pair<uint32_t, uint64_t>>& required_values) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes final_rs = PlcErrorCodes::PLC_SUCCESS;

  for (const auto& pair : required_values) {
    PlcErrorCodes rs = this->setRequiredValueById(pair.first, pair.second);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      final_rs = rs;
      break;
    }
  }

  return final_rs;
}

template <typename T, typename FieldType>
PlcErrorCodes Module::getField(const std::map<uint16_t, T>& container, uint16_t address,
                               FieldType T::* member, FieldType& out) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  auto it = container.find(address);
  if (it == container.end()) {
    log_error("Module::getField",
              "Address: " + std::to_string(address) + " not found for module_id " +
                  std::to_string(this->_module_id),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  out = it->second.*member;
  return PlcErrorCodes::PLC_SUCCESS;
}

template <typename T, typename FieldType>
PlcErrorCodes Module::setField(std::map<uint16_t, T>& container, uint16_t address,
                               FieldType T::* member, FieldType value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  auto it = container.find(address);
  if (it == container.end()) {
    log_error("Module::setField",
              "Address: " + std::to_string(address) + " not found for module_id " +
                  std::to_string(this->_module_id),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  it->second.*member = value;
  return PlcErrorCodes::PLC_SUCCESS;
}

// --- Standard Input Accessor Implementations ---
PlcErrorCodes Module::getInputBitValue(uint16_t address, bool& value) const {
  return getField(_input_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::setInputBitValue(uint16_t address, bool value) {
  return setField(_input_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::getInputBitUpdateValue(uint16_t address, bool& value) const {
  return getField(_input_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::setInputBitUpdateValue(uint16_t address, bool value) {
  return setField(_input_bits, address, &bitUnit::updateValue, value);
}

PlcErrorCodes Module::getInputRegisterValue(uint16_t address, uint64_t& value) const {
  return getField(_input_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::setInputRegisterValue(uint16_t address, uint64_t value) {
  return setField(_input_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::getInputRegisterUpdateValue(uint16_t address, bool& value) const {
  return getField(_input_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::setInputRegisterUpdateValue(uint16_t address, bool value) {
  return setField(_input_registers, address, &registerUnit::updateValue, value);
}

// --- Standard Output Accessor Implementations ---
PlcErrorCodes Module::getOutputBitValue(uint16_t address, bool& value) const {
  return getField(_output_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::setOutputBitValue(uint16_t address, bool value) {
  return setField(_output_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::getOutputBitRequired(uint16_t address, bool& value) const {
  return getField(_output_bits, address, &bitUnit::required, value);
}
PlcErrorCodes Module::setOutputBitRequired(uint16_t address, bool value) {
  return setField(_output_bits, address, &bitUnit::required, value);
}
PlcErrorCodes Module::getOutputBitUpdateValue(uint16_t address, bool& value) const {
  return getField(_output_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::setOutputBitUpdateValue(uint16_t address, bool value) {
  return setField(_output_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::getOutputBitUpdateRequired(uint16_t address, bool& value) const {
  return getField(_output_bits, address, &bitUnit::updateRequired, value);
}
PlcErrorCodes Module::setOutputBitUpdateRequired(uint16_t address, bool value) {
  return setField(_output_bits, address, &bitUnit::updateRequired, value);
}

PlcErrorCodes Module::requestOutputBit(uint16_t address, bool value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs = setOutputBitRequired(address, value);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::requestOutputBit",
              "Failed to set output bit required for address " + std::to_string(address), rs);
    return rs;
  }
  return setOutputBitUpdateRequired(address, true);
}

PlcErrorCodes Module::getOutputRegisterValue(uint16_t address, uint64_t& value) const {
  return getField(_output_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::setOutputRegisterValue(uint16_t address, uint64_t value) {
  return setField(_output_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::getOutputRegisterRequired(uint16_t address, uint64_t& value) const {
  return getField(_output_registers, address, &registerUnit::required, value);
}
PlcErrorCodes Module::setOutputRegisterRequired(uint16_t address, uint64_t value) {
  return setField(_output_registers, address, &registerUnit::required, value);
}
PlcErrorCodes Module::getOutputRegisterUpdateValue(uint16_t address, bool& value) const {
  return getField(_output_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::setOutputRegisterUpdateValue(uint16_t address, bool value) {
  return setField(_output_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::getOutputRegisterUpdateRequired(uint16_t address, bool& value) const {
  return getField(_output_registers, address, &registerUnit::updateRequired, value);
}
PlcErrorCodes Module::setOutputRegisterUpdateRequired(uint16_t address, bool value) {
  return setField(_output_registers, address, &registerUnit::updateRequired, value);
}

PlcErrorCodes Module::requestOutputRegister(uint16_t address, uint64_t value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs = setOutputRegisterRequired(address, value);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::requestOutputRegister",
              "Failed to set output register required for address " + std::to_string(address), rs);
    return rs;
  }
  return setOutputRegisterUpdateRequired(address, true);
}

// --- Secure State Accessor Implementations ---
PlcErrorCodes Module::getSecureStateBitValue(uint16_t address, bool& value) const {
  return getField(_secure_state_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::setSecureStateBitValue(uint16_t address, bool value) {
  return setField(_secure_state_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::getSecureStateBitRequired(uint16_t address, bool& value) const {
  return getField(_secure_state_bits, address, &bitUnit::required, value);
}
PlcErrorCodes Module::setSecureStateBitRequired(uint16_t address, bool value) {
  return setField(_secure_state_bits, address, &bitUnit::required, value);
}
PlcErrorCodes Module::getSecureStateBitUpdateValue(uint16_t address, bool& value) const {
  return getField(_secure_state_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::setSecureStateBitUpdateValue(uint16_t address, bool value) {
  return setField(_secure_state_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::getSecureStateBitUpdateRequired(uint16_t address, bool& value) const {
  return getField(_secure_state_bits, address, &bitUnit::updateRequired, value);
}
PlcErrorCodes Module::setSecureStateBitUpdateRequired(uint16_t address, bool value) {
  return setField(_secure_state_bits, address, &bitUnit::updateRequired, value);
}

PlcErrorCodes Module::requestSecureStateBit(uint16_t address, bool value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs = setSecureStateBitRequired(address, value);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::requestSecureStateBit",
              "Failed to set secure state bit required for address " + std::to_string(address), rs);
    return rs;
  }
  return setSecureStateBitUpdateRequired(address, true);
}

PlcErrorCodes Module::getSecureStateRegisterValue(uint16_t address, uint64_t& value) const {
  return getField(_secure_state_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::setSecureStateRegisterValue(uint16_t address, uint64_t value) {
  return setField(_secure_state_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::getSecureStateRegisterRequired(uint16_t address, uint64_t& value) const {
  return getField(_secure_state_registers, address, &registerUnit::required, value);
}
PlcErrorCodes Module::setSecureStateRegisterRequired(uint16_t address, uint64_t value) {
  return setField(_secure_state_registers, address, &registerUnit::required, value);
}
PlcErrorCodes Module::getSecureStateRegisterUpdateValue(uint16_t address, bool& value) const {
  return getField(_secure_state_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::setSecureStateRegisterUpdateValue(uint16_t address, bool value) {
  return setField(_secure_state_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::getSecureStateRegisterUpdateRequired(uint16_t address, bool& value) const {
  return getField(_secure_state_registers, address, &registerUnit::updateRequired, value);
}
PlcErrorCodes Module::setSecureStateRegisterUpdateRequired(uint16_t address, bool value) {
  return setField(_secure_state_registers, address, &registerUnit::updateRequired, value);
}

PlcErrorCodes Module::requestSecureStateRegister(uint16_t address, uint64_t value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs = setSecureStateRegisterRequired(address, value);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::requestSecureStateRegister",
              "Failed to set secure state register required for address " + std::to_string(address),
              rs);
    return rs;
  }
  return setSecureStateRegisterUpdateRequired(address, true);
}

// --- Configuration Accessor Implementations ---
PlcErrorCodes Module::getConfigBitValue(uint16_t address, bool& value) const {
  return getField(_config_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::setConfigBitValue(uint16_t address, bool value) {
  return setField(_config_bits, address, &bitUnit::value, value);
}
PlcErrorCodes Module::getConfigBitRequired(uint16_t address, bool& value) const {
  return getField(_config_bits, address, &bitUnit::required, value);
}
PlcErrorCodes Module::setConfigBitRequired(uint16_t address, bool value) {
  return setField(_config_bits, address, &bitUnit::required, value);
}
PlcErrorCodes Module::getConfigBitUpdateValue(uint16_t address, bool& value) const {
  return getField(_config_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::setConfigBitUpdateValue(uint16_t address, bool value) {
  return setField(_config_bits, address, &bitUnit::updateValue, value);
}
PlcErrorCodes Module::getConfigBitUpdateRequired(uint16_t address, bool& value) const {
  return getField(_config_bits, address, &bitUnit::updateRequired, value);
}
PlcErrorCodes Module::setConfigBitUpdateRequired(uint16_t address, bool value) {
  return setField(_config_bits, address, &bitUnit::updateRequired, value);
}

PlcErrorCodes Module::requestConfigBit(uint16_t address, bool value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs = setConfigBitRequired(address, value);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::requestConfigBit",
              "Failed to set config bit value for address " + std::to_string(address), rs);
    return rs;
  }
  return setConfigBitUpdateRequired(address, true);
}

PlcErrorCodes Module::getConfigRegisterValue(uint16_t address, uint64_t& value) const {
  return getField(_config_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::setConfigRegisterValue(uint16_t address, uint64_t value) {
  return setField(_config_registers, address, &registerUnit::value, value);
}
PlcErrorCodes Module::getConfigRegisterRequired(uint16_t address, uint64_t& value) const {
  return getField(_config_registers, address, &registerUnit::required, value);
}
PlcErrorCodes Module::setConfigRegisterRequired(uint16_t address, uint64_t value) {
  return setField(_config_registers, address, &registerUnit::required, value);
}
PlcErrorCodes Module::getConfigRegisterUpdateValue(uint16_t address, bool& value) const {
  return getField(_config_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::setConfigRegisterUpdateValue(uint16_t address, bool value) {
  return setField(_config_registers, address, &registerUnit::updateValue, value);
}
PlcErrorCodes Module::getConfigRegisterUpdateRequired(uint16_t address, bool& value) const {
  return getField(_config_registers, address, &registerUnit::updateRequired, value);
}
PlcErrorCodes Module::setConfigRegisterUpdateRequired(uint16_t address, bool value) {
  return setField(_config_registers, address, &registerUnit::updateRequired, value);
}

PlcErrorCodes Module::requestConfigRegister(uint16_t address, uint64_t value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs = setConfigRegisterRequired(address, value);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("Module::requestConfigRegister",
              "Failed to set config register value for address " + std::to_string(address), rs);
    return rs;
  }
  return setConfigRegisterUpdateRequired(address, true);
}

PlcErrorCodes Module::getInputBitUpdateIfChanged(uint16_t address, bool& out_value,
                                                 bool& has_changed) {
  return getUpdateIfChanged<bitUnit, bool>(_input_bits, address, out_value, has_changed);
}

PlcErrorCodes Module::getOutputBitUpdateIfChanged(uint16_t address, bool& out_value,
                                                  bool& has_changed) {
  return getUpdateIfChanged<bitUnit, bool>(_output_bits, address, out_value, has_changed);
}

PlcErrorCodes Module::getInputRegisterUpdateIfChanged(uint16_t address, uint64_t& out_value,
                                                      bool& has_changed) {
  return getUpdateIfChanged<registerUnit, uint64_t>(_input_registers, address, out_value,
                                                    has_changed);
}

PlcErrorCodes Module::getOutputRegisterUpdateIfChanged(uint16_t address, uint64_t& out_value,
                                                       bool& has_changed) {
  return getUpdateIfChanged<registerUnit, uint64_t>(_output_registers, address, out_value,
                                                    has_changed);
}

PlcErrorCodes Module::getSecureStateBitUpdateIfChanged(uint16_t address, bool& out_value,
                                                       bool& has_changed) {
  return getUpdateIfChanged<bitUnit, bool>(_secure_state_bits, address, out_value, has_changed);
}

PlcErrorCodes Module::getSecureStateRegisterUpdateIfChanged(uint16_t address, uint64_t& out_value,
                                                            bool& has_changed) {
  return getUpdateIfChanged<registerUnit, uint64_t>(_secure_state_registers, address, out_value,
                                                    has_changed);
}

PlcErrorCodes Module::getConfigBitUpdateIfChanged(uint16_t address, bool& out_value,
                                                  bool& has_changed) {
  return getUpdateIfChanged<bitUnit, bool>(_config_bits, address, out_value, has_changed);
}

PlcErrorCodes Module::getConfigRegisterUpdateIfChanged(uint16_t address, uint64_t& out_value,
                                                       bool& has_changed) {
  return getUpdateIfChanged<registerUnit, uint64_t>(_config_registers, address, out_value,
                                                    has_changed);
}

template <typename T, typename ValueT>
PlcErrorCodes Module::getUpdateIfChanged(std::map<uint16_t, T>& container, uint16_t address,
                                         ValueT& out_value, bool& has_changed) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  has_changed = false;
  auto it = container.find(address);
  if (it == container.end()) {
    log_error("Module::getUpdateIfChanged",
              "Address " + std::to_string(address) + " not found for module_id " +
                  std::to_string(_module_id),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }

  if (it->second.updateValue) {  // If the value has been updated in hardware
    TRACE_STREAM("[CHANGED] addr=" << address << " module=" << _module_id
                                   << " new_value=" << it->second.value);
    out_value = it->second.value;
    it->second.updateValue = false;  // Reset the flag
    has_changed = true;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes Module::_buildIOBlocks(const std::vector<IoDefinition>& sorted_definitions,
                                     std::vector<IO_Block>& out_blocks) {
  out_blocks.clear();
  if (sorted_definitions.empty()) {
    return PlcErrorCodes::PLC_SUCCESS;
  }

  // Create the first block from the first definition
  IO_Block current_block;
  const auto& first_def = sorted_definitions.front();
  current_block.io_type = first_def.io_type;
  current_block.hardware_access = first_def.hardware_access;
  current_block.physical_start_address = first_def.physical_address;
  current_block.quantity = (first_def.io_type == 1) ? 1 : first_def.register_count;
  current_block.contained_definitions.push_back(first_def);

  // Loop through the rest of the definitions
  for (size_t i = 1; i < sorted_definitions.size(); ++i) {
    const auto& current_def = sorted_definitions[i];

    // --- Determine if current_def could potentially merge with current_block
    // ---
    bool potentially_mergeable = false;
    bool is_contiguous = false;
    bool is_bitmask_on_same = false;

    // Check 1: Must be same type and access direction
    if (current_def.io_type == current_block.io_type &&
        current_def.hardware_access == current_block.hardware_access) {
      // Check 2a: Is it contiguous?
      is_contiguous = (current_def.physical_address ==
                       current_block.physical_start_address + current_block.quantity);

      // Check 2b: Is it a bitmask on the last register of the current block?
      uint16_t last_reg_addr = current_block.physical_start_address + current_block.quantity - 1;
      is_bitmask_on_same = (current_def.access_method == 2 &&  // bitmask
                            current_block.io_type == 2 &&      // register block
                            current_def.physical_address == last_reg_addr);

      // Potential merge allowed if bitmask on same OR contiguous (with
      // restrictions)
      potentially_mergeable = is_bitmask_on_same;
      // Contiguous only allowed if writing (access=2) or if it's bits (type=1)
      if (is_contiguous && (current_block.hardware_access == 2 || current_block.io_type == 1)) {
        potentially_mergeable = true;
      }
    }

    // --- Decide whether to merge based on potential and limits ---
    bool should_merge = false;
    if (potentially_mergeable) {
      uint16_t potential_new_quantity = current_block.quantity;
      if (is_contiguous) {
        potential_new_quantity += (current_block.io_type == 1) ? 1 : current_def.register_count;
      }

      // Select the appropriate limit
      uint16_t current_limit = 65535;
      bool is_read = (current_block.hardware_access == 1);
      if (current_block.io_type == 1) {
        current_limit = is_read ? _max_read_bit_block_size : _max_write_bit_block_size;
      } else {
        current_limit = is_read ? _max_read_register_block_size : _max_write_register_block_size;
      }
      if (current_limit == 0) current_limit = 65535;

      if (potential_new_quantity <= current_limit) {
        should_merge = true;
      }
    }

    // --- Perform action: Merge or Start New Block ---
    if (should_merge) {
      current_block.contained_definitions.push_back(current_def);
      if (is_contiguous) {
        current_block.quantity += (current_block.io_type == 1) ? 1 : current_def.register_count;
      }
    } else {
      out_blocks.push_back(current_block);
      current_block = {};
      current_block.io_type = current_def.io_type;
      current_block.hardware_access = current_def.hardware_access;
      current_block.physical_start_address = current_def.physical_address;
      current_block.quantity = (current_def.io_type == 1) ? 1 : current_def.register_count;
      current_block.contained_definitions.push_back(current_def);
    }
  }

  // Add the very last block to the list
  out_blocks.push_back(current_block);
  return PlcErrorCodes::PLC_SUCCESS;
}