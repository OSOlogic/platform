# OSOLadder — IEC 61131-3 Ladder Logic Editor

> Part of the **OsoLogic® Open Industrial Automation Platform** — [osologic.org](https://osologic.org)

**OSOLadder** is a browser-based Ladder Logic (LD) editor for IEC 61131-3 PLC programming.
It runs as a single HTML file — no build step, no server required to edit diagrams.
Connect it to an osoLogic PLC over REST, MQTT, Redis, or database for live I/O.

**OSOLadder** es un editor de Diagrama de Escalera (LD) IEC 61131-3 basado en navegador.
Funciona como un único archivo HTML — sin paso de compilación, sin servidor necesario para editar.
Conéctelo a un PLC osoLogic via REST, MQTT, Redis o base de datos para I/O en tiempo real.

---

## Files / Archivos

| File             | Purpose                                                          |
|------------------|------------------------------------------------------------------|
| `index.html`     | Single-page app — open directly in browser / Abrir en navegador |
| `osological.js`  | Ladder editor logic (canvas rendering, element drag & drop)      |
| `osological.css` | Design system styles (dark theme, CSS variables)                 |
| `osoplc.js`      | PLC connection layer (REST, MQTT, Redis, DB bridge)              |
| `LICENSE`        | AGPL-3.0                                                         |
| `publish.sh`     | Deploy to GitHub repository / Desplegar al repositorio GitHub    |
| `osoST/`         | Structured Text toolchain — see [osoST/README.md](osoST/README.md) |

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

No installation required. All functionality runs in the browser.
Sin instalación. Toda la funcionalidad corre en el navegador.

---

## Features / Características

- **Visual Ladder editor** — drag-and-drop contacts, coils, function blocks, math blocks
- **IEC 61131-3 LD** — NO, NC contacts; output coils; SR/RS latches; TON/TOF timers; CTU/CTD counters
- **Variable table** — declare and manage PLC variables with types
- **PLC connection** — prototype live connection via:
  - REST API/JSON (direct `fetch()`)
  - MQTT over WebSocket
  - Database bridge (MySQL/MariaDB/PostgreSQL)
  - Redis bridge
- **Export** — save project to JSON, export ladder diagram

- **Editor Ladder visual** — contactos, bobinas, bloques de función arrastrables
- **IEC 61131-3 LD** — contactos NA/NC; bobinas; Latches SR/RS; temporizadores TON/TOF; contadores CTU/CTD
- **Tabla de variables** — declarar y gestionar variables PLC con tipos
- **Conexión PLC** — prototipado de conexión en vivo via REST, MQTT, base de datos, Redis
- **Exportar** — guardar proyecto en JSON, exportar diagrama

---

## PLC connection / Conexión PLC

Open **Settings → Conexión PLC** (gear icon in toolbar) and configure:

| Protocol | Transport              | Notes                                    |
|----------|------------------------|------------------------------------------|
| REST     | HTTP/HTTPS `fetch()`   | Direct — no bridge needed                |
| MQTT     | WebSocket (port 9001)  | Direct — broker must support WS          |
| Redis    | Bridge required        | Use `osoplc-bridge` service              |
| Database | Bridge required        | MySQL/MariaDB/PostgreSQL via bridge      |

Connection state is persisted in `localStorage` (key `osol_plc_cfg`).
El estado de conexión se persiste en `localStorage` (clave `osol_plc_cfg`).

---

## Architecture / Arquitectura

```
 OSOLadder (browser)
     │  REST/MQTT/WebSocket
     ▼
 osoLogic PLC runtime (osoruntime + pcodevm)
     │
     ├── GPIO  (hardware_linux.c / hardware_bare.c)
     ├── Modbus TCP
     └── osodb process tag database
```

For Structured Text programs compiled to run on the same VM,
see the [osoST toolchain](osoST/README.md).

Para programas ST compilados para ejecutarse en el mismo VM,
ver el [toolchain osoST](osoST/README.md).

---

## Publish / Publicar

```bash
./publish.sh
```

Deploys to `iec61131/ladder/osoLadder/` in the
[OSOlogic-OpenSourceOsPLC-CE](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE)
repository.

---

## License / Licencia

[AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.html)

---

## Copyright / Derechos de autor

Copyright (C) 2026 Jose Roig Borrell, Roig Borrell SL, Ibercomp SL

Part of the **OsoLogic®** open-source PLC project — [osologic.org](https://osologic.org)
