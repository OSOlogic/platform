# OPC-UA gateway — native C++ (open62541)

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

A native C++ OPC-UA server built on [open62541](https://open62541.org), reading the
[`osodb`](../../../../core/osodb) hub in-process. It honours the **same contract** as the
[Python reference](../python/): one Variable per tag, reversible NodeId
`ns=2;s=<module_id>.<io_definition_id>`, DataType/AccessLevel from the tag meta, live reads and
guarded writes (writes become set-points via `hub.set_required`).

## Why a C++ option

It links the hub **in the runtime's own process** — no socket, no separate service — so it is
the right fit for the edge, for tight-loop deployments, and (trimmed) for the smaller boards.
The Python server stays the easiest way to run a standalone gateway or read the MariaDB views.
Same behaviour, pick by deployment — see [multi-implementation](../../README.md#multiple-implementations).

## Build & run

```bash
sudo apt install libopen62541-dev        # or build open62541 from source
cmake -S . -B build && cmake --build build
./build/osologic_opcua_server            # opc.tcp://0.0.0.0:4840, ns=2 urn:osologic:platform
```

Verify from any OPC-UA client, or reuse the Python checker:

```bash
python3 ../python/verify_client.py
```

## Type & access mapping

| osodb `DataType` | OPC-UA type |
|---|---|
| Boolean | `Boolean` |
| UInt16 / Int16 | `UInt16` / `Int16` |
| UInt32 / Int32 | `UInt32` / `Int32` |
| Float | `Float` |
| Raw | `UInt64` |

`Access::ReadWrite` → `CurrentRead | CurrentWrite`; `ReadOnly` → `CurrentRead`. Quality maps to
`StatusCode` (`Good` / `Bad_NotConnected` / `Uncertain`); the source timestamp is carried through.

> **Enterprise:** OPC-UA security (certificates, user roles, auditing), Historical Access,
> Alarms & Conditions and redundancy are Enterprise add-ons.
