# os-dist/ — OSOlogic® OS Distributions

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

OSOlogic Linux (Debian-based) and Baremetal builds for microcontrollers.

This directory contains the build systems, configuration, and packaging scripts for distributing OSOlogic as ready-to-run operating system images or MCU firmware.

## Directory Structure

```
os-dist/
├── osologic-linux/     # OSOlogic Linux — Debian-based, PREEMPT_RT
│   ├── x86_64/         # PC / embedded x86_64 targets
│   ├── arm64/          # 64-bit ARM (RPi4, RPi5, CM4, BeagleBone)
│   └── armv7/          # 32-bit ARMv7 (legacy boards)
└── osologic-baremetal/ # Baremetal firmware builds for MCUs
    ├── rp2040/         # Raspberry Pi RP2040 (BorrellPLC MCU co-processor)
    ├── stm32/          # STM32 family (fieldbus co-processors)
    └── esp32/          # ESP32 (wireless I/O nodes)
```

### `osologic-linux/`
Debian-based Linux distribution customized for industrial real-time use. Built around a PREEMPT_RT patched kernel from [`core/osokernel`](../core/). Ships with the full OSOlogic stack pre-installed and configured: `osoruntime`, `osodb`, gateways, CLI tools, and the web UI.

Targets:
- **x86_64** — industrial PCs, embedded x86 platforms, virtual machines
- **arm64** — Raspberry Pi 4/5, CM4, BeagleBone, and compatible 64-bit SBCs
- **armv7** — legacy 32-bit ARM boards and older SBCs

### `osologic-baremetal/`
Lightweight firmware for microcontroller targets without a full OS. Used as co-processors for deterministic fieldbus communication, remote I/O nodes, or stand-alone simple automation tasks.

Targets:
- **rp2040** — RP2040-based BorrellPLC co-processor for real-time I/O and fieldbus
- **stm32** — STM32 fieldbus interface co-processors
- **esp32** — ESP32 wireless I/O nodes with MQTT and OSOlogic protocol support

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
