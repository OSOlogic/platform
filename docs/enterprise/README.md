# OSOlogic Enterprise — add-ons

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — The Modern & Open Automation Platform

---

The **Community Edition** you are looking at is a complete, production-capable open
automation platform (AGPL-3.0). **OSOlogic Enterprise** adds optional, commercially
licensed modules for organizations that need scale, certification, high availability,
advanced security, or turnkey vertical solutions.

> **How this works (open-core).** Enterprise features appear in the Community Edition
> as **documented module stubs**: the directory and its public interface exist here,
> the *implementation* ships with OSOlogic Enterprise. Each stub explains what the
> feature does and includes **working examples of how to connect to it** from your
> CE deployment — over the same open interfaces (OPC-UA, REST, MQTT, the `osodb` hub).
> Nothing is locked away that you need to build real systems on CE; Enterprise is
> additive.

## Enterprise add-ons

| Add-on | What it adds | Teaser + connect example |
|---|---|---|
| **Computer Vision / Advanced AI** | Real-time inspection, defect detection, sorting, and on-edge inference; model training pipeline; vision→PLC actuation. | [vision-ai.md](vision-ai.md) |
| **Advanced Real-Time Multi-Axis CNC** | Deterministic multi-axis motion, coordinated trajectory/interpolation, look-ahead, kinematics, G-code runtime. | [cnc-multiaxis.md](cnc-multiaxis.md) |
| **OPC-UA Advanced** | Security (certificates, roles, auditing), Historical Access, Alarms & Conditions, aggregation, redundancy. | see [`gateways/opc-ua`](../../gateways/opc-ua/) |
| **Certified Protocol Gateways** | PROFINET, EtherNet/IP, OPC-DA legacy — with conformance certification. | *(stub)* |
| **High Availability / Redundancy** | Hot-standby runtime, redundant I/O, seamless failover. | *(stub)* |
| **SCADA/Historian at scale** | Multi-station SCADA, time-series historian, alarm management, analytics. | *(stub)* |
| **Fleet Management** | Central provisioning, fleet-wide OTA, device inventory. | *(stub)* |
| **Security & Compliance** | RBAC, LDAP/AD/SSO, audit trails, IEC 62443 packs, secure-boot management. | *(stub)* |
| **Functional Safety (certified)** | Certified safe-state / SIL-rated packages (the base `secure_state` mechanism is in CE). | *(stub)* |

## Connecting from Community Edition

Every Enterprise module speaks the **same open interfaces** as the rest of the platform,
so CE code integrates without lock-in:

- **OPC-UA** — browse/read/write/subscribe to enterprise services as standard nodes.
- **REST / WebSocket / GraphQL** — via [`api/`](../../api/).
- **MQTT** — event/telemetry bus.
- **`osodb`** — the in-memory hub every module reads from and writes to.

See each add-on page for a concrete, runnable connection example.

## Get Enterprise

Contact **licensing@osologic.com** or visit **[osologic.com](https://osologic.com)**.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
