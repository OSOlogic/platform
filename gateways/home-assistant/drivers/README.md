# Home Assistant driver library

> These are `domain-map` **OSOLogic drivers** — see [RFC 0002 · driver model](../../../standard/rfcs/0002-osologic-driver-model.md) (file-defined, non-core) and the plan to *strip* HA integrations into drivers.

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

One JSON per Home Assistant **domain**, describing how that class of device maps to an
[`osodb`](../../../core/osodb) tag. The [mapper](../reference/hass_mapper.py) reads these + the entities from a Home Assistant
instance and emits typed osodb tags (and DB SQL) — an interoperability layer that lets devices
HA already supports appear as OSOLogic tags. It reads HA over its public API; it does not
re-implement or embed HA's drivers.

## Driver schema

```jsonc
{
  "domain": "light",
  "label": "Light", "icon": "💡",
  "osodb": { "type": "Boolean", "access": "ReadWrite" },   // the tag's type/access
  "state_map": { "on": 1, "off": 0 },                      // HA state → value
  "write": { "true": "light.turn_on", "false": "light.turn_off" },  // set-point → HA service
  "attributes": {                                          // extra tags per attribute
    "brightness": { "type": "UInt16", "access": "ReadWrite", "range": [0, 255] }
  },
  "units_from": "unit_of_measurement"                       // (sensors) copy units from HA
}
```

## Populated so far — 26 domains

**Switchable actuators** — Boolean, ReadWrite

| Driver | Extra tags | Write service |
|--------|-----------|---------------|
| `light` | brightness, color_temp | `light.turn_on/off` |
| `switch` | — | `switch.turn_on/off` |
| `fan` | percentage | `fan.turn_on/off` |
| `cover` | current_position | `cover.open/close_cover` |
| `valve` | current_position | `valve.open/close_valve` |
| `lock` | — | `lock.lock/unlock` |
| `siren` | — | `siren.turn_on/off` |
| `humidifier` | humidity setpoint | `humidifier.turn_on/off` |
| `media_player` | volume_level | `media_player.turn_on/off` |
| `input_boolean` | — | `input_boolean.turn_on/off` |
| `button` *(momentary)* | — | `button.press` |
| `scene` *(trigger)* | — | `scene.turn_on` |

**Analog & enum**

| Driver | osodb type | Access | Write service |
|--------|-----------|--------|---------------|
| `number` · `input_number` | Float | RW | `*.set_value` |
| `sensor` | Float (units from HA) | RO | — |
| `climate` | Float current + temperature setpoint | RO/RW | `climate.set_temperature` |
| `water_heater` | Float current + temperature setpoint | RO/RW | `water_heater.set_temperature` |
| `select` · `input_select` | String (enum) | RW | `*.select_option` |
| `alarm_control_panel` | String | RW | `alarm_arm_away/arm_home/disarm` |

**Read-only status** — Boolean / composite

| Driver | osodb type | Notes |
|--------|-----------|-------|
| `binary_sensor` | Boolean | on/off |
| `device_tracker` · `person` | Boolean | home / not_home |
| `sun` | Boolean | above/below horizon |
| `vacuum` | Boolean (+battery_level) | cleaning; `vacuum.start/return_to_base` |
| `weather` | String (+temperature, humidity) | condition |

**Add a driver** = drop a new `<domain>.json` here; the mapper picks it up automatically.
