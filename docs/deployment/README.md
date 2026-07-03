# Deploying OSOLogic

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0

**Start here** to install, download and deploy OSOLogic on your PLC hardware —
from a bare board to a running PLC.

---

## Supported targets

| Target | Status | Guide |
|---|---|---|
| **Orange Pi** (SBC, SPI I/O) | ✅ **Supported** | [orange-pi.md](orange-pi.md) |
| **Raspberry Pi** (official, 4 / 5 / CM4) | 🔜 Planned | [raspberry-pi.md](raspberry-pi.md) |
| **Generic ARM** (arm64 / armv7) | 🔜 Planned | *(see raspberry-pi.md)* |
| **x86_64** (industrial PC / VM) | 🔜 Planned | [x86_64.md](x86_64.md) |
| **MCU baremetal** (RP2040 · STM32 · ESP32) | 🔜 Planned | [baremetal.md](baremetal.md) |

> The reference deployment today is the **Orange Pi**. Raspberry Pi (official images),
> generic ARM and x86_64 targets are on the roadmap; the runtime and installer are
> largely platform-independent, so those targets mostly need image builds and BSP work
> (see [`bsp/`](../../bsp/) and [`os-dist/`](../../os-dist/)).
>
> **Microcontrollers** (RP2040/STM32/ESP32) are a distinct baremetal target: no Linux, no
> database and no `oso-setup` — a trimmed runtime with the same data-centric model (osodb
> as a local cache) is flashed as firmware. See [`os-dist/osologic-baremetal/`](../../os-dist/osologic-baremetal/)
> and [baremetal.md](baremetal.md).

## Deploy in two steps

### Step 1 — Get OSOLogic onto the board

- **Flash a pre-built image** *(recommended — fastest)*: download the latest system
  image (`.img.xz`) from the [Releases](https://github.com/OSOlogic/platform/releases)
  page and flash it to a MicroSD / eMMC with [balenaEtcher](https://etcher.balena.io/)
  (≥ 16 GB, Class 10). The image ships a pre-configured Linux (Ubuntu/Armbian-based)
  with the code already on board.
- **Or use an existing Linux**: clone the platform onto a board you already run
  (`git clone https://github.com/OSOlogic/platform.git`), or build a custom image —
  see [`os-dist/`](../../os-dist/) and its [`scripts/`](../../os-dist/scripts/).

### Step 2 — Run the installer

Boot the board and choose how to install. Both paths configure the same stack;
full details in **[`packaging/INSTALL.md`](../../packaging/INSTALL.md)**.

- **Guided install — `oso-setup`** *(recommended)*: a fast, intuitive wizard with a
  full-screen **ncurses UI** (`dialog`/`whiptail`) and a plain-text fallback for serial
  consoles. Autodetects the network, can generate strong passwords, done in minutes.
  ```bash
  sudo oso-setup
  ```
- **Advanced install — `install_OsoLogic.sh`** *(full control)*: every component, every
  prompt, fully scriptable — including an unattended `--config` mode for reproducible
  and CI deployments.
  ```bash
  cd /home/oso && sudo ./install_OsoLogic.sh
  ```

## What gets deployed

| Component | Role |
|---|---|
| **PLC core** (C++) | Real-time scan cycle, SPI/Modbus I/O — [`core/`](../../core/) |
| **osodb** | In-memory data hub |
| **Gateways** | MQTT, Modbus TCP, OPC-UA — [`gateways/`](../../gateways/) |
| **APIs** | REST + WebSocket, MCP — [`api/`](../../api/) |
| **Web UIs** | Service Manager, GUI, Node-RED, webmin-oso — [`ui/`](../../ui/) |
| **Database** | MariaDB (config + status), Mosquitto (MQTT broker) |

## Reference documentation

The full, rendered documentation (developer + user guides) lives under
[`docs/mintlify/`](../mintlify/) — including the detailed
[installation guide](../mintlify/users/installation.mdx).

---

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
