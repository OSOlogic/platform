# osodb — the real-time data cache of OSOLogic

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

> Automation is about **data**. OSOLogic is data-centric: everything — hardware, SQL,
> OPC-UA, MQTT, REST, MCP — meets at the data. **MariaDB is the central hub and source of
> truth**; **`osodb` is the in-memory real-time cache in front of it** — think *Redis in
> front of MySQL*. The cache makes the hot path fast and portable; the database keeps the
> truth.

---

## The model

The OSOLogic PRO core already synchronizes everything through a shared **MariaDB** database
(config, device/model tables, the `rtmirror` state table) and lets any service control the
PLC with `UPDATE … SET required_value = …`. That stays: **MariaDB is the source of truth and
the SQL control surface**, and at large scale it *is* the central hub — a hotel with thousands
of rooms or a cloud fleet wants exactly what an RDBMS gives you (thousands of connections,
authentication, security, replication, HA).

`osodb` adds a **caching / acceleration tier** in front of it:

```
        Hardware / scan cycle              Control surfaces (all peers, permission-gated)
        (SPI, RS-485, Modbus, IEC 61131)    SQL · OPC-UA · MQTT · REST · MCP
                 │                                     │
                 ▼                                     ▼
        ┌──────────────────────────────────────────────────────┐
        │   osodb — in-memory real-time cache  (Redis-style)     │   ns access, decoupled
        └───────────────────────────┬───────────────────────────┘
                    write-through /  │  read-through
                     write-back      ▼
        ┌──────────────────────────────────────────────────────┐
        │   MariaDB — central hub · source of truth · SQL        │   auth, security, scale, HA
        └──────────────────────────────────────────────────────┘
```

Why a cache in front of a perfectly good database?

- **Speed & jitter** — the real-time scan and the gateways hit memory (nanoseconds), not a
  SQL socket (µs–ms). Right for a 10 ms (or tighter) cycle on a small board.
- **Decoupling** — control keeps running through a database hiccup; writes flush to MariaDB
  write-through or write-back.
- **Portability** — the same cache runs at the **edge and on a baremetal MCU** (RP2040/STM32/
  ESP32) where no database exists; it holds the live state locally and syncs to MariaDB when
  connected — like a Redis that hasn't flushed yet.

Nothing about Diego's architecture is discarded: his diagrams (MariaDB as the central hub) are
correct and remain the picture at scale. `osodb` is layered *on top* as the fast path.

## What a "tag" is

Every I/O point is keyed by the core's own identity — **`(module_id, io_definition_id)`** — and
carries: **current** (live value: raw bits + quality + timestamp), an optional **required**
set-point, **meta** (type, access, label, units, scale/offset, purpose), and its module's
connection **status**. The key has one canonical string form, **`<module_id>.<io_definition_id>`**,
used verbatim as the OPC-UA NodeId (`ns=2;s=…`) and by MQTT/REST/MCP, so every interface agrees.

## Control is symmetric — every protocol is a peer

Reading is obvious; **control** is the point. Any surface writes a **set-point** (the `required`
value); the scan applies it to hardware and clears it — always **gated by that surface's
credentials**:

| Surface | Read | Control (write set-point) | Gate |
|---|---|---|---|
| **SQL / MariaDB** | `SELECT … FROM rtmirror` / views | `UPDATE … SET required_value = …` | MariaDB GRANTs |
| **OPC-UA** | Read | Write | AccessLevel + user token |
| **MQTT** | subscribe | publish to a set-point topic | broker ACLs |
| **REST / MCP** | GET / `read_tag` | PUT / `write_tag` | API auth |

**With the right database credentials you can control the PLC from any language** — Python, PHP,
Node, C#, plain SQL — just by writing a set-point. SQL is a first-class control surface, not a
read-only mirror. Read-only credentials can only observe.

## The interface (`IHub`)

The runtime and every gateway depend on `IHub` ([`include/osodb.hpp`](include/osodb.hpp)), never
on a concrete store, so the cache backing can vary by deployment without touching other code:

- **`MemoryHub`** — the in-process cache. Nanosecond access, dependency-free, runs on an MCU.
  MariaDB sits behind it as the source-of-truth adapter (write-through/back + read-through).
  *Implemented, `-Wall -Wextra` clean, unit-tested — [`src/osodb.cpp`](src/osodb.cpp), [`tests/`](tests/).*

- At **large scale**, MariaDB is the central hub directly (Diego's model), with `MemoryHub`
  caches in front per node/service — like Redis fronting MySQL in a big PHP app.

Write-through means: `write_current`/`set_required` update the cache immediately and the MariaDB
adapter mirrors them to `rtmirror` / consumes `required_value`. Same data model, same SQL-control
ergonomics, now with a real-time in-memory fast path.

## Status

- ✅ `IHub` interface + `MemoryHub` cache — implemented, unit-tested.
- 🚧 MariaDB adapter (bridges `PLC_Database`: mirror current out, pull `required_value` in) — [`adapters/`](adapters/).
- 🚧 Write-back policy & edge/MCU offline sync.

Everything here is standard-C++ only; external systems live behind adapters.
