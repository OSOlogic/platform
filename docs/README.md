# docs/ — OSOlogic® Documentation

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · CC-BY-4.0

---

Architecture documentation, API reference, hardware guides, and user documentation for the OSOlogic platform.

## Directory Structure

```
docs/
├── architecture/   # System architecture: component diagrams, design decisions
├── api/            # API reference documentation (REST, GraphQL, gRPC, WebSocket)
├── hardware/       # Hardware design documentation: schematics, BOM guides, wiring
└── user-guide/     # End-user guides: installation, configuration, first program
```

### `architecture/`
High-level and detailed architecture documentation. Covers the overall system design, component interactions, data flow through `osodb`, the scan cycle model, and key design decisions and trade-offs. Reference material for contributors and integrators.

### `api/`
Reference documentation for all OSOlogic API endpoints. Documents REST routes, GraphQL schema, gRPC service definitions, WebSocket subscription protocol, and MCP tools. Complements the machine-readable specs in [`api/openapi/`](../api/openapi/).

### `hardware/`
Hardware documentation: component selection guides, BOM interpretation, PCB assembly notes, wiring diagrams for field I/O, and enclosure mounting instructions. Supplements the design files in [`hardware/`](../hardware/).

### `user-guide/`
Practical guides for operators and system integrators: installing OSOlogic on a device, first-time configuration, writing a first PLC program in Ladder or ST, connecting a Modbus device, and setting up the HMI.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
