# Changelog

OSOLogic releases are named after bears — **Teddy → Misha → Grizzly → Kodiak → Polar → Ursa** —
starting friendly and growing fiercer as the platform matures. Each is a
[GitHub Release](https://github.com/OSOlogic/platform/releases).

This project adheres to [Semantic Versioning](https://semver.org). Dates are ISO-8601.

## [v1.1.0] — "Misha" — 2026-07-05

The second bear. Everything in Teddy, plus:

### IEC 61131-3
- **Structured Text — Milestone 4 complete.** The pure-Python compiler (`ostc`) now runs casts and
  mixed INT/REAL arithmetic, explicit IEC conversions, arrays (global/local, multi-dim, non-zero
  bounds), and strings (const pool, comparison, IEC single-quotes) on the VM — verified end-to-end.

### osodb — pluggable database backends
- One `SqlAdapter` over `ISqlConn` + `SqlDialect`, so backends differ only in a driver and a dialect:
  **SQLite** (tested), **PostgreSQL** (native libpq), and an **MCU engine** — SQLite + MariaDB
  emulation for MCUs with a filesystem, plus a **fixed-size, allocation-free bare-metal store** for
  microcontrollers with none. All the SQL paths are ACL-enforced and unit-tested (ctest).
- The engine choice is a deployment decision, not a performance one: osodb owns the real-time path,
  so the store works without MariaDB's MEMORY tables.

### Admin & UX
- New config modules, each with an exquisite web UI **and** an ncurses TUI (`packaging/oso-config`):
  **SSH**, **oso-cron** scheduler, **Date & Time** (NTP client + server), **osowatchdog** (service/
  process monitor), and **Database** (backend/DSN selector).
- **Global search** across DB, config, real-time, historian, hardware, alarms and logs — one box,
  grouped results, keyboard navigation, reachable from every page.
- **Navigation** — breadcrumbs, related-app links, and a Home/Admin bar on every page.

## [v1.0.0-beta.1] — "Teddy" — 2026-07-05

The first tagged release. A functional base — the real-time PLC core, `osodb`, gateways, APIs,
IEC 61131-3 toolchain and web UI — already running on BorrellPLC devices.

### Core & data
- **osodb** in-memory tag hub (C++) with a MariaDB write-through/write-back adapter, and a C
  binding so the runtime reaches it natively — ACL by the tag's own `Access`.
- x86_64 **sandbox** reference core (`sandbox/`) + one-command bring-up.

### IEC 61131-3 (Ladder + Structured Text)
- **osoLadder** editor: IEC coils, tag browser / device selectors, full **simulation** (contacts,
  coils, timers, counters, math/compare, **PID**, power-flow), **parallel branches**, **sub-ladders**
  (POU manager + CALL), a **step debugger**, and a **whole-project ST viewer**.
- **osoST**: Java (STLite, authoritative) **and** a pure-Python compiler (`ostc`) now retargeted to
  the C VM — the no-Java backend produces bytecode that **runs on `osoruntime`** (globals, typed
  ops, jumps, procedures/functions with frames, traps, osodb tag I/O with ACL).
- **Ladder → ST → P-code → runtime → osodb (ACL) → MariaDB** verified end-to-end.

### Connectivity
- Native Modbus RTU/TCP, OPC-UA (server), MQTT, SPI; REST + WebSocket, MCP, Node-RED.
- **Driver model** (RFC 0002): file-defined drivers; a tool to *strip* Home Assistant integrations
  into drivers; Protocol builder (RFC 0001).

### Admin (web UI + ncurses TUI)
- **SSH configurator** (secure by default), **oso-cron** scheduler (with a clock), **Date & Time**
  (timezone, NTP client + server), plus Logs, Alarms, Historian, Users & Roles, Firewall, fail2ban,
  Script editor, Home Assistant. Terminal fallback via `packaging/oso-config`.

### Content
- Website (osologic.com), the **Build cool things** guide, and an Arduino-style **example library**
  (`examples/`) — Blink LED upward across Ladder, ST, scripts, SQL, REST, Node-RED, dashboards and
  custom drivers.

> Beta: interfaces may still change before **v1.0.0** ("Misha"). Feedback and contributions welcome.

[v1.0.0-beta.1]: https://github.com/OSOlogic/platform/releases/tag/v1.0.0-beta.1
