# osquery integration — OSOLogic tags as a SQL table

OSOLogic exposes its live tags to [osquery](https://osquery.io) as the virtual table
**`oso_tags`**, so you can query the plant with the same SQL you use for the host and
**join** the two:

```sql
-- which listening service belongs to a machine that is running hot?
SELECT t.id, t.value AS temp, l.port, l.address
FROM oso_tags t, listening_ports l
WHERE t.units = '°C' AND t.value > 30;
```

## How it works (Community Edition — Option C)

The core runs a small **exporter** that snapshots every tag into a standalone,
**read-only SQLite** file (`oso_tags.db`) every couple of seconds. osquery reads it through
**Auto Table Construction (ATC)**. Because the snapshot is a *separate* file, osquery queries
never contend with the real-time scan loop — there is zero impact on control timing. The
trade-off is snapshot staleness bounded by the export interval (`OSO_OSQUERY_EXPORT_MS`,
default 2000 ms).

> **Enterprise** replaces the snapshot with a live **Thrift extension** that reads osodb
> directly, backend-agnostic and with per-tag ACL enforcement — no staleness, any backend.

## Wiring it up

1. Install osquery on the host (`apt install osquery`, or the vendor repo).
2. Install the ATC config. Either:
   - **From the admin UI** → *osquery* module → **Install ATC config**, or the TUI
     (`oso-config` → osquery → atc), or
   - copy the generated snippet into `/etc/osquery/osquery.conf.d/osologic.conf`.
3. Query interactively with `osqueryi` or via the daemon (`osqueryd`).

The snippet the core generates looks like:

```json
{
  "auto_table_construction": {
    "oso_tags": {
      "query": "SELECT id, name, data_type, value, value_s, units, access, updated FROM oso_tags",
      "path": "/var/lib/osologic/oso_tags.db",
      "columns": ["id","name","data_type","value","value_s","units","access","updated"]
    }
  }
}
```

## Environment

| Variable                  | Default                                          | Purpose                              |
|---------------------------|--------------------------------------------------|--------------------------------------|
| `OSO_OSQUERY_ENABLE`      | `1`                                              | Enable the exporter                  |
| `OSO_OSQUERY_DB`          | `/var/lib/osologic/oso_tags.db`                  | Read-only snapshot path              |
| `OSO_OSQUERY_EXPORT_MS`   | `2000`                                           | Snapshot interval (staleness bound)  |
| `OSO_OSQUERY_ATC`         | `/etc/osquery/osquery.conf.d/osologic.conf`      | Where **Install ATC config** writes  |

## Host metrics → tags (the reverse bridge)

The loop also closes the other way: host metrics can be pulled **into** osodb as `osq.*` tags
(CPU load, memory, disk, process/listening-port counts, uptime), so they become first-class
tags — usable by alarms, the HMI, the historian and any gateway, exactly like a sensor. It uses
osquery when installed and falls back to `/proc` otherwise.

- **On demand** — the *osquery* admin module → **Import now**, the TUI, or `POST /osquery/metrics`.
- **Continuous** — set `OSO_OSQUERY_METRICS=1` to import on a timer.

## Roadmap

- **Enterprise Thrift extension** — live, backend-agnostic, ACL-enforced (Option B).
