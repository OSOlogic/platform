# ui/hmi-web — Web HMI / SCADA

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

Browser-based HMI for OSOLogic — process visualization and operator control.
Everything binds to **osodb** (the in-memory hub) through the REST API, so a
mimic reads live tags and writes set-points exactly like every other surface.

Two engines, one data model:

```
ui/hmi-web/
├── svg-hmi/     ← SVG engine: draw/import a mimic, bind shapes to tag state-colours
├── html5-hmi/   ← HTML5 engine: lamps/values/gauges/buttons over a background image
└── ../shared/osodb-client.js   ← shared REST client (read/write/poll → osodb:update)
```

## SVG engine (`svg-hmi/`)

A vector mimic where **SVG shapes change colour by tag state** — the classic
process-diagram approach (as in Borrell Plant Manager). Import or edit an SVG (any
element with an `id` is bindable), click a shape, and bind it to an osodb tag plus
**colour rules** (`== 1 → green`, `> 80 → red`, default grey) and an optional live
value label. In **Run** mode it polls osodb and recolours in real time.

Best for: P&IDs, pipes/tanks/valves, plant overviews — anything drawn.

## HTML5 engine (`html5-hmi/`)

A widget board over a **background image**: lamps, numeric values, gauges, push
buttons (write a set-point) and labels, drag-positioned and bound to tags. In Run
mode values update live and buttons write to osodb.

Best for: control panels, dashboards, quick operator screens.

## Binding model

Both store a small JSON (in the browser, exportable):

- **Shape/widget → tag** — an osodb NodeId/address (`2.5`) or name.
- **Rules** — SVG: value→colour; HTML5: on/off colours, gauge min/max, button set-value.
- Live values arrive via the shared client's `osodb:update` event.

## Connect

Click **⚙ osodb** and set the osoLogic REST base URL (it fronts osodb and the DB).
No build step — open `index.html` in a browser, or serve the folder.

> Prototype — both engines are foundations: the binding model, live polling and the
> osodb data path are in place; richer editing, alarms and historisation come next.

---

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
