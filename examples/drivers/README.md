# Custom driver / protocol examples

A driver is **files, not code you compile in** ([RFC 0002](../../standard/rfcs/0002-osologic-driver-model.md)):
drop a folder under `drivers/<id>/` and the runtime loads it, mapping a device to osodb tags.

- **[01-mqtt-light](01-mqtt-light/)** — a `domain-map` driver: an MQTT light. `driver.json` is the manifest;
  `map.json` maps each MQTT topic → a typed osodb tag (with on/off text, scaling, access/ACL).

For a **new serial/TCP protocol** (framing, CRC, field maps), see the Protocol builder
([RFC 0001](../../standard/rfcs/0001-protocol-builder-and-integrations.md)) — its output is a `protocol.json`
that a driver references. HA integrations can be *stripped* into drivers with
[`cli/oso-strip-hass`](../../cli/oso-strip-hass/).
