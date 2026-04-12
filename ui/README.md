# ui/ — OSOlogic® Web Interfaces

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

Web interfaces for administering, programming, monitoring, and operating OSOlogic devices.

All UIs communicate with the platform through the [`api/`](../api/) layer (REST + WebSocket). They are served by the OSOlogic web server embedded in the system, accessible from any browser on the local network or over a secure remote connection.

## Directory Structure

```
ui/
├── webmin-oso/     # System administration panel (services, network, users, runtime config)
├── dbadmin/        # osodb admin interface (browse tags, read/write values, inspect DB)
├── ladder-editor/  # Visual Ladder Diagram editor for PLC programming
├── st-editor/      # Structured Text editor with syntax highlighting and validation
├── hmi-web/        # Web-based HMI / SCADA (process visualization, operator interface)
├── node-red-oso/   # Node-RED integration with OSOlogic custom nodes
├── dashboard/      # Operational dashboard (KPIs, trends, alarms overview)
└── shared/         # Shared UI components, design system, API client library
```

### `webmin-oso/`
System administration panel inspired by Webmin, adapted for OSOlogic. Manage system services, network configuration, user accounts, file system, and OSOlogic runtime settings from a browser. No SSH required for routine administration.

### `dbadmin/`
Real-time browser interface for `osodb`. Browse the full tag namespace, read live values, write test values, filter by group, and inspect alarm and event history. The primary tool for commissioning and debugging process variable mappings.

### `ladder-editor/`
Visual Ladder Diagram editor running entirely in the browser. Draw rungs, add contacts and coils, insert timers and counters, define function blocks, and upload programs directly to `osoruntime`. Syntax-aware with real-time variable binding to `osodb` tags.

### `st-editor/`
Structured Text code editor with IEC 61131-3 syntax highlighting, auto-complete, and static validation. Integrates with the ST engine in [`iec61131/st/`](../iec61131/st/) for compilation feedback directly in the browser.

### `hmi-web/`
Web-based HMI and SCADA interface builder. Create operator screens with process graphics, trend charts, alarm lists, and control widgets. Screens subscribe to `osodb` via WebSocket for live updates. No proprietary software required — any modern browser works.

### `node-red-oso/`
Custom Node-RED nodes for OSOlogic integration. Provides `osodb-read`, `osodb-write`, `osodb-subscribe`, and `osologic-alarm` nodes, enabling visual flow-based automation logic on top of the OSOlogic data bus.

### `dashboard/`
Read-only operational dashboard showing KPIs, live trends, and alarm summaries. Designed for plant floor screens and management displays. Configurable widgets with data sourced from `osodb` and historical logs.

### `shared/`
Shared UI component library, design system tokens (colors, typography, spacing), and the common JavaScript/TypeScript API client used by all UIs to communicate with the OSOlogic REST and WebSocket endpoints.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
