<div align="center">
  <img src="../../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>osoLadder</h1>
  <p><strong>IEC 61131-3 Ladder Diagram visual editor — browser-based, no build step</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-Ladder_Diagram-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_Roig_Borrell_S.L.-Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Files / Archivos

```
iec61131/ladder/osoLadder/
├── index.html       ← visual editor entry point / punto de entrada del editor visual
├── osological.js    ← editor engine (canvas, rungs, contacts, coils)
├── osological.css   ← design system styles (dark theme, CSS variables)
├── osoplc.js        ← PLC connection layer (REST, MQTT, Redis, DB bridge)
└── LICENSE
```

> **osoLadder** is maintained as a standalone project and synchronised into this repository via `publish.sh`. Do not edit files here directly — submit changes upstream at `OSOLadder/`.

---

## Features / Características

- **Visual Ladder editor** — drag-and-drop NO/NC contacts, output coils, function blocks, math blocks
- **IEC 61131-3 LD elements** — SR/RS latches, TON/TOF timers, CTU/CTD counters
- **Variable table** — declare and manage PLC variables with types
- **PLC connection** — prototype live I/O via REST API, MQTT (WebSocket), Redis bridge, DB bridge
- **Single-file app** — open `index.html` directly in any modern browser, no server required

---

## Quick start / Inicio rápido

```bash
# Open directly / Abrir directamente
xdg-open index.html       # Linux
open index.html           # macOS

# Or serve locally / O servir localmente
python3 -m http.server 8080
# Open http://localhost:8080
```

---

## PLC connection / Conexión PLC

Open **Settings → Conexión PLC** (⚙ in toolbar). Connection config persists in `localStorage`.

| Protocol | Transport              | Notes                                    |
|----------|------------------------|------------------------------------------|
| REST     | HTTP/HTTPS `fetch()`   | Direct — no bridge needed                |
| MQTT     | WebSocket (port 9001)  | Broker must support WebSocket            |
| Redis    | Bridge required        | Use `osoplc-bridge` service              |
| Database | Bridge required        | MySQL / MariaDB / PostgreSQL via bridge  |

---

## Architecture / Arquitectura

```
┌──────────────────────────────────────────────────────────┐
│  Browser                                                 │
│                                                          │
│  osoLadder (index.html)     ← this folder               │
│    └── talks to ──────────────────────────────────┐     │
│                                                    ▼     │
│              api/rest  +  api/websocket            │     │
│                (OSOlogic REST/WS API)              │     │
│                                                    │     │
│    └── runs on ───────────────────────────────┐   │     │
│                                                ▼   │     │
│              core/osoruntime                   │   │     │
│                (real-time scan cycle)          │   │     │
└──────────────────────────────────────────────────────────┘
```

---

## Related / Relacionado

- [`iec61131/st/osoST/`](../st/osoST/) — Structured Text toolchain (osoST)
- [`ui/ladder-editor/`](../../../ui/ladder-editor/) — webmin-oso integration layer
- [`core/osoruntime/`](../../../core/osoruntime/) — real-time scan cycle engine
- [`api/openapi/osologic-admin-api.yaml`](../../../api/openapi/osologic-admin-api.yaml) — REST API contract

---

<div align="center">
  <sub>(C) Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
