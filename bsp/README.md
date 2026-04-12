# bsp/ — OSOlogic® Board Support Packages

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

Board Support Packages (BSPs) for all officially supported hardware targets.

Each BSP provides the hardware-specific configuration, boot files, device-tree overlays, and integration scripts needed to run OSOlogic on that platform.

## Directory Structure

```
bsp/
├── rpi4/           # Raspberry Pi 4 (BCM2711, quad-core Cortex-A72)
├── rpi5/           # Raspberry Pi 5 (BCM2712, quad-core Cortex-A76)
├── cm4/            # Raspberry Pi Compute Module 4 (industrial carrier variants)
├── beaglebone/     # BeagleBone Black / Green (TI AM335x)
└── custom-plc/     # BorrellPLC custom boards (Roig Borrell S.L. designs)
```

### `rpi4/`
BSP for Raspberry Pi 4. Includes PREEMPT_RT kernel configuration, device-tree overlays for industrial I/O, and boot configuration for OSOlogic Linux.

### `rpi5/`
BSP for Raspberry Pi 5. Updated for the BCM2712 SoC and the new RP1 I/O controller, taking advantage of improved PCIe and GPIO performance.

### `cm4/`
BSP for the Compute Module 4, targeting industrial carrier boards with DIN-rail form factors, expanded I/O headers, and eMMC storage. Ideal for panel-mount deployments.

### `beaglebone/`
BSP for BeagleBone Black and Green (TI AM335x). Leverages the PRU real-time co-processors for deterministic fieldbus and I/O operations where low-latency is critical.

### `custom-plc/`
BSP for BorrellPLC — the custom hardware platform designed and manufactured by Roig Borrell S.L. Includes pinout definitions, hardware-specific drivers, and factory provisioning scripts.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
