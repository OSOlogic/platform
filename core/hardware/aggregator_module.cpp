/**
 * @file aggregator_module.cpp
 * @author Diego Arcos Sapena
 * @brief Implementation of the AggregatorModule class.
 * @version a-2.1.0
 * @date 2025/08/31
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "aggregator_module.hpp"

#include <algorithm>

#include "../common/debug.hpp"
#include "../common/errors.hpp"
#include "../database/database.hpp"

// --- Constructor ---

AggregatorModule::AggregatorModule(uint32_t module_id, uint32_t model_id,
                                   const std::string &module_name,
                                   const std::string &address_on_channel,
                                   const std::string &channel, const std::string &protocol,
                                   const std::vector<IModulePtr> &child_modules,
                                   const std::vector<AggregatedMappingEntry> &raw_mappings)
    : _module_id(module_id),
      _model_id(model_id),
      _module_name(module_name),
      _address_on_channel(address_on_channel),
      _channel(channel),
      _protocol(protocol),
      _child_modules(child_modules),
      _raw_mappings(raw_mappings),
      _connected(0) {}

// --- IModule Interface Implementation ---

PlcErrorCodes AggregatorModule::initialize() {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  DEBUG_STREAM("AggregatorModule::initialize: Initializing aggregated module " + _module_name +
               " (ID: " + std::to_string(_module_id) + ")");

  // --- 1. CLEAR AND GET DEFINITIONS ---
  _input_bit_map.clear();
  _output_bit_map.clear();
  _secure_state_bit_map.clear();
  _input_register_map.clear();
  _output_register_map.clear();
  _secure_state_register_map.clear();
  _config_bit_map.clear();
  _config_register_map.clear();

  std::shared_ptr<PLC_Database> database;
  PlcErrorCodes rs = PLC_Database::getInstance(database);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("AggregatorModule::initialize", "Failed to get database instance.", rs);
    return rs;
  }

  std::vector<IoDefinition> self_definitions;

  // Get ALL I/O definitions for this aggregator (no mode filtering -
  // aggregators mirror children)
  rs = database->getIoDefinitions(_model_id, _module_id, self_definitions);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("AggregatorModule::initialize",
              "Failed to get I/O definitions for own model_id: " + std::to_string(_model_id), rs);
    return rs;
  }

  // --- 2. BUILD TRANSLATION MAPS ---
  // Iterate through our OWN I/O definitions first.
  for (const auto &def : self_definitions) {
    bool mapping_found = false;
    // For each definition, find its corresponding mapping rule.
    for (const auto &mapping : _raw_mappings) {
      if (def.io_definition_id == mapping.fk_aggregated_io_definition_id) {
        // Rule found. Now, resolve the slot index to a child module pointer.
        if (mapping.child_slot_index >= _child_modules.size()) {
          log_error("AggregatorModule::initialize",
                    "Child slot index " + std::to_string(mapping.child_slot_index) +
                        " out of range (have " + std::to_string(_child_modules.size()) +
                        " children).",
                    PlcErrorCodes::ERROR_MODULE_NOT_FOUND);
          return PlcErrorCodes::ERROR_MODULE_NOT_FOUND;
        }
        IModulePtr target_child = _child_modules[mapping.child_slot_index];

        // Create the target object with the child pointer and its logical
        // address.
        MappingTarget target = {target_child, mapping.child_logical_address};

        // Logic to sort the mapping into the correct internal map.
        if (def.purpose == 1)  // standard
        {
          if (def.hardware_access == 1)  // 1 = readonly -> input
          {
            if (def.io_type == 1)
              _input_bit_map[def.logical_address] = target;
            else
              _input_register_map[def.logical_address] = target;
          } else  // 2 = readwrite -> output
          {
            if (def.io_type == 1)
              _output_bit_map[def.logical_address] = target;
            else
              _output_register_map[def.logical_address] = target;
          }
        } else if (def.purpose == 2)  // secure_state
        {
          if (def.io_type == 1)
            _secure_state_bit_map[def.logical_address] = target;
          else
            _secure_state_register_map[def.logical_address] = target;
        } else if (def.purpose == 3)  // configuration
        {
          if (def.io_type == 1)
            _config_bit_map[def.logical_address] = target;
          else
            _config_register_map[def.logical_address] = target;
        }

        mapping_found = true;
        break;  // Mapping found, continue to the next definition.
      }
    }
    if (!mapping_found) {
      log_error("AggregatorModule::initialize",
                "No mapping entry found for aggregated I/O definition ID " +
                    std::to_string(def.io_definition_id),
                PlcErrorCodes::ERROR_NOT_FOUND);
    }
  }

  // --- 3. Fill _io_blocks ---

  if (self_definitions.empty()) {
    log_error("AggregatorModule::initialize", "No self definitions found.",
              PlcErrorCodes::ERROR_NOT_FOUND);
    return PlcErrorCodes::PLC_SUCCESS;  // No I/O points to structure
  }

  // --- Populate the io_definition_map ---
  _io_definition_map.clear();
  for (const auto &def : self_definitions) {
    _io_definition_map[def.io_definition_id] = def;
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::setRequiredValueById(uint32_t io_definition_id, uint64_t value) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  TRACE_STREAM("[DB->MEM][AGG] setRequiredValueById io_def=" << io_definition_id
                                                             << " value=" << value);
  // 1. Find the mapping rule for this aggregated I/O point.
  AggregatedMappingEntry mapping_rule;
  PlcErrorCodes rs = findChildMappingById(io_definition_id, mapping_rule);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("AggregatorModule::setRequiredValueById",
              "No child mapping found for aggregated_io_definition_id: " +
                  std::to_string(io_definition_id),
              rs);
    return rs;
  }

  // 2. Resolve the slot index to a child module pointer.
  if (mapping_rule.child_slot_index >= _child_modules.size()) {
    log_error(
        "AggregatorModule::setRequiredValueById",
        "Child slot index " + std::to_string(mapping_rule.child_slot_index) + " out of range.",
        PlcErrorCodes::ERROR_MODULE_NOT_FOUND);
    return PlcErrorCodes::ERROR_MODULE_NOT_FOUND;
  }
  IModulePtr target_child = _child_modules[mapping_rule.child_slot_index];

  // 3. Redirect the call to the child module, using the CHILD's
  // io_definition_id.
  return target_child->setRequiredValueById(mapping_rule.fk_child_io_definition_id, value);
}

PlcErrorCodes AggregatorModule::setAllRequiredValues(
    const std::vector<std::pair<uint32_t, uint64_t>> &required_values) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes final_rs = PlcErrorCodes::PLC_SUCCESS;

  for (const auto &pair : required_values) {
    PlcErrorCodes rs = this->setRequiredValueById(pair.first, pair.second);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      final_rs = rs;
      break;
    }
  }

  return final_rs;
}

PlcErrorCodes AggregatorModule::sync() {
  // Aggregators do not sync directly with hardware. This is done by the
  // physical children.

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::findChildMapping(uint16_t aggregated_address, uint8_t purpose,
                                                 MappingTarget &out_target) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  std::map<uint16_t, MappingTarget>::iterator it;

  if (purpose == 1)  // Standard I/O
  {
    // Check standard input bits
    it = _input_bit_map.find(aggregated_address);
    if (it != _input_bit_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }

    // Check standard output bits
    it = _output_bit_map.find(aggregated_address);
    if (it != _output_bit_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }

    // Check standard input registers
    it = _input_register_map.find(aggregated_address);
    if (it != _input_register_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }

    // Check standard output registers
    it = _output_register_map.find(aggregated_address);
    if (it != _output_register_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }
  } else if (purpose == 2)  // Secure State
  {
    // Check secure state bits
    it = _secure_state_bit_map.find(aggregated_address);
    if (it != _secure_state_bit_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }

    // Check secure state registers
    it = _secure_state_register_map.find(aggregated_address);
    if (it != _secure_state_register_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }
  } else if (purpose == 3)  // Configuration
  {
    // Check configuration bits
    it = _config_bit_map.find(aggregated_address);
    if (it != _config_bit_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }

    // Check configuration registers
    it = _config_register_map.find(aggregated_address);
    if (it != _config_register_map.end()) {
      out_target = it->second;
      return PlcErrorCodes::PLC_SUCCESS;
    }
  }

  // If we get here, no mapping was found for the given address and purpose.
  log_error("AggregatorModule::findChildMapping",
            "No child mapping found for aggregated address " + std::to_string(aggregated_address) +
                " with purpose " + std::to_string(purpose),
            PlcErrorCodes::ERROR_NOT_FOUND);
  return PlcErrorCodes::ERROR_NOT_FOUND;
}

PlcErrorCodes AggregatorModule::setSafeState() {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes final_rs = PlcErrorCodes::PLC_SUCCESS;
  for (const auto &child : _child_modules) {
    PlcErrorCodes rs = child->setSafeState();
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("AggregatorModule::setSafeState", "Failed to set safe state on child module.", rs);
      final_rs = rs;  // Report the first error encountered
    }
  }
  return final_rs;
}

PlcErrorCodes AggregatorModule::freeModule() {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  _connected = 0;

  // Don't do this; in case freeModule is called for a aggregator module, this
  // maps won't be generated again (because of the empty sync() function)

  /*_input_bit_map.clear();
  _output_bit_map.clear();
  _secure_state_bit_map.clear();
  _input_register_map.clear();
  _output_register_map.clear();
  _secure_state_register_map.clear();
  _config_bit_map.clear();
  _config_register_map.clear();*/
  return PlcErrorCodes::PLC_SUCCESS;
}

// --- Identity & Configuration Getters ---

PlcErrorCodes AggregatorModule::getModuleId(uint32_t &moduleId) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  moduleId = _module_id;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getModuleName(std::string &name) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  name = _module_name;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getModelId(uint32_t &modelId) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  modelId = _model_id;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getChannel(std::string &channel) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  channel = _channel;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getAllIoDefinitions(std::vector<IoDefinition> &out_defs) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  out_defs.clear();
  // Reserve space for efficiency if the map is large
  out_defs.reserve(_io_definition_map.size());

  // Iterate through the internal map containing all definitions for this module
  for (const auto &pair : _io_definition_map) {
    out_defs.push_back(pair.second);  // Add the IoDefinition struct to the output vector
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

// --- State & Status Getters ---

PlcErrorCodes AggregatorModule::getConnected(uint8_t &connected) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  // 1. Calculate the "real" state based on the children's states.
  PlcErrorCodes rs;
  uint8_t real_child_state = 0;
  if (!_child_modules.empty()) {
    std::vector<uint8_t> child_statuses;
    for (const auto &child : _child_modules) {
      uint8_t child_connected_state = 0;
      // We don't need to handle the error here; if a child fails,
      // its state will be 0 (disconnected), which is the correct assumption.
      rs = child->getConnected(child_connected_state);
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("AggregatorModule::getConnected",
                  "Failed to get connected state from child module.", rs);
      }
      child_statuses.push_back(child_connected_state);
    }
    real_child_state = *std::min_element(child_statuses.begin(), child_statuses.end());
  }

  // 2. Compare the real state with our internal state and apply transition
  // logic.
  bool children_are_connected = (real_child_state >= 2);
  bool self_is_connected = (_connected == 2 || _connected == 3);

  if (children_are_connected && !self_is_connected) {
    // TRANSITION: DISCONNECTED -> CONNECTED
    // The children are connected, but our internal state was disconnected.
    // We force our state to 2 so that databaseSyncTask will process it.
    _connected = 2;
  } else if (!children_are_connected && self_is_connected) {
    // TRANSITION: CONNECTED -> DISCONNECTED
    // The children have disconnected, but our internal state was connected.
    // We force our state to 0.
    _connected = 0;
  }

  // 3. Return the current internal state (which may have been modified by the
  // logic above).
  connected = _connected;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::setConnected(uint8_t connected) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  _connected = connected;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::forceFullSync() {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  DEBUG_STREAM(
      "Propagating full sync to children for aggregator module_id: " << std::to_string(_module_id));
  PlcErrorCodes rs;
  for (const auto &child : _child_modules) {
    if (child) {
      rs = child->forceFullSync();
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        uint32_t child_id = 0;
        rs = child->getModuleId(child_id);
        if (rs != PlcErrorCodes::PLC_SUCCESS) {
          log_error("AggregatorModule::forceFullSync",
                    "Failed to get child module ID for error logging.", rs);
          return rs;
        }
        log_error("AggregatorModule::forceFullSync",
                  "Failed to propagate forceFullSync to child " + std::to_string(child_id), rs);
        return rs;
      }
    }
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::collectAllCurrentValues(
    OperationMode mode, std::vector<DbUpdateInstruction> &out_updates) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  PlcErrorCodes rs;

  for (const auto &pair : _io_definition_map) {
    const IoDefinition &def = pair.second;

    // Filter by operation mode
    if (mode == OperationMode::EXECUTION && def.purpose != 1) continue;
    if (mode == OperationMode::CONFIGURATION && (def.purpose != 2 && def.purpose != 3)) continue;

    uint64_t current_val_bits = 0;

    // Read the current value using the existing redirection methods
    if (def.io_type == 1)  // bit
    {
      bool bit_val = false;
      if (def.purpose == 1 && def.hardware_access == 1) {
        rs = getInputBitValue(def.logical_address, bit_val);
      } else if (def.purpose == 1 && def.hardware_access == 2) {
        rs = getOutputBitValue(def.logical_address, bit_val);
      } else if (def.purpose == 2) {
        rs = getSecureStateBitValue(def.logical_address, bit_val);
      } else if (def.purpose == 3) {
        rs = getConfigBitValue(def.logical_address, bit_val);
      } else {
        continue;  // Skip unknown combinations
      }

      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("AggregatorModule::collectAllCurrentValues",
                  "Failed to get bit value for io_def " + std::to_string(def.io_definition_id), rs);
        return rs;
      }
      current_val_bits = static_cast<uint64_t>(bit_val);
    } else  // register (io_type == 2)
    {
      if (def.purpose == 1 && def.hardware_access == 1) {
        rs = getInputRegisterValue(def.logical_address, current_val_bits);
      } else if (def.purpose == 1 && def.hardware_access == 2) {
        rs = getOutputRegisterValue(def.logical_address, current_val_bits);
      } else if (def.purpose == 2) {
        rs = getSecureStateRegisterValue(def.logical_address, current_val_bits);
      } else if (def.purpose == 3) {
        rs = getConfigRegisterValue(def.logical_address, current_val_bits);
      } else {
        continue;  // Skip unknown combinations
      }

      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("AggregatorModule::collectAllCurrentValues",
                  "Failed to get register value for io_def " + std::to_string(def.io_definition_id),
                  rs);
        return rs;
      }
    }

    out_updates.push_back({_module_id, def.io_definition_id, current_val_bits});
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::findChildMappingById(uint32_t io_definition_id,
                                                     AggregatedMappingEntry &out_mapping) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  for (const auto &mapping : _raw_mappings) {
    if (mapping.fk_aggregated_io_definition_id == io_definition_id) {
      out_mapping = mapping;
      return PlcErrorCodes::PLC_SUCCESS;
    }
  }
  log_error("AggregatorModule::findChildMappingById",
            "No mapping found for aggregated I/O definition ID " + std::to_string(io_definition_id),
            PlcErrorCodes::ERROR_NOT_FOUND);
  return PlcErrorCodes::ERROR_NOT_FOUND;  // Mapping for the given ID was not
                                          // found
}

PlcErrorCodes AggregatorModule::getChildBySlotIndex(uint8_t slot_index, IModulePtr &out_child) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);

  if (slot_index >= _child_modules.size()) {
    log_error("AggregatorModule::getChildBySlotIndex",
              "Slot index " + std::to_string(slot_index) + " out of range (have " +
                  std::to_string(_child_modules.size()) + " children).",
              PlcErrorCodes::ERROR_MODULE_NOT_FOUND);
    return PlcErrorCodes::ERROR_MODULE_NOT_FOUND;
  }

  out_child = _child_modules[slot_index];
  return PlcErrorCodes::PLC_SUCCESS;
}

// --- Generic Redirection Templates ---

template <typename T, typename Func>
PlcErrorCodes AggregatorModule::_getRedirect(uint16_t address,
                                             const std::map<uint16_t, MappingTarget> &map, T &value,
                                             Func func) const {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  auto it = map.find(address);
  if (it == map.end()) {
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  const auto &target = it->second;
  // Invoke the lambda, which calls the correct method on the child module
  return func(target.module, target.address, value);
}

template <typename T, typename Func>
PlcErrorCodes AggregatorModule::_setRedirect(uint16_t address,
                                             const std::map<uint16_t, MappingTarget> &map, T value,
                                             Func func) {
  std::unique_lock<std::recursive_mutex> lock(_mutex);
  auto it = map.find(address);
  if (it == map.end()) {
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  const auto &target = it->second;
  // Invoke the lambda, which calls the correct method on the child module
  return func(target.module, target.address, value);
}

// --- In-Memory Data Accessors (Redirected to Children) ---

// --- Standard Input Bit ---
PlcErrorCodes AggregatorModule::getInputBitValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _input_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getInputBitValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::setInputBitValue(uint16_t address, bool value) {
  return _setRedirect(address, _input_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setInputBitValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::getInputBitUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _input_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getInputBitUpdateValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::setInputBitUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _input_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setInputBitUpdateValue(addr, val);
  });
}

// --- Standard Input Register ---
PlcErrorCodes AggregatorModule::getInputRegisterValue(uint16_t address, uint64_t &value) const {
  return _getRedirect(address, _input_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getInputRegisterValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setInputRegisterValue(uint16_t address, uint64_t value) {
  return _setRedirect(address, _input_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setInputRegisterValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getInputRegisterUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _input_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getInputRegisterUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setInputRegisterUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _input_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setInputRegisterUpdateValue(addr, val);
                      });
}

// --- Standard Output Bit ---
PlcErrorCodes AggregatorModule::getOutputBitValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getOutputBitValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::setOutputBitValue(uint16_t address, bool value) {
  return _setRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setOutputBitValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::getOutputBitRequired(uint16_t address, bool &value) const {
  return _getRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getOutputBitRequired(addr, val);
  });
}
PlcErrorCodes AggregatorModule::setOutputBitRequired(uint16_t address, bool value) {
  return _setRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setOutputBitRequired(addr, val);
  });
}
PlcErrorCodes AggregatorModule::getOutputBitUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getOutputBitUpdateValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::setOutputBitUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setOutputBitUpdateValue(addr, val);
  });
}
PlcErrorCodes AggregatorModule::getOutputBitUpdateRequired(uint16_t address, bool &value) const {
  return _getRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getOutputBitUpdateRequired(addr, val);
  });
}
PlcErrorCodes AggregatorModule::setOutputBitUpdateRequired(uint16_t address, bool value) {
  return _setRedirect(address, _output_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setOutputBitUpdateRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::requestOutputBit(uint16_t address, bool value) {
  auto it = _output_bit_map.find(address);
  if (it == _output_bit_map.end()) {
    log_error("AggregatorModule::requestOutputBit", "Invalid address: " + std::to_string(address),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  return it->second.module->requestOutputBit(it->second.address, value);
}

// --- Standard Output Register ---
PlcErrorCodes AggregatorModule::getOutputRegisterValue(uint16_t address, uint64_t &value) const {
  return _getRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getOutputRegisterValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setOutputRegisterValue(uint16_t address, uint64_t value) {
  return _setRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setOutputRegisterValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getOutputRegisterRequired(uint16_t address, uint64_t &value) const {
  return _getRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getOutputRegisterRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setOutputRegisterRequired(uint16_t address, uint64_t value) {
  return _setRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setOutputRegisterRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getOutputRegisterUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getOutputRegisterUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setOutputRegisterUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setOutputRegisterUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getOutputRegisterUpdateRequired(uint16_t address,
                                                                bool &value) const {
  return _getRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getOutputRegisterUpdateRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setOutputRegisterUpdateRequired(uint16_t address, bool value) {
  return _setRedirect(address, _output_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setOutputRegisterUpdateRequired(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::requestOutputRegister(uint16_t address, uint64_t value) {
  auto it = _output_register_map.find(address);
  if (it == _output_register_map.end()) {
    log_error("AggregatorModule::requestOutputRegister",
              "Invalid address: " + std::to_string(address),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  return it->second.module->requestOutputRegister(it->second.address, value);
}

// --- Secure State Bit ---
PlcErrorCodes AggregatorModule::getSecureStateBitValue(uint16_t address, bool &value) const {
  return _getRedirect(
      address, _secure_state_bit_map, value,
      [](IModulePtr m, uint16_t addr, bool &val) { return m->getSecureStateBitValue(addr, val); });
}
PlcErrorCodes AggregatorModule::setSecureStateBitValue(uint16_t address, bool value) {
  return _setRedirect(
      address, _secure_state_bit_map, value,
      [](IModulePtr m, uint16_t addr, bool val) { return m->setSecureStateBitValue(addr, val); });
}
PlcErrorCodes AggregatorModule::getSecureStateBitRequired(uint16_t address, bool &value) const {
  return _getRedirect(address, _secure_state_bit_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getSecureStateBitRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateBitRequired(uint16_t address, bool value) {
  return _setRedirect(address, _secure_state_bit_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setSecureStateBitRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getSecureStateBitUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _secure_state_bit_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getSecureStateBitUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateBitUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _secure_state_bit_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setSecureStateBitUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getSecureStateBitUpdateRequired(uint16_t address,
                                                                bool &value) const {
  return _getRedirect(address, _secure_state_bit_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getSecureStateBitUpdateRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateBitUpdateRequired(uint16_t address, bool value) {
  return _setRedirect(address, _secure_state_bit_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setSecureStateBitUpdateRequired(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::requestSecureStateBit(uint16_t address, bool value) {
  auto it = _secure_state_bit_map.find(address);
  if (it == _secure_state_bit_map.end()) {
    log_error("AggregatorModule::requestSecureStateBit",
              "Invalid address: " + std::to_string(address),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  return it->second.module->requestSecureStateBit(it->second.address, value);
}

// --- Secure State Register ---
PlcErrorCodes AggregatorModule::getSecureStateRegisterValue(uint16_t address,
                                                            uint64_t &value) const {
  return _getRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getSecureStateRegisterValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateRegisterValue(uint16_t address, uint64_t value) {
  return _setRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setSecureStateRegisterValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getSecureStateRegisterRequired(uint16_t address,
                                                               uint64_t &value) const {
  return _getRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getSecureStateRegisterRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateRegisterRequired(uint16_t address, uint64_t value) {
  return _setRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setSecureStateRegisterRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getSecureStateRegisterUpdateValue(uint16_t address,
                                                                  bool &value) const {
  return _getRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getSecureStateRegisterUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateRegisterUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setSecureStateRegisterUpdateValue(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::getSecureStateRegisterUpdateRequired(uint16_t address,
                                                                     bool &value) const {
  return _getRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool &val) {
                        return m->getSecureStateRegisterUpdateRequired(addr, val);
                      });
}
PlcErrorCodes AggregatorModule::setSecureStateRegisterUpdateRequired(uint16_t address, bool value) {
  return _setRedirect(address, _secure_state_register_map, value,
                      [](IModulePtr m, uint16_t addr, bool val) {
                        return m->setSecureStateRegisterUpdateRequired(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::getConfigBitValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getConfigBitValue(addr, val);
  });
}

PlcErrorCodes AggregatorModule::setConfigBitValue(uint16_t address, bool value) {
  return _setRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setConfigBitValue(addr, val);
  });
}

PlcErrorCodes AggregatorModule::getConfigBitRequired(uint16_t address, bool &value) const {
  return _getRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getConfigBitRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::setConfigBitRequired(uint16_t address, bool value) {
  return _setRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setConfigBitRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::getConfigBitUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getConfigBitUpdateValue(addr, val);
  });
}

PlcErrorCodes AggregatorModule::setConfigBitUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setConfigBitUpdateValue(addr, val);
  });
}

PlcErrorCodes AggregatorModule::getConfigBitUpdateRequired(uint16_t address, bool &value) const {
  return _getRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getConfigBitUpdateRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::setConfigBitUpdateRequired(uint16_t address, bool value) {
  return _setRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setConfigBitUpdateRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::requestConfigBit(uint16_t address, bool value) {
  auto it = _config_bit_map.find(address);
  if (it == _config_bit_map.end()) {
    log_error("AggregatorModule::requestConfigBit", "Invalid address: " + std::to_string(address),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  return it->second.module->requestConfigBit(it->second.address, value);
}

PlcErrorCodes AggregatorModule::getConfigRegisterValue(uint16_t address, uint64_t &value) const {
  return _getRedirect(address, _config_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getConfigRegisterValue(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::setConfigRegisterValue(uint16_t address, uint64_t value) {
  return _setRedirect(address, _config_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setConfigRegisterValue(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::getConfigRegisterRequired(uint16_t address, uint64_t &value) const {
  return _getRedirect(address, _config_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t &val) {
                        return m->getConfigRegisterRequired(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::setConfigRegisterRequired(uint16_t address, uint64_t value) {
  return _setRedirect(address, _config_register_map, value,
                      [](IModulePtr m, uint16_t addr, uint64_t val) {
                        return m->setConfigRegisterRequired(addr, val);
                      });
}

PlcErrorCodes AggregatorModule::getConfigRegisterUpdateValue(uint16_t address, bool &value) const {
  return _getRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getConfigRegisterUpdateValue(addr, val);
  });
}

PlcErrorCodes AggregatorModule::setConfigRegisterUpdateValue(uint16_t address, bool value) {
  return _setRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setConfigRegisterUpdateValue(addr, val);
  });
}

PlcErrorCodes AggregatorModule::getConfigRegisterUpdateRequired(uint16_t address,
                                                                bool &value) const {
  return _getRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool &val) {
    return m->getConfigRegisterUpdateRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::requestConfigRegister(uint16_t address, uint64_t value) {
  auto it = _config_register_map.find(address);
  if (it == _config_register_map.end()) {
    log_error("AggregatorModule::requestConfigRegister",
              "Invalid address: " + std::to_string(address),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  return it->second.module->requestConfigRegister(it->second.address, value);
}

PlcErrorCodes AggregatorModule::setConfigRegisterUpdateRequired(uint16_t address, bool value) {
  return _setRedirect(address, _config_bit_map, value, [](IModulePtr m, uint16_t addr, bool val) {
    return m->setConfigRegisterUpdateRequired(addr, val);
  });
}

PlcErrorCodes AggregatorModule::requestSecureStateRegister(uint16_t address, uint64_t value) {
  auto it = _secure_state_register_map.find(address);
  if (it == _secure_state_register_map.end()) {
    log_error("AggregatorModule::requestSecureStateRegister",
              "Invalid address: " + std::to_string(address),
              PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS);
    return PlcErrorCodes::ERROR_REQUEST_INVALID_ADDRESS;
  }
  return it->second.module->requestSecureStateRegister(it->second.address, value);
}

PlcErrorCodes AggregatorModule::getInputBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                           bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getInputRegisterUpdateIfChanged(uint16_t address,
                                                                uint64_t &out_value,
                                                                bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getOutputBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                            bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getOutputRegisterUpdateIfChanged(uint16_t address,
                                                                 uint64_t &out_value,
                                                                 bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getSecureStateBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                                 bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getSecureStateRegisterUpdateIfChanged(uint16_t address,
                                                                      uint64_t &out_value,
                                                                      bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getConfigBitUpdateIfChanged(uint16_t address, bool &out_value,
                                                            bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes AggregatorModule::getConfigRegisterUpdateIfChanged(uint16_t address,
                                                                 uint64_t &out_value,
                                                                 bool &has_changed) {
  return PlcErrorCodes::PLC_SUCCESS;
}
