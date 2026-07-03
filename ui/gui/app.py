#!/usr/bin/env python3
# 
# @file app.py
# @author Diego Arcos Sapena
# @brief PLC OsoLogic Database Management GUI (Web Interface)
# @version a-1.0.0
# @date 2024/11/23
#
# @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
#

from flask import Flask, render_template, jsonify, request, session, redirect, url_for
from functools import wraps
import mysql.connector
from mysql.connector import Error
import hashlib
import secrets
from datetime import datetime

import sys
import os

# Add common directory to path to import config_loader
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../common')))
from config_loader import get_config

# Load configuration
try:
    config = get_config()
    db_config = config['database']
    gui_config = config['services']['gui']

    DB_HOST = db_config['host']
    DB_USER = db_config['user']
    DB_PASSWORD = db_config['password']
    DB_NAME = db_config['db_name']

    GUI_PORT = gui_config['port']
    GUI_HOST = gui_config['host']
    SECRET_KEY = gui_config['secret_key']
    SESSION_TIMEOUT = gui_config['session_timeout']
except KeyError as e:
    print(f"CRITICAL ERROR: High-level configuration key missing: {e}")
    sys.exit(1)
except Exception as e:
    print(f"CRITICAL ERROR: Failed to load configuration: {e}")
    sys.exit(1)

app = Flask(__name__)
app.secret_key = SECRET_KEY
app.config['PERMANENT_SESSION_LIFETIME'] = SESSION_TIMEOUT * 60

# =============================================================================
# DATABASE CONNECTION
# =============================================================================

def get_db_connection():
    """Create a database connection"""
    try:
        connection = mysql.connector.connect(
            host=DB_HOST,
            user=DB_USER,
            password=DB_PASSWORD,
            database=DB_NAME,
            charset='utf8mb4',
            collation='utf8mb4_general_ci'
        )
        return connection
    except Error as e:
        print(f"Database connection error: {e}")
        return None

def execute_query(query, params=None, fetch=True, commit=False):
    """Execute a query and return results"""
    connection = get_db_connection()
    if not connection:
        return None, "Database connection failed"
    
    try:
        cursor = connection.cursor(dictionary=True)
        cursor.execute(query, params or ())
        
        if fetch:
            result = cursor.fetchall()
        else:
            result = cursor.lastrowid if commit else cursor.rowcount
        
        if commit:
            connection.commit()
        
        cursor.close()
        connection.close()
        return result, None
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return None, str(e)

# =============================================================================
# AUTHENTICATION & AUTHORIZATION
# =============================================================================

# Role hierarchy: admin > operator > viewer
ROLE_HIERARCHY = {'admin': 3, 'operator': 2, 'viewer': 1}

def login_required(f):
    """Decorator to require login for routes"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'logged_in' not in session:
            if request.is_json:
                return jsonify({'error': 'Authentication required'}), 401
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated_function

def admin_required(f):
    """Decorator to require admin role"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'logged_in' not in session:
            if request.is_json:
                return jsonify({'error': 'Authentication required'}), 401
            return redirect(url_for('login'))
        if session.get('role') != 'admin':
            if request.is_json:
                return jsonify({'error': 'Admin privileges required'}), 403
            return redirect(url_for('index'))
        return f(*args, **kwargs)
    return decorated_function

def operator_required(f):
    """Decorator to require operator or higher role (operator, admin)"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'logged_in' not in session:
            if request.is_json:
                return jsonify({'error': 'Authentication required'}), 401
            return redirect(url_for('login'))
        role = session.get('role', 'viewer')
        if ROLE_HIERARCHY.get(role, 0) < ROLE_HIERARCHY['operator']:
            if request.is_json:
                return jsonify({'error': 'Operator privileges required'}), 403
            return redirect(url_for('index'))
        return f(*args, **kwargs)
    return decorated_function

# =============================================================================
# INPUT VALIDATION
# =============================================================================

def validate_fields(data, rules):
    """Validate and coerce input fields. Returns (cleaned_dict, error_string_or_None).
    rules: list of tuples (field_name, label, type_str, required, extra)
      type_str: 'int', 'float', 'str', 'bool'
      extra: dict with optional keys: min, max, choices, min_len, max_len
    """
    cleaned = {}
    if data is None:
        return None, 'Request body is empty or not valid JSON'
    for field, label, typ, required, extra in rules:
        val = data.get(field)
        extra = extra or {}

        # Required check
        if val is None or (isinstance(val, str) and val.strip() == ''):
            if required:
                return None, f'{label} is required'
            cleaned[field] = None
            continue

        # Type coercion and check
        try:
            if typ == 'int':
                val = int(val)
            elif typ == 'float':
                val = float(val)
            elif typ == 'str':
                val = str(val).strip()
            elif typ == 'bool':
                if isinstance(val, str):
                    val = val.lower() in ('true', '1', 'yes')
                else:
                    val = bool(val)
        except (ValueError, TypeError):
            return None, f'{label} must be a valid {typ}'

        # Range checks (int/float)
        if typ in ('int', 'float'):
            if 'min' in extra and val < extra['min']:
                return None, f'{label} must be at least {extra["min"]}'
            if 'max' in extra and val > extra['max']:
                return None, f'{label} must be at most {extra["max"]}'

        # Choices check
        if 'choices' in extra and val not in extra['choices']:
            return None, f'{label} must be one of: {", ".join(str(c) for c in extra["choices"])}'

        # String length
        if typ == 'str':
            if 'min_len' in extra and len(val) < extra['min_len']:
                return None, f'{label} must be at least {extra["min_len"]} characters'
            if 'max_len' in extra and len(val) > extra['max_len']:
                return None, f'{label} must be at most {extra["max_len"]} characters'

        cleaned[field] = val
    return cleaned, None


def validate_item(item, rules, row_label=''):
    """Validate a single item dict against rules. Returns (cleaned, error)."""
    prefix = f'{row_label}: ' if row_label else ''
    cleaned = {}
    for field, label, typ, required, extra in rules:
        val = item.get(field)
        extra = extra or {}
        if val is None or (isinstance(val, str) and val.strip() == ''):
            if required:
                return None, f'{prefix}{label} is required'
            cleaned[field] = None
            continue
        try:
            if typ == 'int':
                val = int(val)
            elif typ == 'float':
                val = float(val)
            elif typ == 'str':
                val = str(val).strip()
            elif typ == 'bool':
                val = val.lower() in ('true', '1', 'yes') if isinstance(val, str) else bool(val)
        except (ValueError, TypeError):
            return None, f'{prefix}{label} must be a valid {typ}'
        if typ in ('int', 'float'):
            if 'min' in extra and val < extra['min']:
                return None, f'{prefix}{label} must be at least {extra["min"]}'
            if 'max' in extra and val > extra['max']:
                return None, f'{prefix}{label} must be at most {extra["max"]}'
        if 'choices' in extra and val not in extra['choices']:
            return None, f'{prefix}{label} must be one of: {", ".join(str(c) for c in extra["choices"])}'
        if typ == 'str':
            if 'min_len' in extra and len(val) < extra['min_len']:
                return None, f'{prefix}{label} must be at least {extra["min_len"]} characters'
            if 'max_len' in extra and len(val) > extra['max_len']:
                return None, f'{prefix}{label} must be at most {extra["max_len"]} characters'
        cleaned[field] = val
    return cleaned, None


# Reusable rule sets
MODEL_RULES = [
    ('model_name', 'Model name', 'str', True, {'min_len': 1, 'max_len': 100}),
    ('protocol', 'Protocol', 'str', False, {'choices': ['borrell-spi', 'modbus-rtu', 'modbus-tcp', 'aggregated']}),
    ('default_timeout_ms', 'Default timeout', 'int', False, {'min': 0, 'max': 60000}),
    ('max_read_bit_block_size', 'Max read bit block', 'int', False, {'min': 1, 'max': 256}),
    ('max_read_register_block_size', 'Max read register block', 'int', False, {'min': 1, 'max': 256}),
    ('max_write_bit_block_size', 'Max write bit block', 'int', False, {'min': 1, 'max': 256}),
    ('max_write_register_block_size', 'Max write register block', 'int', False, {'min': 1, 'max': 256}),
]

IO_DEF_RULES = [
    ('fk_model_id', 'Model ID', 'int', True, {'min': 1}),
    ('logical_address', 'Logical address', 'int', True, {'min': 0, 'max': 65535}),
    ('io_type', 'I/O type', 'str', True, {'choices': ['bit', 'register']}),
    ('purpose', 'Purpose', 'str', True, {'choices': ['standard', 'secure_state', 'config']}),
    ('hardware_access', 'Hardware access', 'str', True, {'choices': ['readonly', 'readwrite']}),
    ('physical_address', 'Physical address', 'int', True, {'min': 0, 'max': 65535}),
    ('access_method', 'Access method', 'str', False, {'choices': ['direct', 'bitmask']}),
    ('bitmask_offset', 'Bitmask offset', 'int', False, {'min': 0, 'max': 15}),
    ('register_count', 'Register count', 'int', False, {'min': 1, 'max': 4}),
    ('endianess', 'Endianness', 'str', False, {'choices': ['big', 'little']}),
    ('default_io_label', 'Default label', 'str', False, {'max_len': 100}),
]

DEVICE_RULES = [
    ('module_name', 'Module name', 'str', True, {'min_len': 1, 'max_len': 100}),
    ('fk_model_id', 'Model', 'int', True, {'min': 1}),
    ('channel', 'Channel', 'str', True, {'choices': ['spi', 'rs485', 'tcp', 'aggregated']}),
    ('connection_string', 'Connection string', 'str', False, {'max_len': 255}),
    ('address_on_channel', 'Address on channel', 'str', False, {'max_len': 50}),
    ('timeout_ms', 'Timeout', 'int', False, {'min': 0, 'max': 60000}),
]

PLC_SETTINGS_RULES = [
    ('rs485_baud_rate', 'Baud rate', 'int', False, {'choices': [9600, 19200, 38400, 57600, 115200]}),
    ('rs485_data_bits', 'Data bits', 'int', False, {'choices': [7, 8]}),
    ('rs485_parity', 'Parity', 'str', False, {'choices': ['N', 'E', 'O']}),
    ('rs485_stop_bits', 'Stop bits', 'int', False, {'choices': [1, 2]}),
    ('operation_mode', 'Operation mode', 'str', False, {'choices': ['execution', 'configuration']}),
]

# =============================================================================
# PASSWORD UTILITIES
# =============================================================================

def verify_password(plain_password, hashed_password):
    """Verify password using SHA256 with salt (format: salt$hash)"""
    try:
        salt, stored_hash = hashed_password.split('$')
        computed_hash = hashlib.sha256((salt + plain_password).encode('utf-8')).hexdigest()
        return secrets.compare_digest(computed_hash, stored_hash)
    except ValueError:
        return False

def hash_password(password):
    """Hash password using SHA256 with random salt (format: salt$hash)"""
    salt = secrets.token_hex(16)
    password_hash = hashlib.sha256((salt + password).encode('utf-8')).hexdigest()
    return f"{salt}${password_hash}"

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        data = request.get_json() if request.is_json else request.form
        username = data.get('username', '')
        password = data.get('password', '')
        
        # Query user from database
        query = "SELECT * FROM gui_users WHERE username = %s AND is_active = TRUE"
        result, error = execute_query(query, (username,))
        
        if error or not result:
            if request.is_json:
                return jsonify({'error': 'Invalid credentials'}), 401
            return render_template('login.html', error='Invalid credentials')
        
        user = result[0]
        
        # Verify password
        if verify_password(password, user['password_hash']):
            session['logged_in'] = True
            session['username'] = username
            session['user_id'] = user['user_id']
            session['role'] = user['role']
            session.permanent = True
            
            # Update last login
            execute_query(
                "UPDATE gui_users SET last_login = %s WHERE user_id = %s",
                (datetime.now(), user['user_id']),
                fetch=False, commit=True
            )
            
            if request.is_json:
                return jsonify({
                    'success': True,
                    'username': username,
                    'role': user['role']
                })
            return redirect(url_for('index'))
        
        if request.is_json:
            return jsonify({'error': 'Invalid credentials'}), 401
        return render_template('login.html', error='Invalid credentials')
    
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login'))

@app.route('/api/auth/me', methods=['GET'])
@login_required
def get_current_user():
    """Get current logged-in user info"""
    return jsonify({
        'username': session.get('username'),
        'role': session.get('role'),
        'user_id': session.get('user_id')
    })

# =============================================================================
# MAIN ROUTES
# =============================================================================

@app.route('/')
@login_required
def index():
    return render_template('index.html')


# =============================================================================
# API: MODELS (model_config)
# =============================================================================

@app.route('/api/models', methods=['GET'])
@login_required
def get_models():
    query = "SELECT * FROM model_config ORDER BY model_id"
    result, error = execute_query(query)
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/models', methods=['POST'])
@login_required
def create_model():
    data = request.get_json()
    cleaned, err = validate_fields(data, MODEL_RULES)
    if err:
        return jsonify({'error': err}), 400
    query = """
        INSERT INTO model_config 
        (model_name, protocol, default_timeout_ms,
         max_read_bit_block_size, max_read_register_block_size, 
         max_write_bit_block_size, max_write_register_block_size)
        VALUES (%s, %s, %s, %s, %s, %s, %s)
    """
    params = (
        cleaned['model_name'],
        cleaned['protocol'] or 'modbus-rtu',
        cleaned['default_timeout_ms'],
        cleaned['max_read_bit_block_size'] or 16,
        cleaned['max_read_register_block_size'] or 16,
        cleaned['max_write_bit_block_size'] or 16,
        cleaned['max_write_register_block_size'] or 16
    )
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True, 'id': result}), 201

@app.route('/api/models/<int:model_id>', methods=['GET'])
@login_required
def get_model(model_id):
    query = "SELECT * FROM model_config WHERE model_id = %s"
    result, error = execute_query(query, (model_id,))
    if error:
        return jsonify({'error': error}), 500
    if not result:
        return jsonify({'error': 'Model not found'}), 404
    return jsonify(result[0])

@app.route('/api/models/<int:model_id>', methods=['PUT'])
@login_required
def update_model(model_id):
    data = request.get_json()
    cleaned, err = validate_fields(data, MODEL_RULES)
    if err:
        return jsonify({'error': err}), 400
    query = """
        UPDATE model_config SET
        model_name = %s,
        protocol = %s,
        default_timeout_ms = %s,
        max_read_bit_block_size = %s,
        max_read_register_block_size = %s,
        max_write_bit_block_size = %s,
        max_write_register_block_size = %s
        WHERE model_id = %s
    """
    params = (
        cleaned['model_name'],
        cleaned['interface_protocol'],
        cleaned['default_timeout_ms'],
        cleaned['max_read_bit_block_size'],
        cleaned['max_read_register_block_size'],
        cleaned['max_write_bit_block_size'],
        cleaned['max_write_register_block_size'],
        model_id
    )
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

@app.route('/api/models/<int:model_id>', methods=['DELETE'])
@login_required
def delete_model(model_id):
    query = "DELETE FROM model_config WHERE model_id = %s"
    result, error = execute_query(query, (model_id,), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

@app.route('/api/models/<int:model_id>/full', methods=['GET'])
@login_required
def get_model_full(model_id):
    """Get a model with all its IO definitions and secure state mappings"""
    # Get model config
    model_result, error = execute_query(
        "SELECT * FROM model_config WHERE model_id = %s", (model_id,))
    if error:
        return jsonify({'error': error}), 500
    if not model_result:
        return jsonify({'error': 'Model not found'}), 404
    model = model_result[0]

    # Get IO definitions
    io_result, error = execute_query("""
        SELECT * FROM model_io_definition
        WHERE fk_model_id = %s
        ORDER BY purpose, logical_address
    """, (model_id,))
    if error:
        return jsonify({'error': error}), 500
    model['io_definitions'] = io_result or []

    # Get secure state mappings with labels
    ssm_result, error = execute_query("""
        SELECT ssm.*,
               std.logical_address as standard_address, std.default_io_label as standard_label,
               std.io_type as standard_io_type, std.hardware_access as standard_access,
               sec.logical_address as secure_address, sec.default_io_label as secure_label,
               sec.io_type as secure_io_type, sec.hardware_access as secure_access
        FROM model_secure_state_mapping ssm
        JOIN model_io_definition std ON ssm.fk_standard_io_definition_id = std.io_definition_id
        JOIN model_io_definition sec ON ssm.fk_secure_state_io_definition_id = sec.io_definition_id
        WHERE ssm.fk_model_id = %s
    """, (model_id,))
    if error:
        return jsonify({'error': error}), 500
    model['secure_state_mappings'] = ssm_result or []

    # Get aggregated model children (if any)
    children_result, error = execute_query("""
        SELECT amc.*, m.model_name as child_model_name
        FROM aggregated_model_children amc
        JOIN model_config m ON amc.fk_child_model_id = m.model_id
        WHERE amc.fk_aggregated_model_id = %s
        ORDER BY amc.slot_index
    """, (model_id,))
    if error:
        return jsonify({'error': error}), 500
    model['aggregated_children'] = children_result or []

    # Get aggregated IO map entries (if any)
    io_map_result, error = execute_query("""
        SELECT aim.*, mid_child.logical_address as child_logical_address
        FROM aggregated_io_map aim
        JOIN model_io_definition mid ON aim.fk_aggregated_io_definition_id = mid.io_definition_id
        LEFT JOIN model_io_definition mid_child ON aim.fk_child_io_definition_id = mid_child.io_definition_id
        WHERE mid.fk_model_id = %s
    """, (model_id,))
    if error:
        return jsonify({'error': error}), 500
    model['aggregated_io_map'] = io_map_result or []

    return jsonify(model)

@app.route('/api/io-definitions/batch', methods=['POST'])
@login_required
def create_io_definitions_batch():
    """Create multiple IO definitions in one transaction"""
    data = request.get_json()
    if data is None:
        return jsonify({'error': 'Request body is empty or not valid JSON'}), 400
    items = data.get('items', [])
    if not items:
        return jsonify({'error': 'No items provided'}), 400

    # Validate all items first
    cleaned_items = []
    for i, item in enumerate(items):
        cleaned, err = validate_item(item, IO_DEF_RULES, f'Row {i+1}')
        if err:
            return jsonify({'error': err}), 400
        cleaned_items.append(cleaned)

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor(dictionary=True)
        ids = []
        for c in cleaned_items:
            cursor.execute("""
                INSERT INTO model_io_definition
                (fk_model_id, logical_address, io_type, purpose, hardware_access,
                 physical_address, access_method, bitmask_offset, register_count, endianess, default_io_label)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            """, (
                c['fk_model_id'],
                c['logical_address'],
                c['io_type'],
                c['purpose'],
                c['hardware_access'],
                c['physical_address'],
                c['access_method'] or 'direct',
                c['bitmask_offset'],
                c['register_count'] or 1,
                c['endianess'] or 'big',
                c['default_io_label']
            ))
            ids.append(cursor.lastrowid)
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True, 'ids': ids}), 201
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

@app.route('/api/io-definitions/batch-update', methods=['PUT'])
@login_required
def update_io_definitions_batch():
    """Update multiple IO definitions in one transaction"""
    data = request.get_json()
    if data is None:
        return jsonify({'error': 'Request body is empty or not valid JSON'}), 400
    items = data.get('items', [])
    if not items:
        return jsonify({'error': 'No items provided'}), 400

    # IO_DEF_RULES but fk_model_id is not required for update, and we need io_definition_id
    update_rules = [r for r in IO_DEF_RULES if r[0] != 'fk_model_id']
    update_rules.append(('io_definition_id', 'IO Definition ID', 'int', True, {'min': 1}))

    cleaned_items = []
    for i, item in enumerate(items):
        cleaned, err = validate_item(item, update_rules, f'Row {i+1}')
        if err:
            return jsonify({'error': err}), 400
        cleaned_items.append(cleaned)

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor()
        for c in cleaned_items:
            cursor.execute("""
                UPDATE model_io_definition SET
                logical_address = %s,
                io_type = %s,
                purpose = %s,
                hardware_access = %s,
                physical_address = %s,
                access_method = %s,
                bitmask_offset = %s,
                register_count = %s,
                endianess = %s,
                default_io_label = %s
                WHERE io_definition_id = %s
            """, (
                c['logical_address'],
                c['io_type'],
                c['purpose'],
                c['hardware_access'],
                c['physical_address'],
                c['access_method'] or 'direct',
                c['bitmask_offset'],
                c['register_count'] or 1,
                c['endianess'] or 'big',
                c['default_io_label'],
                c['io_definition_id']
            ))
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True})
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

@app.route('/api/io-definitions/batch-delete', methods=['POST'])
@login_required
def delete_io_definitions_batch():
    """Delete multiple IO definitions in one transaction"""
    data = request.get_json()
    if data is None:
        return jsonify({'error': 'Request body is empty or not valid JSON'}), 400
    ids = data.get('ids', [])
    if not ids:
        return jsonify({'error': 'No IDs provided'}), 400

    # Validate all IDs are integers
    for i, io_id in enumerate(ids):
        try:
            ids[i] = int(io_id)
        except (ValueError, TypeError):
            return jsonify({'error': f'ID at position {i+1} must be a valid integer'}), 400

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor()
        for io_id in ids:
            cursor.execute("DELETE FROM model_io_definition WHERE io_definition_id = %s", (io_id,))
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True})
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

@app.route('/api/secure-state-mapping/batch', methods=['POST'])
@login_required
def create_secure_state_mapping_batch():
    """Create multiple secure state mappings in one transaction"""
    data = request.get_json()
    items = data.get('items', [])
    if not items:
        return jsonify({'error': 'No items provided'}), 400

    ssm_rules = [
        ('fk_model_id', 'Model ID', 'int', True, {'min': 1}),
        ('fk_standard_io_definition_id', 'Standard IO definition', 'int', True, {'min': 1}),
        ('fk_secure_state_io_definition_id', 'Secure state IO definition', 'int', True, {'min': 1}),
    ]

    cleaned_items = []
    for i, item in enumerate(items):
        cleaned, err = validate_item(item, ssm_rules, f'Row {i+1}')
        if err:
            return jsonify({'error': err}), 400
        cleaned_items.append(cleaned)

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor(dictionary=True)
        # If model_id is provided, delete existing mappings first (safe: no CASCADE to module_io_config)
        model_id = data.get('model_id')
        if model_id:
            cursor.execute("DELETE FROM model_secure_state_mapping WHERE fk_model_id = %s", (model_id,))
        for item in cleaned_items:
            cursor.execute("""
                INSERT INTO model_secure_state_mapping
                (fk_model_id, fk_standard_io_definition_id, fk_secure_state_io_definition_id)
                VALUES (%s, %s, %s)
            """, (
                item['fk_model_id'],
                item['fk_standard_io_definition_id'],
                item['fk_secure_state_io_definition_id']
            ))
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True}), 201
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

# =============================================================================
# API: AGGREGATED MODEL CHILDREN
# =============================================================================

@app.route('/api/aggregated-model-children', methods=['GET'])
@login_required
def get_aggregated_model_children():
    model_id = request.args.get('model_id')
    if not model_id:
        return jsonify({'error': 'model_id parameter required'}), 400
    query = """
        SELECT amc.*, m.model_name as child_model_name
        FROM aggregated_model_children amc
        JOIN model_config m ON amc.fk_child_model_id = m.model_id
        WHERE amc.fk_aggregated_model_id = %s
        ORDER BY amc.slot_index
    """
    result, error = execute_query(query, (model_id,))
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/aggregated-model-children/batch', methods=['POST'])
@login_required
def create_aggregated_model_children_batch():
    """Replace all children for an aggregated model. Deletes existing first, then inserts."""
    data = request.get_json()
    model_id = data.get('model_id')
    children = data.get('children', [])  # [{fk_child_model_id: X}, ...] in slot order
    if not model_id:
        return jsonify({'error': 'model_id is required'}), 400

    child_rules = [
        ('fk_child_model_id', 'Child model ID', 'int', True, {'min': 1})
    ]

    cleaned_children = []
    for i, child in enumerate(children):
        cleaned, err = validate_item(child, child_rules, f'Slot {i}')
        if err:
            return jsonify({'error': err}), 400
        cleaned_children.append(cleaned)

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor(dictionary=True)
        # Delete existing children for this model
        cursor.execute("DELETE FROM aggregated_model_children WHERE fk_aggregated_model_id = %s", (model_id,))
        # Insert new children in slot order
        for slot_index, child in enumerate(cleaned_children):
            cursor.execute("""
                INSERT INTO aggregated_model_children
                (fk_aggregated_model_id, slot_index, fk_child_model_id)
                VALUES (%s, %s, %s)
            """, (model_id, slot_index, child['fk_child_model_id']))
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True}), 201
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

# =============================================================================
# API: AGGREGATED IO MAP
# =============================================================================

@app.route('/api/aggregated-io-map/batch', methods=['POST'])
@login_required
def create_aggregated_io_map_batch():
    """Create aggregated IO map entries in batch. Expects model_id to delete old entries first."""
    data = request.get_json()
    model_id = data.get('model_id')
    items = data.get('items', [])
    if not model_id:
        return jsonify({'error': 'model_id is required'}), 400

    agg_rules = [
        ('fk_aggregated_io_definition_id', 'Aggregated IO definition', 'int', True, {'min': 1}),
        ('child_slot_index', 'Child slot index', 'int', True, {'min': 0, 'max': 255}),
        ('fk_child_io_definition_id', 'Child IO definition', 'int', True, {'min': 1}),
    ]

    cleaned_items = []
    for i, item in enumerate(items):
        cleaned, err = validate_item(item, agg_rules, f'Row {i+1}')
        if err:
            return jsonify({'error': err}), 400
        cleaned_items.append(cleaned)

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor(dictionary=True)
        # Delete existing aggregated_io_map entries for this model's IO definitions
        cursor.execute("""
            DELETE aim FROM aggregated_io_map aim
            JOIN model_io_definition mid ON aim.fk_aggregated_io_definition_id = mid.io_definition_id
            WHERE mid.fk_model_id = %s
        """, (model_id,))
        # Insert new entries
        for item in cleaned_items:
            cursor.execute("""
                INSERT INTO aggregated_io_map
                (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
                VALUES (%s, %s, %s)
            """, (
                item['fk_aggregated_io_definition_id'],
                item['child_slot_index'],
                item['fk_child_io_definition_id']
            ))
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True}), 201
    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

# =============================================================================
# API: DEVICES (devices + devices tables)
# =============================================================================

@app.route('/api/devices', methods=['GET'])
@login_required
def get_devices():
    query = """
        SELECT mc.*, m.model_name, m.protocol, d.is_connected, d.last_seen
        FROM devices mc
        LEFT JOIN model_config m ON mc.fk_model_id = m.model_id
        LEFT JOIN device_status d ON mc.module_id = d.fk_module_id
        ORDER BY mc.module_id
    """
    result, error = execute_query(query)
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/devices', methods=['POST'])
@login_required
def create_device():
    data = request.get_json()
    
    # Auto-fill SPI specific fields if missing
    if data and data.get('channel') == 'spi':
        if not data.get('connection_string'):
            data['connection_string'] = 'SPI'
    cleaned, err = validate_fields(data, DEVICE_RULES)
    if err:
        return jsonify({'error': err}), 400
        
    # Final check for NOT NULL fields that were optional in validation
    if not cleaned['connection_string']:
        return jsonify({'error': 'Connection string is required'}), 400
    if not cleaned['address_on_channel']:
        return jsonify({'error': 'Address on channel is required'}), 400

    # Extract io_config array from request (sent by wizard Step 2)
    io_config = data.get('io_config', [])

    # --- Transactional: Insert device + all IO config in one go ---
    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor(dictionary=True)
        connection.start_transaction()

        # 1. Insert the device
        cursor.execute("""
            INSERT INTO devices 
            (module_name, fk_model_id, channel, connection_string, address_on_channel, timeout_ms)
            VALUES (%s, %s, %s, %s, %s, %s)
        """, (
            cleaned['module_name'],
            cleaned['fk_model_id'],
            cleaned['channel'],
            cleaned['connection_string'],
            cleaned['address_on_channel'],
            cleaned['timeout_ms']
        ))
        new_module_id = cursor.lastrowid

        # 2. Insert all IO config rows (from wizard Step 2)
        if io_config:
            for io in io_config:
                user_label = io.get('user_label', '')
                if not user_label:
                    return jsonify({'error': f"User label is required for all I/O points (io_def_id: {io.get('fk_io_definition_id', '?')})"}), 400

                # Replace the frontend 'Auto' placeholder with the real device ID
                user_label = user_label.replace('(Dev Auto)', f'(Dev {new_module_id})')

                cursor.execute("""
                    INSERT INTO module_io_config 
                    (fk_module_id, fk_io_definition_id, user_label, units, scale_factor, `offset`, 
                     visibility, visibility_mode, refresh_rate, sync)
                    VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                """, (
                    new_module_id,
                    io['fk_io_definition_id'],
                    user_label,
                    io.get('units') or None,
                    io.get('scale_factor', 1.0),
                    io.get('offset', 0.0),
                    io.get('visibility', 'visible'),
                    io.get('visibility_mode', 'periodically'),
                    io.get('refresh_rate', 1),
                    io.get('sync', True)
                ))

        # 3. All good — commit everything
        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True, 'id': new_module_id}), 201

    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

@app.route('/api/devices/<int:module_id>', methods=['GET'])
@login_required
def get_device(module_id):
    query = """
        SELECT mc.*, m.model_name, m.protocol, d.is_connected, d.last_seen
        FROM devices mc
        LEFT JOIN model_config m ON mc.fk_model_id = m.model_id
        LEFT JOIN device_status d ON mc.module_id = d.fk_module_id
        WHERE mc.module_id = %s
    """
    result, error = execute_query(query, (module_id,))
    if error:
        return jsonify({'error': error}), 500
    if not result:
        return jsonify({'error': 'Device not found'}), 404
    return jsonify(result[0])

@app.route('/api/devices/<int:module_id>', methods=['PUT'])
@login_required
def update_device(module_id):
    data = request.get_json()
    cleaned, err = validate_fields(data, DEVICE_RULES)
    if err:
        return jsonify({'error': err}), 400
        
    io_config = data.get('io_config', None)

    connection = get_db_connection()
    if not connection:
        return jsonify({'error': 'Database connection failed'}), 500

    try:
        cursor = connection.cursor(dictionary=True)
        connection.start_transaction()

        # Update devices table
        cursor.execute("""
            UPDATE devices SET
            module_name = %s, channel = %s, connection_string = %s,
            address_on_channel = %s, fk_model_id = %s, timeout_ms = %s
            WHERE module_id = %s
        """, (
            cleaned['module_name'], cleaned['channel'],
            cleaned['connection_string'], cleaned['address_on_channel'],
            cleaned['fk_model_id'], cleaned['timeout_ms'], module_id
        ))

        # If io_config array is provided, it means the model was changed in the frontend wizard
        # or the frontend explicitly wants a full I/O config replacement.
        if io_config is not None:
            # Wipe old configuration
            cursor.execute("DELETE FROM module_io_config WHERE fk_module_id = %s", (module_id,))
            
            # Insert new configuration
            for io in io_config:
                user_label = io.get('user_label', '')
                if not user_label:
                    return jsonify({'error': f"User label is required for all I/O points (io_def_id: {io.get('fk_io_definition_id', '?')})"}), 400

                user_label = user_label.replace('(Dev Auto)', f'(Dev {module_id})')

                cursor.execute("""
                    INSERT INTO module_io_config 
                    (fk_module_id, fk_io_definition_id, user_label, units, scale_factor, `offset`, 
                     visibility, visibility_mode, refresh_rate, sync)
                    VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                """, (
                    module_id,
                    io['fk_io_definition_id'],
                    user_label,
                    io.get('units') or None,
                    io.get('scale_factor', 1.0),
                    io.get('offset', 0.0),
                    io.get('visibility', 'visible'),
                    io.get('visibility_mode', 'periodically'),
                    io.get('refresh_rate', 1),
                    io.get('sync', True)
                ))

        connection.commit()
        cursor.close()
        connection.close()
        return jsonify({'success': True})

    except Error as e:
        if connection:
            connection.rollback()
            connection.close()
        return jsonify({'error': str(e)}), 500

@app.route('/api/devices/<int:module_id>', methods=['DELETE'])
@login_required
def delete_device(module_id):
    # Deleting from devices will cascade to devices table
    query = "DELETE FROM devices WHERE module_id = %s"
    result, error = execute_query(query, (module_id,), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: I/O DEFINITIONS (model_io_definition)
# =============================================================================

@app.route('/api/io-definitions', methods=['GET'])
@login_required
def get_io_definitions():
    model_id = request.args.get('model_id')
    if model_id:
        query = """
            SELECT iod.*, m.model_name 
            FROM model_io_definition iod
            JOIN model_config m ON iod.fk_model_id = m.model_id
            WHERE iod.fk_model_id = %s
            ORDER BY iod.logical_address
        """
        result, error = execute_query(query, (model_id,))
    else:
        query = """
            SELECT iod.*, m.model_name 
            FROM model_io_definition iod
            JOIN model_config m ON iod.fk_model_id = m.model_id
            ORDER BY iod.fk_model_id, iod.logical_address
        """
        result, error = execute_query(query)
    
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/io-definitions', methods=['POST'])
@login_required
def create_io_definition():
    data = request.get_json()
    cleaned, err = validate_fields(data, IO_DEF_RULES)
    if err:
        return jsonify({'error': err}), 400
    query = """
        INSERT INTO model_io_definition 
        (fk_model_id, logical_address, io_type, purpose, hardware_access, 
         physical_address, access_method, bitmask_offset, register_count, endianess, default_io_label)
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
    """
    params = (
        cleaned['fk_model_id'],
        cleaned['logical_address'],
        cleaned['io_type'],
        cleaned['purpose'],
        cleaned['hardware_access'],
        cleaned['physical_address'],
        cleaned['access_method'] or 'direct',
        cleaned['bitmask_offset'],
        cleaned['register_count'] or 1,
        cleaned['endianess'] or 'big',
        cleaned['default_io_label']
    )
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True, 'id': result}), 201


@app.route('/api/io-definitions/<int:io_id>', methods=['DELETE'])
@login_required
def delete_io_definition(io_id):
    query = "DELETE FROM model_io_definition WHERE io_definition_id = %s"
    result, error = execute_query(query, (io_id,), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: MODULE I/O CONFIG
# =============================================================================

@app.route('/api/module-io-config', methods=['GET'])
@login_required
def get_module_io_config():
    module_id = request.args.get('module_id')
    if module_id:
        query = """
            SELECT mio.*, iod.logical_address, iod.io_type, iod.purpose, iod.hardware_access, iod.default_io_label
            FROM module_io_config mio
            JOIN model_io_definition iod ON mio.fk_io_definition_id = iod.io_definition_id
            WHERE mio.fk_module_id = %s
            ORDER BY iod.logical_address
        """
        result, error = execute_query(query, (module_id,))

    else:
        query = """
            SELECT mio.*, d.module_name, iod.logical_address, iod.io_type, iod.purpose, iod.default_io_label
            FROM module_io_config mio
            JOIN devices d ON mio.fk_module_id = d.module_id
            JOIN model_io_definition iod ON mio.fk_io_definition_id = iod.io_definition_id
            ORDER BY mio.fk_module_id, iod.logical_address
        """
        result, error = execute_query(query)
    
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/module-io-config/<int:module_id>/<int:io_def_id>', methods=['PUT'])
@login_required
def update_module_io_config(module_id, io_def_id):
    data = request.get_json()
    io_config_rules = [
        ('user_label', 'User label', 'str', False, {'max_len': 100}),
        ('units', 'Units', 'str', False, {'max_len': 20}),
        ('scale_factor', 'Scale factor', 'float', False, None),
        ('offset', 'Offset', 'float', False, None),
        ('visibility', 'Visibility', 'str', False, {'choices': ['visible', 'hidden']}),
        ('visibility_mode', 'Visibility mode', 'str', False, {'choices': ['periodically', 'on_change']}),
        ('refresh_rate', 'Refresh rate', 'int', False, {'min': 0, 'max': 3600}),
        ('sync', 'Sync', 'bool', False, None),
    ]
    cleaned, err = validate_fields(data, io_config_rules)
    if err:
        return jsonify({'error': err}), 400
    query = """
        UPDATE module_io_config SET
        user_label = %s,
        units = %s,
        scale_factor = %s,
        `offset` = %s,
        visibility = %s,
        visibility_mode = %s,
        refresh_rate = %s,
        sync = %s
        WHERE fk_module_id = %s AND fk_io_definition_id = %s
    """
    params = (
        cleaned['user_label'],
        cleaned['units'],
        cleaned['scale_factor'] if cleaned['scale_factor'] is not None else 1.0,
        cleaned['offset'] if cleaned['offset'] is not None else 0.0,
        cleaned['visibility'] or 'visible',
        cleaned['visibility_mode'] or 'periodically',
        cleaned['refresh_rate'] if cleaned['refresh_rate'] is not None else 1,
        cleaned['sync'] if cleaned['sync'] is not None else True,
        module_id,
        io_def_id
    )
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: SECURE STATE MAPPING
# =============================================================================

@app.route('/api/secure-state-mapping', methods=['GET'])
@login_required
def get_secure_state_mapping():
    model_id = request.args.get('model_id')
    if model_id:
        query = """
            SELECT ssm.*, 
                   std.logical_address as standard_address, std.default_io_label as standard_label,
                   sec.logical_address as secure_address, sec.default_io_label as secure_label
            FROM model_secure_state_mapping ssm
            JOIN model_io_definition std ON ssm.fk_standard_io_definition_id = std.io_definition_id
            JOIN model_io_definition sec ON ssm.fk_secure_state_io_definition_id = sec.io_definition_id
            WHERE ssm.fk_model_id = %s
        """
        result, error = execute_query(query, (model_id,))
    else:
        query = """
            SELECT ssm.*, m.model_name,
                   std.logical_address as standard_address, std.default_io_label as standard_label,
                   sec.logical_address as secure_address, sec.default_io_label as secure_label
            FROM model_secure_state_mapping ssm
            JOIN model_config m ON ssm.fk_model_id = m.model_id
            JOIN model_io_definition std ON ssm.fk_standard_io_definition_id = std.io_definition_id
            JOIN model_io_definition sec ON ssm.fk_secure_state_io_definition_id = sec.io_definition_id
        """
        result, error = execute_query(query)
    
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/secure-state-mapping', methods=['POST'])
@login_required
def create_secure_state_mapping():
    data = request.get_json()
    ssm_rules = [
        ('fk_model_id', 'Model ID', 'int', True, {'min': 1}),
        ('fk_standard_io_definition_id', 'Standard IO definition', 'int', True, {'min': 1}),
        ('fk_secure_state_io_definition_id', 'Secure state IO definition', 'int', True, {'min': 1}),
    ]
    cleaned, err = validate_fields(data, ssm_rules)
    if err:
        return jsonify({'error': err}), 400
    query = """
        INSERT INTO model_secure_state_mapping 
        (fk_model_id, fk_standard_io_definition_id, fk_secure_state_io_definition_id)
        VALUES (%s, %s, %s)
    """
    params = (
        cleaned['fk_model_id'],
        cleaned['fk_standard_io_definition_id'],
        cleaned['fk_secure_state_io_definition_id']
    )
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True}), 201

@app.route('/api/secure-state-mapping/<int:model_id>/<int:standard_id>', methods=['DELETE'])
@login_required
def delete_secure_state_mapping(model_id, standard_id):
    query = """
        DELETE FROM model_secure_state_mapping 
        WHERE fk_model_id = %s AND fk_standard_io_definition_id = %s
    """
    result, error = execute_query(query, (model_id, standard_id), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: AGGREGATED I/O MAPPING
# =============================================================================

@app.route('/api/aggregated-io-map', methods=['GET'])
@login_required
def get_aggregated_io_map():
    query = """
        SELECT vim.*, 
               vio.logical_address as aggregated_address, vio.default_io_label as aggregated_label,
               vio.fk_model_id as aggregated_model_id,
               amc.fk_child_model_id, cm.model_name as child_model_name,
               cio.logical_address as child_address, cio.default_io_label as child_label
        FROM aggregated_io_map vim
        JOIN model_io_definition vio ON vim.fk_aggregated_io_definition_id = vio.io_definition_id
        LEFT JOIN aggregated_model_children amc ON amc.fk_aggregated_model_id = vio.fk_model_id AND amc.slot_index = vim.child_slot_index
        LEFT JOIN model_config cm ON amc.fk_child_model_id = cm.model_id
        JOIN model_io_definition cio ON vim.fk_child_io_definition_id = cio.io_definition_id
    """
    result, error = execute_query(query)
    if error:
        return jsonify({'error': error}), 500
    return jsonify(result)

@app.route('/api/aggregated-io-map', methods=['POST'])
@login_required
def create_aggregated_io_map():
    data = request.get_json()
    agg_rules = [
        ('fk_aggregated_io_definition_id', 'Aggregated IO definition', 'int', True, {'min': 1}),
        ('child_slot_index', 'Child slot index', 'int', True, {'min': 0, 'max': 255}),
        ('fk_child_io_definition_id', 'Child IO definition', 'int', True, {'min': 1}),
    ]
    cleaned, err = validate_fields(data, agg_rules)
    if err:
        return jsonify({'error': err}), 400
    query = """
        INSERT INTO aggregated_io_map 
        (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
        VALUES (%s, %s, %s)
    """
    params = (
        cleaned['fk_aggregated_io_definition_id'],
        cleaned['child_slot_index'],
        cleaned['fk_child_io_definition_id']
    )
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True, 'id': result}), 201

@app.route('/api/aggregated-io-map/<int:map_id>', methods=['DELETE'])
@login_required
def delete_aggregated_io_map(map_id):
    query = "DELETE FROM aggregated_io_map WHERE map_id = %s"
    result, error = execute_query(query, (map_id,), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: PLC SETTINGS
# =============================================================================

@app.route('/api/plc-settings', methods=['GET'])
@login_required
def get_plc_settings():
    query = "SELECT * FROM plc_settings LIMIT 1"
    result, error = execute_query(query)
    if error:
        return jsonify({'error': error}), 500
    if not result:
        return jsonify({})
    return jsonify(result[0])

@app.route('/api/plc-settings', methods=['PUT'])
@operator_required
def update_plc_settings():
    data = request.get_json()
    cleaned, err = validate_fields(data, PLC_SETTINGS_RULES)
    if err:
        return jsonify({'error': err}), 400
    
    # Check if a record exists
    check_query = "SELECT id FROM plc_settings LIMIT 1"
    result, _ = execute_query(check_query)
    
    if result:
        setting_id = result[0]['id']
        query = """
            UPDATE plc_settings SET
            rs485_baud_rate = %s,
            rs485_data_bits = %s,
            rs485_parity = %s,
            rs485_stop_bits = %s,
            operation_mode = %s
            WHERE id = %s
        """
        params = (
            cleaned['rs485_baud_rate'],
            cleaned['rs485_data_bits'],
            cleaned['rs485_parity'],
            cleaned['rs485_stop_bits'],
            cleaned['operation_mode'],
            setting_id
        )
    else:
        query = """
            INSERT INTO plc_settings 
            (rs485_baud_rate, rs485_data_bits, rs485_parity, rs485_stop_bits, operation_mode)
            VALUES (%s, %s, %s, %s, %s)
        """
        params = (
            cleaned['rs485_baud_rate'],
            cleaned['rs485_data_bits'],
            cleaned['rs485_parity'],
            cleaned['rs485_stop_bits'],
            cleaned['operation_mode']
        )
    
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: RTMIRROR (Read-only)
# =============================================================================

@app.route('/api/rtmirror', methods=['GET'])
@login_required
def get_rtmirror():
    module_id = request.args.get('module_id')
    if module_id:
        query = """
            SELECT rt.*, mc.module_name, iod.logical_address, iod.io_type, iod.purpose, iod.hardware_access,
                   mio.units, mio.user_label
            FROM rtmirror rt
            JOIN devices mc ON rt.fk_module_id = mc.module_id
            JOIN model_io_definition iod ON rt.fk_io_definition_id = iod.io_definition_id
            LEFT JOIN module_io_config mio ON rt.fk_module_id = mio.fk_module_id AND rt.fk_io_definition_id = mio.fk_io_definition_id
            WHERE rt.fk_module_id = %s
            ORDER BY iod.logical_address
        """
        result, error = execute_query(query, (module_id,))
    else:
        query = """
            SELECT rt.*, mc.module_name, iod.logical_address, iod.io_type, iod.purpose, iod.hardware_access,
                   mio.units, mio.user_label
            FROM rtmirror rt
            JOIN devices mc ON rt.fk_module_id = mc.module_id
            JOIN model_io_definition iod ON rt.fk_io_definition_id = iod.io_definition_id
            LEFT JOIN module_io_config mio ON rt.fk_module_id = mio.fk_module_id AND rt.fk_io_definition_id = mio.fk_io_definition_id
            ORDER BY rt.fk_module_id, iod.logical_address
        """
        result, error = execute_query(query)
    
    if error:
        return jsonify({'error': error}), 500
    
    # Format timestamp
    for row in result:
        if row.get('timestamp'):
            row['timestamp'] = row['timestamp'].strftime('%Y-%m-%dT%H:%M:%S')

    return jsonify(result)

@app.route('/api/rtmirror/<int:module_id>/<int:io_def_id>', methods=['PUT'])
@operator_required
def update_rtmirror_value(module_id, io_def_id):
    """Update required_value or net_required_value for an I/O point"""
    data = request.get_json()
    if data is None:
        return jsonify({'error': 'Request body is empty or not valid JSON'}), 400
    
    # Check if the I/O point is an output (write-enabled)
    check_query = """
        SELECT iod.hardware_access, iod.io_type
        FROM module_io_config mio
        JOIN model_io_definition iod ON mio.fk_io_definition_id = iod.io_definition_id
        WHERE mio.fk_module_id = %s AND mio.fk_io_definition_id = %s
    """
    check_result, error = execute_query(check_query, (module_id, io_def_id))
    if error:
        return jsonify({'error': error}), 500
    if not check_result:
        return jsonify({'error': 'I/O point not found'}), 404
    
    if check_result[0]['hardware_access'] == 'read':
        return jsonify({'error': 'Cannot write to input (read-only) I/O point'}), 403
    
    io_type = check_result[0].get('io_type', 'register')
    
    # Build update query based on provided fields
    updates = []
    params = []
    
    for field_name in ('required_value', 'net_required_value'):
        if field_name in data:
            val = data[field_name]
            try:
                val = int(val)
            except (ValueError, TypeError):
                return jsonify({'error': f'{field_name} must be a valid integer'}), 400
            if val < 0:
                return jsonify({'error': f'{field_name} must be at least 0'}), 400
            if io_type == 'bit' and val not in (0, 1):
                return jsonify({'error': f'{field_name} for a bit I/O must be 0 or 1'}), 400
            if io_type == 'register' and val > 4294967295:
                return jsonify({'error': f'{field_name} must be at most 4294967295'}), 400
            updates.append(f'{field_name} = %s')
            params.append(val)
    
    if not updates:
        return jsonify({'error': 'No fields to update'}), 400
    
    params.extend([module_id, io_def_id])
    
    query = f"""
        UPDATE rtmirror SET {', '.join(updates)}
        WHERE fk_module_id = %s AND fk_io_definition_id = %s
    """
    
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    return jsonify({'success': True})

# =============================================================================
# API: USER MANAGEMENT (Admin only)
# =============================================================================

@app.route('/api/users', methods=['GET'])
@admin_required
def get_users():
    """Get all users (admin only)"""
    query = """
        SELECT user_id, username, role, created_at, updated_at, last_login, is_active
        FROM gui_users
        ORDER BY user_id
    """
    result, error = execute_query(query)
    if error:
        return jsonify({'error': error}), 500
    
    # Convert datetime objects to strings for JSON serialization
    for user in result:
        if user['created_at']:
            user['created_at'] = user['created_at'].strftime('%Y-%m-%d %H:%M:%S')
        if user['updated_at']:
            user['updated_at'] = user['updated_at'].strftime('%Y-%m-%d %H:%M:%S')
        if user['last_login']:
            user['last_login'] = user['last_login'].strftime('%Y-%m-%d %H:%M:%S')
    
    return jsonify(result)

@app.route('/api/users', methods=['POST'])
@admin_required
def create_user():
    """Create a new user (admin only). New users cannot be admin - use role transfer instead."""
    data = request.get_json()
    
    username = data.get('username', '').strip()
    password = data.get('password', '')
    role = data.get('role', 'viewer')
    
    # Validate inputs
    if not username or len(username) < 3:
        return jsonify({'error': 'Username must be at least 3 characters'}), 400
    if not password or len(password) < 6:
        return jsonify({'error': 'Password must be at least 6 characters'}), 400
    
    # Cannot create admin users directly - must use role transfer
    if role == 'admin':
        return jsonify({'error': 'Cannot create admin user directly. Create as operator/viewer, then transfer admin role.'}), 400
    if role not in ['operator', 'viewer']:
        return jsonify({'error': 'Invalid role. Must be operator or viewer'}), 400
    
    # Check if username already exists
    check_result, _ = execute_query(
        "SELECT user_id FROM gui_users WHERE username = %s",
        (username,)
    )
    if check_result:
        return jsonify({'error': 'Username already exists'}), 400
    
    # Hash password and create user
    password_hash = hash_password(password)
    
    query = """
        INSERT INTO gui_users (username, password_hash, role)
        VALUES (%s, %s, %s)
    """
    result, error = execute_query(query, (username, password_hash, role), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    
    return jsonify({'success': True, 'user_id': result}), 201

@app.route('/api/users/<int:user_id>', methods=['GET'])
@admin_required
def get_user(user_id):
    """Get a single user (admin only)"""
    query = """
        SELECT user_id, username, role, created_at, updated_at, last_login, is_active
        FROM gui_users
        WHERE user_id = %s
    """
    result, error = execute_query(query, (user_id,))
    if error:
        return jsonify({'error': error}), 500
    if not result:
        return jsonify({'error': 'User not found'}), 404
    
    user = result[0]
    if user['created_at']:
        user['created_at'] = user['created_at'].strftime('%Y-%m-%d %H:%M:%S')
    if user['updated_at']:
        user['updated_at'] = user['updated_at'].strftime('%Y-%m-%d %H:%M:%S')
    if user['last_login']:
        user['last_login'] = user['last_login'].strftime('%Y-%m-%d %H:%M:%S')
    
    return jsonify(user)

@app.route('/api/users/<int:user_id>', methods=['PUT'])
@admin_required
def update_user(user_id):
    """Update a user (admin only). Only one admin allowed - promoting someone demotes current admin."""
    data = request.get_json()
    current_user_id = session.get('user_id')
    
    # Build update query dynamically
    updates = []
    params = []
    
    if 'username' in data:
        username = data['username'].strip()
        if len(username) < 3:
            return jsonify({'error': 'Username must be at least 3 characters'}), 400
        # Check if username is taken by another user
        check_result, _ = execute_query(
            "SELECT user_id FROM gui_users WHERE username = %s AND user_id != %s",
            (username, user_id)
        )
        if check_result:
            return jsonify({'error': 'Username already taken'}), 400
        updates.append('username = %s')
        params.append(username)
    
    if 'password' in data and data['password']:
        password = data['password']
        if len(password) < 6:
            return jsonify({'error': 'Password must be at least 6 characters'}), 400
        updates.append('password_hash = %s')
        params.append(hash_password(password))
    
    if 'role' in data:
        role = data['role']
        if role not in ['admin', 'operator', 'viewer']:
            return jsonify({'error': 'Invalid role'}), 400
        
        # Get current user's role and target user's role
        target_user, _ = execute_query(
            "SELECT role FROM gui_users WHERE user_id = %s", (user_id,)
        )
        if not target_user:
            return jsonify({'error': 'User not found'}), 404
        
        target_current_role = target_user[0]['role']
        
        # RULE 1: Admin cannot demote themselves
        if user_id == current_user_id and role != 'admin':
            return jsonify({'error': 'You cannot change your own admin role. Transfer admin to another user first.'}), 400
        
        # RULE 2: Only one admin - if promoting someone to admin, demote current admin to operator
        if role == 'admin' and target_current_role != 'admin':
            # Demote current admin (the one making this request) to operator
            demote_result, demote_error = execute_query(
                "UPDATE gui_users SET role = 'operator' WHERE user_id = %s",
                (current_user_id,),
                fetch=False, commit=True
            )
            if demote_error:
                return jsonify({'error': f'Failed to transfer admin: {demote_error}'}), 500
            
            # Update session to reflect new role
            session['role'] = 'operator'
        
        updates.append('role = %s')
        params.append(role)
    
    if 'is_active' in data:
        # Prevent deactivating yourself
        if not data['is_active'] and user_id == current_user_id:
            return jsonify({'error': 'Cannot deactivate your own account'}), 400
        
        updates.append('is_active = %s')
        params.append(data['is_active'])
    
    if not updates:
        return jsonify({'error': 'No fields to update'}), 400
    
    params.append(user_id)
    query = f"UPDATE gui_users SET {', '.join(updates)} WHERE user_id = %s"
    
    result, error = execute_query(query, params, fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    
    # Check if current user's role was changed (admin transfer)
    response_data = {'success': True}
    if session.get('role') == 'operator' and 'role' in data and data['role'] == 'admin':
        response_data['role_transferred'] = True
        response_data['new_role'] = 'operator'
        response_data['message'] = 'Admin role transferred. You are now an operator.'
    
    return jsonify(response_data)

@app.route('/api/users/<int:user_id>', methods=['DELETE'])
@admin_required
def delete_user(user_id):
    """Delete a user (admin only)"""
    current_user_id = session.get('user_id')
    
    # Prevent self-deletion - admin must transfer role first
    if user_id == current_user_id:
        return jsonify({'error': 'Cannot delete your own account. First, designate another user as admin.'}), 400
    
    # Prevent deleting the last admin
    user_check, _ = execute_query(
        "SELECT role FROM gui_users WHERE user_id = %s",
        (user_id,)
    )
    if not user_check:
        return jsonify({'error': 'User not found'}), 404
    
    if user_check[0]['role'] == 'admin':
        admin_count, _ = execute_query(
            "SELECT COUNT(*) as count FROM gui_users WHERE role = 'admin' AND user_id != %s",
            (user_id,)
        )
        if admin_count and admin_count[0]['count'] == 0:
            return jsonify({'error': 'Cannot delete the last admin user'}), 400
    
    query = "DELETE FROM gui_users WHERE user_id = %s"
    result, error = execute_query(query, (user_id,), fetch=False, commit=True)
    if error:
        return jsonify({'error': error}), 500
    
    return jsonify({'success': True})

# =============================================================================
# API: DASHBOARD STATS
# =============================================================================

@app.route('/api/dashboard', methods=['GET'])
@login_required
def get_dashboard():
    stats = {}
    
    # Count models
    result, _ = execute_query("SELECT COUNT(*) as count FROM model_config")
    stats['models'] = result[0]['count'] if result else 0
    
    # Count devices (configured modules)
    result, _ = execute_query("SELECT COUNT(*) as count FROM devices")
    stats['devices'] = result[0]['count'] if result else 0
    
    # Count connected devices
    result, _ = execute_query("SELECT COUNT(*) as count FROM device_status WHERE is_connected = 1")
    stats['connected_devices'] = result[0]['count'] if result else 0
    
    # Count I/O definitions
    result, _ = execute_query("SELECT COUNT(*) as count FROM model_io_definition")
    stats['io_definitions'] = result[0]['count'] if result else 0
    
    # Get operation mode
    result, _ = execute_query("SELECT operation_mode FROM plc_settings LIMIT 1")
    stats['operation_mode'] = result[0]['operation_mode'] if result else 'unknown'
    
    # Count aggregated mappings
    result, _ = execute_query("SELECT COUNT(*) as count FROM aggregated_io_map")
    stats['aggregated_mappings'] = result[0]['count'] if result else 0
    
    # Count secure state mappings
    result, _ = execute_query("SELECT COUNT(*) as count FROM model_secure_state_mapping")
    stats['secure_state_mappings'] = result[0]['count'] if result else 0
    
    return jsonify(stats)



# =============================================================================
# MAIN
# =============================================================================

if __name__ == '__main__':
    # SSL Context for HTTPS
    import os
    current_dir = os.path.dirname(os.path.abspath(__file__))
    # Go up one level to 'services' then into 'certs'
    cert_dir = os.path.join(os.path.dirname(current_dir), 'certs')
    cert_file = os.path.join(cert_dir, 'cert.pem')
    key_file = os.path.join(cert_dir, 'key.pem')
    
    if os.path.exists(cert_file) and os.path.exists(key_file):
        app.run(host=GUI_HOST, port=GUI_PORT, debug=False, ssl_context=(cert_file, key_file))
    else:
        print(f"Warning: SSL certificates not found at {cert_dir}. Running in HTTP mode.")
        app.run(host=GUI_HOST, port=GUI_PORT, debug=False)
