-- ============================================================
-- OSOLogic sandbox — MariaDB init (the source-of-truth DB)
-- The `tags` table IS the plant: every value is a row. osodb caches it,
-- REST/OPC-UA/MQTT front it, and you can read/control it with plain SQL.
--   SELECT id, value, units FROM tags;
--   UPDATE tags SET required_value = 1 WHERE id = 'hass.switch.pump';
-- (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
-- ============================================================
CREATE DATABASE IF NOT EXISTS osodb;
USE osodb;

CREATE TABLE IF NOT EXISTS tags (
  id             VARCHAR(64)  PRIMARY KEY,          -- osodb key / NodeId ("2.5", "hass.light.hall")
  name           VARCHAR(160),
  data_type      VARCHAR(16)  DEFAULT 'Float',      -- Boolean | Float | Int | String
  value          DOUBLE       DEFAULT 0,            -- current value (numeric)
  value_s        VARCHAR(255) DEFAULT NULL,         -- current value (string tags)
  required_value DOUBLE       DEFAULT NULL,         -- set-point (write target; applied by the scan)
  units          VARCHAR(24)  DEFAULT NULL,
  access         VARCHAR(12)  DEFAULT 'ReadWrite',  -- ReadOnly | ReadWrite
  sim            VARCHAR(16)  DEFAULT NULL,         -- sandbox only: 'sine' | 'ramp' | 'follow'
  updated_at     TIMESTAMP    DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

INSERT INTO tags (id, name, data_type, value, units, access, sim) VALUES
  ('2.1',  'Machine run',       'Boolean', 0, NULL,   'ReadWrite', 'follow'),
  ('2.5',  'Motor speed',       'Float',   0, 'rpm',  'ReadWrite', 'follow'),
  ('hass.switch.pump',       'Water Pump',   'Boolean', 0, NULL,  'ReadWrite', 'follow'),
  ('hass.light.hall',        'Hall Light',   'Boolean', 0, NULL,  'ReadWrite', 'follow'),
  ('hass.cover.gate',        'Loading Gate', 'Boolean', 0, NULL,  'ReadWrite', 'follow'),
  ('hass.lock.door',         'Access Door',  'Boolean', 0, NULL,  'ReadWrite', 'follow'),
  ('hass.sensor.tank_level', 'Tank Level',   'Float',  42, '%',   'ReadOnly',  'sine'),
  ('hass.sensor.temperature','Ambient Temp', 'Float',  21, '°C',  'ReadOnly',  'sine'),
  ('hass.climate.hvac',      'HVAC',         'Float',  22, '°C',  'ReadOnly',  'ramp'),
  ('hass.binary_sensor.jam', 'Jam Detector', 'Boolean', 0, NULL,  'ReadOnly',  NULL)
ON DUPLICATE KEY UPDATE name=VALUES(name);

-- ── Node-RED (Diego's PLCBorrell) compatibility views over `tags` ──────────
-- His flows read rtmirror_complete and write net_required_value; these views map
-- that schema onto the sandbox tags table so his flows run unchanged.
CREATE OR REPLACE VIEW rtmirror_complete AS
  SELECT id  AS io_definition_id,
         name AS user_label,
         value AS net_value,
         required_value AS net_required_value,
         CASE data_type WHEN 'Boolean' THEN 'bit' ELSE 'register' END AS io_type,
         units,
         'standard' AS purpose,
         'visible'  AS visibility
  FROM tags;

-- Simple projection → updatable, so `UPDATE rtmirror SET net_required_value=..` reaches tags.
CREATE OR REPLACE VIEW rtmirror AS
  SELECT id AS io_definition_id, value AS net_value, required_value AS net_required_value
  FROM tags;

-- A least-privilege app user (the REST/osodb core connects as this).
CREATE USER IF NOT EXISTS 'osoapp'@'%' IDENTIFIED BY 'osoapp';
GRANT SELECT, INSERT, UPDATE ON osodb.* TO 'osoapp'@'%';
FLUSH PRIVILEGES;
