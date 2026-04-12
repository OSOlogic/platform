# core/ — OSOlogic® Platform Kernel

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

The `core/` directory contains the three fundamental components of the OSOlogic runtime: the real-time in-memory database, the PLC scan-cycle engine, and the real-time Linux kernel configuration.

Together these form the beating heart of the platform — everything else (gateways, UI, language engines, CLI tools) reads from and writes to this core layer.

## Directory Structure

```
core/
├── osodb/          # OSOlogic real-time in-memory database
├── osoruntime/     # IEC 61131-3 scan cycle and task scheduler
└── osokernel/      # PREEMPT_RT kernel patches and build configs
```

### `osodb/`
The **OSOlogic Database** — a real-time, in-memory data bus that acts as the shared state of the entire platform. All modules (gateways, language engines, UI, API) read and write through osodb. Designed for deterministic, low-latency access to process variables, alarms, events, and configuration.

### `osoruntime/`
The **OSOlogic Runtime** — implements the IEC 61131-3 scan cycle (read inputs → execute programs → write outputs). Manages task scheduling (cyclic, event-driven, free-running), program instantiation, and the interface between language engines and osodb.

### `osokernel/`
Kernel configuration and PREEMPT_RT patch sets for OSOlogic Linux targets. Includes build scripts and defconfigs for x86_64, arm64, and armv7 targets. Provides the deterministic scheduling foundation that the runtime depends on.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
