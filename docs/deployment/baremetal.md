# Deploying OSOLogic on microcontrollers (baremetal)

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

> **Status: 🔜 Planned.** A dedicated baremetal line for **RP2040 · STM32 · ESP32**. These are
> the smallest OSOLogic targets — no Linux, no database, no installer. Instead of running a
> deployment wizard you **flash firmware** that carries a trimmed runtime and the same
> data-centric model. Build systems live in
> [`os-dist/osologic-baremetal/`](../../os-dist/osologic-baremetal/).

---

## How it differs from the Linux targets

| | Linux targets (Orange Pi, Pi, x86_64) | MCU baremetal (RP2040/STM32/ESP32) |
|---|---|---|
| OS | Linux (PREEMPT_RT) | none — firmware on the metal |
| Install | `oso-setup` / `install_OsoLogic.sh` | flash a `.uf2` / `.bin` / `.elf` image |
| Data hub | MariaDB (source of truth) + `osodb` cache | **`osodb` cache only**, on-device |
| Reach | full stack, all gateways | trimmed: essential I/O + one or two protocols |

## What runs on an MCU (and what does not)

The **data-centric model is the same** — I/O is still keyed by `(module_id, io_definition_id)`
and lives in the `osodb` in-memory cache, which is deliberately dependency-free and small enough
to run here. What changes are the **limitations of the class of device**:

- **No database on-device.** MariaDB (the source of truth at the edge/server) is remote. The MCU
  holds live state in its local `osodb` cache and **syncs when connected** — like a Redis that
  hasn't flushed yet. Offline, it keeps controlling with its last known configuration.
- **Trimmed protocol surface.** Expect a small selection (e.g. Modbus and/or MQTT) rather than the
  full OPC-UA/REST/MCP stack — chosen per board by flash/RAM budget.
- **IEC 61131-3 subset.** Program size is bounded by device memory.
- **Roles.** An MCU is typically a smart I/O node, a remote gateway, or a compact standalone
  controller — not a control-room server.

## Toolchains (planned)

- **RP2040** — Pico SDK (`.uf2`).
- **STM32** — CMake + arm-none-eabi / STM32Cube HAL.
- **ESP32** — ESP-IDF.

Bootloader and shared baremetal pieces: [`os-dist/osologic-baremetal/`](../../os-dist/osologic-baremetal/)
(`rp2040/`, `stm32/`, `esp32/`, `bootloader/`, `libc-nano/`).

---

Interested in the MCU line, or building a board around it? Track or contribute on
[GitHub](https://github.com/OSOlogic/platform).

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
