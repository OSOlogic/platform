# gateways/opc-ua — OPC-UA gateway

**© 2026 Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOLogic](https://github.com/OSOlogic/platform) — The Modern & Open Automation Platform · AGPL-3.0

---

Exposes the OSOLogic data hub as an **OPC-UA** address space, following OPC-UA
information-model conventions **without changing the core data model**.

The device/register model (Modbus/SPI oriented) is projected onto OPC-UA
Objects/Folders/Variables through a thin **alias layer**:

| OSOLogic (hub / relational) | OPC-UA |
|---|---|
| device (`module`) | Object |
| point `purpose` (standard / secure_state / config) | Folder (`IO` / `SafeState` / `Config`) |
| I/O point (`module_io_config`) | Variable |
| `io_type=bit` | DataType `Boolean` |
| `io_type=register`, width 1 / 2 | `UInt16`/`Int16` / `UInt32`/`Int32`/`Float` |
| `hardware_access` readonly / readwrite | AccessLevel `CurrentRead` / `+CurrentWrite` |
| `units` (engineering units) | `EngineeringUnits` property |
| `rtmirror` value + `timestamp` | Value + `SourceTimestamp` |
| `device_status.is_connected` | StatusCode `Good` / `Bad_NotConnected` |

### Reversible NodeId

```
ns=2;s=<module_id>.<io_definition_id>
```

Deterministic and parseable both ways. **Reads** return the tag's current value; **writes**
parse the NodeId back to `(module_id, io_definition_id)` and become a **set-point** — the same
`required_value` path any SQL client uses, so control is consistent across protocols.

## Multiple implementations

The gateway ships **more than one reference implementation** — a recurring pattern across
OSOLogic modules. Both honour the identical contract above (address space, NodeId scheme, type
mapping, read/write semantics); pick the one that fits your deployment:

| Implementation | Best for | Reads from |
|---|---|---|
| [`reference/python/`](reference/python/) (asyncua) | a standalone gateway service; reading the MariaDB SQL views | in-memory source **or** `opcua_*` views |
| [`reference/cpp/`](reference/cpp/) (open62541) | in-process with the runtime; edge / tight-loop; trimmed for small boards | the `osodb` hub in-process |

Because OSOLogic is data-centric, both simply read the same data — the hub
([`osodb`](../../core/osodb), the in-memory cache in front of MariaDB) or its SQL views. They are
interchangeable.

## Layout

```
opc-ua/
├── sql/opcua_views.sql              # alias VIEWS over the core tables (no migration)
├── reference/
│   ├── python/                      # asyncua server + verify client
│   │   ├── osologic_opcua_server.py
│   │   └── verify_client.py
│   └── cpp/                         # native open62541 server (reads osodb)
│       ├── opcua_server.cpp
│       ├── CMakeLists.txt
│       └── README.md
├── src/                             # (production C++ gateway lands here)
└── include/
```

## Try it

**Python** (easiest; standalone or over the SQL views):

```bash
pip install --user asyncua
python3 reference/python/osologic_opcua_server.py --example   # opc.tcp://0.0.0.0:4840/osologic/server/
python3 reference/python/verify_client.py                     # browse, read, write round-trip

# Production (MariaDB views — core tables untouched):
mysql PLC < sql/opcua_views.sql
python3 reference/python/osologic_opcua_server.py --db-host 127.0.0.1 --db-user plc --db-name PLC
```

**C++** (in-process with the hub):

```bash
sudo apt install libopen62541-dev
cmake -S reference/cpp -B reference/cpp/build && cmake --build reference/cpp/build
./reference/cpp/build/osologic_opcua_server                   # opc.tcp://0.0.0.0:4840
```

## Status

Community Edition: **anonymous server, browse, read, write, StatusCode, SourceTimestamp,
engineering units** — in both Python and C++.

> **Enterprise:** OPC-UA **security** (certificates, user roles, auditing), **Historical
> Access**, **Alarms & Conditions**, aggregation and **redundancy** are provided by the
> OSOLogic Enterprise add-ons. See [`docs/enterprise/`](../../docs/enterprise/).

---

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
