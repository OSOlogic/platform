# Deploying OSOLogic on Orange Pi

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0

The **Orange Pi** is OSOLogic's reference target — the platform that today's images, I/O
layer and installer are validated against. This guide takes you from a blank board to a
running PLC.

---

## What you need

- An **Orange Pi** single-board computer.
- A **MicroSD card** (≥ 16 GB, Class 10 / A1) or onboard **eMMC**.
- A **5 V power supply** rated for your board.
- **Ethernet** (recommended) or configured Wi-Fi.
- A host computer to flash the card ([balenaEtcher](https://etcher.balena.io/) or `dd`).
- *(For real I/O)* the OSOLogic **SPI I/O modules** or your own hardware wired to the SPI bus.

---

## Step 1 — Flash the image

1. Download the latest Orange Pi system image (`.img.xz`) from the
   [Releases](https://github.com/OSOlogic/platform/releases) page.
2. Flash it to the MicroSD / eMMC:
   - **balenaEtcher** — select the image, select the card, flash. *(Simplest.)*
   - **Command line** — use the helper in [`os-dist/scripts/img_flash.sh`](../../os-dist/scripts/img_flash.sh),
     or `xzcat osologic-orangepi.img.xz | sudo dd of=/dev/sdX bs=4M status=progress conv=fsync`
     (double-check `/dev/sdX`).

The image ships a pre-configured Armbian/Debian-based Linux with the OSOLogic code already
on board.

## Step 2 — First boot

1. Insert the card, connect Ethernet, and power the board.
2. Find its IP address (from your router, or `ping osologic.local` if mDNS is up).
3. Log in over SSH as the **`oso`** user:
   ```bash
   ssh oso@<board-ip>
   ```

## Step 3 — Run the installer

From the board, launch the guided wizard:

```bash
sudo oso-setup
```

`oso-setup` autodetects the network and storage, lets you pick **Express** (smart defaults)
or **Advanced**, can generate strong passwords for you, and then sets up the database, MQTT
broker, services and web Manager. It renders a full-screen ncurses UI and falls back to
plain-text prompts on a serial console.

> Prefer full manual control? Run the advanced installer instead:
> `cd /home/oso && sudo ./install_OsoLogic.sh`. See [`packaging/INSTALL.md`](../../packaging/INSTALL.md).

## Step 4 — Verify

When the installer finishes:

- **Service Manager** — `https://<board-ip>:8080`
- **GUI** — port `8082` · **Node-RED** — port `1880` · **MQTT** — port `1883`
- Check the runtime:
  ```bash
  systemctl status plc_osologic-manager
  ```

If you let `oso-setup` generate passwords, they are saved to
`/root/osologic-credentials.txt` (readable by root only).

---

## Wiring the I/O

OSOLogic's reference I/O modules attach to the board's **SPI** bus. Match each module's
chip-select and address to the I/O map you configure in the Manager (or in `osodb`). See the
open hardware designs under [`hardware/`](../../hardware/) and the I/O layer in
[`io/`](../../io/).

## Troubleshooting

- **No web Manager** — confirm the service is up (`systemctl status plc_osologic-manager`) and
  that nothing else uses port `8080`.
- **Installer can't find the source** — the image ships the code under `/home/oso`; on a plain
  Armbian install, clone the platform there first.
- **Serial console** — `oso-setup` automatically drops to its plain-text mode on a dumb `$TERM`.

## Next steps

- Build your first program with the IEC 61131-3 engines — see [`iec61131/`](../../iec61131/).
- Connect over REST/MQTT/OPC-UA or expose the plant to AI agents via MCP — see [`api/`](../../api/).

---

*OSOLogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
