# RFC 0002 — OSOLogic driver model (file-defined, modular, non-core)

**Status:** Draft · **Author:** José Roig Borrell · **© 2026 Roig Borrell S.L. · Ibercomp S.L.** · AGPL-3.0-or-later

> Design capture. Defines what an OSOLogic *driver* is, so drivers can be added by dropping
> files — no core recompile — and so Home Assistant integrations can be "stripped" into them.

## Summary

An **OSOLogic driver** is a **modular, file-defined, non-core** unit that describes how to talk to a
device or system and expose it in [`osodb`](../../core/osodb) as typed tags. It is **not** compiled
into the runtime: it is loaded from files at start (or hot-reloaded), so the device long-tail grows
without touching the core.

- **Core** = the fast, native path (C++): scan cycle, osodb, the handful of hard-real-time fieldbus
  stacks (Modbus, SPI…).
- **Drivers** = everything else, declared in files: consumer/IoT devices, custom serial/TCP protocols,
  and adapters over other ecosystems. Low/normal cadence, not the hard-RT path.

## What a driver is (files)

A driver is a folder (or a single file) under `drivers/<id>/`:

```
drivers/acme-tank/
├── driver.json      # manifest — id, name, version, kind, transport, capabilities
├── protocol.json    # (kind=protocol) an RFC-0001 protocol definition
├── map.json         # source field/entity → osodb tag (type, access, units, scaling, state map)
└── hooks.(py|js)    # optional sandboxed decode/CRC hooks
```

`driver.json` manifest:

```jsonc
{
  "id": "acme-tank", "name": "ACME Tank Controller", "version": "1.0.0",
  "kind": "protocol",              // protocol | domain-map | bridge-adapter
  "transport": "serial",           // serial | tcp | udp | bridge
  "cadence": "low",                // low | normal   (never hard-RT — that's core)
  "provides": ["tank.level", "tank.valve"],
  "requires": [],                  // optional runtime deps (declared, sandboxed)
  "license": "AGPL-3.0-or-later"
}
```

### Three kinds

1. **`domain-map`** — maps a *class* of entities to tags. The existing
   [Home Assistant driver library](../../gateways/home-assistant/drivers/) is exactly this
   (`light` → Boolean RW, `sensor` → Float RO…).
2. **`protocol`** — a serial/TCP protocol declared per [RFC 0001](0001-protocol-builder-and-integrations.md)
   (framing, CRC, field maps, hooks).
3. **`bridge-adapter`** — wraps an external source (Home Assistant, Node-RED, an MQTT broker) and maps
   its entities into tags.

All three end at the same place: **osodb tags**. The runtime loads them from the `drivers/` tree,
validates against a schema, and runs them on the low-cadence scheduler.

## Stripping Home Assistant integrations into drivers

Home Assistant has ~2000 **integrations**. We don't want the whole runtime — we want, for each, the
*protocol and the entity→tag mapping*, distilled into an OSOLogic driver. The sweep:

1. **Collect** every integration `manifest.json` (`domain`, `iot_class`, `requirements`,
   `dependencies`, `mqtt`/`zeroconf`/`dhcp` discovery hints).
2. **Classify** by:
   - **transport / protocol** — MQTT · REST/HTTP · Modbus · CoAP · Zigbee · Z-Wave · Matter · local TCP …
   - **iot_class** — `local_push` / `local_polling` (easy, on-prem) vs `cloud_*` (needs auth flows, harder).
   - **entity domains produced** — light, switch, sensor, climate, cover… (already covered by our
     domain-map drivers).
3. **Score "easily mappable"** — an integration is easy when **both**:
   - it produces **standard entity domains** we already map, **and**
   - it speaks a **standard transport** we have (or can declare via RFC 0001): MQTT, REST, Modbus, CoAP.
   Those become **native OSOLogic drivers** (domain-map + a connection config) that run without HA.
4. **Everything else stays bridged** — integrations bound to bespoke Python client libs or cloud APIs
   keep using the runtime [compatibility gateway](../../gateways/home-assistant/) until (if ever)
   ported. No functionality lost; just not "stripped" yet.
5. **Emit** a driver folder per stripped integration + a coverage report (how many devices reachable
   natively vs still-bridged).

> Licensing: HA core and most integrations are Apache-2.0 → compatible one-way with our AGPL-3.0;
> keep notices/attribution. This distils the *protocol knowledge*, it does not vendor a running product.

## Why file-defined & non-core

- **Add a device = drop a folder.** No build, no core release. Community-contributable (`contrib/`).
- **Auditable & portable** — a driver is data + optional sandboxed hooks, not linked binary.
- **Safe** — drivers run off the hard-RT path; a bad driver can't stall the scan cycle.

## Driver registry (database)

Drivers live in a **registry** with three tiers, all resolving to the same tag hub:

1. **Core-native** — a small set compiled into the OSO core (the hard/real-time fieldbus + the base ones).
2. **`oso-driver` loaded** — file-defined drivers loaded at runtime from the `drivers/` tree (this RFC),
   including the ones *stripped* from HA integrations.
3. **User-custom** — a user's own drivers/protocols (Protocol builder output, private folders).

On top of that, a **public catalog** — a browsable listing on **GitHub** and **osologic.com** (name,
transport, kind, coverage, status) so people can find and contribute drivers. The registry is the
source for that catalog.

## Open questions

- Driver registry & versioning; a signed community driver index; catalog generation from the registry.
- Hook sandbox (WASM? restricted Python/JS?) — shared with RFC 0001.
- The `strip` tool: where it lives (`cli/`), and how it reads HA manifests without importing HA.
- Schema for `driver.json` / `map.json` under [`standard/schema`](../schema/).
