<div align="center">
  <img src="../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>OSOlogic Ladder Editor</h1>
  <p><strong>Web integration layer for the IEC 61131-3 Ladder Diagram editor</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-Ladder_Diagram-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_Roig_Borrell_S.L.-Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Where is the editor code?

The Ladder Diagram editor core lives in:

```
iec61131/ladder/osoLadder/
├── index.html       ← visual editor entry point
├── osological.js    ← editor engine (canvas, rungs, contacts, coils)
├── osoplc.js        ← IEC 61131-3 Ladder runtime (scan cycle, JS)
└── osological.css   ← editor styles
```

> **osoLadder** is maintained as a standalone project and synchronised into this repository automatically. Do not edit files there directly — submit changes upstream.

---

## What belongs here

This folder (`ui/ladder-editor/`) is the **integration layer** that embeds osoLadder inside the OSOlogic web panel (`webmin-oso`). It adds:

| Component | Description |
|-----------|-------------|
| `src/embed.js` | Iframe/module wrapper that loads osoLadder into webmin-oso |
| `src/project-api.js` | Save / load projects via `POST /api/v1/projects` |
| `src/runtime-bridge.js` | Connects the JS editor to the real `osoruntime` scan cycle over WebSocket |
| `public/` | Static assets specific to the integration (icons, fonts) |

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  webmin-oso / Cockpit module                             │
│                                                          │
│  ui/ladder-editor/          ← integration (this folder) │
│    └── embeds ──────────────────────────────────────┐   │
│                                                      ▼   │
│              iec61131/ladder/osoLadder/              │   │
│                 (editor + JS runtime)                │   │
│                                                      │   │
│    └── talks to ────────────────────────────────┐   │   │
│                                                  ▼   │   │
│              api/rest  +  api/websocket          │   │   │
│                (OSOlogic REST/WS API)            │   │   │
│                                                  │   │   │
│    └── runs on ─────────────────────────────┐   │   │   │
│                                              ▼   │   │   │
│              core/osoruntime                 │   │   │   │
│                (real-time scan cycle)        │   │   │   │
└──────────────────────────────────────────────────────────┘
```

---

## Related

- [`iec61131/ladder/osoLadder/`](../../iec61131/ladder/osoLadder/) — editor core (osoLadder)
- [`iec61131/runtime-bridge/`](../../iec61131/runtime-bridge/) — IEC runtime ↔ osodb bridge
- [`core/osoruntime/`](../../core/osoruntime/) — real-time scan cycle engine
- [`api/openapi/osologic-admin-api.yaml`](../../api/openapi/osologic-admin-api.yaml) — REST API contract
- [`ui/webmin-oso/`](../webmin-oso/) — admin panel (Cockpit + embedded)

---

<div align="center">
  <sub>(C) Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
