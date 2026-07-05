# osodb driver catalog

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

**File-defined** drivers, loaded at runtime by `oso-driver` — **not compiled into the core**. Only a
tiny set of drivers is core-native (Modbus, SPI); everything else lives here as files you can read,
copy and edit, following [RFC 0002](../../standard/rfcs/0002-osologic-driver-model.md):

```
io/drivers/<transport>/<id>/
├── driver.json   # manifest — id, name, version, kind, transport, provides
└── map.json      # source topic/register/entity → osodb tag (type, access, units, scaling, state)
```

Drop a folder here (or under `~/.config/osologic/drivers/`) and the runtime maps that device to
osodb tags — no rebuild.

## Catalog

See [`catalog.json`](catalog.json) for the machine-readable index (used by the web catalog and
`oso-driver list`).

| Transport | Driver | Kind | Provides |
|---|---|---|---|
| mqtt | **mqtt-light** | domain-map | light.state · light.brightness |
| mqtt | **shelly** | domain-map | relay.state · power.w · energy.wh |
| mqtt | **tasmota** | domain-map | power.state · sensor.temperature |
| modbus | **modbus-generic** | domain-map | reg.value · coil.state |
| rest | **rest-sensor** | domain-map | sensor.value |
| coap | **tradfri** | bridge-adapter | light.state · light.brightness |

Many started as Home Assistant integrations, *stripped* into file drivers with
[`cli/oso-strip-hass`](../../cli/oso-strip-hass/) (HA is Apache-2.0 → one-way compatible with our
AGPL-3.0; upstream notices kept). What isn't easily mappable stays bridged via the
[HA compatibility gateway](../../gateways/home-assistant/).

The hardware-bus folders (`gpio/`, `i2c/`, `spi/`, `uart/`, `fieldbus/`) hold local-bus drivers.

## Add one

1. `cp -r mqtt/mqtt-light io/drivers/mqtt/my-device` and edit `driver.json` + `map.json`.
2. Point `map.json` at your broker/host and map each source field to an osodb tag.
3. `oso-driver reload` (or restart the runtime). It appears in `oso-driver list` and the web catalog.
