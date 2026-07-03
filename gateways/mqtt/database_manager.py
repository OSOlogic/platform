# database_manager.py
# Encapsulates all logic for interacting with the MySQL database.

import pymysql
from typing import Dict
import sys
import os

# Add common directory to path to import config_loader
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../common')))
from config_loader import get_config

# Load configuration
try:
    config = get_config()
    db_config_raw = config['database']
    mqtt_config = config['services']['mqtt']
    
    DB_CONFIG = {
        'host': db_config_raw['host'],
        'user': db_config_raw['user'],
        'password': db_config_raw['password'],
        'database': db_config_raw['db_name']
    }
    
    # Constants for MQTT cleanup logic
    MQTT_TOPIC_DATA_PREFIX = mqtt_config['topic_data_prefix']
    
except KeyError as e:
    print(f"CRITICAL ERROR: High-level configuration key missing in mqtt database_manager: {e}")
    sys.exit(1)
except Exception as e:
    print(f"CRITICAL ERROR: Failed to load configuration in mqtt database_manager: {e}")
    sys.exit(1)

from module import ModuleData

class DatabaseManager:
    def __init__(self):
        """Initializes the connection to the database."""
        self.conn = None
        try:
            # Create a copy of the config 
            db_connection_args = DB_CONFIG.copy()
            if 'collation' in db_connection_args:
                del db_connection_args['collation'] # pymysql might not support this arg directly
            
            # pymysql uses 'autocommit' as a keyword arg
            self.conn = pymysql.connect(
                host=db_connection_args['host'],
                user=db_connection_args['user'],
                password=db_connection_args['password'],
                database=db_connection_args['database'],
                autocommit=True,
                charset='utf8mb4'
            )
            print("DATABASE: Connection successful.", flush=True)

            # --- Load Operation Mode ONCE at startup (Consistency with Core) ---
            with self.conn.cursor() as cursor:
                 cursor.execute("SELECT operation_mode FROM plc_settings LIMIT 1")
                 row = cursor.fetchone()
                 self.current_mode = row[0] if row else 'execution'
                 print(f"DATABASE: Global Operation Mode initialized to: {self.current_mode}", flush=True)

        except pymysql.MySQLError as err:
            print(f"DATABASE: Error connecting: {err}", flush=True)
            raise

    def load_initial_devices(self) -> Dict[int, ModuleData]:
        """
        Queries the 'devices' table on startup to build the initial
        in-memory dictionary of all configured devices.
        """
        devices = {}
        # pymysql uses cursorclass for dict cursors
        cursor = self.conn.cursor(pymysql.cursors.DictCursor)
        try:
            cursor.execute("SELECT module_id FROM devices")
            rows = cursor.fetchall()
            for row in rows:
                module_id = row['module_id']
                devices[module_id] = ModuleData(module_id)
            print(f"DATABASE: Loaded {len(devices)} devices from the database.", flush=True)
            return devices
        except pymysql.MySQLError as err:
            print(f"DATABASE: Error loading initial devices: {err}", flush=True)
            return {}
        finally:
            cursor.close()

    def sync_from_database(self, modules: Dict[int, ModuleData]):
        """
        Synchronizes the state of all modules with the database, using a two-phase
        approach to ensure disconnected states are published before memory cleanup.
        """
        try:
            # ping(reconnect=True) is the standard pymysql way to ensure connection
            self.conn.ping(reconnect=True)
        except pymysql.MySQLError as err:
             print(f"DATABASE: Reconnection/Ping failed: {err}", flush=True)
             return

        # --- 1. Cleanup Phase: Remove modules that were marked as disconnected in a PREVIOUS cycle ---
        db_device_ids = self._get_all_device_ids_from_db()
        modules_to_delete = [
            module_id for module_id, module in modules.items()
            if not module.is_connected and module_id not in db_device_ids
        ]
        for module_id in modules_to_delete:
            print(f"DB_CLEANUP: Removing fully processed disconnected device ID {module_id} from memory.", flush=True)
            del modules[module_id]

        # --- 2. Sync Phase: Detect new changes for THIS cycle ---
        memory_device_ids = set(modules.keys())

        added_ids = db_device_ids - memory_device_ids
        for module_id in added_ids:
            print(f"DB_CONFIG_CHANGE: New device ID {module_id} found in DB. Adding to memory.", flush=True)
            modules[module_id] = ModuleData(module_id)

        removed_ids = memory_device_ids - db_device_ids
        for module_id in removed_ids:
            if module_id in modules and modules[module_id].is_connected:
                print(f"DB_CONFIG_CHANGE: Device ID {module_id} was removed from DB. Marking as disconnected.", flush=True)
                modules[module_id].is_connected = False
                modules[module_id].config_topic_needs_update = True

        for module_id in db_device_ids:
            if module_id in modules:
                module = modules[module_id]
                self._read_device_config(module)
                if module.is_connected:
                    self._read_device_io_data(module)

    def _get_all_device_ids_from_db(self) -> set:
        """Helper to get a set of all current module_ids from the devices table."""
        ids = set()
        cursor = self.conn.cursor()
        try:
            cursor.execute("SELECT module_id FROM devices")
            for (module_id,) in cursor:
                ids.add(module_id)
        except pymysql.MySQLError as err:
            print(f"DATABASE: Error getting device IDs: {err}", flush=True)
        finally:
            cursor.close()
        return ids

    def _read_device_config(self, module: ModuleData):
        """
        Reads device config using the new schema. 
        devices has instance configuration, model_config has protocol, devices has status.
        """
        cursor = self.conn.cursor(pymysql.cursors.DictCursor)
        try:
            # 1. Read Device Config + Model Info + Status
            query_device = """
                SELECT 
                    mc.module_id, mc.module_name, mc.channel AS channel_type, 
                    m.protocol, mc.connection_string, mc.address_on_channel, 
                    mc.fk_model_id, mc.timeout_ms,
                    m.model_name, d.is_connected, d.last_seen
                FROM devices mc
                LEFT JOIN model_config m ON mc.fk_model_id = m.model_id
                LEFT JOIN device_status d ON mc.module_id = d.fk_module_id
                WHERE mc.module_id = %s
            """
            cursor.execute(query_device, (module.id,))
            config_row = cursor.fetchone()

            # Handle case where device is removed from DB
            if not config_row:
                if module.is_connected:
                    module.is_connected = False
                    module.config_topic_needs_update = True
                return

            # 1b. Use cached Operation Mode (loaded at startup)
            # cursor.execute("SELECT operation_mode FROM plc_settings LIMIT 1") -> REMOVED
            # mode_row = cursor.fetchone() -> REMOVED
            
            # 2. Get IO Addresses directly from model_io_definition
            # This ensures we handle non-contiguous addresses (e.g. registers 100, 200, 300) correctly
            # instead of assuming range(0, count).
            bits_addrs = []
            registers_addrs = []
            fk_model_id = config_row.get('fk_model_id')
            
            if fk_model_id:
                # Build filter based on mode
                purpose_filter = ""
                if self.current_mode == 'execution':
                    purpose_filter = "AND purpose = 'standard'"
                else: # configuration
                    purpose_filter = "AND purpose IN ('secure_state', 'config')"

                query_addrs = f"""
                    SELECT io_type, logical_address
                    FROM model_io_definition 
                    WHERE fk_model_id = %s {purpose_filter}
                """
                cursor.execute(query_addrs, (fk_model_id,))
                for row in cursor.fetchall():
                    if row['io_type'] == 'bit':
                        bits_addrs.append(row['logical_address'])
                    elif row['io_type'] == 'register':
                        registers_addrs.append(row['logical_address'])
            
            # --- Detect Major Changes ---
            has_changed = False
            
            # Explicitly check for major changes first (connection status, model change)
            new_connected = bool(config_row.get('is_connected'))
            new_fk_model_id = config_row.get('fk_model_id')

            if module.is_connected != new_connected or module.fk_model_id != new_fk_model_id:
                has_changed = True
                print(f"DB_MAJOR_CHANGE: id={module.id} status changed. Conn: {module.is_connected}->{new_connected}, Model: {module.fk_model_id}->{new_fk_model_id}", flush=True)
                
                # 1. Store the old I/O address lists BEFORE clearing anything.
                old_bit_keys = list(module.bits.keys())
                old_register_keys = list(module.registers.keys())
                
                # 2. Update the model ID in memory
                module.fk_model_id = new_fk_model_id
                
                self._read_permission_map(module)

                # 3. Clear the I/O maps.
                for map_name in ['bits', 'registers', 'required_bits', 'required_registers', 'bits_info', 'registers_info']:
                    module.get_io_map_by_type(map_name).clear()

                # 4. Repopulate the maps with the actual addresses.
                if new_connected:
                    if bits_addrs:
                        module.bits = {addr: None for addr in bits_addrs}
                        module.required_bits = {addr: 0 for addr in bits_addrs}
                    if registers_addrs:
                        module.registers = {addr: None for addr in registers_addrs}
                        module.required_registers = {addr: 0 for addr in registers_addrs}

                # 5. Finally, assign the saved keys to previous_io_maps for the cleanup.
                module.previous_io_maps = {'bits': old_bit_keys, 'registers': old_register_keys}
            
            # Check for other minor changes if no major change was detected yet
            if not has_changed:
                fields_to_check = [
                    'module_name', 'channel_type', 'connection_string',
                    'address_on_channel', 'timeout_ms'
                ]
                for field in fields_to_check:
                    db_value = config_row.get(field)
                    mem_value = getattr(module, field)
                    # Safely compare by casting to string to handle None
                    if str(db_value) != str(mem_value):
                        print(f"[INFO DETECTED] Device: {module.id} | Field: '{field}' | Val: '{mem_value}' -> '{db_value}'", flush=True)
                        has_changed = True
                        break
            
            # --- Flag for MQTT update if any change was found ---
            if has_changed:
                module.config_topic_needs_update = True

            # --- Always synchronize the entire in-memory state ---
            all_fields_to_sync = [
                'module_name', 'channel_type', 'protocol', 'connection_string',
                'address_on_channel', 'fk_model_id', 'is_connected', 'timeout_ms',
                'last_seen', 'model_name'
            ]
            for field in all_fields_to_sync:
                db_value = config_row.get(field)
                if field == 'is_connected':
                    setattr(module, field, bool(db_value))
                elif field == 'last_seen':
                    setattr(module, field, str(db_value) if db_value is not None else "")
                else:
                    # Direct assignment, allowing None to be stored in memory (e.g. for timeout_ms)
                    setattr(module, field, db_value)

        except pymysql.MySQLError as err:
            print(f"DATABASE: Error reading config for device {module.id}: {err}", flush=True)
        finally:
            cursor.close()

    def _read_device_io_data(self, module: ModuleData):
        """Reads I/O values from rtmirror_complete using the new schema logic (NET values).
        Also populates bits_info/registers_info with complete io_point metadata for rich MQTT payloads.
        """
        cursor = self.conn.cursor(pymysql.cursors.DictCursor)
        try:
            # Extended query to get all relevant io_point information
            # Joins: rtmirror -> model_io_definition (for io metadata) -> module_io_config (for visibility and user config)
            query = """
                SELECT 
                    r.fk_io_definition_id as io_definition_id,
                    mid.io_type as type_value, 
                    mid.logical_address as address,
                    mid.purpose,
                    mid.hardware_access,
                    r.value as raw_value,
                    r.net_value as value,
                    r.required_value as raw_required_value,
                    r.net_required_value as required_value,
                    r.scale_factor,
                    r.offset,
                    mic.user_label as label,
                    mic.units,
                    mic.visibility, 
                    mic.visibility_mode, 
                    mic.refresh_rate
                FROM rtmirror r
                JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
                LEFT JOIN module_io_config mic ON r.fk_module_id = mic.fk_module_id AND r.fk_io_definition_id = mic.fk_io_definition_id
                WHERE r.fk_module_id = %s
            """
            cursor.execute(query, (module.id,))
            
            data_map = {'bit': {}, 'register': {}}
            for row in cursor.fetchall():
                data_map[row['type_value']][row['address']] = row

            for type_value_str, io_map in {'bit': module.bits, 'register': module.registers}.items():
                vis_map = module.get_io_map_by_type(f"visibility_{type_value_str}s")
                update_map = module.get_io_map_by_type(f"{type_value_str}s_to_update")
                req_map = module.get_io_map_by_type(f"required_{type_value_str}s")
                req_update_map = module.get_io_map_by_type(f"{type_value_str}s_req_to_update")
                info_map = module.get_io_map_by_type(f"{type_value_str}s_info")
                
                # Iterate over what we expect to have based on model definition
                for addr in list(io_map.keys()): 
                    row = data_map.get(type_value_str, {}).get(addr)
                    if row:
                        if io_map.get(addr) != row['value']:
                            io_map[addr] = row['value']
                            update_map[addr] = True
                        
                        db_required_value = row.get('required_value')
                        # Only update required map if we aren't currently intending to write to it from MQTT
                        if addr in req_map and req_map.get(addr) != db_required_value and not req_update_map.get(addr):
                            req_map[addr] = db_required_value
                        
                        old_visibility = vis_map.get(addr, {}).get('visibility')
                        new_visibility = row.get('visibility', 'visible') # Default to visible if no config
                        
                        if old_visibility == 'visible' and new_visibility != 'visible':
                            module.data_topics_to_delete.append(f"{MQTT_TOPIC_DATA_PREFIX}/{module.id}/{type_value_str}s/{addr}")
                        
                        vis_map[addr] = {
                            'visibility': new_visibility, 
                            'visibility_mode': row.get('visibility_mode', 'changes'), 
                            'refresh_rate': row.get('refresh_rate', 1000)
                        }
                        
                        # Store complete io_point information for rich MQTT payloads
                        info_map[addr] = {
                            'label': row.get('label') or f"{type_value_str}_{module.id}_{addr}",
                            'units': row.get('units'),
                            'purpose': row.get('purpose', 'standard'),
                            'hardware_access': row.get('hardware_access', 'readonly'),
                            'scale_factor': float(row.get('scale_factor', 1.0)),
                            'offset': float(row.get('offset', 0.0)),
                            'raw_value': row.get('raw_value')
                        }
            
            module.data_is_prepared = True
        except pymysql.MySQLError as err:
            print(f"DATABASE: Error reading I/O for device {module.id}: {err}", flush=True)
        finally:
            cursor.close()

    def write_pending_changes(self, module: ModuleData):
        """Writes required values from MQTT to the rtmirror table (net_required_value)."""
        if not module.db_write_pending or not module.is_connected:
            if module.db_write_pending:
                module.db_write_pending = False
            return

        cursor = self.conn.cursor()
        try:
            updates = []
            for io_type in ['bits', 'registers']:
                req_map = module.get_io_map_by_type(f"required_{io_type}")
                req_update_map = module.get_io_map_by_type(f"{io_type}_req_to_update")
                type_value_str = 'bit' if io_type == 'bits' else 'register'
                
                for addr, needs_update in list(req_update_map.items()):
                    if needs_update:
                        # Params: (required_value, fk_module_id, logical_address, io_type)
                        updates.append((req_map[addr], module.id, addr, type_value_str))
                        req_update_map[addr] = False

            if updates:
                # Update using the mapping from model_io_definition (mid) to find the correct rtmirror entry (r)
                # We write to net_required_value so the trigger calculates the raw required_value
                query = """
                    UPDATE rtmirror r
                    JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
                    SET r.net_required_value = %s
                    WHERE r.fk_module_id = %s 
                      AND mid.logical_address = %s 
                      AND mid.io_type = %s
                """
                cursor.executemany(query, updates)
                self.conn.commit()
                print(f"DB_WRITE_EXEC: Wrote {len(updates)} changes for device id={module.id}", flush=True)
            
            module.db_write_pending = False
        except pymysql.MySQLError as err:
            self.conn.rollback()
            print(f"DATABASE: Transaction failed for device {module.id}: {err}", flush=True)
        finally:
            cursor.close()

    def _read_permission_map(self, module: ModuleData):
        """
        Reads permissions directly from model_io_definition (hardware_access).
        """
        module.permissions['bit'].clear()
        module.permissions['register'].clear()

        if not module.fk_model_id or module.fk_model_id <= 0:
            return

        cursor = self.conn.cursor(pymysql.cursors.DictCursor)
        try:
            query = """
                SELECT io_type, logical_address, hardware_access 
                FROM model_io_definition 
                WHERE fk_model_id = %s
            """
            cursor.execute(query, (module.fk_model_id,))
            
            for row in cursor.fetchall():
                permission_str = row['hardware_access'] # 'readonly' or 'readwrite'
                # Convert 'bit'/'register' to match our internal map keys
                module.permissions[row['io_type']][row['logical_address']] = permission_str

        except pymysql.MySQLError as err:
            print(f"DATABASE: Error reading permissions for model {module.fk_model_id}: {err}", flush=True)
        finally:
            cursor.close()

    def close(self):
        if self.conn and self.conn.open:
            self.conn.close()
            print("DATABASE: Connection closed.", flush=True)
