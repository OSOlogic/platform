# module.py
# Defines the primary data structure for storing the state of a device.

from typing import Dict, Union

class ModuleData:
    """
    Represents the complete in-memory state of a single device.
    This structure is now aligned with the 'devices' table in the database.
    """
    def __init__(self, module_id: int):
        # --- Primary Identifier ---
        self.id: int = module_id  # Primary key from the 'devices' table

        # --- Model permissions from the 'permission_map' table ---
        self.permissions: Dict[str, Dict[int, str]] = {'bit': {}, 'register': {}}

        # --- Configuration from the 'devices' table ---
        self.module_name: str = ""
        self.channel_type: str = ""
        self.protocol: str = ""
        self.connection_string: str = ""
        self.address_on_channel: str = ""
        self.fk_model_id: int = -1

        # --- Real-time state from the 'devices' table ---
        self.is_connected: bool = None  # Initialized to None to force an update on the first cycle
        self.last_seen: str = ""
        self.timeout_ms: int = 1000

        # --- Data from the 'model_config' table (joined via fk_model_id) ---
        self.model_name: str = ""
        self.bits_start: int = None
        self.registers_start: int = None

        # --- I/O Containers (simplified to 'bits' and 'registers') ---
        self.bits: Dict[int, int] = {}
        # Registers can now hold floats (net values), so we use Union[int, float]
        self.registers: Dict[int, Union[int, float]] = {}

        # --- Complete I/O Point Information for rich MQTT payloads ---
        # Each entry: { label, units, purpose, hardware_access, scale_factor, offset, raw_value, required_value, net_required_value }
        self.bits_info: Dict[int, Dict] = {}
        self.registers_info: Dict[int, Dict] = {}

        # --- Required values from MQTT writes ---
        self.required_bits: Dict[int, int] = {}
        self.required_registers: Dict[int, Union[int, float]] = {}

        # --- Visibility configuration for MQTT ---
        self.visibility_bits: Dict[int, Dict] = {}
        self.visibility_registers: Dict[int, Dict] = {}

        # --- Control Flags ---
        self.config_topic_needs_update: bool = True  # To refresh info topics on MQTT
        self.db_write_pending: bool = False         # To indicate pending writes to the DB
        self.data_is_prepared: bool = False         # To ensure I/O data has been read

        # --- Flags to track which specific values need updating ---
        self.bits_to_update: Dict[int, bool] = {}
        self.registers_to_update: Dict[int, bool] = {}
        self.bits_req_to_update: Dict[int, bool] = {}
        self.registers_req_to_update: Dict[int, bool] = {}

        # --- Storage of previous I/O maps (lists of addresses) for MQTT topic cleanup ---
        self.previous_io_maps = {'bits': [], 'registers': []}
        self.data_topics_to_delete: list = []

    def get_io_map_by_type(self, type_name: str) -> Dict:
        """Helper function to get the correct I/O dictionary by its string name."""
        return getattr(self, type_name, {})

    def __repr__(self):
        """String representation for easy debugging."""
        status = "CONNECTED" if self.is_connected else "DISCONNECTED" if self.is_connected is not None else "UNKNOWN"
        return f"<ModuleData id={self.id} name='{self.module_name}' model_id={self.fk_model_id} status={status}>"
