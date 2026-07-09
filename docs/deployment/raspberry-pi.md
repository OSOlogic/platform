# Deploying OSOLogic on Raspberry Pi & generic ARM

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0

> **Status: 🔜 Planned.** Official ready-to-flash Raspberry Pi images are on the roadmap.
> The runtime and installer are largely platform-independent, so you can already run OSOLogic
> on a Raspberry Pi (or other arm64/armv7 board) today by installing onto an existing Linux —
> see below.

This guide also applies to **generic ARM** boards (arm64 / armv7).

---

## Board support

Board Support Packages live under [`bsp/`](../../bsp/):

| Board | BSP |
|---|---|
| Raspberry Pi 4 | [`bsp/rpi4`](../../bsp/rpi4) |
| Raspberry Pi 5 | [`bsp/rpi5`](../../bsp/rpi5) |
| Compute Module 4 | [`bsp/cm4`](../../bsp/cm4) |
| BeagleBone | [`bsp/beaglebone`](../../bsp/beaglebone) |
| Custom PLC boards | [`bsp/custom-plc`](../../bsp/custom-plc) |

Linux distribution targets are built from [`os-dist/osologic-linux/targets/arm64`](../../os-dist/osologic-linux/targets/arm64)
and [`.../armv7`](../../os-dist/osologic-linux/targets/armv7).

## Install on an existing Linux (available today)

1. Start from a 64-bit Raspberry Pi OS / Debian / Ubuntu on your board.
2. Put the platform on the board, under the `oso` home:
   ```bash
   sudo useradd -m -s /bin/bash oso 2>/dev/null || true
   sudo -u oso git clone https://github.com/OSOlogic/platform.git /home/oso/PLC_OsoLogic
   ```
3. Run the installer (it lives in the checkout you just cloned):
   ```bash
   cd /home/oso/PLC_OsoLogic/packaging
   sudo ./oso-setup            # guided wizard (ncurses + plain-text fallback)
   # or, for full control:
   sudo ./install_OsoLogic.sh
   ```
   In the wizard, choose **Advanced → source: already on this board** (or point it at the clone).

4. Verify — Service Manager at `https://<board-ip>:8080`. See [orange-pi.md](orange-pi.md)
   for the full walkthrough (the installer steps are identical).

## Build an image yourself

To produce a flashable image for your board, use the build systems in
[`os-dist/`](../../os-dist/) and the helpers in
[`os-dist/scripts/`](../../os-dist/scripts/) (`img_extract.sh`, `img_flash.sh`,
`pishrink.sh`, `universal_chroot.sh`).

## I/O

Raspberry Pi exposes GPIO, SPI, I²C and UART; wire your I/O accordingly and map it in the
Manager. See [`hardware/`](../../hardware/) and [`io/`](../../io/).

---

Want official Pi images sooner? Track or contribute on
[GitHub](https://github.com/OSOlogic/platform).

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
