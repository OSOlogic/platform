/**
 * @file plc.cpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief PLC class
 * @version a-1.0.0
 * @date 2024/11/22
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "plc.hpp"

#include <map>

#include "../common/debug.hpp"
#include "../common/errors.hpp"
#include "aggregator_module.hpp"

std::shared_ptr<OsoLogicPLC> OsoLogicPLC::_instance_ptr = nullptr;
std::recursive_mutex OsoLogicPLC::_mutex;
std::atomic<OperationMode> OsoLogicPLC::_operation_mode = OperationMode::EXECUTION;

OsoLogicPLC::OsoLogicPLC(const std::vector<IModulePtr> &modulePtrs) : _modules(modulePtrs) {}

PlcErrorCodes OsoLogicPLC::initializeInstance(const std::vector<IModulePtr> &modules) {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  if (_instance_ptr != nullptr) {
    // Already initialized, can't be done again
    return PlcErrorCodes::ERROR_PLC_ALREADY_INITIALIZED;
  }
  _instance_ptr = std::shared_ptr<OsoLogicPLC>(new OsoLogicPLC(modules));
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes OsoLogicPLC::getInstance(std::shared_ptr<OsoLogicPLC> &instance_ref) {
  std::lock_guard<std::recursive_mutex> lock(_mutex);

  if (!_instance_ptr) {
    log_error("OsoLogicPLC::getInstance",
              "PLC instance not initialized. Call initializeInstance() first.",
              PlcErrorCodes::ERROR_PLC_NO_MODULE);
    return PlcErrorCodes::ERROR_PLC_NO_MODULE;
  }

  instance_ref = _instance_ptr;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes OsoLogicPLC::getModuleById(uint32_t module_id, IModulePtr &module_ptr) const {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  // Iterate through the vector to find the module by its ID
  for (const auto &module : _modules) {
    uint32_t current_id;
    if (module->getModuleId(current_id) == PlcErrorCodes::PLC_SUCCESS) {
      if (current_id == module_id) {
        module_ptr = module;
        return PlcErrorCodes::PLC_SUCCESS;
      }
    }
  }

  log_error("OsoLogicPLC::getModuleById",
            "Module with ID " + std::to_string(module_id) + " not found.",
            PlcErrorCodes::ERROR_PLC_NO_MODULE);
  return PlcErrorCodes::ERROR_PLC_NO_MODULE;
}

PlcErrorCodes OsoLogicPLC::getReverseMap(
    std::map<std::pair<int32_t, uint32_t>, std::vector<AggregatedTarget>> &out_map) const {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  out_map = _reverse_map;
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes OsoLogicPLC::_findUltimatePhysicalSource(IModulePtr start_module,
                                                       uint32_t start_io_def_id,
                                                       std::pair<int32_t, uint32_t> &out_target) {
  std::string channel;
  start_module->getChannel(channel);

  // Base Case: If the module is physical, we have found the source.
  if (channel != "aggregated") {
    uint32_t physical_module_id;
    start_module->getModuleId(physical_module_id);
    out_target = {physical_module_id, start_io_def_id};
    return PlcErrorCodes::PLC_SUCCESS;
  }

  // Recursive Step: The module is aggregated, so we must go deeper.
  auto aggregator_module = std::dynamic_pointer_cast<AggregatorModule>(start_module);
  if (!aggregator_module) {
    log_error("OsoLogicPLC::_findUltimatePhysicalSource",
              "Module has channel_type 'aggregated' but failed to cast to AggregatorModule.",
              PlcErrorCodes::ERROR_INVALID_CAST);
    return PlcErrorCodes::ERROR_INVALID_CAST;
  }

  // Find the next link in the chain using the aggregator's internal map.
  AggregatedMappingEntry child_mapping;
  PlcErrorCodes rs = aggregator_module->findChildMappingById(start_io_def_id, child_mapping);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("OsoLogicPLC::_findUltimatePhysicalSource",
              "Could not find child mapping for aggregated io_definition_id: " +
                  std::to_string(start_io_def_id),
              rs);
    return rs;  // Mapping not found, chain is broken.
  }

  IModulePtr child_module;
  rs = aggregator_module->getChildBySlotIndex(child_mapping.child_slot_index, child_module);
  if (rs != PlcErrorCodes::PLC_SUCCESS) {
    log_error("OsoLogicPLC::_findUltimatePhysicalSource",
              "Child module at slot index " + std::to_string(child_mapping.child_slot_index) +
                  " not found in aggregator.",
              rs);
    return rs;  // Child module does not exist.
  }

  return _findUltimatePhysicalSource(child_module, child_mapping.fk_child_io_definition_id,
                                     out_target);
}

PlcErrorCodes OsoLogicPLC::buildReverseMap() {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  _reverse_map.clear();
  PlcErrorCodes rs;

  // Iterate through all modules to find the aggregated ones
  for (const auto &module : _modules) {
    std::string channel;
    rs = module->getChannel(channel);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("OsoLogicPLC::buildReverseMap", "Failed to get channel for a module.", rs);
      continue;  // Skip this module
    }

    if (channel == "aggregated") {
      // Get all I/O definitions for this aggregated module
      std::vector<IoDefinition> io_definitions;
      rs = module->getAllIoDefinitions(io_definitions);
      if (rs != PlcErrorCodes::PLC_SUCCESS) {
        log_error("OsoLogicPLC::buildReverseMap",
                  "Failed to get I/O definitions for aggregated module.", rs);
        continue;  // Skip this module
      }

      // For every I/O point this aggregated module defines
      for (const auto &def : io_definitions) {
        // find its ultimate physical source.
        std::pair<int32_t, uint32_t> physical_key;
        rs = _findUltimatePhysicalSource(module, def.io_definition_id, physical_key);
        if (rs == PlcErrorCodes::PLC_SUCCESS) {
          // The key is the final physical point {module_id, io_definition_id}.
          DEBUG_STREAM("[PLC] Mapping physical point (Module: "
                       << physical_key.first << ", IO_Def_ID: " << physical_key.second << ")");

          uint32_t aggregated_module_id;
          rs = module->getModuleId(aggregated_module_id);
          if (rs != PlcErrorCodes::PLC_SUCCESS) {
            log_error("OsoLogicPLC::buildReverseMap",
                      "Failed to get module ID for aggregated module while building reverse map.",
                      rs);
            continue;  // Skip adding this mapping
          }
          // The value is the aggregated point that depends on it.
          AggregatedTarget v_target = {aggregated_module_id, def.io_definition_id};

          _reverse_map[physical_key].push_back(v_target);
        } else {
          log_error("OsoLogicPLC::buildReverseMap",
                    "Could not resolve physical source for aggregated point (IO_Def_ID: " +
                        std::to_string(def.io_definition_id) + "). Check configuration.",
                    rs);
          // Continue building the map for other points
        }
      }
    }
  }
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes OsoLogicPLC::getModules(std::vector<IModulePtr> &modules) const {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  // Ensure there are modules available
  if (_modules.empty()) {
    log_error("OsoLogicPLC::getModules", "Attempted to retrieve modules, but none are available.",
              PlcErrorCodes::ERROR_PLC_NO_MODULE);
    return PlcErrorCodes::ERROR_PLC_NO_MODULE;
  }

  modules = _modules;
  return PlcErrorCodes::PLC_SUCCESS;
}

// The child modules of a virtual one will be set to safe state more than once, but it's not a
// problem.
PlcErrorCodes OsoLogicPLC::turnSafeState() {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  // Ensure there are modules available
  if (_modules.empty()) {
    log_error("OsoLogicPLC::turnSafeState",
              "Attempted to set safe state, but no modules are available.",
              PlcErrorCodes::ERROR_PLC_NO_MODULE);
    return PlcErrorCodes::ERROR_PLC_NO_MODULE;
  }

  PlcErrorCodes rs = PlcErrorCodes::PLC_SUCCESS;

  for (auto &module : _modules) {
    uint32_t module_id;
    rs = module->getModuleId(module_id);
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("OsoLogicPLC::turnSafeState", "Failed to get module ID while setting safe state.",
                rs);
      return rs;
    }

    rs = module->setSafeState();
    if (rs != PlcErrorCodes::PLC_SUCCESS) {
      log_error("OsoLogicPLC::turnSafeState",
                "Failed to set safe state for module with ID " + std::to_string(module_id), rs);
      return rs;
    }
  }

  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes OsoLogicPLC::setOperationMode(OperationMode mode) {
  _operation_mode.store(mode, std::memory_order_release);
  return PlcErrorCodes::PLC_SUCCESS;
}

PlcErrorCodes OsoLogicPLC::getOperationMode(OperationMode &mode) {
  mode = _operation_mode.load(std::memory_order_acquire);
  return PlcErrorCodes::PLC_SUCCESS;
}
