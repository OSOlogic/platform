# OSOLogic examples

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

An Arduino-style library — from the simplest **Blink LED** to real machines — so you can copy, run and
learn. Everything meets at the [`osodb`](../core/osodb) tags, so the *same* task appears in every language:
Ladder, Structured Text, scripts (any language), SQL and REST all read/write the same tags.

> **One idea:** *every value is a tag.* Turn a tag on and off → that's Blink LED. Everything else is more tags.

## Catalogue

| # | Example | Ladder | ST | Script | SQL | REST |
|---|---|---|---|---|---|---|
| 01 | **Blink LED** — toggle a tag every second | [json](ladder/01-blink-led/) | [st](st/01-blink-led.st) | [py](scripts/python/01-blink-led.py) · [js](scripts/node/01-blink-led.js) · [sh](scripts/bash/01-blink-led.sh) | [sql](sql/01-blink-led.sql) | [sh](rest/01-blink-led.sh) |
| 02 | **Tank control** — pump on below set-point, off above (hysteresis) | [json](ladder/02-tank-control/) | [st](st/02-tank-control.st) | [py](scripts/python/02-tank-control.py) | [sql](sql/02-tank-report.sql) | [sh](rest/02-poll-tags.sh) |

*(more landing here — traffic light, PID heater, conveyor sort, alarm latch, …)*

## By type

| Type | What's here |
|---|---|
| [**Ladder**](ladder/) | relay logic — build/simulate/compile in the editor |
| [**Structured Text**](st/) | IEC 61131-3 ST — compiles to P-code, runs on the runtime |
| [**Scripts**](scripts/) | [Python](scripts/python/) · [Node](scripts/node/) · [Bash](scripts/bash/) (any language works) |
| [**SQL**](sql/) | control & report straight from the `tags` table (ACL-gated) |
| [**REST**](rest/) | `/var/<tag>` GET/PUT — the hub from any HTTP client (ACL-gated) |
| [**Node-RED**](node-red/) | importable flows over REST or the DB-mirror nodes |
| [**Dashboards**](dashboards/) | Grafana/BI over SQL, the Historian, and OSOLogic HMIs |
| [**Custom drivers/protocols**](drivers/) | file-defined drivers ([RFC 0002](../standard/rfcs/0002-osologic-driver-model.md)) + Protocol builder |

## The tag used by Blink LED

All the Blink examples drive one boolean tag. In the sandbox it is `hass.switch.led` (string id); on a real
core it's an osodb key like `1.5` (`<module>.<io_definition>`). Point the example at whatever tag you have.

## SQL and REST are scripts too

SQL and REST snippets **run like any other script** through the Script editor / runtime — **subject to the
tag ACL**. A REST `PUT /var/<tag>` or a SQL `UPDATE tags SET required_value=…` only succeeds if the caller is
allowed to write that tag. See [read & write tags from any language](../ui/get-started/db-access.html).

## Run them

- **Ladder / ST** — open in the editor (`iec61131/ladder`, `iec61131/st/osoST/editor`), compile, run.
- **Scripts / SQL / REST** — paste into the Script editor (osoadmin) or run from a shell against the core.
