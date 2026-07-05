# Dashboard examples

Every tag lives in the `tags` table and is live over REST, so any BI/dashboard tool works — no export step.

## Grafana (SQL datasource on MariaDB)

Add MariaDB as a data source, then a Time series / Stat panel:

```sql
SELECT id AS metric, value
  FROM tags
 WHERE id IN ('hass.sensor.tank_level', 'hass.switch.pump');
```

For trends, point Grafana at the [Historian](../../ui/webmin-oso/historian/) time-series DB
(InfluxDB / TimescaleDB / QuestDB) instead of the live table.

## OSOLogic HMIs

Build operator screens without Grafana:
- **SVG HMI** / **HTML5 HMI** — [`ui/hmi-web`](../../ui/hmi-web/): bind a shape/widget to a tag, hit Run.
- **Plant Manager** — device tree + mimic bound to tags.
- **Node-RED dashboard** — flow-built panels (see [../node-red](../node-red/)).

Also works out of the box: **Power BI · Tableau · Metabase · Superset** over SQL/ODBC/JDBC.
