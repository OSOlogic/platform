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

The Structured Text stack lives in:

```
iec61131/st/osoST/
├── editor/
│   ├── index.html              ← web editor entry point
│   ├── osost.js                ← editor engine (CodeMirror-based, syntax highlight, autocomplete)
│   └── osost.css               ← editor styles
├── compiler-python/ostc/       ← Python compiler (lexer → parser → AST → codegen → hex)
│   ├── lexer.py / tokens.py
│   ├── parser.py / ast_nodes.py
│   ├── codegen.py              ← pcode bytecode emitter
│   └── hex_writer.py           ← .osoproj output
├── compiler-java/              ← alternative Java compiler + REST server (server.py)
├── runtime/
│   ├── pcodevm.c / pcodevm.h   ← pcode virtual machine (C, bare metal + Linux)
│   ├── osoruntime.c            ← scan cycle integration
│   ├── hardware_bare.c         ← RP2040 / STM32 HAL
│   ├── hardware_linux.c        ← Linux HAL
│   └── hardware_demo.c         ← simulation / CI
└── examples/
    ├── blink.st
    ├── counter.st
    └── pid.st
```

> **osoST** is maintained as a standalone project and synchronised into this repository automatically. Do not edit files there directly — submit changes upstream.

---

## What belongs here

This folder (`ui/st-editor/`) is the **integration layer** that embeds osoST inside the OSOlogic web panel (`webmin-oso`). It adds:

| Component | Description |
|-----------|-------------|
| `src/embed.js` | Wrapper that loads `iec61131/st/osoST/editor/` into webmin-oso |
| `src/project-api.js` | Save / load `.osoproj` files via `POST /api/v1/projects` |
| `src/compiler-proxy.js` | Calls the Python or Java compiler (`ostc`) and returns bytecode |
| `src/runtime-bridge.js` | Sends compiled pcode to `osoruntime` over WebSocket |
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
osoST editor  (iec61131/st/osoST/editor/)
    │  writes
    ▼
ST source (.st)
    │  compiled by  ostc  (compiler-python or compiler-java)
    ▼
pcode bytecode (.osoproj)
    │  uploaded via  POST /api/v1/projects
    ▼
pcodevm  (iec61131/st/osoST/runtime/pcodevm.c)
    │  executed inside
    ▼
osoruntime scan cycle  ←→  osodb  (process tags)
```

---

## Related

- [`iec61131/st/osoST/`](../../iec61131/st/osoST/) — full ST stack: editor, compilers, pcodevm runtime, examples
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
