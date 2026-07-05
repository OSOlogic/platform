# osodb adapters

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

Adapters connect the in-memory [`osodb`](../) cache to something durable or remote. They are
the seam that makes osodb a *cache*: the hub stays fast and standard-C++, adapters carry the
writes out and the truth in. All implement [`IAdapter`](adapter.hpp).

| Adapter | Role |
|---|---|
| [`mariadb_adapter.hpp`](mariadb_adapter.hpp) | Bridges the cache to **MariaDB, the source of truth** (wraps the PRO `PLC_Database`). |
| [`sql_backend.hpp`](sql_backend.hpp) | **`SqlAdapter`** — the shared IAdapter logic, written once against an `ISqlConn` + `SqlDialect`. Postgres/SQLite/MCU reuse it. |
| [`postgres_adapter.hpp`](postgres_adapter.hpp) | **PostgreSQL** via native libpq. |
| [`sqlite_adapter.hpp`](sqlite_adapter.hpp) | **SQLite** — embedded, serverless; the MCU foundation. |
| [`mcu/mcudb.hpp`](mcu/mcudb.hpp) | **MCU engine** — embedded store with a **MariaDB emulation** layer. |
| [`factory.hpp`](factory.hpp) | `make_adapter(spec)` — pick a backend by name/DSN at run time. |

## Choosing a backend

Every SQL backend reuses one `SqlAdapter` (in [`sql_backend.hpp`](sql_backend.hpp)); they differ only
in a driver (`ISqlConn`) and a `SqlDialect` (identifier quoting, the upsert clause). The reconciliation
logic — definitions in, `required_value` set-points in, current values back — is written once.

| Backend | Driver | Use it for | Notes |
|---|---|---|---|
| **MariaDB** | PRO `PLC_Database` | the source of truth today | MEMORY-table mirror; existing schema unchanged |
| **PostgreSQL** | libpq (native) | a Postgres-first plant / analytics | `"`-quotes, `ON CONFLICT` upsert; plain/`UNLOGGED` mirror |
| **SQLite** | libsqlite3 | single-box gateway, bench, CI | one file or `:memory:` |
| **MCU** | embedded (SQLite / built-in) | microcontrollers, and a light Linux alt | emulates MariaDB; *compatible, not complete* |

**Why the engine choice is not a performance decision.** MariaDB keeps the real-time mirror
(`rtmirror`) in a MEMORY table for speed, which Postgres and SQLite don't have. It doesn't matter:
**osodb is the in-memory cache and owns the real-time path, so the hot loop never touches the DB.**
The backing store only has to persist definitions/values and carry the SQL control surface. That is
what makes Postgres (or SQLite, or the MCU engine) usable as the source of truth without MariaDB's
MEMORY engine — the mirror becomes a plain (or `UNLOGGED`) table and latency is unchanged.

**MCU + MariaDB emulation.** On a microcontroller there is no server: SQLite (or a fixed-size built-in
store) holds the data, and a **MariaDB emulation** layer accepts the `` ` ``-quoted, `ON DUPLICATE KEY`
dialect the store uses — so the *same* code and SQL run unchanged. It implements the subset osodb needs
(bounded rows, no arbitrary joins, single writer): **compatible, not complete, by design.** The same
engine builds on Linux as a dependency-free lightweight alternative, with the same subset limits.

## MariaDB adapter — reconciling the cache with the source of truth

The adapter uses the PRO core's `PLC_Database` and the existing schema **unchanged**:

**Startup (read-through).** `attach()` loads `devices` + `model_io_definition` + `module_io_config`
and `define()`s every tag in the hub.

**Scan → DB (write-back).** Current values written to the hub are buffered and flushed to the
`rtmirror` MEMORY table with `batch_update_rtmirror_values()` — the same call `databaseSyncTask`
already makes. SELECTs and the OPC-UA SQL views stay live.

**SQL → scan (control).** `poll_control()` calls `get_all_required_values()` and pushes each row
into the hub as `set_required()`, then the core clears them with
`update_required_values_to_null_batch()`. This is why **any SQL client can control the PLC**:
`UPDATE … SET required_value = …` becomes a hub set-point applied to hardware.

### IoDefinition → TagMeta mapping

| IoDefinition field | osodb TagMeta |
|---|---|
| `io_type = 1` (bit) | `DataType::Boolean` |
| `io_type = 2`, `register_count = 1` | `UInt16` (or `Int16` if signed) |
| `io_type = 2`, `register_count = 2` | `UInt32` / `Int32` / `Float` |
| `hardware_access = 1` (readonly) | `Access::ReadOnly` |
| `hardware_access = 2` (readwrite) | `Access::ReadWrite` |
| `scale_factor`, `offset` | `scale`, `offset` |
| `user_label` | `label` |
| `purpose` (1/2/3) | `purpose` |
| module `is_connected` / `last_seen` | `ModuleStatus` → StatusCode on the tag |

The tag key is `(module_id, io_definition_id)` — identical to the core — so the NodeId
(`ns=2;s=<module>.<iodef>`) and every other interface line up with no translation table.

## How this reconciles with `databaseSyncTask`

`databaseSyncTask` already moves data MEM↔DB every ~10 ms. With osodb it moves data through the
cache instead of directly: the hardware/scan side writes the hub; the MariaDB adapter mirrors to
`rtmirror` and pulls `required_value`. Same cycle, same SQL ergonomics — plus a nanosecond,
DB-independent fast path for the gateways and (eventually) the MCU targets. Diego's core keeps its
role as the MariaDB layer; the adapter is a thin wrapper over it.

## Building

The adapter's `.cpp` links MariaDB and the PRO core. Build it alongside the core (it is not part
of the dependency-free `osodb` library, which builds standalone).
