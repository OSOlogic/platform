# -*- coding: utf-8 -*-

# ==============================================================================
# File Information
# ==============================================================================

# 
# @file server.py
# @author Diego Arcos Sapena
# @brief PLC OsoLogic Modbus TCP Gateway Server
# @version a-1.0.0
# @date 2024/11/23
#
# @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
# 
 
import asyncio
import logging
from pymodbus.server import StartAsyncTcpServer
from pymodbus.datastore import ModbusDeviceContext, ModbusServerContext, ModbusSparseDataBlock
import pymysql
from pymodbus.exceptions import ModbusIOException
from pymodbus.pdu import ExceptionResponse

import sys
import os

# Add common directory to path to import config_loader
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../common')))
from config_loader import get_config

# Load configuration
try:
    config = get_config()
    db_config = config['database']
    modbus_config = config['services']['modbustcp']

    # Server and Database Configuration
    SERVER_HOST = modbus_config['host']
    SERVER_PORT = modbus_config['port']

    # Database configuration
    DB_HOST = db_config['host']
    DB_USER = db_config['user']
    DB_PASSWORD = db_config['password']
    DB_NAME = db_config['db_name']
except KeyError as e:
    print(f"CRITICAL ERROR: High-level configuration key missing in modbustcp server: {e}")
    sys.exit(1)
except Exception as e:
    print(f"CRITICAL ERROR: Failed to load configuration in modbustcp server: {e}")
    sys.exit(1)

# Logging Configuration
logging.basicConfig(level=logging.DEBUG, # Set to DEBUG to see all logs
                    format='%(asctime)s - %(levelname)s - %(name)s - %(funcName)s - %(message)s')
_logger = logging.getLogger(__name__)

# ==============================================================================
# Class to manage database connection and queries
# ==============================================================================
class DatabaseManager:
    """
    Encapsulates database connection and query logic.
    For production, consider implementing a connection pool.
    """
    def __init__(self):
        _logger.debug("Initializing DatabaseManager instance.")
        self.conn = None

    def connect(self):
        """Initializes the database connection."""
        _logger.debug(f"Attempting to connect to database with config: HOST={DB_HOST}, USER={DB_USER}, DB={DB_NAME}")
        try:
            self.conn = pymysql.connect(
                host=DB_HOST, user=DB_USER, password=DB_PASSWORD,
                database=DB_NAME, cursorclass=pymysql.cursors.DictCursor
            )
            _logger.info("Database connection established successfully.")
        except pymysql.MySQLError as e:
            _logger.error(f"Error connecting to the database: {e}")
            raise

    def close(self):
        """Closes the database connection."""
        if self.conn and self.conn.open:
            _logger.info("Closing database connection.")
            self.conn.close()
        else:
            _logger.debug("Database connection was already closed or not open.")

    def _ensure_connection(self):
        """Ensures the database connection is active, reconnecting if necessary."""
        if not self.conn or not self.conn.open:
            _logger.warning("Database connection lost. Attempting to reconnect...")
            try:
                self.connect()
            except pymysql.MySQLError as e:
                _logger.error(f"Failed to reconnect to the database: {e}")
                raise
        else:
            _logger.debug("Database connection is active.")

    def execute_query(self, query, params=None):
        """Executes a single SQL query and returns the result."""
        _logger.debug(f"Preparing to execute query. Template: \"{query.strip()}\" | Params: {params}")
        self._ensure_connection()
        try:
            with self.conn.cursor() as cursor:
                cursor.execute(query, params)
                result = cursor.fetchall()
            _logger.debug(f"Query executed successfully. Fetched {len(result)} rows.")
            return result
        except pymysql.MySQLError as e:
            _logger.error(f"Error executing query: {e}", exc_info=True)
            raise

    def validate_address_range(self, module_id, start_address, count, io_type, hardware_access):
        """
        Validates an entire address range using the strict key definition:
        (module_id, logical_address, purpose='standard', io_type, hardware_access).
        """
        _logger.debug(f"Validating range for module {module_id}: start={start_address}, count={count}, type={io_type}, access={hardware_access}")
        end_address = start_address + count - 1
        
        # We query rtmirror joined with model_io_definition to ensure the point actually exists and is instantiated
        query = """
            SELECT COUNT(*) AS valid_count 
            FROM rtmirror r
            JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
            WHERE r.fk_module_id = %s
              AND mid.logical_address BETWEEN %s AND %s
              AND mid.io_type = %s
              AND mid.hardware_access = %s;
        """
        try:
            params = (module_id, start_address, end_address, io_type, hardware_access)
            result = self.execute_query(query, params)
            
            valid_count = result[0]['valid_count'] if result else 0
            if valid_count == count:
                _logger.info(f"Range validation successful: Found {valid_count}/{count} valid addresses.")
                return True
            else:
                _logger.warning(f"Range validation failed: Found {valid_count}/{count} valid addresses for the requested operation.")
                return False
        except pymysql.MySQLError as e:
            _logger.error(f"DB Error validating address range for module {module_id}: {e}")
            raise

    def get_values(self, module_id, start_address, quantity, io_type, hardware_access):
        """
        Gets values from the DB using strict mapping.
        """
        _logger.debug(f"Getting values for module {module_id}, start_addr={start_address}, quantity={quantity}, type={io_type}, access={hardware_access}")
        end_address = start_address + quantity - 1
        
        query = """
            SELECT r.value 
            FROM rtmirror r
            JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
            WHERE r.fk_module_id = %s
              AND mid.logical_address BETWEEN %s AND %s
              AND mid.io_type = %s
              AND mid.hardware_access = %s
            ORDER BY mid.logical_address ASC;
        """
        try:
            params = (module_id, start_address, end_address, io_type, hardware_access)
            result = self.execute_query(query, params)
            
            if len(result) != quantity:
                _logger.warning(f"Data integrity error: Requested {quantity} items but found {len(result)} in rtmirror table.")
                raise ModbusIOException("A requested address does not exist in the data table.", 0x02) # Illegal Data Address

            # Handle None values as 0
            return [row['value'] if row['value'] is not None else 0 for row in result]
        except pymysql.MySQLError as e:
            _logger.error(f"DB Error getting values for module {module_id}: {e}", exc_info=True)
            raise

    def write_values(self, module_id, start_address, values_to_write, io_type, hardware_access):
        """
        Writes values to 'required_value' in rtmirror using a SINGLE atomic UPDATE query with CASE.
        This ensures all values are written in a single transaction, preventing partial updates.
        """
        count = len(values_to_write)
        _logger.debug(f"Writing {count} values to module {module_id} starting at address {start_address}, type={io_type}, access={hardware_access}")

        if count == 0:
            return True

        # Pre-calculate addresses and sanitize values
        updates = []
        logical_addresses = []
        
        for offset, val in enumerate(values_to_write):
            addr = start_address + offset
            # Sanitize value: Boolean/Integer -> Integer (0/1 or raw int)
            # This ensures DB compatibility and avoids 'True'/'False' string issues
            try:
                safe_val = int(val)
            except (ValueError, TypeError):
                # Fallback for unexpected types, though pymodbus usually sends ints/bools
                _logger.warning(f"Could not cast value {val} to int. Using 0.")
                safe_val = 0
                
            updates.append((addr, safe_val))
            logical_addresses.append(addr)

        # Build the CASE statement parts: "WHEN ? THEN ?"
        # We use %s placeholder for pymysql parametrization
        case_parts = ["WHEN %s THEN %s"] * count
        case_stmt = f"CASE mid.logical_address {' '.join(case_parts)} END"
        
        # Build the IN clause placeholders: "%s, %s, ..."
        in_placeholders = ', '.join(['%s'] * count)

        # Build the final comprehensive query
        query = f"""
            UPDATE rtmirror r
            JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
            SET r.required_value = {case_stmt}
            WHERE r.fk_module_id = %s
              AND mid.io_type = %s
              AND mid.hardware_access = %s
              AND mid.logical_address IN ({in_placeholders});
        """

        # Construct the aligned parameters list
        params = []
        
        # 1. CASE parameters: [addr, val, addr, val, ...]
        for addr, val in updates:
            params.extend([addr, val])
        
        # 2. WHERE clause parameters: [module_id, io_type, hardware_access]
        params.extend([module_id, io_type, hardware_access])
        
        # 3. IN clause parameters: [addr, addr, ...]
        params.extend(logical_addresses)
        
        self._ensure_connection()
        try:
            with self.conn.cursor() as cursor:
                # Single execute call is inherently atomic for the statement
                cursor.execute(query, tuple(params))
                self.conn.commit() # Ensure data is persisted
                
                # Verify that we actually touched rows (optional but good for debug)
                # Note: rowcount returns matched rows or changed rows depending on config
                _logger.info(f"Successfully atomic wrote {count} values. DB Info: {cursor.rowcount} rows affected.")
                return True
        except pymysql.MySQLError as e:
            _logger.error(f"Error in atomic update transaction: {e}", exc_info=True)
            if self.conn.open: self.conn.rollback()
            raise ModbusIOException("Transaction to DB failed", 0x04) # Slave Device Failure

    def get_all_module_ids(self):
        """Retrieves all module IDs from the 'devices' table for dynamic configuration."""
        _logger.debug("Retrieving all module IDs from 'devices' table.")
        try:
            result = self.execute_query("SELECT module_id FROM devices ORDER BY module_id ASC;")
            if result:
                return [row['module_id'] for row in result]
            _logger.warning("No modules found in 'devices' table."); return []
        except pymysql.MySQLError as e:
            _logger.error(f"DB Error retrieving module IDs: {e}")
            raise

# ==============================================================================
# Custom Data Store class with strict validation
# ==============================================================================
class DatabaseDataStore(ModbusDeviceContext):
    """
    A Modbus datastore that uses a database backend and enforces strict permission checks.
    """
    def __init__(self, module_id, db_manager, **kwargs):
        _logger.debug(f"Initializing DatabaseDataStore for module_id: {module_id}")
        self.module_id = module_id
        self.db_manager = db_manager
        super().__init__(co=ModbusSparseDataBlock(), di=ModbusSparseDataBlock(), hr=ModbusSparseDataBlock(), ir=ModbusSparseDataBlock(), **kwargs)
        _logger.debug(f"ModbusDeviceContext initialized for module {module_id}.")

    def _get_op_params(self, function_code):
        """
        Determines (io_type, hardware_access) from Function Code to map strict Memory Areas.
        """
        # Mapping Modbus Function Codes to Internal DB Schema Keys
        if function_code in [1, 5, 15]:  # Coils (Read/Write Output Bits)
            return 'bit', 'readwrite'
        
        elif function_code == 2:  # Discrete Inputs (Read-Only Input Bits)
            return 'bit', 'readonly'
        
        elif function_code in [3, 6, 16]:  # Holding Registers (Read/Write Output Registers)
            return 'register', 'readwrite'
        
        elif function_code == 4:  # Input Registers (Read-Only Input Registers)
            return 'register', 'readonly'
        
        else:
            return None, None

    def validate(self, function_code, address, count=1):
        """
        Validates access using the DB manager.
        """
        _logger.debug(f"---[VALIDATION REQUEST]--- FC: {function_code}, Address: {address}, Count: {count}")
        io_type, hardware_access = self._get_op_params(function_code)
        
        if not io_type:
            _logger.warning(f"Unsupported function code for validation: {function_code}")
            return False
            
        return self.db_manager.validate_address_range(self.module_id, address, count, io_type, hardware_access)

    def getValues(self, function_code, address, count=1):
        """
        Gets values from the datastore.
        """
        _logger.info(f"---[GET REQUEST START]--- Module: {self.module_id}, FC: {function_code}, Address: {address}, Count: {count}")
        try:
            io_type, hardware_access = self._get_op_params(function_code)
            if not io_type:
                return ExceptionResponse(function_code, 0x01) # Illegal Function
                
            values = self.db_manager.get_values(self.module_id, address, count, io_type, hardware_access)
            return values
        except Exception as e:
            _logger.error(f"Unhandled error in getValues: {e}", exc_info=True)
            return ExceptionResponse(function_code, 0x04) # Slave Device Failure

    def setValues(self, function_code, address, values):
        """
        Sets values in the datastore.
        """
        _logger.info(f"---[SET REQUEST START]--- Module: {self.module_id}, FC: {function_code}, Address: {address}, Values: {values}")
        try:
            io_type, hardware_access = self._get_op_params(function_code)
            # Note: For writing (FC 5,6,15,16), the hardware_access is naturally 'readwrite'.
            
            if not io_type:
                 return ExceptionResponse(function_code, 0x01) # Illegal Function

            self.db_manager.write_values(self.module_id, address, values, io_type, hardware_access)
        except Exception as e:
            _logger.error(f"Unhandled error in setValues: {e}", exc_info=True)
            return ExceptionResponse(function_code, 0x04) # Slave Device Failure

# ==============================================================================
# Main server coroutine
# ==============================================================================
async def main():
    """Main entry point for running the Modbus TCP server."""
    _logger.info("Initializing Modbus TCP server...")
    db_manager = DatabaseManager()
    try:
        db_manager.connect()
        _logger.info("Retrieving active module IDs from database for dynamic configuration.")
        module_ids = db_manager.get_all_module_ids()
        if not module_ids:
            _logger.error("No active modules found in the database. Shutting down server.")
            return

        _logger.debug(f"Creating slave contexts for {len(module_ids)} modules: {module_ids}")
        slave_contexts = {i: DatabaseDataStore(i, db_manager) for i in module_ids}
        
        # For pymodbus >= 3.10 / 3.12+ (slaves -> devices)
        context = ModbusServerContext(devices=slave_contexts, single=False)

        
        address = (SERVER_HOST, SERVER_PORT)
        _logger.info(f"Server configured. Starting to listen on {address[0]}:{address[1]}")
        await StartAsyncTcpServer(context=context, address=address)
    except Exception as e:
        _logger.critical(f"A critical error occurred in the main server loop: {e}", exc_info=True)
    finally:
        _logger.info("Shutdown sequence initiated...")
        db_manager.close()
        _logger.info("Server shutdown complete.")

if __name__ == "__main__":
    _logger.info("--- Starting Modbus TCP Gateway Script ---")
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        _logger.info("Shutdown requested by user (Ctrl+C).")
    except Exception as e:
        _logger.critical(f"A fatal error occurred at the top level: {e}", exc_info=True)
    _logger.info("--- Modbus TCP Gateway Script Finished ---")