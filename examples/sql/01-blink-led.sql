-- Blink LED — SQL. MariaDB is the plant: control a tag by writing its set-point.
-- Runs like any script through the runtime, subject to the tag ACL.
-- Toggle (run repeatedly, e.g. from an event scheduler every second):
UPDATE tags
   SET required_value = 1 - COALESCE(required_value, 0)
 WHERE id = 'hass.switch.led';

-- Read it back:
SELECT id, value, required_value FROM tags WHERE id = 'hass.switch.led';
