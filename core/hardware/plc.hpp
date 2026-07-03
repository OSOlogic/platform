/**
 * @file plc.hpp
 * @author Original C code: Diego Arcos Sapena
 * @author Diego Arcos Sapena
 * @brief PLC class header
 * @version a-1.0.0
 * @date 2024/11/22
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#pragma once
#include <atomic>
#include <map>
#include <mutex>

#include "IModule.hpp"

/**
 * @struct AggregatedTarget
 * @brief A struct to describe an aggregated I/O point that depends on a physical one
 */
struct AggregatedTarget {
  uint32_t aggregated_module_id;
  uint32_t aggregated_io_definition_id;
};

/**
 * @enum OperationMode
 * @brief Defines the operation mode of the PLC.
 */
enum class OperationMode { EXECUTION, CONFIGURATION };

/**
 * @brief Holds global configuration settings for the PLC application.
 */
struct PLC_Config {
  uint32_t rs485_baudrate;
  uint8_t rs485_data_bits;
  char rs485_parity;
  uint8_t rs485_bit_stop;
  OperationMode operation_mode;
};

/**
 * @class OsoLogicPLC
 * @brief Represents a PLC system that manages multiple PLC modules.
 *
 * This class provides methods to retrieve modules by ID, get all modules,
 * turn all modules to a safe state, and initialize all modules.
 */
class OsoLogicPLC {
  public:
  OsoLogicPLC(const OsoLogicPLC &) = delete;
  void operator=(const OsoLogicPLC &) = delete;

  /**
   * @brief Creates the single instance of the PLC with the necessary modules.
   * This should only be called once at application startup.
   * @param modules The fully initialized modules for the PLC to manage.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  static PlcErrorCodes initializeInstance(const std::vector<IModulePtr> &modules);

  /**
   * @brief Gets the single, globally accessible instance of the PLC.
   * @param[out] instance_ref A shared pointer to be filled with the PLC instance.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or an error if not initialized.
   */
  static PlcErrorCodes getInstance(std::shared_ptr<OsoLogicPLC> &instance_ref);

  /**
   * @brief Retrieves a module from the internal list by its unique module_id.
   *
   * @param[in] module_id The unique ID of the module to retrieve.
   * @param[out] module_ptr A shared pointer to the found module.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or ERROR_PLC_NO_MODULE if not found.
   */
  PlcErrorCodes getModuleById(uint32_t module_id, IModulePtr &module_ptr) const;

  /**
   * @brief Get all modules
   * @param [out] modules Vector of pointers to modules
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes getModules(std::vector<IModulePtr> &modules) const;

  /**
   * @brief Turn all modules to safe state
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  PlcErrorCodes turnSafeState();

  /**
   * @brief Builds the hierarchical reverse map.
   * @details This method iterates through all aggregated modules and resolves their
   * I/O points down to their ultimate physical sources, populating the reverse map.
   * This should be called once after all modules are created.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes buildReverseMap();

  /**
   * @brief Gets a const reference to the pre-built reverse map.
   * @param[out] out_map A reference to the map object to be filled.
   * @return PlcErrorCodes::PLC_SUCCESS on success.
   */
  PlcErrorCodes getReverseMap(
      std::map<std::pair<int32_t, uint32_t>, std::vector<AggregatedTarget>> &out_map) const;

  private:
  /**
   * @brief Constructor
   * @param [in] modules Vector of modules pointers
   */
  OsoLogicPLC(const std::vector<IModulePtr> &modules);

  /**
   * @brief Vector of modules in PLC
   */
  std::vector<IModulePtr> _modules;

  /**
   * @brief The single instance pointer
   */
  static std::shared_ptr<OsoLogicPLC> _instance_ptr;

  /**
   * @brief mutex for multithread safety
   */
  static std::recursive_mutex _mutex;

  /**
   * @brief The hierarchical reverse map for aggregated to physical I/O resolution.
   * The key is a pair of {physical_module_id, physical_io_definition_id}.
   * The value is a vector of AggregatedTarget structs representing all aggregated points
   * that depend on this physical point, directly or indirectly.
   */
  std::map<std::pair<int32_t, uint32_t>, std::vector<AggregatedTarget>> _reverse_map;

  /**
   * @brief Recursively finds the ultimate physical source for a given I/O point.
   * This function traverses the hierarchy of aggregated modules until it finds the
   * non-aggregated (physical) module at the end of the chain.
   * @param start_module The module to start the search from.
   * @param start_io_def_id The I/O definition ID on the starting module.
   * @param out_target The final physical target.
   * @return PlcErrorCodes
   */
  PlcErrorCodes _findUltimatePhysicalSource(IModulePtr start_module, uint32_t start_io_def_id,
                                            std::pair<int32_t, uint32_t> &out_target);

  /**
   * @brief Current operation mode of the PLC
   */
  static std::atomic<OperationMode> _operation_mode;

  public:
  /**
   * @brief Sets the operation mode of the PLC.
   * @param mode The new operation mode.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  static PlcErrorCodes setOperationMode(OperationMode mode);

  /**
   * @brief Gets the current operation mode of the PLC.
   * @param mode The current operation mode.
   * @return PlcErrorCodes::PLC_SUCCESS on success, or negative number on failure.
   */
  static PlcErrorCodes getOperationMode(OperationMode &mode);
};

using OsoLogicPLCPtr = std::shared_ptr<OsoLogicPLC>;
