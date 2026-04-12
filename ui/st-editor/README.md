<div align="center">
  <img src="../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>OSOlogic ST Editor</h1>
  <p><strong>Web integration layer for the IEC 61131-3 Structured Text editor</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-Structured_Text-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_Roig_Borrell_S.L.-Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Where is the editor code?

The Structured Text editor core lives in:

```
iec61131/st/osoST/          ← (sync pending)
├── index.html              ← editor entry point
├── osoST.js                ← ST parser, compiler and editor engine
├── osoplc-st.js            ← IEC 61131-3 ST runtime
└── osoST.css               ← editor styles
```

> **osoST** is maintained as a standalone project and synchronised into this repository automatically. Do not edit files there directly — submit changes upstream.

---

## What belongs here

This folder (`ui/st-editor/`) is the **integration layer** that embeds osoST inside the OSOlogic web panel (`webmin-oso`). It adds:

| Component | Description |
|-----------|-------------|
| `src/embed.js` | Wrapper that loads osoST into webmin-oso |
| `src/project-api.js` | Save / load ST programs via `POST /api/v1/projects` |
| `src/runtime-bridge.js` | Connects compiled ST output to `osoruntime` over WebSocket |
| `src/lsp-proxy.js` | Language Server Protocol proxy for autocompletion and diagnostics |
| `public/` | Static assets specific to the integration |

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  webmin-oso / Cockpit module                             │
│                                                          │
│  ui/st-editor/              ← integration (this folder) │
│    └── embeds ──────────────────────────────────────┐   │
│                                                      ▼   │
│              iec61131/st/osoST/                      │   │
│                 (ST editor + compiler + JS runtime)  │   │
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

### ST-specific: compilation pipeline

```
osoST editor
    │  writes
    ▼
ST source (.st)
    │  compiled by osoST.js  (browser-side)
    ▼
Bytecode / IR
    │  uploaded via  POST /api/v1/projects
    ▼
core/osoruntime
    │  executed in
    ▼
scan cycle  ←→  core/osodb  (process tags)
```

---

## Related

- [`iec61131/st/`](../../iec61131/st/) — ST engine (osoST, sync pending)
- [`iec61131/ladder/osoLadder/`](../../iec61131/ladder/osoLadder/) — Ladder editor (reference implementation)
- [`iec61131/runtime-bridge/`](../../iec61131/runtime-bridge/) — IEC runtime ↔ osodb bridge
- [`core/osoruntime/`](../../core/osoruntime/) — real-time scan cycle engine
- [`api/openapi/osologic-admin-api.yaml`](../../api/openapi/osologic-admin-api.yaml) — REST API contract
- [`ui/ladder-editor/`](../ladder-editor/) — Ladder editor integration (reference)
- [`ui/webmin-oso/`](../webmin-oso/) — admin panel (Cockpit + embedded)

---

<div align="center">
  <sub>(C) Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
