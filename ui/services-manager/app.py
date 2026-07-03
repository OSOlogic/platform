#!/usr/bin/env python3
"""
PLC OSOlogic Service Manager - Web Interface
Web interface to control the PLC services
"""

from flask import Flask, render_template_string, jsonify, request
import subprocess
import os
import sys

# Get the directory where this script is located
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STATIC_DIR = os.path.join(SCRIPT_DIR, 'static')

# Add common directory to path to import config_loader
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../common')))
from config_loader import get_config, save_config

# Load configuration
try:
    config = get_config()
    services_config = config['services']
    gui_config = services_config['gui']
    nodered_config = services_config['nodered']

    # Get external URLs from configuration
    NODERED_URL = nodered_config['external_url']
    GUI_URL = gui_config['external_url']
except KeyError as e:
    print(f"CRITICAL ERROR: High-level configuration key missing in services_manager: {e}")
    sys.exit(1)
except Exception as e:
    print(f"CRITICAL ERROR: Failed to load configuration in services_manager: {e}")
    sys.exit(1)

# Create static directory if it doesn't exist
os.makedirs(STATIC_DIR, exist_ok=True)

app = Flask(__name__, static_folder=STATIC_DIR)

# Configuration of the services
SERVICES = {
    'core': {
        'name': 'plc_osologic-core',
        'display_name': 'PLC Core',
        'description': 'Main PLC application',
        'icon': '🔧'
    },
    'mqtt': {
        'name': 'plc_osologic-mqtt',
        'display_name': 'MQTT Gateway',
        'description': 'Gateway MQTT for IoT communication',
        'icon': '📡'
    },
    'modbustcp': {
        'name': 'plc_osologic-modbustcp',
        'display_name': 'Modbus TCP Gateway',
        'description': 'Modbus TCP server',
        'icon': '🔌'
    },
    'nodered': {
        'name': 'nodered',
        'display_name': 'Node-RED',
        'description': 'Visual programming for IoT flows',
        'icon': '🔴',
        'external_url': NODERED_URL
    },
    'gui': {
        'name': 'plc_osologic-gui',
        'display_name': 'Graphic User Interface',
        'description': 'PLC Management Interface',
        'icon': '🗄️',
        'external_url': GUI_URL
    }
}

# HTML Template with modern design
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PLC OsoLogic - Management Console</title>
    <!-- Ace Editor for Premium JSON Editing -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/ace.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/mode-json.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/theme-monokai.min.js"></script>
    <style>
        :root {
            --bg-primary: #0a0a1a;
            --bg-secondary: #12122b;
            --bg-card: rgba(22, 33, 62, 0.8);
            --accent: #00d9ff;
            --accent-green: #00ff88;
            --accent-red: #ff4757;
            --accent-yellow: #ffa502;
            --text-primary: #ffffff;
            --text-secondary: #a0a0a0;
            --border-radius: 20px;
            --shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
            --glass-border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
            background: radial-gradient(circle at top right, #1a1a3e, var(--bg-primary));
            min-height: 100vh;
            color: var(--text-primary);
            overflow-x: hidden;
        }
        
        /* Custom Scrollbar */
        ::-webkit-scrollbar { width: 8px; }
        ::-webkit-scrollbar-track { background: var(--bg-primary); }
        ::-webkit-scrollbar-thumb { background: #333; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #444; }

        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 2rem;
        }
        
        /* Navigation Tabs */
        .nav-container {
            display: flex;
            justify-content: center;
            margin-bottom: 3rem;
            position: relative;
        }

        .nav-tabs {
            display: flex;
            background: rgba(255, 255, 255, 0.05);
            padding: 0.5rem;
            border-radius: 50px;
            backdrop-filter: blur(10px);
            border: var(--glass-border);
            gap: 0.5rem;
        }

        .nav-tab {
            padding: 0.8rem 2rem;
            border-radius: 40px;
            cursor: pointer;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            font-weight: 600;
            color: var(--text-secondary);
            display: flex;
            align-items: center;
            gap: 0.5rem;
            border: none;
            background: transparent;
        }

        .nav-tab:hover {
            color: var(--text-primary);
            background: rgba(255, 255, 255, 0.05);
        }

        .nav-tab.active {
            background: var(--accent);
            color: #000;
            box-shadow: 0 0 20px rgba(0, 217, 255, 0.4);
        }

        header {
            text-align: center;
            margin-bottom: 2.5rem;
        }
        
        .logo-container {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 1.5rem;
            margin-bottom: 0.5rem;
        }
        
        .logo-image {
            width: 120px;
            height: 120px;
            border-radius: 24px;
            object-fit: contain;
            background: rgba(255, 255, 255, 0.08);
            padding: 12px;
            border: 2px solid rgba(0, 217, 255, 0.3);
            box-shadow: 0 8px 30px rgba(0, 217, 255, 0.2);
        }
        
        h1 {
            font-size: 3.5rem;
            font-weight: 800;
            background: linear-gradient(90deg, #fff, var(--accent));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            letter-spacing: -1px;
        }
        
        .subtitle {
            color: var(--text-secondary);
            font-size: 1.1rem;
            margin-top: 0.2rem;
            letter-spacing: 2px;
            text-transform: uppercase;
        }

        /* View Sections */
        .view-section {
            display: none;
            animation: fadeIn 0.5s ease-out;
        }

        .view-section.active {
            display: block;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
        }
        
        /* Services Grid */
        .services-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
            gap: 2rem;
        }
        
        .service-card {
            background: var(--bg-card);
            border-radius: var(--border-radius);
            padding: 2rem;
            backdrop-filter: blur(12px);
            border: var(--glass-border);
            transition: all 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            position: relative;
            overflow: hidden;
        }

        .service-card::before {
            content: '';
            position: absolute;
            top: 0; left: 0; width: 100%; height: 100%;
            background: linear-gradient(45deg, transparent, rgba(0, 217, 255, 0.03), transparent);
            pointer-events: none;
        }
        
        .service-card:hover {
            transform: translateY(-8px) scale(1.02);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
            border-color: rgba(0, 217, 255, 0.3);
        }
        
        .service-header {
            display: flex;
            align-items: center;
            gap: 1.2rem;
            margin-bottom: 1.5rem;
        }
        
        .service-icon {
            font-size: 2.8rem;
            filter: drop-shadow(0 0 10px rgba(0, 217, 255, 0.3));
        }
        
        .service-title {
            font-size: 1.5rem;
            font-weight: 700;
            color: #fff;
        }
        
        .service-description {
            color: var(--text-secondary);
            font-size: 0.95rem;
            margin-bottom: 2rem;
            line-height: 1.6;
        }
        
        .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 0.6rem;
            padding: 0.5rem 1rem;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 30px;
            margin-bottom: 2rem;
            font-size: 0.9rem;
            font-weight: 600;
        }
        
        .indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
        }
        
        .indicator.running { background: var(--accent-green); box-shadow: 0 0 12px var(--accent-green); animation: pulse 2s infinite; }
        .indicator.stopped { background: var(--accent-red); box-shadow: 0 0 12px var(--accent-red); }
        .indicator.unknown { background: var(--accent-yellow); box-shadow: 0 0 12px var(--accent-yellow); }
        
        @keyframes pulse {
            0% { transform: scale(1); opacity: 1; }
            50% { transform: scale(1.3); opacity: 0.6; }
            100% { transform: scale(1); opacity: 1; }
        }

        .actions {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 1rem;
        }
        
        .btn {
            padding: 0.9rem 1.2rem;
            border: none;
            border-radius: 12px;
            font-size: 0.9rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.3s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.6rem;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .btn:disabled { opacity: 0.4; cursor: not-allowed; filter: grayscale(1); }
        
        .btn-start { background: linear-gradient(135deg, #00ff88, #00d9ff); color: #000; }
        .btn-start:hover:not(:disabled) { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0, 255, 136, 0.3); }
        
        .btn-stop { background: linear-gradient(135deg, #ff4757, #ff6b81); color: #fff; }
        .btn-stop:hover:not(:disabled) { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(255, 71, 87, 0.3); }
        
        .btn-restart { background: rgba(255, 165, 2, 0.15); color: var(--accent-yellow); border: 1px solid var(--accent-yellow); }
        .btn-restart:hover:not(:disabled) { background: var(--accent-yellow); color: #000; transform: translateY(-2px); }
        
        .btn-logs { background: rgba(255, 255, 255, 0.05); color: #fff; border: 1px solid rgba(255, 255, 255, 0.1); }
        .btn-logs:hover:not(:disabled) { background: rgba(255, 255, 255, 0.1); }

        .btn-external { background: linear-gradient(135deg, #6c5ce7, #a29bfe); color: #fff; grid-column: span 2; }
        .btn-external:hover:not(:disabled) { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(108, 92, 231, 0.3); }

        .toggle-wrapper {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 12px;
            background: rgba(255, 255, 255, 0.03);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 12px;
            padding: 0.6rem;
            grid-column: span 2;
        }
        
        .switch {
            position: relative;
            display: inline-block;
            width: 46px;
            height: 24px;
        }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0; left: 0; right: 0; bottom: 0;
            background-color: rgba(255, 255, 255, 0.1);
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 3px;
            bottom: 3px;
            background-color: var(--text-secondary);
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: var(--accent-green);
        }
        input:checked + .slider:before {
            transform: translateX(22px);
            background-color: #000;
        }
        .toggle-label {
            font-size: 0.85rem;
            font-weight: 700;
            letter-spacing: 1px;
        }

        /* Configuration View */
        .config-container {
            background: var(--bg-card);
            border-radius: var(--border-radius);
            padding: 2.5rem;
            border: var(--glass-border);
            max-width: 900px;
            margin: 0 auto;
        }

        .config-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 2rem;
        }

        .config-title-group h2 {
            font-size: 1.8rem;
            margin-bottom: 0.5rem;
        }

        .config-title-group p {
            color: var(--text-secondary);
            font-size: 0.9rem;
        }

        #json-editor {
            height: 500px;
            border-radius: 12px;
            font-size: 14px;
            border: 1px solid rgba(255, 255, 255, 0.1);
            margin-bottom: 2rem;
            box-shadow: inset 0 4px 10px rgba(0,0,0,0.5);
        }

        .config-actions {
            display: flex;
            justify-content: flex-end;
            gap: 1.5rem;
        }

        .btn-save {
            background: var(--accent-green);
            color: #000;
            padding: 1rem 2.5rem;
            font-size: 1rem;
            box-shadow: 0 4px 15px rgba(0, 255, 136, 0.2);
        }

        .btn-save:hover {
            transform: scale(1.05);
            box-shadow: 0 8px 25px rgba(0, 255, 136, 0.4);
        }

        /* Modal & Toast Styles Remain similar but polished */
        .modal {
            display: none;
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(0, 0, 0, 0.85);
            z-index: 2000;
            align-items: center;
            justify-content: center;
            backdrop-filter: blur(8px);
        }
        
        .modal.show { display: flex; }
        .modal-content {
            background: var(--bg-secondary);
            border-radius: 24px;
            width: 90%;
            max-width: 900px;
            max-height: 85vh;
            display: flex;
            flex-direction: column;
            border: var(--glass-border);
            box-shadow: 0 30px 60px rgba(0,0,0,0.5);
        }
        
        .modal-header {
            padding: 1.5rem 2rem;
            border-bottom: 1px solid rgba(255,255,255,0.05);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .modal-body { padding: 1.5rem; overflow-y: auto; flex: 1; }
        .log-content {
            font-family: 'Fira Code', 'Monaco', monospace;
            font-size: 0.85rem;
            line-height: 1.6;
            white-space: pre-wrap;
            color: #d1d1d1;
            background: #000;
            padding: 1.5rem;
            border-radius: 12px;
        }
        
        .toast {
            position: fixed;
            bottom: 2rem;
            right: 2rem;
            padding: 1.2rem 2rem;
            background: #1a1a2e;
            border-radius: 15px;
            border-left: 5px solid var(--accent);
            box-shadow: 0 10px 40px rgba(0,0,0,0.5);
            transform: translateX(200%);
            transition: transform 0.5s cubic-bezier(0.68, -0.55, 0.265, 1.55);
            z-index: 3000;
            font-weight: 600;
        }
        
        .toast.show { transform: translateX(0); }
        .toast.success { border-left-color: var(--accent-green); }
        .toast.error { border-left-color: var(--accent-red); }

        .global-actions {
            margin-top: 4rem;
            padding: 2.5rem;
            background: var(--bg-card);
            border-radius: var(--border-radius);
            border: var(--glass-border);
            text-align: center;
        }

        .global-title {
            font-size: 1.2rem;
            color: var(--text-secondary);
            margin-bottom: 1.5rem;
            text-transform: uppercase;
            letter-spacing: 2px;
        }

        .global-buttons {
            display: flex;
            gap: 1.5rem;
            justify-content: center;
            flex-wrap: wrap;
        }

        .global-buttons .btn { min-width: 200px; }

    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="logo-container">
                <img src="/static/logo.png" alt="OsoLogic Logo" class="logo-image" onerror="this.style.display='none'">
                <h1>OSOLOGIC</h1>
            </div>
            <p class="subtitle">System Intelligence Manager</p>
        </header>

        <div class="nav-container">
            <div class="nav-tabs">
                <button class="nav-tab active" onclick="switchView('services')">
                    <span>⚡</span> Services
                </button>
                <button class="nav-tab" onclick="switchView('config')">
                    <span>⚙️</span> Configuration
                </button>
            </div>
        </div>
        
        <!-- Services View -->
        <div id="view-services" class="view-section active">
            <div class="services-grid" id="services-grid">
                <!-- Services will be loaded dynamically -->
            </div>
            
            <div class="global-actions">
                <p class="global-title">Global Control</p>
                <div class="global-buttons">
                    <button class="btn btn-start" onclick="controlAll('start')">
                        ▶️ Start All
                    </button>
                    <button class="btn btn-stop" onclick="controlAll('stop')">
                        ⏹️ Stop All
                    </button>
                    <button class="btn btn-restart" onclick="controlAll('restart')">
                        🔄 Restart All
                    </button>
                    <a href="javascript:void(0)" onclick="window.open('http://' + window.location.hostname + '/phpmyadmin/', '_blank')" class="btn btn-external" style="text-decoration: none;">
                        🐘 phpMyAdmin
                    </a>
                </div>
            </div>
        </div>

        <!-- Configuration View -->
        <div id="view-config" class="view-section">
            <div class="config-container">
                <div class="config-header">
                    <div class="config-title-group">
                        <h2>System Configuration</h2>
                        <p>Modify core JSON parameters (config/config.json)</p>
                    </div>
                    <div>
                        <span id="config-status" style="font-size: 0.8rem; opacity: 0.7;">Autosave off</span>
                    </div>
                </div>
                
                <div id="json-editor"></div>
                
                <div class="config-actions">
                    <button class="btn btn-logs" onclick="loadConfig()">
                        RESET CHANGES
                    </button>
                    <button class="btn btn-save" onclick="saveConfigData()">
                        PROCEED & SAVE CONFIG
                    </button>
                </div>
            </div>
        </div>
    </div>
    
    <div id="toast" class="toast"></div>
    
    <div id="logsModal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h3 id="logsTitle">Service Logs</h3>
                <div>
                    <span id="logStatus" style="font-size: 0.8rem; color: var(--accent-green); margin-right: 1.5rem; display: none;">● LIVE FEED</span>
                    <button style="background:none; border:none; color:#fff; font-size:2rem; cursor:pointer;" onclick="closeLogsModal()">&times;</button>
                </div>
            </div>
            <div class="modal-body">
                <pre class="log-content" id="logsContent">Loading trace stream...</pre>
            </div>
        </div>
    </div>
    
    <script>
        const services = {{ services|tojson|safe }};
        let editor = null;
        
        // Initialize Ace Editor
        window.onload = function() {
            editor = ace.edit("json-editor");
            editor.setTheme("ace/theme/monokai");
            editor.session.setMode("ace/mode/json");
            editor.setShowPrintMargin(false);
            editor.setOptions({
                enableBasicAutocompletion: true,
                enableLiveAutocompletion: true,
                fontSize: "14px",
                tabSize: 2,
                useSoftTabs: true
            });
            
            // Initial load
            fetchStatus();
            loadConfig();
            
            setInterval(fetchStatus, 5000);
        };

        function switchView(view) {
            document.querySelectorAll('.view-section').forEach(s => s.classList.remove('active'));
            document.querySelectorAll('.nav-tab').forEach(t => t.classList.remove('active'));
            
            document.getElementById(`view-${view}`).classList.add('active');
            event.currentTarget.classList.add('active');
            
            if (view === 'config') {
                editor.resize();
            }
        }
        
        async function loadConfig() {
            try {
                const response = await fetch('/api/config');
                const data = await response.json();
                editor.setValue(JSON.stringify(data, null, 2), -1);
            } catch (error) {
                showToast('Failed to load config', 'error');
            }
        }

        async function saveConfigData() {
            try {
                const content = editor.getValue();
                let jsonData;
                
                try {
                    jsonData = JSON.parse(content);
                } catch (e) {
                    showToast('Invalid JSON format!', 'error');
                    return;
                }

                showToast('Applying configuration...', 'info');
                
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(jsonData)
                });
                
                const data = await response.json();
                if (data.success) {
                    showToast('Configuration saved successfully!', 'success');
                } else {
                    showToast(data.message || 'Error saving config', 'error');
                }
            } catch (error) {
                showToast('Network error while saving', 'error');
            }
        }

        function renderServices(statuses) {
            const grid = document.getElementById('services-grid');
            grid.innerHTML = '';
            
            for (const [key, service] of Object.entries(services)) {
                const status = statuses[key] || { status: 'unknown', enabled: false };
                const isRunning = status.status === 'running';
                const statusClass = isRunning ? 'running' : (status.status === 'stopped' ? 'stopped' : 'unknown');
                const statusText = isRunning ? 'Active & Healthy' : (status.status === 'stopped' ? 'Service Stopped' : 'Status Unknown');
                
                const externalButton = service.external_url 
                    ? `<a href="${service.external_url}" target="_blank" class="btn btn-external">
                           🌐 OPEN WEB INTERFACE
                       </a>`
                    : '';
                
                const card = document.createElement('div');
                card.className = 'service-card';
                card.innerHTML = `
                    <div class="service-header">
                        <span class="service-icon">${service.icon}</span>
                        <span class="service-title">${service.display_name}</span>
                    </div>
                    <p class="service-description">${service.description}</p>
                    <div class="status-badge">
                        <span class="indicator ${statusClass}"></span>
                        <span class="status-text" style="color: ${isRunning ? 'var(--accent-green)' : 'var(--text-secondary)'}">${statusText}</span>
                    </div>
                    <div class="actions">
                        <button class="btn btn-start" onclick="controlService('${key}', 'start')" ${isRunning ? 'disabled' : ''}>
                            ▶️ Start
                        </button>
                        <button class="btn btn-stop" onclick="controlService('${key}', 'stop')" ${!isRunning ? 'disabled' : ''}>
                            ⏹️ Stop
                        </button>
                        <button class="btn btn-restart" onclick="controlService('${key}', 'restart')">
                            🔄 Restart
                        </button>
                        <button class="btn btn-logs" onclick="showLogs('${key}')">
                            📋 Logs
                        </button>
                        <div class="toggle-wrapper">
                            <label class="switch" title="Auto-start on boot">
                                <input type="checkbox" ${status.enabled ? 'checked' : ''} onchange="controlService('${key}', this.checked ? 'enable' : 'disable')">
                                <span class="slider"></span>
                            </label>
                            <span class="toggle-label" style="color: ${status.enabled ? 'var(--accent-green)' : 'var(--accent-red)'}">
                                ${status.enabled ? 'ENABLED' : 'DISABLED'}
                            </span>
                        </div>
                        ${externalButton}
                    </div>
                `;
                grid.appendChild(card);
            }
        }
        
        async function fetchStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                renderServices(data);
            } catch (error) { console.error('Error fetching status:', error); }
        }
        
        async function controlService(serviceKey, action) {
            try {
                showToast(`${action.toUpperCase()} in progress...`, 'info');
                const response = await fetch(`/api/service/${serviceKey}/${action}`, { method: 'POST' });
                const data = await response.json();
                if (data.success) {
                    showToast(data.message, 'success');
                    setTimeout(fetchStatus, 1000);
                } else {
                    showToast(data.message, 'error');
                }
            } catch (error) { showToast('Connection failure', 'error'); }
        }
        
        async function controlAll(action) {
            showToast(`Global ${action} initiated`, 'info');
            for (const key of Object.keys(services)) {
                await controlService(key, action);
            }
        }
        
        let logInterval = null;
        let currentLogService = null;

        async function showLogs(serviceKey) {
            const modal = document.getElementById('logsModal');
            const title = document.getElementById('logsTitle');
            const content = document.getElementById('logsContent');
            const logStatus = document.getElementById('logStatus');
            
            currentLogService = serviceKey;
            title.textContent = `${services[serviceKey].display_name} Traces`;
            content.textContent = 'Pulling log stream...';
            modal.classList.add('show');
            logStatus.style.display = 'inline';
            
            fetchLogs();
            if (logInterval) clearInterval(logInterval);
            logInterval = setInterval(fetchLogs, 2500);
        }

        async function fetchLogs() {
            if (!currentLogService) return;
            const content = document.getElementById('logsContent');
            const body = content.parentElement;
            try {
                const response = await fetch(`/api/service/${currentLogService}/logs`);
                const data = await response.json();
                const isAtBottom = body.scrollHeight - body.clientHeight <= body.scrollTop + 50;
                content.textContent = data.logs || 'No output detected';
                if (isAtBottom) body.scrollTop = body.scrollHeight;
            } catch (error) { console.error('Error loading logs:', error); }
        }
        
        function closeLogsModal() {
            document.getElementById('logsModal').classList.remove('show');
            if (logInterval) clearInterval(logInterval);
            currentLogService = null;
        }
        
        function showToast(message, type = 'info') {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.className = 'toast show ' + type;
            setTimeout(() => { toast.classList.remove('show'); }, 4000);
        }
        
        document.addEventListener('keydown', (e) => { if (e.key === 'Escape') closeLogsModal(); });
    </script>
</body>
</html>
"""

def run_systemctl(command, service_name):
    """Run a systemctl command and return the result"""
    try:
        result = subprocess.run(
            ['systemctl', command, service_name],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result.returncode == 0, result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return False, "Timeout executing command"
    except Exception as e:
        return False, str(e)

def get_service_status(service_name):
    """Get the status of a service"""
    try:
        # Check if it is active
        result = subprocess.run(
            ['systemctl', 'is-active', service_name],
            capture_output=True,
            text=True
        )
        status = result.stdout.strip()
        
        # Check if it is enabled
        result_enabled = subprocess.run(
            ['systemctl', 'is-enabled', service_name],
            capture_output=True,
            text=True
        )
        enabled = result_enabled.stdout.strip() == 'enabled'
        
        return {
            'status': 'running' if status == 'active' else 'stopped',
            'enabled': enabled
        }
    except Exception as e:
        return {'status': 'unknown', 'enabled': False}

def get_service_logs(service_name, lines=150):
    """Get the last logs of a service (tripled limit)"""
    try:
        result = subprocess.run(
            ['journalctl', '-u', service_name, '-n', str(lines), '--no-pager'],
            capture_output=True,
            text=True,
            timeout=10
        )
        return result.stdout
    except Exception as e:
        return f"Error getting logs: {e}"

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE, services=SERVICES)

@app.route('/api/status')
def api_status():
    statuses = {}
    for key, service in SERVICES.items():
        statuses[key] = get_service_status(service['name'])
    return jsonify(statuses)

@app.route('/api/service/<service_key>/<action>', methods=['POST'])
def api_service_action(service_key, action):
    if service_key not in SERVICES:
        return jsonify({'success': False, 'message': 'Service not found'})
    
    service_name = SERVICES[service_key]['name']
    display_name = SERVICES[service_key]['display_name']
    
    if action not in ['start', 'stop', 'restart', 'enable', 'disable']:
        return jsonify({'success': False, 'message': 'Invalid action'})
    
    success, output = run_systemctl(action, service_name)
    
    action_messages = {
        'start': 'started',
        'stop': 'stopped',
        'restart': 'restarted',
        'enable': 'enabled for automatic start',
        'disable': 'disabled for automatic start'
    }
    
    if success:
        return jsonify({
            'success': True,
            'message': f'{display_name} {action_messages.get(action, action)}'
        })
    else:
        return jsonify({
            'success': False,
            'message': f'Error: {output}'
        })

@app.route('/api/service/<service_key>/logs')
def api_service_logs(service_key):
    if service_key not in SERVICES:
        return jsonify({'logs': 'Service not found'})
    
    service_name = SERVICES[service_key]['name']
    logs = get_service_logs(service_name)
    return jsonify({'logs': logs})

@app.route('/api/config', methods=['GET', 'POST'])
def api_config():
    if request.method == 'GET':
        try:
            return jsonify(get_config())
        except Exception as e:
            return jsonify({'success': False, 'message': str(e)}), 500
    else:
        try:
            config_data = request.json
            save_config(config_data)
            return jsonify({'success': True, 'message': 'Configuration saved successfully'})
        except Exception as e:
            return jsonify({'success': False, 'message': str(e)}), 500

if __name__ == '__main__':
    # SSL Context for HTTPS
    # Go up one level to 'services' then into 'certs'
    cert_dir = os.path.join(os.path.dirname(SCRIPT_DIR), 'certs')
    cert_file = os.path.join(cert_dir, 'cert.pem')
    key_file = os.path.join(cert_dir, 'key.pem')
    
    # Run on port 8080, accessible from any IP
    if os.path.exists(cert_file) and os.path.exists(key_file):
        app.run(host='0.0.0.0', port=8080, debug=False, ssl_context=(cert_file, key_file))
    else:
        print(f"Warning: SSL certificates not found at {cert_dir}. Running in HTTP mode.")
        app.run(host='0.0.0.0', port=8080, debug=False)
