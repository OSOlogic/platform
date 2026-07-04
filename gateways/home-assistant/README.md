# gateways/home-assistant — Home Assistant compatibility gateway

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

An **optional compatibility gateway** that lets OSOLogic **interoperate with
[Home Assistant](https://www.home-assistant.io/)**. Through HA's open REST and
WebSocket APIs, a device HA already manages — a light, sensor, switch — can appear in
[`osodb`](../../core/osodb) as a tag, so OSOLogic can read it and (with permission)
command it alongside its own I/O.

It **complements** Home Assistant — it doesn't replace or repackage it. If you already
run HA, this brings its devices into OSOLogic; if you don't, nothing here is required.

---

## Native + compatible

OSOLogic ships **native** drivers for industrial protocols (Modbus, OPC-UA, MQTT, SPI…).
For the wide world of consumer and smart-home devices, rather than duplicating a large,
well-maintained ecosystem, it **interoperates** with Home Assistant via this gateway —
using HA's public APIs, respecting its project and its authentication:

- point at a running Home Assistant instance you own,
- **mirror its entity states into osodb** as tags (in),
- **call HA services** for set-points (out).

Every OSOLogic surface — Ladder/ST logic, SQL, OPC-UA, MQTT, MCP, the HMI — then uses
those tags like any other. See the full [connectivity matrix](../../docs/connectivity.md)
for what's native, on the roadmap, or reached via a gateway like this one.

```
 Devices Home Assistant already supports (Zigbee/Z-Wave/Matter/MQTT/…)
        │
        ▼
 Home Assistant  ──public REST + WebSocket──►  gateway  ──►  osodb (tags)  ──►  OSOLogic
        ▲                                              ◄── set-points ◄──
        └──────────────  service calls  ◄──────────────────────────────
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

## How it connects

- **Runtime gateway** *(this reference)* — talk to a running HA instance over its public
  API. Everything HA already supports becomes available, with no code from HA embedded here.
  See [`reference/`](reference/).
- Tighter interoperability (deeper mapping of HA's device registry, areas and units) is on
  the roadmap, always via HA's documented APIs and honouring its licences.

## Pieces

| Piece | Role |
|---|---|
| [`reference/hass_discover.py`](reference/hass_discover.py) | **Crawler + sniffer** — enumerate all HA entities/services, auto-generate a mapping (`--out`), or live-tail events (`--sniff`). |
| [`drivers/`](drivers/) | **Driver library** — one JSON per HA domain: how it maps to an osodb tag (type/access/units/write service). |
| [`reference/hass_mapper.py`](reference/hass_mapper.py) | **Mapper** — HA entities + drivers → typed **osodb tags** (JSON) and **DB SQL** (`hass_tags` registry). |
| [`reference/hass_bridge.py`](reference/hass_bridge.py) | **Runtime bridge** — mirror HA states → osodb; osodb set-points → HA service calls. |
| [`ui/hmi-web/plant-manager`](../../ui/hmi-web/plant-manager/) | **Plant Manager** — drops the mapped devices onto a plant mimic. |

## Try it

```bash
pip install --user aiohttp
export HASS_URL=http://homeassistant.local:8123
export HASS_TOKEN=<long-lived access token>
export OSO_REST=http://localhost:8080/api        # osoLogic REST (fronts osodb)

python3 reference/hass_discover.py                # crawl: what does HA expose?
python3 reference/hass_discover.py --sniff        # live event tail
python3 reference/hass_mapper.py --live --out-json devices.json --out-sql hass_tags.sql
python3 reference/hass_bridge.py --map reference/mapping.example.json   # live bridge
```

## Status

Reference runtime bridge (Python): subscribe to HA state changes → mirror to osodb;
osodb set-points → HA service calls. Prototype — auth, reconnection and the mapping
schema are minimal and evolving.

---

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
