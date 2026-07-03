/**
 * @file plc_osologic_database.sql
 * @author Diego Arcos Sapena
 * @brief PLC database schema definition
 * @version a-1.0.0
 * @date 2024/11/23
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

-- Drops the database if it exists to ensure a clean setup.
DROP DATABASE IF EXISTS PLC;

-- Creates the new database.
CREATE DATABASE PLC;

-- Selects the database to use for subsequent commands.
USE PLC;

-- =================================================================================
-- DEFINITION TABLES (Configured by the user, they define WHAT exists)
-- =================================================================================

-- Model Templates Library (Hardware Definition)
-- Contains intrinsic device information. All devices of this model ARE like this.
CREATE TABLE model_config (
    `model_id` INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `model_name` VARCHAR(100) NOT NULL UNIQUE COMMENT 'A descriptive name, e.g., ''Borrell 8ED''',
    `protocol` ENUM('borrell-spi', 'modbus-rtu', 'modbus-tcp', 'aggregated') NOT NULL,
    `default_timeout_ms` INT UNSIGNED DEFAULT 1000 COMMENT 'Default timeout suggested by manufacturer/model',
    `max_read_bit_block_size` SMALLINT UNSIGNED NOT NULL DEFAULT 16 COMMENT 'Max bits per Modbus block read. 0 = unlimited/default.',
    `max_read_register_block_size` SMALLINT UNSIGNED NOT NULL DEFAULT 16 COMMENT 'Max registers per Modbus block read. 0 = unlimited/default.',
    `max_write_bit_block_size` SMALLINT UNSIGNED NOT NULL DEFAULT 16 COMMENT 'Max bits per Modbus block write. 0 = unlimited/default.',
    `max_write_register_block_size` SMALLINT UNSIGNED NOT NULL DEFAULT 16 COMMENT 'Max registers per Modbus block write. 0 = unlimited/default.'
);

-- Module Configuration Registry (Instance Configuration)
-- Contains "who is" and "where is". Parameters defined when installing a device in the project.
CREATE TABLE devices (
    `module_id` INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `module_name` VARCHAR(100) NOT NULL,
    `fk_model_id` INT UNSIGNED,                          -- Foreign key to the model templates table.
    `channel` ENUM('spi', 'rs485', 'tcp', 'aggregated') NOT NULL,
    `connection_string` VARCHAR(255) NOT NULL, -- For RS-485: '/dev/ttyUSB0', for TCP: '192.168.1.50:502'
    `address_on_channel` VARCHAR(50) NOT NULL,             -- For RS-485: '5' (Slave ID), for SPI: '0' (Slot), for TCP: can be empty
    `timeout_ms` INT UNSIGNED DEFAULT NULL COMMENT 'Instance-specific timeout. If NULL, uses model default.',

    UNIQUE KEY `unique_device_on_bus` (`channel`, `connection_string`, `address_on_channel`),
    FOREIGN KEY (`fk_model_id`) REFERENCES `model_config`(`model_id`) ON DELETE SET NULL
);

-- Runtime Status Registry (Live Observations)
-- Contains only live state. Deleting this data doesn't lose configuration.
CREATE TABLE device_status (
    `fk_module_id` INT UNSIGNED NOT NULL PRIMARY KEY,
    `is_connected` BOOLEAN NOT NULL DEFAULT FALSE,
    `last_seen` TIMESTAMP NULL,
    
    FOREIGN KEY (`fk_module_id`) REFERENCES `devices`(`module_id`) ON DELETE CASCADE
);


-- 2. Master I/O definition table
CREATE TABLE `model_io_definition` (
    `io_definition_id` INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `fk_model_id` INT UNSIGNED NOT NULL,
    
    -- Logical Attributes
    `logical_address` SMALLINT UNSIGNED NOT NULL,
    `io_type` ENUM('bit', 'register') NOT NULL,
    `purpose` ENUM('standard', 'secure_state', 'config') NOT NULL,
    `register_count` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Number of contiguous 16-bit registers this point occupies (e.g., 2 for a 32-bit value)',
    `endianess` ENUM('big', 'little') NOT NULL DEFAULT 'big' COMMENT 'Byte order for multi-register values',
    `default_io_label` VARCHAR(100) NULL COMMENT 'A default label for the I/O point, can be overridden by user',
    
    -- Explicit Hardware Access Type. Needs to be the same as defined in the device.
    `hardware_access` ENUM('readonly', 'readwrite') NOT NULL COMMENT 'Corresponds to input (readonly) vs output (readwrite)',

    -- Physical Attributes
    `physical_address` SMALLINT UNSIGNED,
    `access_method` ENUM('direct', 'bitmask') NOT NULL DEFAULT 'direct',
    `bitmask_offset` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'For bitmask access, the bit within the register',

    -- Constraint to ensure logical uniqueness
    UNIQUE KEY `unique_logical_point` (`fk_model_id`, `logical_address`, `io_type`, `hardware_access`), -- As Modbus
    -- CHECK (NOT (io_type = 'bit' AND access_method = 'bitmask')),
    -- Constraint to ensure physical uniqueness and compatibility with Modbus protocol, where each type of data has its own memory space
    UNIQUE KEY `unique_physical_point` (`fk_model_id`, `physical_address`, `io_type`, `hardware_access`, `bitmask_offset`),

    FOREIGN KEY (`fk_model_id`) REFERENCES `model_config`(`model_id`) ON DELETE CASCADE
);


-- Aggregated Model Children (Model Level)
-- Defines which child models compose an aggregated model, with ordering.
-- The slot_index defines the order of children: 0, 1, 2, ...
-- At device instantiation level, the connection_string lists child module instance IDs
-- in the same slot order defined here.
CREATE TABLE `aggregated_model_children` (
    `fk_aggregated_model_id` INT UNSIGNED NOT NULL,
    `slot_index` TINYINT UNSIGNED NOT NULL COMMENT 'Order of the child within the aggregated model (0, 1, 2, ...)',
    `fk_child_model_id` INT UNSIGNED NOT NULL COMMENT 'The model_id of the child module in this slot',

    PRIMARY KEY (`fk_aggregated_model_id`, `slot_index`),

    FOREIGN KEY (`fk_aggregated_model_id`) REFERENCES `model_config`(`model_id`) ON DELETE CASCADE,
    FOREIGN KEY (`fk_child_model_id`) REFERENCES `model_config`(`model_id`) ON DELETE CASCADE
);

-- Aggregated I/O Map (Model Level)
-- Maps aggregated I/O definitions to child model I/O definitions.
-- Uses child_slot_index to reference a slot in aggregated_model_children,
-- not a specific device instance. This makes the mapping reusable across
-- all instances of the same aggregated model.
CREATE TABLE `aggregated_io_map` (
    `map_id` INT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- A unique ID for the mapping itself

    -- This defines the AGGREGATED I/O point (the "destination")
    `fk_aggregated_io_definition_id` INT UNSIGNED NOT NULL,

    -- This defines the CHILD slot and I/O point (the "source") at the model level
    `child_slot_index` TINYINT UNSIGNED NOT NULL COMMENT 'References the slot_index in aggregated_model_children',
    `fk_child_io_definition_id` INT UNSIGNED NOT NULL,

    -- A unique constraint to prevent mapping the same aggregated point twice
    UNIQUE KEY `unique_aggregated_point_mapping` (`fk_aggregated_io_definition_id`),
    
    FOREIGN KEY (`fk_aggregated_io_definition_id`) REFERENCES `model_io_definition`(`io_definition_id`) ON DELETE CASCADE,
    FOREIGN KEY (`fk_child_io_definition_id`) REFERENCES `model_io_definition`(`io_definition_id`) ON DELETE CASCADE
);

-- Secure State Mapping Table (Model Level)
-- This table defines which secure state value should be applied to each output
-- when setSafeState() is called. The mapping is defined at the model level,
-- so all modules of the same model share the same mapping.
CREATE TABLE `model_secure_state_mapping` (
    `fk_model_id` INT UNSIGNED NOT NULL,
    `fk_standard_io_definition_id` INT UNSIGNED NOT NULL,
    `fk_secure_state_io_definition_id` INT UNSIGNED NOT NULL,
    
    PRIMARY KEY (`fk_model_id`, `fk_standard_io_definition_id`),
    UNIQUE KEY `unique_secure_mapping` (`fk_model_id`, `fk_secure_state_io_definition_id`),
    
    FOREIGN KEY (`fk_model_id`) REFERENCES `model_config`(`model_id`) ON DELETE CASCADE,
    FOREIGN KEY (`fk_standard_io_definition_id`) REFERENCES `model_io_definition`(`io_definition_id`) ON DELETE CASCADE,
    FOREIGN KEY (`fk_secure_state_io_definition_id`) REFERENCES `model_io_definition`(`io_definition_id`) ON DELETE CASCADE
);

CREATE TABLE `module_io_config` (
    -- Composite Primary Key
    `fk_module_id` INT UNSIGNED NOT NULL,
    `fk_io_definition_id` INT UNSIGNED NOT NULL,

    -- 1. Identification (Former io_point_labels table)
    `user_label` VARCHAR(100) NOT NULL, 
    `units` VARCHAR(20) DEFAULT NULL COMMENT 'Engineering units (e.g. kV, mm, bar)',


    -- 2. Data Processing (Moved from model_io_definition)
    `scale_factor` DECIMAL(10, 4) NOT NULL DEFAULT 1.0000,
    `offset` DECIMAL(10, 4) NOT NULL DEFAULT 0.0000,

    -- 3. Visualization and Behavior (Former visibility_config table)
    `visibility` ENUM('visible', 'hidden') NOT NULL DEFAULT 'visible',
    `visibility_mode` ENUM('periodically', 'changes') NOT NULL DEFAULT 'periodically',
    `refresh_rate` SMALLINT UNSIGNED NOT NULL DEFAULT 1,

    -- 4. Synchronization Control
    `sync` BOOLEAN NOT NULL DEFAULT TRUE DEFAULT 1 COMMENT 'Whether this I/O should be synchronized in current operation mode',

    -- Constraints
    PRIMARY KEY (`fk_module_id`, `fk_io_definition_id`),
    UNIQUE KEY `unique_label_per_project` (`user_label`),
    
    FOREIGN KEY (`fk_module_id`) REFERENCES `devices`(`module_id`) ON DELETE CASCADE,
    FOREIGN KEY (`fk_io_definition_id`) REFERENCES `model_io_definition`(`io_definition_id`) ON DELETE CASCADE
);



-- =================================================================================
-- REAL-TIME STATE TABLES (Constantly updated by the program)
-- =================================================================================


CREATE TABLE `rtmirror` (
    `fk_module_id` INT UNSIGNED NOT NULL,
    `fk_io_definition_id` INT UNSIGNED NOT NULL,
    -- Scaling factors (copied from module_io_config on INSERT, synced via trigger on UPDATE)
    `scale_factor` DECIMAL(10, 4) NOT NULL DEFAULT 1.0000,
    `offset` DECIMAL(10, 4) NOT NULL DEFAULT 0.0000,
    -- RAW values (direct hardware representation)
    `value` BIGINT UNSIGNED DEFAULT NULL,
    `required_value` BIGINT UNSIGNED DEFAULT NULL,
    -- NET values (calculated using local scale_factor/offset - no JOIN needed)
    `net_value` DOUBLE DEFAULT NULL,
    `net_required_value` DOUBLE DEFAULT NULL,
    `timestamp` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`fk_module_id`, `fk_io_definition_id`),
    FOREIGN KEY (`fk_module_id`) REFERENCES `devices`(`module_id`) ON DELETE CASCADE,
    FOREIGN KEY (`fk_io_definition_id`) REFERENCES `model_io_definition`(`io_definition_id`) ON DELETE CASCADE
) ENGINE=MEMORY;


-- =================================================================================
-- PLC GENERAL SETTINGS
-- =================================================================================

-- Stores global configuration parameters for the PLC core application.
CREATE TABLE plc_settings (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `rs485_baud_rate` INT UNSIGNED NOT NULL COMMENT 'Serial communication speed in bits per second (e.g., 9600, 19200)',
    `rs485_data_bits` TINYINT UNSIGNED NOT NULL COMMENT 'Number of data bits (typically 7 or 8)',
    `rs485_parity` CHAR(1) NOT NULL COMMENT 'Parity setting: N=None, E=Even, O=Odd',
    `rs485_stop_bits` TINYINT UNSIGNED NOT NULL COMMENT 'Number of stop bits (typically 1 or 2)',
    `operation_mode` ENUM('execution', 'configuration') NOT NULL DEFAULT 'execution' COMMENT 'PLC operation mode'
);

-- =================================================================================
-- TRIGGERS AND INITIAL DATA
-- =================================================================================

-- NOTE: module_io_config rows are created by the GUI wizard, NOT by a trigger.
-- This ensures transactional integrity and allows the user to configure labels before saving.

DELIMITER //
-- Trigger: Create device_status row when a new module is configured
CREATE TRIGGER `after_devices_insert_init_status`
AFTER INSERT ON `devices`
FOR EACH ROW
BEGIN
    INSERT INTO `device_status` (`fk_module_id`, `is_connected`, `last_seen`)
    VALUES (NEW.module_id, FALSE, NULL);
END//

DELIMITER ;

-- =================================================================================
-- RTMIRROR TRIGGERS: Bidirectional RAW <-> NET synchronization
-- Scale factor and offset are fetched via JOIN from module_io_config (always up-to-date)
-- =================================================================================

-- Trigger: INSERT - Copy scale_factor/offset from module_io_config, calculate NET
DELIMITER //
CREATE TRIGGER `rtmirror_before_insert`
BEFORE INSERT ON `rtmirror`
FOR EACH ROW
BEGIN
    DECLARE v_scale DECIMAL(10,4) DEFAULT 1.0000;
    DECLARE v_offset DECIMAL(10,4) DEFAULT 0.0000;
    
    -- Get scale_factor and offset from module_io_config (only on INSERT)
    SELECT scale_factor, `offset` INTO v_scale, v_offset
    FROM module_io_config
    WHERE fk_module_id = NEW.fk_module_id 
    AND fk_io_definition_id = NEW.fk_io_definition_id
    LIMIT 1;
    
    -- Store in local columns for fast access on future UPDATEs
    SET NEW.scale_factor = COALESCE(v_scale, 1.0000);
    SET NEW.offset = COALESCE(v_offset, 0.0000);
    
    -- Calculate net_value if value is provided
    IF NEW.value IS NOT NULL THEN
        SET NEW.net_value = NEW.value * NEW.scale_factor + NEW.offset;
    END IF;
    
    -- Calculate net_required_value if required_value is provided
    IF NEW.required_value IS NOT NULL THEN
        SET NEW.net_required_value = NEW.required_value * NEW.scale_factor + NEW.offset;
    END IF;
END//
DELIMITER ;

-- Trigger: UPDATE - Use local scale_factor/offset (NO JOIN = FAST)
DELIMITER //
CREATE TRIGGER `rtmirror_before_update`
BEFORE UPDATE ON `rtmirror`
FOR EACH ROW
BEGIN
    -- If value (RAW) changed, recalculate net_value using LOCAL columns
    IF NEW.value IS NOT NULL AND (OLD.value IS NULL OR NEW.value != OLD.value) THEN
        SET NEW.net_value = NEW.value * NEW.scale_factor + NEW.offset;
    END IF;
    
    -- If required_value (RAW) changed, recalculate net_required_value
    IF NEW.required_value IS NOT NULL AND (OLD.required_value IS NULL OR NEW.required_value != OLD.required_value) THEN
        SET NEW.net_required_value = NEW.required_value * NEW.scale_factor + NEW.offset;
    END IF;
    
    -- If net_required_value (NET) changed, calculate required_value (RAW)
    IF NEW.net_required_value IS NOT NULL 
       AND (OLD.net_required_value IS NULL OR NEW.net_required_value != OLD.net_required_value)
       AND (OLD.required_value IS NULL OR NEW.required_value = OLD.required_value) THEN
        IF NEW.scale_factor != 0 THEN
            SET NEW.required_value = CAST((NEW.net_required_value - NEW.offset) / NEW.scale_factor AS UNSIGNED);
        END IF;
    END IF;
END//
DELIMITER ;

-- Trigger: Sync scale_factor/offset from module_io_config to rtmirror when changed
DELIMITER //
CREATE TRIGGER `module_io_config_after_update`
AFTER UPDATE ON `module_io_config`
FOR EACH ROW
BEGIN
    IF NEW.scale_factor != OLD.scale_factor OR NEW.`offset` != OLD.`offset` THEN
        UPDATE rtmirror
        SET 
            scale_factor = NEW.scale_factor,
            `offset` = NEW.`offset`,
            net_value = CASE WHEN value IS NOT NULL THEN value * NEW.scale_factor + NEW.`offset` ELSE NULL END,
            net_required_value = CASE WHEN required_value IS NOT NULL THEN required_value * NEW.scale_factor + NEW.`offset` ELSE NULL END
        WHERE fk_module_id = NEW.fk_module_id 
        AND fk_io_definition_id = NEW.fk_io_definition_id;
    END IF;
END//
DELIMITER ;

-- Trigger: Automatically propagates new IO Definitions to existing devices
-- (Deletion is handled automatically by ON DELETE CASCADE foreign key)
DELIMITER //

CREATE TRIGGER `after_model_io_definition_insert`
AFTER INSERT ON `model_io_definition`
FOR EACH ROW
BEGIN
    -- Insert configuration for ALL devices using this model
    INSERT IGNORE INTO `module_io_config` 
    (`fk_module_id`, `fk_io_definition_id`, `user_label`, `scale_factor`, `offset`, `visibility`)
    SELECT 
        d.module_id, 
        NEW.io_definition_id, 
        -- Generates a name combining: Default Label + Module ID
        CONCAT(COALESCE(NULLIF(NEW.default_io_label, ''), NEW.io_type), ' (Dev ', d.module_id, ')'), 
        1.0000, 
        0.0000, 
        'visible'
    FROM devices d
    WHERE d.fk_model_id = NEW.fk_model_id;
END//
DELIMITER ;


-- Sets the transaction isolation level for the DB session.
SET GLOBAL TRANSACTION ISOLATION LEVEL READ COMMITTED;

INSERT INTO `model_config` (`model_id`, `model_name`, `protocol`, `default_timeout_ms`, `max_read_bit_block_size`, `max_read_register_block_size`, `max_write_bit_block_size`, `max_write_register_block_size`) VALUES
-- SPI (no timeout needed for SPI)
(16, 'Borrell 8SD', 'borrell-spi', NULL, 128, 128, 128, 128),
(17, 'Borrell 8ED', 'borrell-spi', NULL, 128, 128, 128, 128),
(18, 'Borrell 8SA', 'borrell-spi', NULL, 128, 128, 128, 128),
(19, 'Borrell 8EA', 'borrell-spi', NULL, 128, 128, 128, 128),
(20, 'Borrell 32SD', 'borrell-spi', NULL, 128, 128, 128, 128),

-- RS-485
(5, 'RS-485 I8SD', 'modbus-rtu', 1000, 8, 8, 8, 8),
(6, 'RS-485 I8ED', 'modbus-rtu', 1000, 8, 8, 8, 8),
(7, 'RS-485 iBC24SD', 'modbus-rtu', 1000, 8, 8, 8, 8),

-- TCP/IP
(3, 'ETH 8ED', 'modbus-tcp', 1000, 128, 128, 128, 128),
(4, 'ETH 8SD', 'modbus-tcp', 1000, 128, 128, 128, 128);

INSERT INTO `plc_settings`
(`rs485_baud_rate`, `rs485_data_bits`, `rs485_parity`, `rs485_stop_bits`, `operation_mode`)
VALUES
(19200, 8, 'N', 1, 'execution');

INSERT INTO `model_io_definition`
(`fk_model_id`, `logical_address`, `io_type`, `purpose`, `hardware_access`, `physical_address`, `access_method`, `bitmask_offset`, `register_count`, `endianess`, `default_io_label`)
VALUES

-- ============================ SPI MODULES ============================
-- TODO: PARA LAS TARJETAS SPI SE HA MAPEADO EN LAS DIRECCIONES LÓGICAS 100, 101, 102, 103 DE REGISTROS DE CONFIGURACION EL UUID, WDT, STARTS Y FIRMWARE VERSION. COMO NO HAY DOCUMENTACION DE ESTAS TARJETAS, SOLO SE MAPEA LO QUE SE CONOCE. SI EN UN FUTURO SE CONOCE SI HAY, Y QUE HACEN, OTROS REGISTROS, SE PONDRAN.

-- Model 16: Borrell 8SD (8 Outputs, 8 Secure States)
(16, 0, 'bit', 'standard', 'readwrite', 0, 'direct', 0, 1, 'little', 'Output 1'),
(16, 1, 'bit', 'standard', 'readwrite', 1, 'direct', 0, 1, 'little', 'Output 2'),
(16, 2, 'bit', 'standard', 'readwrite', 2, 'direct', 0, 1, 'little', 'Output 3'),
(16, 3, 'bit', 'standard', 'readwrite', 3, 'direct', 0, 1, 'little', 'Output 4'),
(16, 4, 'bit', 'standard', 'readwrite', 4, 'direct', 0, 1, 'little', 'Output 5'),
(16, 5, 'bit', 'standard', 'readwrite', 5, 'direct', 0, 1, 'little', 'Output 6'),
(16, 6, 'bit', 'standard', 'readwrite', 6, 'direct', 0, 1, 'little', 'Output 7'),
(16, 7, 'bit', 'standard', 'readwrite', 7, 'direct', 0, 1, 'little', 'Output 8'),
(16, 8, 'bit', 'secure_state', 'readwrite', 8, 'direct', 0, 1, 'little', 'Safe State Output 1'),
(16, 9, 'bit', 'secure_state', 'readwrite', 9, 'direct', 0, 1, 'little', 'Safe State Output 2'),
(16, 10, 'bit', 'secure_state', 'readwrite', 10, 'direct', 0, 1, 'little', 'Safe State Output 3'),
(16, 11, 'bit', 'secure_state', 'readwrite', 11, 'direct', 0, 1, 'little', 'Safe State Output 4'),
(16, 12, 'bit', 'secure_state', 'readwrite', 12, 'direct', 0, 1, 'little', 'Safe State Output 5'),
(16, 13, 'bit', 'secure_state', 'readwrite', 13, 'direct', 0, 1, 'little', 'Safe State Output 6'),
(16, 14, 'bit', 'secure_state', 'readwrite', 14, 'direct', 0, 1, 'little', 'Safe State Output 7'),
(16, 15, 'bit', 'secure_state', 'readwrite', 15, 'direct', 0, 1, 'little', 'Safe State Output 8'),
-- Physical address not real but stablished at a distance of 2 registers in order to put each register in a separate block as it is intended in core
(16, 100, 'register', 'config', 'readwrite', 17, 'direct', 0, 1, 'little', 'Watchdog Timeout'), -- Watchdog timer configuration in deciseconds
(16, 101, 'register', 'config', 'readwrite', 19, 'direct', 0, 2, 'big', 'Device UUID'),
(16, 102, 'register', 'config', 'readonly', 21, 'direct', 0, 1, 'little', 'Start Counter'),
(16, 103, 'register', 'config', 'readonly', 23, 'direct', 0, 1, 'little', 'Firmware Version'),
(16, 104, 'register', 'config', 'readonly', 25, 'direct', 0, 1, 'little', 'Device Type ID'),
(16, 105, 'register', 'config', 'readonly', 27, 'direct', 0, 1, 'little', 'Num Bit Channels'),
(16, 106, 'register', 'config', 'readonly', 29, 'direct', 0, 1, 'little', 'Num Register Channels'),

-- Model 17: Borrell 8ED (8 Inputs)
(17, 0, 'bit', 'standard', 'readonly', 0, 'direct', 0, 1, 'little', 'Input 1'),
(17, 1, 'bit', 'standard', 'readonly', 1, 'direct', 0, 1, 'little', 'Input 2'),
(17, 2, 'bit', 'standard', 'readonly', 2, 'direct', 0, 1, 'little', 'Input 3'),
(17, 3, 'bit', 'standard', 'readonly', 3, 'direct', 0, 1, 'little', 'Input 4'),
(17, 4, 'bit', 'standard', 'readonly', 4, 'direct', 0, 1, 'little', 'Input 5'),
(17, 5, 'bit', 'standard', 'readonly', 5, 'direct', 0, 1, 'little', 'Input 6'),
(17, 6, 'bit', 'standard', 'readonly', 6, 'direct', 0, 1, 'little', 'Input 7'),
(17, 7, 'bit', 'standard', 'readonly', 7, 'direct', 0, 1, 'little', 'Input 8'),
-- Physical address not real but stablished at a distance of 2 registers in order to put each register in a separate block as it is intended in core
(17, 100, 'register', 'config', 'readwrite', 17, 'direct', 0, 1, 'little', 'Watchdog Timeout'), -- Watchdog timer configuration in deciseconds
(17, 101, 'register', 'config', 'readwrite', 19, 'direct', 0, 2, 'big', 'Device UUID'),
(17, 102, 'register', 'config', 'readonly', 21, 'direct', 0, 1, 'little', 'Start Counter'),
(17, 103, 'register', 'config', 'readonly', 23, 'direct', 0, 1, 'little', 'Firmware Version'),
(17, 104, 'register', 'config', 'readonly', 25, 'direct', 0, 1, 'little', 'Device Type ID'),
(17, 105, 'register', 'config', 'readonly', 27, 'direct', 0, 1, 'little', 'Num Bit Channels'),
(17, 106, 'register', 'config', 'readonly', 29, 'direct', 0, 1, 'little', 'Num Register Channels'),

-- Model 18: Borrell 8SA (8 Analog Outputs, 8 Secure States)
(18, 0, 'register', 'standard', 'readwrite', 0, 'direct', 0, 1, 'little', 'Analog Output 1'),
(18, 1, 'register', 'standard', 'readwrite', 1, 'direct', 0, 1, 'little', 'Analog Output 2'),
(18, 2, 'register', 'standard', 'readwrite', 2, 'direct', 0, 1, 'little', 'Analog Output 3'),
(18, 3, 'register', 'standard', 'readwrite', 3, 'direct', 0, 1, 'little', 'Analog Output 4'),
(18, 4, 'register', 'standard', 'readwrite', 4, 'direct', 0, 1, 'little', 'Analog Output 5'),
(18, 5, 'register', 'standard', 'readwrite', 5, 'direct', 0, 1, 'little', 'Analog Output 6'),
(18, 6, 'register', 'standard', 'readwrite', 6, 'direct', 0, 1, 'little', 'Analog Output 7'),
(18, 7, 'register', 'standard', 'readwrite', 7, 'direct', 0, 1, 'little', 'Analog Output 8'),
(18, 8, 'register', 'secure_state', 'readwrite', 8, 'direct', 0, 1, 'little', 'Safe State Analog 1'),
(18, 9, 'register', 'secure_state', 'readwrite', 9, 'direct', 0, 1, 'little', 'Safe State Analog 2'),
(18, 10, 'register', 'secure_state', 'readwrite', 10, 'direct', 0, 1, 'little', 'Safe State Analog 3'),
(18, 11, 'register', 'secure_state', 'readwrite', 11, 'direct', 0, 1, 'little', 'Safe State Analog 4'),
(18, 12, 'register', 'secure_state', 'readwrite', 12, 'direct', 0, 1, 'little', 'Safe State Analog 5'),
(18, 13, 'register', 'secure_state', 'readwrite', 13, 'direct', 0, 1, 'little', 'Safe State Analog 6'),
(18, 14, 'register', 'secure_state', 'readwrite', 14, 'direct', 0, 1, 'little', 'Safe State Analog 7'),
(18, 15, 'register', 'secure_state', 'readwrite', 15, 'direct', 0, 1, 'little', 'Safe State Analog 8'),
-- Physical address not real but stablished at a distance of 2 registers in order to put each register in a separate block as it is intended in core
(18, 100, 'register', 'config', 'readwrite', 17, 'direct', 0, 1, 'little', 'Watchdog Timeout'), -- Watchdog timer configuration in deciseconds
(18, 101, 'register', 'config', 'readwrite', 19, 'direct', 0, 2, 'big', 'Device UUID'),
(18, 102, 'register', 'config', 'readonly', 21, 'direct', 0, 1, 'little', 'Start Counter'),
(18, 103, 'register', 'config', 'readonly', 23, 'direct', 0, 1, 'little', 'Firmware Version'),
(18, 104, 'register', 'config', 'readonly', 25, 'direct', 0, 1, 'little', 'Device Type ID'),
(18, 105, 'register', 'config', 'readonly', 27, 'direct', 0, 1, 'little', 'Num Bit Channels'),
(18, 106, 'register', 'config', 'readonly', 29, 'direct', 0, 1, 'little', 'Num Register Channels'),

-- Model 19: Borrell 8EA (8 Analog Inputs)
(19, 0, 'register', 'standard', 'readonly', 0, 'direct', 0, 1, 'little', 'Analog Input 1'),
(19, 1, 'register', 'standard', 'readonly', 1, 'direct', 0, 1, 'little', 'Analog Input 2'),
(19, 2, 'register', 'standard', 'readonly', 2, 'direct', 0, 1, 'little', 'Analog Input 3'),
(19, 3, 'register', 'standard', 'readonly', 3, 'direct', 0, 1, 'little', 'Analog Input 4'),
(19, 4, 'register', 'standard', 'readonly', 4, 'direct', 0, 1, 'little', 'Analog Input 5'),
(19, 5, 'register', 'standard', 'readonly', 5, 'direct', 0, 1, 'little', 'Analog Input 6'),
(19, 6, 'register', 'standard', 'readonly', 6, 'direct', 0, 1, 'little', 'Analog Input 7'),
(19, 7, 'register', 'standard', 'readonly', 7, 'direct', 0, 1, 'little', 'Analog Input 8'),
-- Physical address not real but stablished at a distance of 2 registers in order to put each register in a separate block as it is intended in core
(19, 100, 'register', 'config', 'readwrite', 17, 'direct', 0, 1, 'little', 'Watchdog Timeout'), -- Watchdog timer configuration in deciseconds
(19, 101, 'register', 'config', 'readwrite', 19, 'direct', 0, 2, 'big', 'Device UUID'),
(19, 102, 'register', 'config', 'readonly', 21, 'direct', 0, 1, 'little', 'Start Counter'),
(19, 103, 'register', 'config', 'readonly', 23, 'direct', 0, 1, 'little', 'Firmware Version'),
(19, 104, 'register', 'config', 'readonly', 25, 'direct', 0, 1, 'little', 'Device Type ID'),
(19, 105, 'register', 'config', 'readonly', 27, 'direct', 0, 1, 'little', 'Num Bit Channels'),
(19, 106, 'register', 'config', 'readonly', 29, 'direct', 0, 1, 'little', 'Num Register Channels'),

-- Model 20: Borrell 32SD (32 Outputs, 32 Secure States)
(20, 0, 'bit', 'standard', 'readwrite', 0, 'direct', 0, 1, 'little', 'Output 1'),
(20, 1, 'bit', 'standard', 'readwrite', 1, 'direct', 0, 1, 'little', 'Output 2'),
(20, 2, 'bit', 'standard', 'readwrite', 2, 'direct', 0, 1, 'little', 'Output 3'),
(20, 3, 'bit', 'standard', 'readwrite', 3, 'direct', 0, 1, 'little', 'Output 4'),
(20, 4, 'bit', 'standard', 'readwrite', 4, 'direct', 0, 1, 'little', 'Output 5'),
(20, 5, 'bit', 'standard', 'readwrite', 5, 'direct', 0, 1, 'little', 'Output 6'),
(20, 6, 'bit', 'standard', 'readwrite', 6, 'direct', 0, 1, 'little', 'Output 7'),
(20, 7, 'bit', 'standard', 'readwrite', 7, 'direct', 0, 1, 'little', 'Output 8'),
(20, 8, 'bit', 'standard', 'readwrite', 8, 'direct', 0, 1, 'little', 'Output 9'),
(20, 9, 'bit', 'standard', 'readwrite', 9, 'direct', 0, 1, 'little', 'Output 10'),
(20, 10, 'bit', 'standard', 'readwrite', 10, 'direct', 0, 1, 'little', 'Output 11'),
(20, 11, 'bit', 'standard', 'readwrite', 11, 'direct', 0, 1, 'little', 'Output 12'),
(20, 12, 'bit', 'standard', 'readwrite', 12, 'direct', 0, 1, 'little', 'Output 13'),
(20, 13, 'bit', 'standard', 'readwrite', 13, 'direct', 0, 1, 'little', 'Output 14'),
(20, 14, 'bit', 'standard', 'readwrite', 14, 'direct', 0, 1, 'little', 'Output 15'),
(20, 15, 'bit', 'standard', 'readwrite', 15, 'direct', 0, 1, 'little', 'Output 16'),
(20, 16, 'bit', 'standard', 'readwrite', 16, 'direct', 0, 1, 'little', 'Output 17'),
(20, 17, 'bit', 'standard', 'readwrite', 17, 'direct', 0, 1, 'little', 'Output 18'),
(20, 18, 'bit', 'standard', 'readwrite', 18, 'direct', 0, 1, 'little', 'Output 19'),
(20, 19, 'bit', 'standard', 'readwrite', 19, 'direct', 0, 1, 'little', 'Output 20'),
(20, 20, 'bit', 'standard', 'readwrite', 20, 'direct', 0, 1, 'little', 'Output 21'),
(20, 21, 'bit', 'standard', 'readwrite', 21, 'direct', 0, 1, 'little', 'Output 22'),
(20, 22, 'bit', 'standard', 'readwrite', 22, 'direct', 0, 1, 'little', 'Output 23'),
(20, 23, 'bit', 'standard', 'readwrite', 23, 'direct', 0, 1, 'little', 'Output 24'),
(20, 24, 'bit', 'standard', 'readwrite', 24, 'direct', 0, 1, 'little', 'Output 25'),
(20, 25, 'bit', 'standard', 'readwrite', 25, 'direct', 0, 1, 'little', 'Output 26'),
(20, 26, 'bit', 'standard', 'readwrite', 26, 'direct', 0, 1, 'little', 'Output 27'),
(20, 27, 'bit', 'standard', 'readwrite', 27, 'direct', 0, 1, 'little', 'Output 28'),
(20, 28, 'bit', 'standard', 'readwrite', 28, 'direct', 0, 1, 'little', 'Output 29'),
(20, 29, 'bit', 'standard', 'readwrite', 29, 'direct', 0, 1, 'little', 'Output 30'),
(20, 30, 'bit', 'standard', 'readwrite', 30, 'direct', 0, 1, 'little', 'Output 31'),
(20, 31, 'bit', 'standard', 'readwrite', 31, 'direct', 0, 1, 'little', 'Output 32'),
(20, 32, 'bit', 'secure_state', 'readwrite', 32, 'direct', 0, 1, 'little', 'Safe State Output 1'),
(20, 33, 'bit', 'secure_state', 'readwrite', 33, 'direct', 0, 1, 'little', 'Safe State Output 2'),
(20, 34, 'bit', 'secure_state', 'readwrite', 34, 'direct', 0, 1, 'little', 'Safe State Output 3'),
(20, 35, 'bit', 'secure_state', 'readwrite', 35, 'direct', 0, 1, 'little', 'Safe State Output 4'),
(20, 36, 'bit', 'secure_state', 'readwrite', 36, 'direct', 0, 1, 'little', 'Safe State Output 5'),
(20, 37, 'bit', 'secure_state', 'readwrite', 37, 'direct', 0, 1, 'little', 'Safe State Output 6'),
(20, 38, 'bit', 'secure_state', 'readwrite', 38, 'direct', 0, 1, 'little', 'Safe State Output 7'),
(20, 39, 'bit', 'secure_state', 'readwrite', 39, 'direct', 0, 1, 'little', 'Safe State Output 8'),
(20, 40, 'bit', 'secure_state', 'readwrite', 40, 'direct', 0, 1, 'little', 'Safe State Output 9'),
(20, 41, 'bit', 'secure_state', 'readwrite', 41, 'direct', 0, 1, 'little', 'Safe State Output 10'),
(20, 42, 'bit', 'secure_state', 'readwrite', 42, 'direct', 0, 1, 'little', 'Safe State Output 11'),
(20, 43, 'bit', 'secure_state', 'readwrite', 43, 'direct', 0, 1, 'little', 'Safe State Output 12'),
(20, 44, 'bit', 'secure_state', 'readwrite', 44, 'direct', 0, 1, 'little', 'Safe State Output 13'),
(20, 45, 'bit', 'secure_state', 'readwrite', 45, 'direct', 0, 1, 'little', 'Safe State Output 14'),
(20, 46, 'bit', 'secure_state', 'readwrite', 46, 'direct', 0, 1, 'little', 'Safe State Output 15'),
(20, 47, 'bit', 'secure_state', 'readwrite', 47, 'direct', 0, 1, 'little', 'Safe State Output 16'),
(20, 48, 'bit', 'secure_state', 'readwrite', 48, 'direct', 0, 1, 'little', 'Safe State Output 17'),
(20, 49, 'bit', 'secure_state', 'readwrite', 49, 'direct', 0, 1, 'little', 'Safe State Output 18'),
(20, 50, 'bit', 'secure_state', 'readwrite', 50, 'direct', 0, 1, 'little', 'Safe State Output 19'),
(20, 51, 'bit', 'secure_state', 'readwrite', 51, 'direct', 0, 1, 'little', 'Safe State Output 20'),
(20, 52, 'bit', 'secure_state', 'readwrite', 52, 'direct', 0, 1, 'little', 'Safe State Output 21'),
(20, 53, 'bit', 'secure_state', 'readwrite', 53, 'direct', 0, 1, 'little', 'Safe State Output 22'),
(20, 54, 'bit', 'secure_state', 'readwrite', 54, 'direct', 0, 1, 'little', 'Safe State Output 23'),
(20, 55, 'bit', 'secure_state', 'readwrite', 55, 'direct', 0, 1, 'little', 'Safe State Output 24'),
(20, 56, 'bit', 'secure_state', 'readwrite', 56, 'direct', 0, 1, 'little', 'Safe State Output 25'),
(20, 57, 'bit', 'secure_state', 'readwrite', 57, 'direct', 0, 1, 'little', 'Safe State Output 26'),
(20, 58, 'bit', 'secure_state', 'readwrite', 58, 'direct', 0, 1, 'little', 'Safe State Output 27'),
(20, 59, 'bit', 'secure_state', 'readwrite', 59, 'direct', 0, 1, 'little', 'Safe State Output 28'),
(20, 60, 'bit', 'secure_state', 'readwrite', 60, 'direct', 0, 1, 'little', 'Safe State Output 29'),
(20, 61, 'bit', 'secure_state', 'readwrite', 61, 'direct', 0, 1, 'little', 'Safe State Output 30'),
(20, 62, 'bit', 'secure_state', 'readwrite', 62, 'direct', 0, 1, 'little', 'Safe State Output 31'),
(20, 63, 'bit', 'secure_state', 'readwrite', 63, 'direct', 0, 1, 'little', 'Safe State Output 32'),
-- Physical address not real but stablished at a distance of 2 registers in order to put each register in a separate block as it is intended in core
(20, 100, 'register', 'config', 'readwrite', 17, 'direct', 0, 1, 'little', 'Watchdog Timeout'), -- Watchdog timer configuration in deciseconds
(20, 101, 'register', 'config', 'readwrite', 19, 'direct', 0, 2, 'big', 'Device UUID'),
(20, 102, 'register', 'config', 'readonly', 21, 'direct', 0, 1, 'little', 'Start Counter'),
(20, 103, 'register', 'config', 'readonly', 23, 'direct', 0, 1, 'little', 'Firmware Version'),
(20, 104, 'register', 'config', 'readonly', 25, 'direct', 0, 1, 'little', 'Device Type ID'),
(20, 105, 'register', 'config', 'readonly', 27, 'direct', 0, 1, 'little', 'Num Bit Channels'),
(20, 106, 'register', 'config', 'readonly', 29, 'direct', 0, 1, 'little', 'Num Register Channels'),

-- ============================ RS-485 MODULES ============================
-- Model 5: RS-485 I8SD (8 Outputs)
(5, 0, 'bit', 'standard', 'readwrite', 0, 'direct', 0, 1, 'little', 'Output 1'),
(5, 1, 'bit', 'standard', 'readwrite', 1, 'direct', 0, 1, 'little', 'Output 2'),
(5, 2, 'bit', 'standard', 'readwrite', 2, 'direct', 0, 1, 'little', 'Output 3'),
(5, 3, 'bit', 'standard', 'readwrite', 3, 'direct', 0, 1, 'little', 'Output 4'),
(5, 4, 'bit', 'standard', 'readwrite', 4, 'direct', 0, 1, 'little', 'Output 5'),
(5, 5, 'bit', 'standard', 'readwrite', 5, 'direct', 0, 1, 'little', 'Output 6'),
(5, 6, 'bit', 'standard', 'readwrite', 6, 'direct', 0, 1, 'little', 'Output 7'),
(5, 7, 'bit', 'standard', 'readwrite', 7, 'direct', 0, 1, 'little', 'Output 8'),

-- Timed Output Coils (Physical Coils 8-15)
(5, 8, 'bit', 'config', 'readwrite', 8, 'direct', 0, 1, 'little', 'Output 1 Timed Trigger'),
(5, 9, 'bit', 'config', 'readwrite', 9, 'direct', 0, 1, 'little', 'Output 2 Timed Trigger'),
(5, 10, 'bit', 'config', 'readwrite', 10, 'direct', 0, 1, 'little', 'Output 3 Timed Trigger'),
(5, 11, 'bit', 'config', 'readwrite', 11, 'direct', 0, 1, 'little', 'Output 4 Timed Trigger'),
(5, 12, 'bit', 'config', 'readwrite', 12, 'direct', 0, 1, 'little', 'Output 5 Timed Trigger'),
(5, 13, 'bit', 'config', 'readwrite', 13, 'direct', 0, 1, 'little', 'Output 6 Timed Trigger'),
(5, 14, 'bit', 'config', 'readwrite', 14, 'direct', 0, 1, 'little', 'Output 7 Timed Trigger'),
(5, 15, 'bit', 'config', 'readwrite', 15, 'direct', 0, 1, 'little', 'Output 8 Timed Trigger'),

-- Secure State Bits (Physical Holding Register 16, bits 0-7)
(5, 16, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 0, 1, 'little', 'Safe State Output 1'),
(5, 17, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 1, 1, 'little', 'Safe State Output 2'),
(5, 18, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 2, 1, 'little', 'Safe State Output 3'),
(5, 19, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 3, 1, 'little', 'Safe State Output 4'),
(5, 20, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 4, 1, 'little', 'Safe State Output 5'),
(5, 21, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 5, 1, 'little', 'Safe State Output 6'),
(5, 22, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 6, 1, 'little', 'Safe State Output 7'),
(5, 23, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 7, 1, 'little', 'Safe State Output 8'),

-- Initial State Bits (Physical Holding Register 17, bits 0-7)
(5, 24, 'register', 'config', 'readwrite', 17, 'bitmask', 0, 1, 'little', 'Output 1 Initial State'),
(5, 25, 'register', 'config', 'readwrite', 17, 'bitmask', 1, 1, 'little', 'Output 2 Initial State'),
(5, 26, 'register', 'config', 'readwrite', 17, 'bitmask', 2, 1, 'little', 'Output 3 Initial State'),
(5, 27, 'register', 'config', 'readwrite', 17, 'bitmask', 3, 1, 'little', 'Output 4 Initial State'),
(5, 28, 'register', 'config', 'readwrite', 17, 'bitmask', 4, 1, 'little', 'Output 5 Initial State'),
(5, 29, 'register', 'config', 'readwrite', 17, 'bitmask', 5, 1, 'little', 'Output 6 Initial State'),
(5, 30, 'register', 'config', 'readwrite', 17, 'bitmask', 6, 1, 'little', 'Output 7 Initial State'),
(5, 31, 'register', 'config', 'readwrite', 17, 'bitmask', 7, 1, 'little', 'Output 8 Initial State'),

-- Inversion Mask Bits (Physical Holding Register 18, bits 0-7) 
(5, 32, 'register', 'config', 'readwrite', 18, 'bitmask', 0, 1, 'little', 'Output 1 Invert Mask'),
(5, 33, 'register', 'config', 'readwrite', 18, 'bitmask', 1, 1, 'little', 'Output 2 Invert Mask'),
(5, 34, 'register', 'config', 'readwrite', 18, 'bitmask', 2, 1, 'little', 'Output 3 Invert Mask'),
(5, 35, 'register', 'config', 'readwrite', 18, 'bitmask', 3, 1, 'little', 'Output 4 Invert Mask'),
(5, 36, 'register', 'config', 'readwrite', 18, 'bitmask', 4, 1, 'little', 'Output 5 Invert Mask'),
(5, 37, 'register', 'config', 'readwrite', 18, 'bitmask', 5, 1, 'little', 'Output 6 Invert Mask'),
(5, 38, 'register', 'config', 'readwrite', 18, 'bitmask', 6, 1, 'little', 'Output 7 Invert Mask'),
(5, 39, 'register', 'config', 'readwrite', 18, 'bitmask', 7, 1, 'little', 'Output 8 Invert Mask'),

-- Common Header Registers (Writable subset)
(5, 40, 'register', 'config', 'readwrite', 0, 'direct', 0, 1, 'little', 'Start Counter'), -- Num Starts
(5, 41, 'register', 'config', 'readwrite', 3, 'direct', 0, 1, 'little', 'User ID'), -- User ID
(5, 42, 'register', 'config', 'readwrite', 4, 'direct', 0, 1, 'little', 'LED WDT Timeout'), -- Timeout LED WDT (secs) 
(5, 43, 'register', 'config', 'readwrite', 5, 'direct', 0, 1, 'little', 'Baudrate Code'), -- Baudrate Code (if switches ON) 
(5, 44, 'register', 'config', 'readwrite', 10, 'direct', 0, 1, 'little', 'Safe State Timeout'), -- Timeout Secure State (tenths of sec -> scale 0.1) 
(5, 45, 'register', 'config', 'readwrite', 11, 'bitmask', 0, 1, 'little', 'LED Invert Mask 0'), -- LED Inversion Mask 0
(5, 46, 'register', 'config', 'readwrite', 11, 'bitmask', 1, 1, 'little', 'LED Invert Mask 1'), -- LED Inversion Mask 1
(5, 47, 'register', 'config', 'readwrite', 11, 'bitmask', 2, 1, 'little', 'LED Invert Mask 2'), -- LED Inversion Mask 2
(5, 48, 'register', 'config', 'readwrite', 11, 'bitmask', 3, 1, 'little', 'LED Invert Mask 3'), -- LED Inversion Mask 3
(5, 49, 'register', 'config', 'readwrite', 11, 'bitmask', 4, 1, 'little', 'LED Invert Mask 4'), -- LED Inversion Mask 4
(5, 50, 'register', 'config', 'readwrite', 11, 'bitmask', 5, 1, 'little', 'LED Invert Mask 5'), -- LED Inversion Mask 5
(5, 51, 'register', 'config', 'readwrite', 11, 'bitmask', 6, 1, 'little', 'LED Invert Mask 6'), -- LED Inversion Mask 6
(5, 52, 'register', 'config', 'readwrite', 11, 'bitmask', 7, 1, 'little', 'LED Invert Mask 7'), -- LED Inversion Mask 7
(5, 53, 'register', 'config', 'readwrite', 15, 'direct', 0, 1, 'little', 'Remote Reset'), -- Reset

-- Specific Config/State Registers (Writable subset)
(5, 54, 'register', 'config', 'readwrite', 24, 'direct', 0, 1, 'little', 'Output 1 Initial Timer'), -- Initial Timer 1 (tenths of sec) 
(5, 55, 'register', 'config', 'readwrite', 25, 'direct', 0, 1, 'little', 'Output 2 Initial Timer'), -- Initial Timer 2 (tenths of sec) 
(5, 56, 'register', 'config', 'readwrite', 26, 'direct', 0, 1, 'little', 'Output 3 Initial Timer'), -- Initial Timer 3 (tenths of sec) 
(5, 57, 'register', 'config', 'readwrite', 27, 'direct', 0, 1, 'little', 'Output 4 Initial Timer'), -- Initial Timer 4 (tenths of sec) 
(5, 58, 'register', 'config', 'readwrite', 28, 'direct', 0, 1, 'little', 'Output 5 Initial Timer'), -- Initial Timer 5 (tenths of sec) 
(5, 59, 'register', 'config', 'readwrite', 29, 'direct', 0, 1, 'little', 'Output 6 Initial Timer'), -- Initial Timer 6 (tenths of sec) 
(5, 60, 'register', 'config', 'readwrite', 30, 'direct', 0, 1, 'little', 'Output 7 Initial Timer'), -- Initial Timer 7 (tenths of sec) 
(5, 61, 'register', 'config', 'readwrite', 31, 'direct', 0, 1, 'little', 'Output 8 Initial Timer'), -- Initial Timer 8 (tenths of sec) 
(5, 62, 'register', 'config', 'readwrite', 32, 'direct', 0, 1, 'little', 'Output 1 Reload Timer'), -- Reload Timer 1 (tenths of sec) 
(5, 63, 'register', 'config', 'readwrite', 33, 'direct', 0, 1, 'little', 'Output 2 Reload Timer'), -- Reload Timer 2 (tenths of sec) 
(5, 64, 'register', 'config', 'readwrite', 34, 'direct', 0, 1, 'little', 'Output 3 Reload Timer'), -- Reload Timer 3 (tenths of sec) 
(5, 65, 'register', 'config', 'readwrite', 35, 'direct', 0, 1, 'little', 'Output 4 Reload Timer'), -- Reload Timer 4 (tenths of sec) 
(5, 66, 'register', 'config', 'readwrite', 36, 'direct', 0, 1, 'little', 'Output 5 Reload Timer'), -- Reload Timer 5 (tenths of sec) 
(5, 67, 'register', 'config', 'readwrite', 37, 'direct', 0, 1, 'little', 'Output 6 Reload Timer'), -- Reload Timer 6 (tenths of sec) 
(5, 68, 'register', 'config', 'readwrite', 38, 'direct', 0, 1, 'little', 'Output 7 Reload Timer'), -- Reload Timer 7 (tenths of sec) 
(5, 69, 'register', 'config', 'readwrite', 39, 'direct', 0, 1, 'little', 'Output 8 Reload Timer'), -- Reload Timer 8 (tenths of sec) 

-- Common Header Registers (Readonly subset)
(5, 70, 'register', 'config', 'readwrite', 1, 'direct', 0, 1, 'little', 'Firmware Version'), -- Firmware Version (scaled) 
(5, 71, 'register', 'config', 'readwrite', 2, 'direct', 0, 1, 'little', 'Model Code'), -- Model Code
(5, 72, 'register', 'config', 'readwrite', 6, 'direct', 0, 1, 'little', 'Num Input Bits'), -- Num Input Bits
(5, 73, 'register', 'config', 'readwrite', 7, 'direct', 0, 1, 'little', 'Num Output Bits'), -- Num Output Bits
(5, 74, 'register', 'config', 'readwrite', 8, 'direct', 0, 1, 'little', 'Num Input Registers'), -- Num Input Registers
(5, 75, 'register', 'config', 'readwrite', 9, 'direct', 0, 1, 'little', 'Num Output Registers'), -- Num Output Registers
(5, 76, 'register', 'config', 'readwrite', 12, 'direct', 0, 1, 'little', 'Switch Status'), -- Switch Status

-- Specific State Registers (Readonly subset)
(5, 77, 'register', 'config', 'readwrite', 40, 'direct', 0, 1, 'little', 'Output 1 Pending Timer'), -- Pending Timer 1 (tenths of sec) 
(5, 78, 'register', 'config', 'readwrite', 41, 'direct', 0, 1, 'little', 'Output 2 Pending Timer'), -- Pending Timer 2 (tenths of sec) 
(5, 79, 'register', 'config', 'readwrite', 42, 'direct', 0, 1, 'little', 'Output 3 Pending Timer'), -- Pending Timer 3 (tenths of sec) 
(5, 80, 'register', 'config', 'readwrite', 43, 'direct', 0, 1, 'little', 'Output 4 Pending Timer'), -- Pending Timer 4 (tenths of sec) 
(5, 81, 'register', 'config', 'readwrite', 44, 'direct', 0, 1, 'little', 'Output 5 Pending Timer'), -- Pending Timer 5 (tenths of sec) 
(5, 82, 'register', 'config', 'readwrite', 45, 'direct', 0, 1, 'little', 'Output 6 Pending Timer'), -- Pending Timer 6 (tenths of sec) 
(5, 83, 'register', 'config', 'readwrite', 46, 'direct', 0, 1, 'little', 'Output 7 Pending Timer'), -- Pending Timer 7 (tenths of sec) 
(5, 84, 'register', 'config', 'readwrite', 47, 'direct', 0, 1, 'little', 'Output 8 Pending Timer'), -- Pending Timer 8 (tenths of sec) 

-- Model 6: RS-485 I8ED (8 Inputs)
(6, 0, 'bit', 'standard', 'readonly', 0, 'direct', 0, 1, 'little', 'Input 1'),
(6, 1, 'bit', 'standard', 'readonly', 1, 'direct', 0, 1, 'little', 'Input 2'),
(6, 2, 'bit', 'standard', 'readonly', 2, 'direct', 0, 1, 'little', 'Input 3'),
(6, 3, 'bit', 'standard', 'readonly', 3, 'direct', 0, 1, 'little', 'Input 4'),
(6, 4, 'bit', 'standard', 'readonly', 4, 'direct', 0, 1, 'little', 'Input 5'),
(6, 5, 'bit', 'standard', 'readonly', 5, 'direct', 0, 1, 'little', 'Input 6'),
(6, 6, 'bit', 'standard', 'readonly', 6, 'direct', 0, 1, 'little', 'Input 7'),
(6, 7, 'bit', 'standard', 'readonly', 7, 'direct', 0, 1, 'little', 'Input 8'),

-- Model 7: RS-485 iBC24SD (24 Outputs). This model does not implements standard Modbus so compatibility is limited
-- Standard Output Bits (Physical Bits 0-23)
(7, 0, 'bit', 'standard', 'readwrite', 0, 'direct', 0, 1, 'little', 'Output 1'),
(7, 1, 'bit', 'standard', 'readwrite', 1, 'direct', 0, 1, 'little', 'Output 2'),
(7, 2, 'bit', 'standard', 'readwrite', 2, 'direct', 0, 1, 'little', 'Output 3'),
(7, 3, 'bit', 'standard', 'readwrite', 3, 'direct', 0, 1, 'little', 'Output 4'),
(7, 4, 'bit', 'standard', 'readwrite', 4, 'direct', 0, 1, 'little', 'Output 5'),
(7, 5, 'bit', 'standard', 'readwrite', 5, 'direct', 0, 1, 'little', 'Output 6'),
(7, 6, 'bit', 'standard', 'readwrite', 6, 'direct', 0, 1, 'little', 'Output 7'),
(7, 7, 'bit', 'standard', 'readwrite', 7, 'direct', 0, 1, 'little', 'Output 8'),
(7, 8, 'bit', 'standard', 'readwrite', 8, 'direct', 0, 1, 'little', 'Output 9'),
(7, 9, 'bit', 'standard', 'readwrite', 9, 'direct', 0, 1, 'little', 'Output 10'),
(7, 10, 'bit', 'standard', 'readwrite', 10, 'direct', 0, 1, 'little', 'Output 11'),
(7, 11, 'bit', 'standard', 'readwrite', 11, 'direct', 0, 1, 'little', 'Output 12'),
(7, 12, 'bit', 'standard', 'readwrite', 12, 'direct', 0, 1, 'little', 'Output 13'),
(7, 13, 'bit', 'standard', 'readwrite', 13, 'direct', 0, 1, 'little', 'Output 14'),
(7, 14, 'bit', 'standard', 'readwrite', 14, 'direct', 0, 1, 'little', 'Output 15'),
(7, 15, 'bit', 'standard', 'readwrite', 15, 'direct', 0, 1, 'little', 'Output 16'),
(7, 16, 'bit', 'standard', 'readwrite', 16, 'direct', 0, 1, 'little', 'Output 17'),
(7, 17, 'bit', 'standard', 'readwrite', 17, 'direct', 0, 1, 'little', 'Output 18'),
(7, 18, 'bit', 'standard', 'readwrite', 18, 'direct', 0, 1, 'little', 'Output 19'),
(7, 19, 'bit', 'standard', 'readwrite', 19, 'direct', 0, 1, 'little', 'Output 20'),
(7, 20, 'bit', 'standard', 'readwrite', 20, 'direct', 0, 1, 'little', 'Output 21'),
(7, 21, 'bit', 'standard', 'readwrite', 21, 'direct', 0, 1, 'little', 'Output 22'),
(7, 22, 'bit', 'standard', 'readwrite', 22, 'direct', 0, 1, 'little', 'Output 23'),
(7, 23, 'bit', 'standard', 'readwrite', 23, 'direct', 0, 1, 'little', 'Output 24'),


-- ============================ ETHERNET (TCP/IP) MODULES ============================
-- Model 3: ETH 8ED (8 Inputs)
-- Standard Input Bits (Input Bits 0-7)
(3, 0, 'bit', 'standard', 'readonly', 0, 'direct', 0, 1, 'little', 'Input 1'),
(3, 1, 'bit', 'standard', 'readonly', 1, 'direct', 0, 1, 'little', 'Input 2'),
(3, 2, 'bit', 'standard', 'readonly', 2, 'direct', 0, 1, 'little', 'Input 3'),
(3, 3, 'bit', 'standard', 'readonly', 3, 'direct', 0, 1, 'little', 'Input 4'),
(3, 4, 'bit', 'standard', 'readonly', 4, 'direct', 0, 1, 'little', 'Input 5'),
(3, 5, 'bit', 'standard', 'readonly', 5, 'direct', 0, 1, 'little', 'Input 6'),
(3, 6, 'bit', 'standard', 'readonly', 6, 'direct', 0, 1, 'little', 'Input 7'),
(3, 7, 'bit', 'standard', 'readonly', 7, 'direct', 0, 1, 'little', 'Input 8'),

-- Latched Input Bits (Input Bits 8-15)
(3, 8, 'bit', 'config', 'readonly', 8, 'direct', 0, 1, 'little', 'Latched Input 1'),
(3, 9, 'bit', 'config', 'readonly', 9, 'direct', 0, 1, 'little', 'Latched Input 2'),
(3, 10, 'bit', 'config', 'readonly', 10, 'direct', 0, 1, 'little', 'Latched Input 3'),
(3, 11, 'bit', 'config', 'readonly', 11, 'direct', 0, 1, 'little', 'Latched Input 4'),
(3, 12, 'bit', 'config', 'readonly', 12, 'direct', 0, 1, 'little', 'Latched Input 5'),
(3, 13, 'bit', 'config', 'readonly', 13, 'direct', 0, 1, 'little', 'Latched Input 6'),
(3, 14, 'bit', 'config', 'readonly', 14, 'direct', 0, 1, 'little', 'Latched Input 7'),
(3, 15, 'bit', 'config', 'readonly', 15, 'direct', 0, 1, 'little', 'Latched Input 8'),

-- Common Header Registers (Output Registers 0-15)
(3, 16, 'register', 'config', 'readwrite', 0, 'direct', 0, 1, 'little', 'Start Counter'),
(3, 17, 'register', 'config', 'readwrite', 1, 'direct', 0, 1, 'little', 'Firmware Version'),
(3, 18, 'register', 'config', 'readwrite', 2, 'direct', 0, 1, 'little', 'Module Code'),
(3, 19, 'register', 'config', 'readwrite', 4, 'direct', 0, 1, 'little', 'WDT Timeout'),
(3, 20, 'register', 'config', 'readwrite', 6, 'direct', 0, 1, 'little', 'Num Discrete Inputs'),
(3, 21, 'register', 'config', 'readwrite', 7, 'direct', 0, 1, 'little', 'Num Coils'),
(3, 22, 'register', 'config', 'readwrite', 8, 'direct', 0, 1, 'little', 'Num Input Registers'),
(3, 23, 'register', 'config', 'readwrite', 9, 'direct', 0, 1, 'little', 'Num Holding Registers'),
(3, 24, 'register', 'config', 'readwrite', 11, 'direct', 0, 1, 'little', 'LED Mask'),
(3, 25, 'register', 'config', 'readwrite', 12, 'direct', 0, 1, 'little', 'Switch Status'),
(3, 26, 'register', 'config', 'readwrite', 15, 'direct', 0, 1, 'little', 'Remote Reset'),

-- Config Input Mask (Output Registers 16, bits 0-7)
(3, 27, 'register', 'config', 'readwrite', 16, 'bitmask', 0, 1, 'little', 'Input 1 Invert'),
(3, 28, 'register', 'config', 'readwrite', 16, 'bitmask', 1, 1, 'little', 'Input 2 Invert'),
(3, 29, 'register', 'config', 'readwrite', 16, 'bitmask', 2, 1, 'little', 'Input 3 Invert'),
(3, 30, 'register', 'config', 'readwrite', 16, 'bitmask', 3, 1, 'little', 'Input 4 Invert'),
(3, 31, 'register', 'config', 'readwrite', 16, 'bitmask', 4, 1, 'little', 'Input 5 Invert'),
(3, 32, 'register', 'config', 'readwrite', 16, 'bitmask', 5, 1, 'little', 'Input 6 Invert'),
(3, 33, 'register', 'config', 'readwrite', 16, 'bitmask', 6, 1, 'little', 'Input 7 Invert'),
(3, 34, 'register', 'config', 'readwrite', 16, 'bitmask', 7, 1, 'little', 'Input 8 Invert'),

-- Config Input Counters (Output Registers 20-35)
(3, 35, 'register', 'config', 'readwrite', 20, 'direct', 0, 2, 'little', 'Input 1 Counter'),
(3, 36, 'register', 'config', 'readwrite', 22, 'direct', 0, 2, 'little', 'Input 2 Counter'),
(3, 37, 'register', 'config', 'readwrite', 24, 'direct', 0, 2, 'little', 'Input 3 Counter'),
(3, 38, 'register', 'config', 'readwrite', 26, 'direct', 0, 2, 'little', 'Input 4 Counter'),
(3, 39, 'register', 'config', 'readwrite', 28, 'direct', 0, 2, 'little', 'Input 5 Counter'),
(3, 40, 'register', 'config', 'readwrite', 30, 'direct', 0, 2, 'little', 'Input 6 Counter'),
(3, 41, 'register', 'config', 'readwrite', 32, 'direct', 0, 2, 'little', 'Input 7 Counter'),
(3, 42, 'register', 'config', 'readwrite', 34, 'direct', 0, 2, 'little', 'Input 8 Counter'),

-- Standard Frequencies (Output Registers 36-43)
(3, 43, 'register', 'config', 'readwrite', 36, 'direct', 0, 1, 'little', 'Input 1 Frequency'),
(3, 44, 'register', 'config', 'readwrite', 37, 'direct', 0, 1, 'little', 'Input 2 Frequency'),
(3, 45, 'register', 'config', 'readwrite', 38, 'direct', 0, 1, 'little', 'Input 3 Frequency'),
(3, 46, 'register', 'config', 'readwrite', 39, 'direct', 0, 1, 'little', 'Input 4 Frequency'),
(3, 47, 'register', 'config', 'readwrite', 40, 'direct', 0, 1, 'little', 'Input 5 Frequency'),
(3, 48, 'register', 'config', 'readwrite', 41, 'direct', 0, 1, 'little', 'Input 6 Frequency'),
(3, 49, 'register', 'config', 'readwrite', 42, 'direct', 0, 1, 'little', 'Input 7 Frequency'),
(3, 50, 'register', 'config', 'readwrite', 43, 'direct', 0, 1, 'little', 'Input 8 Frequency'),

-- Model 4: ETH 8SD (8 Outputs)
-- Standard Output Bits (Bits 0-7) 
(4, 0, 'bit', 'standard', 'readwrite', 0, 'direct', 0, 1, 'little', 'Output 1'),
(4, 1, 'bit', 'standard', 'readwrite', 1, 'direct', 0, 1, 'little', 'Output 2'),
(4, 2, 'bit', 'standard', 'readwrite', 2, 'direct', 0, 1, 'little', 'Output 3'),
(4, 3, 'bit', 'standard', 'readwrite', 3, 'direct', 0, 1, 'little', 'Output 4'),
(4, 4, 'bit', 'standard', 'readwrite', 4, 'direct', 0, 1, 'little', 'Output 5'),
(4, 5, 'bit', 'standard', 'readwrite', 5, 'direct', 0, 1, 'little', 'Output 6'),
(4, 6, 'bit', 'standard', 'readwrite', 6, 'direct', 0, 1, 'little', 'Output 7'),
(4, 7, 'bit', 'standard', 'readwrite', 7, 'direct', 0, 1, 'little', 'Output 8'),

-- Timed Output Bits (Bits 8-15) 
(4, 8, 'bit', 'config', 'readwrite', 8, 'direct', 0, 1, 'little', 'Output 1 Timed Trigger'),
(4, 9, 'bit', 'config', 'readwrite', 9, 'direct', 0, 1, 'little', 'Output 2 Timed Trigger'),
(4, 10, 'bit', 'config', 'readwrite', 10, 'direct', 0, 1, 'little', 'Output 3 Timed Trigger'),
(4, 11, 'bit', 'config', 'readwrite', 11, 'direct', 0, 1, 'little', 'Output 4 Timed Trigger'),
(4, 12, 'bit', 'config', 'readwrite', 12, 'direct', 0, 1, 'little', 'Output 5 Timed Trigger'),
(4, 13, 'bit', 'config', 'readwrite', 13, 'direct', 0, 1, 'little', 'Output 6 Timed Trigger'),
(4, 14, 'bit', 'config', 'readwrite', 14, 'direct', 0, 1, 'little', 'Output 7 Timed Trigger'),
(4, 15, 'bit', 'config', 'readwrite', 15, 'direct', 0, 1, 'little', 'Output 8 Timed Trigger'),

-- Common Header Registers (Registers 0-15) 
(4, 16, 'register', 'config', 'readwrite', 0, 'direct', 0, 1, 'little', 'Start Counter'),
(4, 17, 'register', 'config', 'readwrite', 1, 'direct', 0, 1, 'little', 'Firmware Version'),
(4, 18, 'register', 'config', 'readwrite', 2, 'direct', 0, 1, 'little', 'Module Code'),
(4, 19, 'register', 'config', 'readwrite', 4, 'direct', 0, 1, 'little', 'WDT Timeout'),
(4, 20, 'register', 'config', 'readwrite', 6, 'direct', 0, 1, 'little', 'Num Input Bits'),
(4, 21, 'register', 'config', 'readwrite', 7, 'direct', 0, 1, 'little', 'Num Output Bits'),
(4, 22, 'register', 'config', 'readwrite', 8, 'direct', 0, 1, 'little', 'Num Input Registers'),
(4, 23, 'register', 'config', 'readwrite', 9, 'direct', 0, 1, 'little', 'Num Output Registers'),
(4, 24, 'register', 'config', 'readwrite', 11, 'direct', 0, 1, 'little', 'LED Mask'),
(4, 25, 'register', 'config', 'readwrite', 12, 'direct', 0, 1, 'little', 'Switch Status'),
(4, 26, 'register', 'config', 'readwrite', 15, 'direct', 0, 1, 'little', 'Remote Reset'),

-- Secure State Registers (Register 16, bits 0-7) 
-- (Logical addresses 0-7 para que coincidan con las salidas 0-7)
(4, 27, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 0, 1, 'little', 'Safe State Output 1'),
(4, 28, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 1, 1, 'little', 'Safe State Output 2'),
(4, 29, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 2, 1, 'little', 'Safe State Output 3'),
(4, 30, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 3, 1, 'little', 'Safe State Output 4'),
(4, 31, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 4, 1, 'little', 'Safe State Output 5'),
(4, 32, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 5, 1, 'little', 'Safe State Output 6'),
(4, 33, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 6, 1, 'little', 'Safe State Output 7'),
(4, 34, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 7, 1, 'little', 'Safe State Output 8'),

-- Config Registers (Registers 17, 18) 
-- (Logical addresses 11-18)
(4, 35, 'register', 'config', 'readwrite', 17, 'bitmask', 0, 1, 'little', 'Output 1 Initial State'),
(4, 36, 'register', 'config', 'readwrite', 17, 'bitmask', 1, 1, 'little', 'Output 2 Initial State'),
(4, 37, 'register', 'config', 'readwrite', 17, 'bitmask', 2, 1, 'little', 'Output 3 Initial State'),
(4, 38, 'register', 'config', 'readwrite', 17, 'bitmask', 3, 1, 'little', 'Output 4 Initial State'),
(4, 39, 'register', 'config', 'readwrite', 17, 'bitmask', 4, 1, 'little', 'Output 5 Initial State'),
(4, 40, 'register', 'config', 'readwrite', 17, 'bitmask', 5, 1, 'little', 'Output 6 Initial State'),
(4, 41, 'register', 'config', 'readwrite', 17, 'bitmask', 6, 1, 'little', 'Output 7 Initial State'),
(4, 42, 'register', 'config', 'readwrite', 17, 'bitmask', 7, 1, 'little', 'Output 8 Initial State'),
-- (Logical addresses 19-26)
(4, 43, 'register', 'config', 'readwrite', 18, 'bitmask', 0, 1, 'little', 'Output 1 Invert Mask'),
(4, 44, 'register', 'config', 'readwrite', 18, 'bitmask', 1, 1, 'little', 'Output 2 Invert Mask'),
(4, 45, 'register', 'config', 'readwrite', 18, 'bitmask', 2, 1, 'little', 'Output 3 Invert Mask'),
(4, 46, 'register', 'config', 'readwrite', 18, 'bitmask', 3, 1, 'little', 'Output 4 Invert Mask'),
(4, 47, 'register', 'config', 'readwrite', 18, 'bitmask', 4, 1, 'little', 'Output 5 Invert Mask'),
(4, 48, 'register', 'config', 'readwrite', 18, 'bitmask', 5, 1, 'little', 'Output 6 Invert Mask'),
(4, 49, 'register', 'config', 'readwrite', 18, 'bitmask', 6, 1, 'little', 'Output 7 Invert Mask'),
(4, 50, 'register', 'config', 'readwrite', 18, 'bitmask', 7, 1, 'little', 'Output 8 Invert Mask'),

-- Config Registers (Registers 24-39)
-- (Logical addresses 27-34)
(4, 51, 'register', 'config', 'readwrite', 24, 'direct', 0, 1, 'little', 'Output 1 Initial Timer'),
(4, 52, 'register', 'config', 'readwrite', 25, 'direct', 0, 1, 'little', 'Output 2 Initial Timer'),
(4, 53, 'register', 'config', 'readwrite', 26, 'direct', 0, 1, 'little', 'Output 3 Initial Timer'),
(4, 54, 'register', 'config', 'readwrite', 27, 'direct', 0, 1, 'little', 'Output 4 Initial Timer'),
(4, 55, 'register', 'config', 'readwrite', 28, 'direct', 0, 1, 'little', 'Output 5 Initial Timer'),
(4, 56, 'register', 'config', 'readwrite', 29, 'direct', 0, 1, 'little', 'Output 6 Initial Timer'),
(4, 57, 'register', 'config', 'readwrite', 30, 'direct', 0, 1, 'little', 'Output 7 Initial Timer'),
(4, 58, 'register', 'config', 'readwrite', 31, 'direct', 0, 1, 'little', 'Output 8 Initial Timer'),
-- (Logical addresses 35-42)
(4, 59, 'register', 'config', 'readwrite', 32, 'direct', 0, 1, 'little', 'Output 1 Reload Timer'),
(4, 60, 'register', 'config', 'readwrite', 33, 'direct', 0, 1, 'little', 'Output 2 Reload Timer'),
(4, 61, 'register', 'config', 'readwrite', 34, 'direct', 0, 1, 'little', 'Output 3 Reload Timer'),
(4, 62, 'register', 'config', 'readwrite', 35, 'direct', 0, 1, 'little', 'Output 4 Reload Timer'),
(4, 63, 'register', 'config', 'readwrite', 36, 'direct', 0, 1, 'little', 'Output 5 Reload Timer'),
(4, 64, 'register', 'config', 'readwrite', 37, 'direct', 0, 1, 'little', 'Output 6 Reload Timer'),
(4, 65, 'register', 'config', 'readwrite', 38, 'direct', 0, 1, 'little', 'Output 7 Reload Timer'),
(4, 66, 'register', 'config', 'readwrite', 39, 'direct', 0, 1, 'little', 'Output 8 Reload Timer'),

-- Standard ReadOnly Registers (Registers 40-47)
-- (Logical addresses 43-50)
(4, 67, 'register', 'config', 'readwrite', 40, 'direct', 0, 1, 'little', 'Output 1 Pending Timer'),
(4, 68, 'register', 'config', 'readwrite', 41, 'direct', 0, 1, 'little', 'Output 2 Pending Timer'),
(4, 69, 'register', 'config', 'readwrite', 42, 'direct', 0, 1, 'little', 'Output 3 Pending Timer'),
(4, 70, 'register', 'config', 'readwrite', 43, 'direct', 0, 1, 'little', 'Output 4 Pending Timer'),
(4, 71, 'register', 'config', 'readwrite', 44, 'direct', 0, 1, 'little', 'Output 5 Pending Timer'),
(4, 72, 'register', 'config', 'readwrite', 45, 'direct', 0, 1, 'little', 'Output 6 Pending Timer'),
(4, 73, 'register', 'config', 'readwrite', 46, 'direct', 0, 1, 'little', 'Output 7 Pending Timer'),
(4, 74, 'register', 'config', 'readwrite', 47, 'direct', 0, 1, 'little', 'Output 8 Pending Timer');

-- =================================================================================
-- =================================================================================
-- SECURE STATE MAPPING INSERTS
-- =================================================================================

INSERT INTO `model_secure_state_mapping` (`fk_model_id`, `fk_standard_io_definition_id`, `fk_secure_state_io_definition_id`) VALUES

-- Model 16: Borrell 8SD
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=0 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=8 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=1 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=9 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=2 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=10 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=3 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=11 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=4 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=12 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=5 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=13 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=6 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=14 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(16, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=7 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=16 AND logical_address=15 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),

-- Model 18: Borrell 8SA
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=0 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=8 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=1 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=9 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=2 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=10 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=3 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=11 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=4 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=12 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=5 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=13 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=6 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=14 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(18, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=7 AND purpose='standard' AND io_type='register' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=18 AND logical_address=15 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),

-- Model 20: Borrell 32SD
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=0 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=32 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=1 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=33 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=2 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=34 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=3 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=35 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=4 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=36 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=5 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=37 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=6 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=38 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=7 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=39 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=8 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=40 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=9 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=41 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=10 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=42 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=11 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=43 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=12 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=44 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=13 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=45 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=14 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=46 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=15 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=47 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=16 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=48 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=17 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=49 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=18 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=50 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=19 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=51 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=20 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=52 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=21 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=53 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=22 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=54 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=23 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=55 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=24 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=56 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=25 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=57 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=26 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=58 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=27 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=59 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=28 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=60 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=29 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=61 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=30 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=62 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(20, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=31 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=20 AND logical_address=63 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),

-- Model 4: ETH 8SD
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=0 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=27 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=1 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=28 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=2 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=29 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=3 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=30 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=4 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=31 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=5 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=32 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=6 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=33 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(4, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=7 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=4 AND logical_address=34 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),


-- Model 5: RS-485 I8SD
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=0 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=16 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=1 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=17 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=2 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=18 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=3 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=19 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=4 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=20 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=5 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=21 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=6 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=22 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(5, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=7 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=5 AND logical_address=23 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1));


-- Master View

CREATE OR REPLACE VIEW `rtmirror_complete` AS
SELECT 
    rt.fk_module_id,                               
    mid.logical_address,                           
    CASE mid.io_type WHEN 'bit' THEN 'bit' ELSE 'register' END AS io_type, 
    mid.hardware_access,                           
    mid.purpose,                                   
    cfg.user_label,                                
    cfg.units,  -- New engineering units column
    cfg.visibility, -- Added visibility column
    rt.timestamp,                                  
    rt.fk_io_definition_id AS io_definition_id,    
    rt.scale_factor,   -- From rtmirror (synced from module_io_config)                            
    rt.offset,         -- From rtmirror (synced from module_io_config)
    -- RAW values (direct from hardware)
    rt.value AS raw_value,
    rt.required_value AS raw_required_value,
    -- NET values (pre-calculated by trigger)
    rt.net_value,
    rt.net_required_value
FROM rtmirror AS rt
JOIN model_io_definition AS mid 
    ON rt.fk_io_definition_id = mid.io_definition_id
LEFT JOIN module_io_config AS cfg
    ON rt.fk_module_id = cfg.fk_module_id 
    AND rt.fk_io_definition_id = cfg.fk_io_definition_id;




    -- Create gui_users table
CREATE TABLE IF NOT EXISTS gui_users (
    user_id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin', 'operator', 'viewer') NOT NULL DEFAULT 'viewer',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    is_active BOOLEAN DEFAULT TRUE,
    
    INDEX idx_username (username),
    INDEX idx_role (role)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Insert default admin user (password: admin123 - should be changed after first login!)
-- Password hash format: salt$sha256_hash (used by verify_password() in app.py)
-- To generate a new hash: hashlib.sha256((salt + plain_password).encode('utf-8')).hexdigest()
INSERT INTO gui_users (username, password_hash, role) VALUES 
('admin', 'a1b2c3d4e5f67890a1b2c3d4e5f67890$7f07f9ab76391bfbbbab134c9ab5d8fd6630e149bf33bcb4a8dc4f7d455e90c2', 'admin')
ON DUPLICATE KEY UPDATE password_hash = VALUES(password_hash);

-- Role permissions reference:
-- admin:    READ + WRITE + USER_MANAGEMENT (create/modify/delete users)
-- operator: READ + WRITE (can modify Required/Net Required values)
-- viewer:   READ only (cannot modify any values)