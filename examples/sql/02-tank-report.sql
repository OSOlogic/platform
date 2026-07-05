-- Tag report — SQL. Snapshot of the plant straight from the tag table.
-- Read-only, so it runs under the ACL for anyone allowed to SELECT tags.
SELECT id,
       value                          AS current,
       required_value                 AS setpoint,
       CASE WHEN value = required_value THEN 'ok' ELSE 'pending' END AS state
  FROM tags
 WHERE id LIKE 'hass.%'
 ORDER BY id;
