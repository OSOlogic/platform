-- =====================================================================
-- OSOlogic — OPC-UA alias layer (VIEWS ONLY — core tables untouched)
-- =====================================================================
--
-- These views project OSOlogic's device/register data model onto OPC-UA
-- information-model conventions WITHOUT modifying any of the core tables
-- (devices, model_io_definition, module_io_config, rtmirror, ...).
--
-- Reversible, zero-migration: DROP the views and nothing else changes.
--
-- The OPC-UA gateway consumes these views for the address space and for
-- reads. WRITES do NOT go through the view — the gateway parses the
-- NodeId back to (module_id, io_definition_id) and calls the existing
-- write path. NodeId is therefore deterministic and reversible:
--
--     ns=2;s=<module_id>.<io_definition_id>
--
-- DataType mapping is intentionally conservative (unsigned by register
-- width). A future `opcua_datatype_hint` column on module_io_config can
-- override to Int/Float/Double per point without changing these views.
-- =====================================================================

USE PLC;

-- Namespace index used for OSOlogic string NodeIds (urn:osologic:platform).
-- Kept as a comment for reference; the gateway registers the namespace and
-- resolves the real index at runtime.

-- ---------------------------------------------------------------------
-- opcua_objects — one OPC-UA Object per configured device (module)
-- ---------------------------------------------------------------------
CREATE OR REPLACE VIEW opcua_objects AS
SELECT
    d.module_id                                   AS module_id,
    CONCAT('ns=2;s=', d.module_id)                AS node_id,
    d.module_name                                 AS browse_name,
    d.module_name                                 AS display_name,
    m.model_name                                  AS model_name,
    m.protocol                                    AS protocol,
    d.channel                                     AS channel,
    COALESCE(ds.is_connected, 0)                  AS is_connected,
    ds.last_seen                                  AS last_seen,
    CASE WHEN COALESCE(ds.is_connected,0) = 1
         THEN 'Good' ELSE 'Bad_NotConnected' END  AS status_code
FROM devices d
LEFT JOIN model_config  m  ON m.model_id   = d.fk_model_id
LEFT JOIN device_status ds ON ds.fk_module_id = d.module_id;

-- ---------------------------------------------------------------------
-- opcua_variables — one OPC-UA Variable per configured I/O point
-- ---------------------------------------------------------------------
CREATE OR REPLACE VIEW opcua_variables AS
SELECT
    -- Reversible NodeId: ns=2;s=<module_id>.<io_definition_id>
    CONCAT('ns=2;s=', c.fk_module_id, '.', c.fk_io_definition_id) AS node_id,
    c.fk_module_id                        AS module_id,
    c.fk_io_definition_id                 AS io_definition_id,

    -- Identity
    c.user_label                          AS browse_name,
    c.user_label                          AS display_name,
    d.module_name                         AS device_object,      -- parent Object browse name
    CONCAT('ns=2;s=', d.module_id)        AS parent_node_id,

    -- Folder organisation (OPC-UA Objects/Folders) by point purpose
    CASE iod.purpose
        WHEN 'standard'     THEN 'IO'
        WHEN 'secure_state' THEN 'SafeState'
        WHEN 'config'       THEN 'Config'
        ELSE 'Other'
    END                                    AS folder,

    -- OPC-UA built-in DataType (conservative: unsigned by width)
    CASE
        WHEN iod.io_type = 'bit'          THEN 'Boolean'
        WHEN iod.register_count <= 1      THEN 'UInt16'
        WHEN iod.register_count = 2       THEN 'UInt32'
        ELSE 'UInt64'
    END                                    AS data_type,

    -- OPC-UA AccessLevel from hardware access
    CASE iod.hardware_access
        WHEN 'readwrite' THEN 'CurrentRead|CurrentWrite'
        ELSE 'CurrentRead'
    END                                    AS access_level,

    -- Engineering info
    c.units                                AS engineering_units,
    c.scale_factor                         AS scale_factor,
    c.offset                               AS `offset`,
    c.refresh_rate                         AS sampling_interval_s,

    -- Live value + quality + timestamp (from in-memory rtmirror)
    rt.net_value                           AS value,          -- scaled (engineering) value
    rt.value                               AS raw_value,      -- raw hardware value
    rt.net_required_value                  AS setpoint,
    CASE WHEN COALESCE(ds.is_connected,0) = 1
         THEN 'Good' ELSE 'Bad_NotConnected' END AS status_code,
    rt.`timestamp`                         AS source_timestamp,

    -- passthrough flags
    iod.io_type                            AS io_type,
    iod.purpose                            AS purpose,
    c.visibility                           AS visibility,
    c.sync                                 AS sync_enabled
FROM module_io_config    c
JOIN devices             d   ON d.module_id       = c.fk_module_id
JOIN model_io_definition iod ON iod.io_definition_id = c.fk_io_definition_id
LEFT JOIN device_status  ds  ON ds.fk_module_id   = c.fk_module_id
LEFT JOIN rtmirror       rt  ON rt.fk_module_id   = c.fk_module_id
                            AND rt.fk_io_definition_id = c.fk_io_definition_id
WHERE c.visibility = 'visible';
