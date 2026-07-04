# Home Assistant driver library

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

One JSON per Home Assistant **domain**, describing how that class of device maps to an
[`osodb`](../../../core/osodb) tag. The [mapper](../reference/hass_mapper.py) reads these +
the live entities and emits typed osodb tags (and DB SQL) — so HA's whole device ecosystem
becomes OSOLogic tags without writing native drivers.

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

## Populated so far

| Driver | osodb type | Access | Write service |
|--------|-----------|--------|---------------|
| `light` | Boolean (+brightness, color_temp) | RW | `light.turn_on/off` |
| `switch` | Boolean | RW | `switch.turn_on/off` |
| `fan` | Boolean (+percentage) | RW | `fan.turn_on/off` |
| `cover` | Boolean (+position) | RW | `cover.open/close_cover` |
| `lock` | Boolean | RW | `lock.lock/unlock` |
| `binary_sensor` | Boolean | RO | — |
| `sensor` | Float (units from HA) | RO | — |
| `number` | Float | RW | `number.set_value` |
| `climate` | Float current + temperature setpoint | RO/RW | `climate.set_temperature` |

**Add a driver** = drop a new `<domain>.json` here; the mapper picks it up automatically.
