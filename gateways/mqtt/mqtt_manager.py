# mqtt_manager.py
# Manages all communication with the MQTT broker.

import json
import paho.mqtt.client as mqtt
import threading
import time
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

    # Map values to variables used in the class
    DB_CONFIG = {
        'host': db_config_raw['host'],
        'user': db_config_raw['user'],
        'password': db_config_raw['password'],
        'database': db_config_raw['db_name']
    }

    MQTT_BROKER_ADDRESS = mqtt_config['broker_address']
    MQTT_PORT = mqtt_config['port']
    MQTT_CLIENT_ID = mqtt_config['client_id']
    MQTT_PASSWORD = mqtt_config['password']

    MQTT_TOPIC_INFO_PREFIX = mqtt_config['topic_info_prefix']
    MQTT_TOPIC_DATA_PREFIX = mqtt_config['topic_data_prefix']
    MQTT_TOPIC_WRITE_PREFIX = mqtt_config['topic_write_prefix']
    MQTT_SUBSCRIBE_TOPIC = f"{MQTT_TOPIC_WRITE_PREFIX}/#"

    LOOP_INTERVAL_SECONDS = mqtt_config['loop_interval_seconds']
except KeyError as e:
    print(f"CRITICAL ERROR: High-level configuration key missing in mqtt_manager: {e}")
    sys.exit(1)
except Exception as e:
    print(f"CRITICAL ERROR: Failed to load configuration in mqtt_manager: {e}")
    sys.exit(1)
from module import ModuleData

class MqttManager:
    def __init__(self, modules: Dict[int, ModuleData], lock):
        self.modules = modules
        self.lock = lock
        self.client = mqtt.Client(client_id=MQTT_CLIENT_ID)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self._cleanup_complete = threading.Event()

    def connect(self):
        """Connects to the broker and starts the network loop in a separate thread."""
        try:
            self.client.username_pw_set(MQTT_CLIENT_ID, MQTT_PASSWORD)
            self.client.connect(MQTT_BROKER_ADDRESS, MQTT_PORT, 60)
            self.client.loop_start()
        except Exception as e:
            print(f"MQTT: Could not connect to broker: {e}", flush=True)
            raise

    def perform_initial_cleanup(self):
        """Cleans up all potentially stale data topics on the broker at startup."""
        # Use a generic wildcard based on the common structure (assumed to be starting with /plc)
        # or use the specific prefixes to be more surgical.
        wildcard ="/plc/#" 
        print(f"MQTT_CLEANUP: Subscribing to {wildcard} to find and clear EVERYTHING...", flush=True)
        self.client.subscribe(wildcard)
        time.sleep(2)  # Give the broker time to send all retained messages
        self._cleanup_complete.set()
        print(f"MQTT_CLEANUP: Unsubscribing from {wildcard}.", flush=True)
        self.client.unsubscribe(wildcard)

    def _on_connect(self, client, userdata, flags, rc):
        """Callback that runs when connecting to the broker."""
        if rc == 0:
            print("MQTT: Connection successful.", flush=True)
            client.subscribe(MQTT_SUBSCRIBE_TOPIC)
            print(f"MQTT: Subscribed to <{MQTT_SUBSCRIBE_TOPIC}>", flush=True)
        else:
            print(f"MQTT: Connection failed with code {rc} (5 = Not Authorized)", flush=True)

    def _on_message(self, client, userdata, msg):
        """Callback for processing all incoming MQTT messages."""
        topic = msg.topic
        payload = msg.payload.decode('utf-8')

        if not self._cleanup_complete.is_set():
            # Check if it belongs to any of our prefixes
            is_our_topic = (
                topic.startswith(MQTT_TOPIC_INFO_PREFIX) or 
                topic.startswith(MQTT_TOPIC_DATA_PREFIX) or 
                topic.startswith(MQTT_TOPIC_WRITE_PREFIX)
            )
            if is_our_topic:
                # Publish a null retained message to clear the topic
                client.publish(topic, payload=None, qos=1, retain=True)
            return

        if not topic.startswith(MQTT_TOPIC_WRITE_PREFIX):
            return

        topic_parts = topic.split('/')
        try:
            # Expected format: /plc/write/{module_id}/{io_type}/{address}
            # The split of a topic like "/a/b" results in ['', 'a', 'b'], so length is +1
            if len(topic_parts) != 6:
                raise ValueError(f"Invalid topic structure, expected 6 parts, got {len(topic_parts)}")

            module_id = int(topic_parts[3])
            io_type = topic_parts[4]  # 'bits' or 'registers'
            address = int(topic_parts[5])

            if not payload.strip():
                return  # Ignore empty payloads

            value = float(payload)

            with self.lock:
                module = self.modules.get(module_id)
                if not module or not module.is_connected:
                    return # Ignore

                if io_type not in ['bits', 'registers']:
                    raise ValueError(f"Invalid io_type '{io_type}'")

                req_map = module.get_io_map_by_type(f"required_{io_type}")
                if address not in req_map:
                    return # Ignore


                permission_type = 'bit' if io_type == 'bits' else 'register'
                # Default permission is 'readonly' if not found, to be safe
                permission = module.permissions[permission_type].get(address, 'readonly') 

                if permission != 'readwrite':
                    print(f"MQTT_WRITE_DENIED: Write permission denied for id={module_id}, {io_type}[{address}]. Model permission is '{permission}'.", flush=True)
                    return 

                if req_map.get(address) != value:
                    print(f"[WRITE REQ] Device: {module_id} | {io_type.upper()} Address: {address} | Requested: {value}", flush=True)
                    req_map[address] = value
                    module.get_io_map_by_type(f"{io_type}_req_to_update")[address] = True
                    module.db_write_pending = True
            
        except (ValueError, IndexError) as e:
            print(f"MQTT_WRITE_FAIL: Error processing topic '{topic}': {e}", flush=True)
            
    def publish_updates(self, sec_counter: int):
        """Publishes all pending configuration, data updates, and deletions."""
        with self.lock:
            # Use list() to create a copy, allowing safe deletion during iteration
            for module in list(self.modules.values()):
                self._publish_deletions(module)

                if module.config_topic_needs_update:
                    self._publish_config(module)
                    module.config_topic_needs_update = False
            
                if module.is_connected and module.data_is_prepared:
                    self._publish_data(module, sec_counter)

    def _publish_config(self, module: ModuleData):
        """Publishes the complete set of module information to the info topics."""
        print(f"[INFO UPDATE] Device: {module.id} | Sending full configuration update.", flush=True)

        # Clean up old data topics from a previous configuration
        for io_type, addr_list in module.previous_io_maps.items():
            for addr in addr_list:
                topic = f"{MQTT_TOPIC_DATA_PREFIX}/{module.id}/{io_type}/{addr}"
                self.client.publish(topic, payload=None, qos=1, retain=True)
        
        base_topic = f"{MQTT_TOPIC_INFO_PREFIX}/{module.id}"
        
        info_payloads = {
            "is_connected": "1" if module.is_connected else "0",
            "module_name": module.module_name,
            "fk_model_id": str(module.fk_model_id),
            "channel_type": module.channel_type,
            "protocol": module.protocol,
            "connection_string": module.connection_string,
            "address_on_channel": module.address_on_channel,
            "timeout_ms": str(module.timeout_ms),
            "last_seen": module.last_seen
        }
        
        for key, value in info_payloads.items():
            topic = f"{base_topic}/{key}"
            # For disconnected modules, only publish 'is_connected=0', clearing other fields.
            payload = value if (module.is_connected or key == "is_connected") else ""
            self.client.publish(topic, payload=payload, qos=1, retain=True)

    def _publish_data(self, module: ModuleData, sec_counter: int):
        """Publishes I/O values as structured JSON payloads based on the visibility rules."""
        import time as time_module
        current_timestamp = time_module.strftime('%Y-%m-%dT%H:%M:%S%z')
        
        for io_type in ["bits", "registers"]:
            io_map = module.get_io_map_by_type(io_type)
            update_map = module.get_io_map_by_type(f"{io_type}_to_update")
            vis_map = module.get_io_map_by_type(f"visibility_{io_type}")
            info_map = module.get_io_map_by_type(f"{io_type}_info")  # bits_info or registers_info

            for addr, value in io_map.items():
                vis_config = vis_map.get(addr, {})
                if vis_config.get('visibility') == 'visible':
                    mode = vis_config.get('visibility_mode', 'changes')
                    period_ms = vis_config.get('refresh_rate', 1000)
                    
                    publish_on_change = (mode == 'changes' and update_map.get(addr, False))
                    publish_periodic = (mode == 'periodically' and (sec_counter * 1000) % period_ms == 0)

                    if publish_on_change or publish_periodic:
                        topic = f"{MQTT_TOPIC_DATA_PREFIX}/{module.id}/{io_type}/{addr}"
                        
                        # Get io_point info from info_map
                        info = info_map.get(addr, {})
                        
                        # Format value based on io_type
                        if io_type == "bits":
                            formatted_value = int(value) if value is not None else None
                        else:
                            formatted_value = value  # Keep as float/int for registers
                        
                        # Build structured JSON payload with all relevant io_point information
                        payload_dict = {
                            "module_id": module.id,
                            "module_name": module.module_name,
                            "io_type": io_type[:-1],  # "bit" or "register"
                            "address": addr,
                            "label": info.get('label', f"{io_type[:-1]}_{module.id}_{addr}"),
                            "value": formatted_value,
                            "raw_value": info.get('raw_value'),
                            "units": info.get('units'),
                            "purpose": info.get('purpose', 'standard'),
                            "hardware_access": info.get('hardware_access', 'readonly'),
                            "scale_factor": info.get('scale_factor', 1.0),
                            "offset": info.get('offset', 0.0),
                            "timestamp": current_timestamp
                        }
                        
                        # Serialize to JSON with sorted keys for consistency
                        json_payload = json.dumps(payload_dict, sort_keys=True, ensure_ascii=False)
                        
                        self.client.publish(topic, payload=json_payload, qos=1, retain=True)
                        if publish_on_change:
                            print(f"[DATA UPDATE] Device: {module.id} | {io_type.upper()} Address: {addr} | Value: {formatted_value}", flush=True)
                            update_map[addr] = False

    def _publish_deletions(self, module: ModuleData):
        """Publishes null retained messages for topics marked for deletion."""
        if not module.data_topics_to_delete:
            return
        
        for topic in module.data_topics_to_delete:
            self.client.publish(topic, payload=None, qos=1, retain=True)
        
        module.data_topics_to_delete.clear()

    def disconnect(self):
        """Stops the network loop and disconnects from the broker."""
        self.client.loop_stop()
        self.client.disconnect()
        print("MQTT: Disconnected from broker.", flush=True)
