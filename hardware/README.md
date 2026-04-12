# hardware/ — OSOlogic® Open Hardware Designs

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

Open-source hardware designs for the OSOlogic platform: PCB schematics, bill of materials, enclosures, and I/O expansion modules.

All designs are released as open hardware. Manufacturing files (Gerbers, BOM, assembly drawings) are included where available.

## Directory Structure

```
hardware/
├── plc-cpu-board/  # Main CPU carrier board (BorrellPLC)
├── io-modules/     # Digital and analog I/O expansion modules
├── bus-modules/    # Fieldbus interface modules (RS-485, CAN, Ethernet)
└── enclosures/     # DIN-rail and panel-mount enclosure designs
```

### `plc-cpu-board/`
The main BorrellPLC CPU carrier board designed by Roig Borrell S.L. Hosts a Raspberry Pi CM4 (or compatible SoM), provides industrial power input (24 VDC), DIN-rail mounting, status LEDs, watchdog circuitry, and expansion connectors for I/O and fieldbus modules.

### `io-modules/`
Plug-in I/O expansion modules: digital input (24 VDC sink/source), digital output (relay and transistor), analog input (4–20 mA, 0–10 V), and analog output. Designed for field wiring in industrial control panels.

### `bus-modules/`
Fieldbus interface expansion modules: RS-485 (for Modbus RTU), CAN (for CANopen), and additional Ethernet ports. Connect directly to the CPU board expansion bus.

### `enclosures/`
Mechanical designs for DIN-rail enclosures and panel-mount housings. Includes 3D printable models (STEP, STL) and sheet-metal drawings for professional fabrication.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
