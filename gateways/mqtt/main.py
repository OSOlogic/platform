# 
# @file main.py
# @author Diego Arcos Sapena
# @brief PLC OsoLogic MQTT Gateway Service
# @version a-1.0.0
# @date 2024/11/23
#
# @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
# 

import time
import threading
from database_manager import DatabaseManager
from mqtt_manager import MqttManager, LOOP_INTERVAL_SECONDS

def main():
    """Main function that orchestrates the application."""
    shared_lock = threading.Lock()
    
    try:
        print("[INIT] Initializing DatabaseManager...", flush=True)
        db_manager = DatabaseManager()
        
        print("[INIT] Loading initial device configurations...", flush=True)
        # The modules dictionary is now dynamically loaded from the DB
        with shared_lock:
            shared_modules = db_manager.load_initial_devices()
        
        print("[INIT] Initializing MqttManager...", flush=True)
        mqtt_manager = MqttManager(shared_modules, shared_lock)
        mqtt_manager.connect()

        print("[INIT] Performing initial MQTT topic cleanup...", flush=True)
        mqtt_manager.perform_initial_cleanup()

    except Exception as e:
        print(f"CRITICAL: Initialization failed: {e}", flush=True)
        # Cleanup in case of partial initialization
        if 'db_manager' in locals() and db_manager.conn:
            db_manager.close()
        if 'mqtt_manager' in locals() and mqtt_manager.client.is_connected():
            mqtt_manager.disconnect()
        return

    print("\n--- PLC to MQTT Gateway Started ---", flush=True)
    print("Press Ctrl+C to stop.", flush=True)

    sec_counter = 0
    try:
        while True:
            print(f"\n[CYCLE {sec_counter}] >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", flush=True)

            # 1. Synchronize from the database (read operation)
            # This will now handle devices being added or removed at runtime.
            with shared_lock:
                print(f"[CYCLE {sec_counter}] PHASE: Syncing state from Database...", flush=True)
                db_manager.sync_from_database(shared_modules)
            
            # 2. Publish updates to MQTT based on the new state
            print(f"[CYCLE {sec_counter}] PHASE: Publishing updates to MQTT...", flush=True)
            mqtt_manager.publish_updates(sec_counter)
            
            # 3. Write pending changes from MQTT to the database
            with shared_lock:
                modules_to_write = [m for m in shared_modules.values() if m.db_write_pending]
                if modules_to_write:
                    print(f"[CYCLE {sec_counter}] PHASE: Writing pending changes to Database...", flush=True)
                    for module in modules_to_write:
                        db_manager.write_pending_changes(module)

            time.sleep(LOOP_INTERVAL_SECONDS)
            sec_counter = (sec_counter + 1) % 3600

    except KeyboardInterrupt:
        print("\n[SHUTDOWN] Stop signal received.", flush=True)
    finally:
        print("[SHUTDOWN] Closing connections...", flush=True)
        mqtt_manager.disconnect()
        db_manager.close()
        print("[SHUTDOWN] Application stopped cleanly.", flush=True)

if __name__ == "__main__":
    main()
