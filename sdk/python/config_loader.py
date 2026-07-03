import json
import os

def get_config():
    """
    Loads the central configuration from config/config.json.
    """
    # Try to find the config.json file in the workspace root
    # This logic assumes the structure: workspace/source/common/config_loader.py
    # And the config file is at: workspace/config/config.json
    
    current_dir = os.path.dirname(os.path.abspath(__file__))
    # workspace/source/common -> workspace/
    workspace_root = os.path.abspath(os.path.join(current_dir, "../../"))
    config_path = os.path.join(workspace_root, "config", "config.json")
    
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Configuration file not found at {config_path}. This file is mandatory.")

    with open(config_path, 'r') as f:
        return json.load(f)

def save_config(config_data):
    """
    Saves the central configuration to config/config.json.
    """
    current_dir = os.path.dirname(os.path.abspath(__file__))
    workspace_root = os.path.abspath(os.path.join(current_dir, "../../"))
    config_path = os.path.join(workspace_root, "config", "config.json")
    
    with open(config_path, 'w') as f:
        json.dump(config_data, f, indent=2)
    return True
