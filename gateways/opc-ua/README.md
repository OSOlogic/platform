# gateways/opc-ua — OPC-UA gateway

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — The Modern & Open Automation Platform · AGPL-3.0

---

Exposes the OSOlogic in-memory data hub (`osodb`) as an **OPC-UA** address space,
following OPC-UA information-model conventions **without changing the core data model**.

The device/register model (Modbus/SPI oriented) is projected onto OPC-UA
Objects/Folders/Variables through a thin **alias layer**:

| OSOlogic (hub / relational) | OPC-UA |
|---|---|
| device (`module`) | Object |
| point `purpose` (standard / secure_state / config) | Folder (`IO` / `SafeState` / `Config`) |
| I/O point (`module_io_config`) | Variable |
| `io_type=bit` | DataType `Boolean` |
| `io_type=register`, width 1 / 2 / 4 | `UInt16` / `UInt32` / `UInt64` |
| `hardware_access` readonly / readwrite | AccessLevel `CurrentRead` / `+CurrentWrite` |
| `units` (engineering units) | `EngineeringUnits` property |
| `rtmirror.net_value` + `timestamp` | Value + `SourceTimestamp` |
| `device_status.is_connected` | StatusCode `Good` / `Bad_NotConnected` |

### Reversible NodeId

```
ns=<idx>;s=<module_id>.<io_definition_id>
```

Deterministic and parseable both ways:

* **reads** come from the alias view (see [`sql/opcua_views.sql`](sql/opcua_views.sql));
* **writes** never touch the view — the gateway parses the NodeId back to
  `(module_id, io_definition_id)` and calls the hub's existing write path.

## Architecture

The gateway talks to a `HubSource` abstraction, consistent with the platform
principle that the **in-memory DB is the canonical hub** and other databases are
adapters behind it:

* `InMemorySource` — the `osodb`-style in-memory hub (used by the demo).
* `MariaDBSource` — production adapter reading the `opcua_*` SQL views.

```
OPC-UA client ─► OPC-UA gateway ─► HubSource ─┬─ InMemorySource (osodb)
                                              └─ MariaDBSource (opcua_* views)
```

## Layout

```
opc-ua/
├── sql/opcua_views.sql              # alias VIEWS over the core tables (no migration)
├── reference/
│   ├── osologic_opcua_server.py     # reference gateway (asyncua)
│   └── verify_client.py             # end-to-end browse/read/write check
├── src/                             # (native C++ gateway — open62541 — planned)
└── include/
```

## Try it

```bash
pip install --user asyncua
cd reference
python3 osologic_opcua_server.py --demo        # endpoint opc.tcp://0.0.0.0:4840/osologic/server/
python3 verify_client.py                       # in another shell: browse, read, write round-trip
```

Production (MariaDB adapter):

```bash
mysql PLC < ../sql/opcua_views.sql             # create the alias views (core tables untouched)
python3 osologic_opcua_server.py --db-host 127.0.0.1 --db-user plc --db-name PLC
```

## Status

Reference implementation (Python/asyncua): **anonymous server, browse, read, write,
StatusCode, SourceTimestamp, engineering units**. Suitable as the Community Edition
OPC-UA server.

> **Enterprise:** OPC-UA **security** (certificates, user roles, auditing),
> **Historical Access**, **Alarms & Conditions**, aggregation and **redundancy** are
> provided by the OSOlogic Enterprise add-ons. See [`docs/`](../../docs/).

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
