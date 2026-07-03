# SCADA / Historian at scale — *Enterprise add-on*

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOlogic](https://github.com/OSOlogic/platform)

> **This is an Enterprise module.** The interface and this documentation are part of the
> Community Edition; the implementation ships with **OSOlogic Enterprise**.

---

## What it adds

Operations and history beyond the single-station HMI included in the Community Edition:

- **Multi-station SCADA** — shared projects, role-based operator workspaces, redundant servers.
- **Time-series historian** — high-rate logging of tags with compression and retention policies.
- **Alarm & event management** — prioritization, shelving, acknowledgement, and audit at scale.
- **Analytics & reporting** — trends, KPIs, and scheduled reports over historized data.

## How it connects

The historian subscribes to the `osodb` hub over the standard interfaces, so any Community
Edition point is historizable without extra instrumentation. Historical data is served back
through **OPC-UA Historical Access** and a REST query API:

```
# REST: query historized values for a tag
GET /api/v1/history?node=ns=2;s=2.0&from=2026-07-01T00:00Z&to=2026-07-02T00:00Z&interval=1m
```

## Why Enterprise

Scale-out SCADA, historian storage engines, and analytics are operational, high-value
capabilities delivered and supported as an Enterprise add-on.

**Get it:** licensing@osologic.com · [osologic.com](https://osologic.com)
