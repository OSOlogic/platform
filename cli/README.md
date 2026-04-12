# cli/ — OSOlogic® Command-Line Tools

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

Command-line tools for system control, diagnostics, I/O inspection, and firmware flashing.

These tools are designed to work both on the OSOlogic device itself and from a remote host. They communicate with `osoruntime` and `osodb` over local sockets or network connections.

## Directory Structure

```
cli/
├── osctl/          # Main system control: start/stop runtime, manage tasks and programs
├── osodb-cli/      # osodb interactive shell: read/write process variables, inspect DB state
├── io-probe/       # Real-time I/O diagnostics: monitor digital/analog channels live
├── plc-flash/      # Firmware flashing tool for Baremetal targets (RP2040, STM32, ESP32)
└── diag/           # System diagnostics: RT latency, CPU load, network, OS health checks
```

### `osctl`
The primary administrative CLI for OSOlogic. Start and stop the runtime, load and unload PLC programs, manage tasks, inspect scan cycle status, and control services.

### `osodb-cli`
Interactive shell for `osodb`. Query and set process variables, inspect tag tables, trigger alarms, and explore the real-time database state. Useful for commissioning and debugging.

### `io-probe`
Real-time I/O monitoring tool. Displays live values of digital inputs/outputs, analog channels, and fieldbus I/O modules. Supports filtering by channel group and logging to file.

### `plc-flash`
Firmware flashing utility for OSOlogic Baremetal targets. Supports RP2040, STM32, and ESP32 via USB, JTAG, or network (OTA). Used for factory provisioning and field updates.

### `diag`
System diagnostics tool. Reports RT scheduling latency, CPU and memory usage, network interface stats, service health, and kernel RT parameters. Essential for commissioning and performance tuning.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
