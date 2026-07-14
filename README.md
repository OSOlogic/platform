# OSOlogic® — The Modern & Open Automation Platform

![OSOlogic logo](logos/osologic_logo.png)

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](./LICENSE)
[![SDK: Apache-2.0](https://img.shields.io/badge/SDK-Apache--2.0-green.svg)](./LICENSE-APACHE)
[![Release: 1.2.1 Teddy](https://img.shields.io/badge/release-1.2.1%20Teddy-f48c06.svg)](#current-status)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](./CONTRIBUTING.md)
[![Website](https://img.shields.io/badge/web-osologic.com-f48c06.svg)](https://osologic.com)

**[🌐 osologic.com](https://osologic.com)** · **[🚀 Deploy](docs/deployment/)** · **[📖 Docs](docs/)** · **[🧩 Enterprise](docs/enterprise/)** · **[🤝 Contributing](./CONTRIBUTING.md)**

> **Community Edition** — the open-source core of OSOlogic, released under AGPL-3.0.

**© 2026 Roig Borrell S.L. · Ibercomp S.L.**

An open-source hardware and software initiative to modernize industrial and home automation, bridging the gap between traditional PLC/SCADA/HMI systems and the powerful, flexible world of modern, software-defined computing.

---

## Get started

### 🐳 Try it on your PC first — Docker, no board needed

Kick the tyres on any machine. It brings up the real-time core, MariaDB, the web Manager,
Node-RED and a **simulated plant** you can drive — then read and control live tags over REST,
SQL, OPC-UA and MQTT.

**Fastest — one command** grabs everything and starts it (installs Docker for you if it's missing):

```bash
curl -fsSL https://osologic.com/get.sh | bash     # Linux / macOS
irm https://osologic.com/get.ps1 | iex            # Windows (PowerShell)
```

**By hand** — if you'd rather clone the repo and drive Docker yourself:

```bash
git clone https://github.com/OSOlogic/platform && cd platform/sandbox
docker compose up --build        # → open http://localhost:8080
```

Either way, open **http://localhost:8080**. See [`sandbox/`](sandbox/) for the endpoints
(REST `:8080`, OPC-UA `:4840`, MariaDB `:3306`).

> The sandbox runs a **simulated plant** out of the box. Turning the same install into a
> **soft-PLC bound to real I/O** — serial/network ports, gateways and stubs instead of the
> simulation — is on the near-term roadmap.

### 🚀 Deploy on hardware — from a bare board (or a spare PC) to a running PLC

1. **Get OSOlogic onto the machine** — flash a pre-built image, or `git clone` the platform.
2. **Run the installer** — a guided ncurses wizard, or the advanced installer for full control:

   ```bash
   sudo oso-setup                # if you flashed a pre-built image (already on PATH)
   sudo packaging/oso-setup      # from a fresh git clone
   # advanced / scriptable:   sudo packaging/install_OsoLogic.sh
   ```

   The wizard finds its own helpers, so any working directory is fine.

Orange Pi is the reference target today; Raspberry Pi, generic ARM/x86_64 and **bare-metal PC as a
soft-PLC** are on the roadmap.

→ **[Deployment guide](docs/deployment/)** · **[Installer reference](packaging/INSTALL.md)**

---

## Overview

OSOlogic is a fully open hardware and software platform designed to serve as an alternative to existing PLC and IoT systems — and as a standard for the ecosystem of single-board computers and microcontroller-based platforms, including maker boards and DIY electronics.

We want to help industry leap forward by adopting modern, flexible technologies, freeing it from planned obsolescence and the limitations of proprietary, closed systems that still dominate machine control, factory automation, and smart environments.

**Why?** Most existing automation platforms are closed, rigid, and expensive. Modern computing offers better tools, better scalability, and better integration — but the gap remains between industrial-grade reliability and the flexibility of modern development tools. **We are building a bridge.**

---

## Core Principles

- **Open and hackable** — Built from the ground up with open-source hardware and software.
- **Real-time, tiered** — honest about determinism: **hard real-time** on microcontrollers (bare-metal, µs jitter — Baby Bear), **firm real-time** on Linux via PREEMPT_RT for logic, sequencing and scale (the CE default), and a **validated deterministic tier** (isolated cores + lock-free shared memory, TSN networking) for the tightest low-level/CNC loops.
- **Data-centric** — In-memory, real-time database (`osodb`) at the core for flexible and immediate system interaction.
- **Modular and compatible** — Interfaces with legacy industrial standards (IEC 61131-3, Ladder, ST) and supports modern technologies.
- **Secure by design** — Encryption, certificates, authentication, firewalls, and more.
- **Universal gateway** — Communicates across industrial protocols (Modbus, CAN, EtherNet/IP, PROFINET, OPC-UA) and modern formats (JSON, XML, Protocol Buffers). See the [connectivity matrix](docs/connectivity.md).
- **Scalable for all** — From microcontrollers and single-board computers to industrial PCs, supercomputers and control rooms.
- **SQL & REST direct control** — read and control the plant with plain SQL (`SELECT` / `UPDATE`) or a JSON/XML REST API (`GET` / `PUT`), from any language, no drivers or SDKs.
- **Ready for AI** — Designed for the AI era: machine-readable, agent-friendly, and open to what comes next.

> **Compatible with** Node-RED, REST, MQTT, WebSockets, GraphQL, gRPC, containers, time-series and
> relational databases — and C, C++, Rust, Python, Node.js, .NET, PHP, Java and SQL.

---

## AI-native — Model Context Protocol

Because every value flows through the in-memory `osodb` hub, OSOlogic can expose the whole
plant through a **Model Context Protocol (MCP)** server — so AI agents can list devices, browse
the tag tree, read live values and perform **guarded, auditable** writes through one safe, open
interface. No scraping, no brittle glue. The same hub is also reachable over REST, WebSocket,
OPC-UA and MQTT, with deterministic, reversible node IDs.

→ See [`api/mcp/`](api/mcp/) and [`api/rest/`](api/rest/).

---

## SQL & REST direct control

Because OSOLogic is data-centric, **every value is a row — and a resource**. With the right
credentials you can read *and control* the plant with plain **SQL** or a plain **REST API
(JSON or XML)** — from any language, with **no drivers, no SDKs, no exotic libraries**:

```sql
-- SQL: read the plant, then control it with a set-point (applied by the scan)
SELECT id, value, units FROM tags;
UPDATE tags SET required_value = 1450 WHERE id = '3.14';   -- module 3, I/O 14
```

```bash
# REST: the same, JSON or XML, curl-simple
curl host/api/tags/3.14                        # → {"value":1450,"units":"rpm",...}
curl -X PUT host/api/tags/3.14 -d '{"required_value":1450}'
```

Transparent and open to data capture (`SELECT` or `GET` straight into any BI tool), dead-simple to
control (`UPDATE` or `PUT`), and gated by permissions (MariaDB GRANTs / API auth) — the same model
that scopes every surface. SQL and REST are first-class control interfaces, symmetric with OPC-UA,
MQTT and MCP. See [`core/osodb`](core/osodb) and [`api/rest`](api/rest).

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

## Use Cases

- General-purpose industrial and home automation
- Integration with machine vision systems
- Educational and research use
- DIY and maker projects with serious capabilities
- Real-time gateway between legacy machines and modern cloud-based services

---

## Repository layout

OSOlogic follows a modular scaffold designed for multi-target, multi-language industrial
automation. Ready-to-flash OS images are published as **release artifacts**, not stored in the
repository; the build systems that produce them live in [`os-dist/`](os-dist/).

<details>
<summary><b>Expand the full top-level layout</b></summary>

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
| [`packaging/`](packaging/) | Installers (`oso-setup`, `install_OsoLogic.sh`) and distribution packages (`.deb`, `.rpm`, `.ipk`) |
| [`contrib/`](contrib/) | Community contributions and third-party integrations |
| [`logos/`](logos/) | Brand assets: OSOlogic logo (PNG, SVG) — trademark, see [`logos/README.md`](logos/README.md) |

</details>

---

## Current Status

**OSOlogic 1.2 (codename _Teddy_) is out.** A functional base — the real-time PLC core, `osodb`
with pluggable database backends (down to a bare-metal MCU store), the IEC 61131-3 toolchain,
gateways, APIs, web + TUI admin and a global search — already runs on our BorrellPLC devices. 1.2
adds a **Runtime mode** that turns the same install into a real soft-PLC bound to physical I/O (not
just the simulated plant), a **loadable driver catalog** stripped from real-world integrations, and
a clearer install story (Docker sandbox vs bare-metal installer). We are actively building out the
scaffold and hardening the codebase through the Teddy (1.x) line.

### Release codenames

OSOlogic **major versions** are named after bears — starting friendly and growing fiercer as the
platform matures: **Teddy (1.x) → Misha (2.0) → Grizzly → Kodiak → Polar → Ursa**. Minor and patch
releases keep the current bear, so all of 1.x is _Teddy_. Each is a
[GitHub Release](https://github.com/OSOlogic/platform/releases).

🐻 Meet the bears in the **[codename fan zone](https://osologic.com/bears/)** — a photo
tribute from the cuddly Teddy to the Great Bear, Ursa (with a nod to Yogi &amp; friends).

### Editions

- **Community Edition (this repository)** — the open-source core, AGPL-3.0. Fully functional
  for building, running and integrating real automation systems.
- **Enterprise add-ons** — optional proprietary modules and SLA support for organizations that
  need to embed OSOlogic in closed products. See [Licensing](#licensing) and [`docs/enterprise/`](docs/enterprise/).

---

## Licensing

OSOlogic uses a **dual-license model**:

| Component | License |
|-----------|---------|
| Core platform (`core/`, `iec61131/`, `gateways/`, `io/`, `ui/`, `api/`, `cli/`) | [AGPL-3.0-or-later](./LICENSE) |
| Client SDKs and schemas (`sdk/`, `standard/`) | [Apache-2.0](./LICENSE-APACHE) |
| Documentation (`docs/`) | CC-BY-4.0 |
| Hardware designs (`hardware/`) | CERN-OHL-S-2.0 |

The **AGPL-3.0** ensures the platform stays open even in cloud/network deployments.
The **Apache-2.0 SDK** lets any application — open or proprietary — integrate with OSOlogic without license friction.

A **commercial license** is available for organizations that need to embed OSOlogic in closed products or require SLA support. Contact: **osologic.team@gmail.com**

See [LICENSING.md](./LICENSING.md) for the full breakdown, [CLA.md](./CLA.md) for contribution terms, and [CONTRIBUTING.md](./CONTRIBUTING.md) to get started.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
