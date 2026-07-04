# gateways/home-assistant — Home Assistant bridge

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

Reuse **Home Assistant's ~2000 integrations** (Zigbee, Z-Wave, Matter, KNX, MQTT,
Modbus, thousands of devices) inside OSOLogic — without re-implementing a single
driver. Home Assistant already does the hard, device-specific work; this gateway
**bridges** its entities into [`osodb`](../../core/osodb), so from OSOLogic's point
of view an HA light, sensor or switch is just another tag.

---

## Why a bridge (not a re-write)

Writing native drivers for every device is a losing race. Home Assistant maintains
that ecosystem already. Because OSOLogic is **data-centric**, the pragmatic path is:

- run (or point at) a Home Assistant instance,
- **mirror its entities into osodb** as tags (states in),
- **call HA services** for set-points (commands out).

Every OSOLogic surface — Ladder/ST logic, SQL, OPC-UA, MQTT, MCP, the HMI — then
uses those tags like any other, with no knowledge of the underlying radio/protocol.

```
 HA integrations (Zigbee/ZWave/Matter/MQTT/…)
        │  (HA does the driver work)
        ▼
 Home Assistant  ──REST + WebSocket──►  hass_bridge  ──►  osodb (tags)  ──►  OSOLogic
        ▲                                            ◄── set-points ◄──
        └──────────────  service calls  ◄────────────────────────────
```

## Entity ↔ tag mapping

| Home Assistant | osodb |
|---|---|
| entity `light.kitchen`, state `on`/`off` | tag `hass.light.kitchen` → Boolean |
| `sensor.temperature`, state `21.4` | tag `hass.sensor.temperature` → Float + units |
| attributes (`brightness`, `battery`, …) | tags `hass.<entity>.<attr>` |
| write a set-point | HA **service call** (`light.turn_on`, `switch.turn_off`, `climate.set_temperature`, …) |

Tag keys are deterministic (`hass.<domain>.<object_id>[.<attr>]`) so they line up with
the OPC-UA NodeId / MQTT topic / REST path conventions. A mapping file lets you
rename, whitelist domains, set units and pick which entities are writable.

## Two levels (roadmap)

1. **Runtime bridge** *(this reference)* — talk to a live HA instance over its API.
   Instant access to everything HA already supports. See [`reference/`](reference/).
2. **Integration adapter / "driver parser"** *(future)* — load selected HA integration
   `manifest.json` + component code under a thin HA-core shim so OSOLogic can host the
   integration directly (no separate HA process). Much deeper; a subset of pure-Python,
   API-based integrations first.

## Try it

```bash
pip install --user aiohttp
export HASS_URL=http://homeassistant.local:8123
export HASS_TOKEN=<long-lived access token>
export OSO_REST=http://localhost:8080/api        # osoLogic REST (fronts osodb)
python3 reference/hass_bridge.py --map reference/mapping.example.json
```

## Status

Reference runtime bridge (Python): subscribe to HA state changes → mirror to osodb;
osodb set-points → HA service calls. Prototype — auth, reconnection and the mapping
schema are minimal and evolving.

---

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
