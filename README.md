# OSOlogic® — Open Industrial Automation Platform

![OSOlogic logo](logos/osologic_logo.png)

**(C) Roig Borrell S.L. · Ibercomp S.L.**

An open-source hardware and software initiative to modernize industrial and home automation, bridging the gap between traditional PLC systems and the powerful, flexible world of modern computing.

---

## Overview

OSOlogic is a fully open hardware and software platform designed to serve as an alternative to existing PLC and IoT systems — and as a standard for the ecosystem of single-board computers and microcontroller-based platforms, including maker boards and DIY electronics.

We want to help industry leap forward by adopting modern, flexible technologies, freeing it from planned obsolescence and the limitations of proprietary, closed systems that still dominate machine control, factory automation, and smart environments.

---

## Why?

Most existing automation platforms are closed, rigid, and expensive. Modern computing offers better tools, better scalability, and better integration — but the gap remains between industrial-grade reliability and the flexibility of modern development tools.

**We are building a bridge.**

---

## Core Principles

- **Open and hackable** — Built from the ground up with open-source hardware and software.
- **Real-time and Linux-based** — Optimized Linux distributions with real-time capabilities via PREEMPT_RT.
- **Data-centric** — In-memory, real-time database (`osodb`) at the core for flexible and immediate system interaction.
- **Modular and compatible** — Interfaces with legacy industrial standards (IEC 61131-3, Ladder, ST) and supports modern technologies.
- **Secure by design** — Encryption, certificates, authentication, firewalls, and more.
- **Universal gateway** — Communicates across industrial protocols (Modbus, CAN, EtherNet/IP, PROFINET, OPC-UA) and modern formats (JSON, XML, Protocol Buffers).

---

## Compatible Tools and Technologies

The platform is designed to support a wide range of modern tools and technologies:
Node-RED, REST APIs, MQTT, WebSockets, GraphQL, gRPC, containers, time-series and relational databases, C, C++, Rust, Python, Node.js, .NET, PHP, Java, SQL, and more.

---

## Use Cases

- General-purpose industrial and home automation
- Integration with machine vision systems
- Educational and research use
- DIY and maker projects with serious capabilities
- Real-time gateway between legacy machines and modern cloud-based services

---

## Repository Structure

OSOlogic follows a modular scaffold designed for multi-target, multi-language industrial automation. Below is the complete top-level layout:

### Core Platform

| Directory | Description |
|-----------|-------------|
| [`core/`](core/) | Platform kernel: **osodb** (RT in-memory DB), **osoruntime** (IEC 61131-3 scan cycle), **osokernel** (PREEMPT_RT patches) |
| [`iec61131/`](iec61131/) | Language engines: Ladder, ST, FBD, SFC, IL — compiled and interpreted |
| [`io/`](io/) | I/O layer: HAL, hardware drivers (GPIO, SPI, I2C, UART, fieldbus), emulated I/O, real-time proxy |
| [`gateways/`](gateways/) | Protocol connectors: OPC-UA, Modbus, PROFINET, EtherNet/IP, CANopen, MQTT, OPC-DA legacy |

### Distributions and Hardware

| Directory | Description |
|-----------|-------------|
| [`os-dist/`](os-dist/) | OSOlogic Linux (Debian-based, x86_64/arm64/armv7) and Baremetal builds (RP2040, STM32, ESP32) |
| [`bsp/`](bsp/) | Board Support Packages: RPi4, RPi5, CM4, BeagleBone, BorrellPLC custom boards |
| [`hardware/`](hardware/) | Open hardware designs: PCB schematics, BOM, I/O modules, enclosures |

### Interfaces and Integration

| Directory | Description |
|-----------|-------------|
| [`ui/`](ui/) | Web interfaces: admin panel, DB admin, Ladder/ST editors, HMI-SCADA, Node-RED, dashboard |
| [`api/`](api/) | REST, GraphQL, WebSocket, gRPC, MCP endpoints and OpenAPI specifications |
| [`cli/`](cli/) | Command-line tools: `osctl`, `osodb-cli`, `io-probe`, `plc-flash`, `diag` |
| [`sdk/`](sdk/) | Developer SDKs: C, C++, .NET, PHP, Python, Node.js |

### Standards, Tests and Tooling

| Directory | Description |
|-----------|-------------|
| [`standard/`](standard/) | OSOlogic open standard: schemas (JSON/XML), canonical data model, protocol spec, RFCs |
| [`tests/`](tests/) | Test suites: unit, integration, E2E, hardware-in-loop, RT performance benchmarks |
| [`ci/`](ci/) | CI/CD workflows, Docker build images, build and release scripts |
| [`docs/`](docs/) | Architecture documentation, API reference, hardware docs, user guide |
| [`packaging/`](packaging/) | Distribution packaging: `.deb`, `.rpm`, `.ipk` |
| [`contrib/`](contrib/) | Community contributions and third-party integrations |

### Legacy and Transitional Directories

| Directory | Status | Notes |
|-----------|--------|-------|
| [`logos/`](logos/) | Active | Brand assets: OSOlogic logo (PNG, SVG) |
| [`OS/`](OS/) | Transitional | Complete bootable OS images (x64, ARM). Location under review. |
| [`modules/`](modules/) | Deprecated | Early module structure, fully superseded by the scaffold above. |
| `IEC 61131-3/` | Archived | Code migrated to [`iec61131/ladder/osoLadder`](iec61131/ladder/osoLadder). |

---

## Architecture at a Glance

```
┌─────────────────────────────────────────────────────┐
│                  UI / API / CLI                      │
│   webmin · HMI · ladder-editor · REST · gRPC · MCP  │
├─────────────────────────────────────────────────────┤
│              IEC 61131-3 Language Engines            │
│          Ladder · ST · FBD · SFC · IL               │
├─────────────────────────────────────────────────────┤
│                   osoruntime                         │
│            Scan cycle · Task scheduler               │
├──────────────────────┬──────────────────────────────┤
│        osodb         │         Gateways              │
│  RT in-memory DB     │  OPC-UA · Modbus · PROFINET  │
│  (shared data bus)   │  EtherNet/IP · CAN · MQTT    │
├──────────────────────┴──────────────────────────────┤
│              I/O Layer (HAL + Drivers)               │
│        GPIO · SPI · I2C · UART · Fieldbus            │
├─────────────────────────────────────────────────────┤
│        OSOlogic Linux / Baremetal (osokernel)        │
│    PREEMPT_RT · Debian · RP2040 · STM32 · ESP32     │
└─────────────────────────────────────────────────────┘
```

---

## Current Status

The code is in an early stage — somewhere between alpha and beta. We are currently building out the scaffold and refactoring the codebase. A minimal functional base is already running on our BorrellPLC devices.

---

## License

This project is released under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.

This license ensures that any modifications or derived works, even when used in a networked environment (e.g., as a backend service), must also be released as open source. It protects the core values of transparency, collaboration, and long-term freedom.

For details, see [LICENSE](./LICENSE).

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
