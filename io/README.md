# io/ — OSOlogic® I/O Layer

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

Hardware Abstraction Layer (HAL), hardware drivers, emulated I/O for testing, and the real-time I/O proxy for network-accessible I/O.

The I/O layer sits between `osoruntime` and the physical hardware. It abstracts all access to physical channels so that PLC programs interact with a uniform tag interface regardless of the underlying hardware or communication medium.

## Directory Structure

```
io/
├── hal/            # Hardware Abstraction Layer — unified I/O channel interface
├── drivers/        # Hardware drivers: GPIO, SPI, I2C, UART, fieldbus adapters
├── emulated/       # Software-emulated I/O for development and testing without hardware
└── realtime-proxy/ # RT proxy exposing local I/O over the network to remote nodes
```

### `hal/`
The Hardware Abstraction Layer defines the uniform interface that `osoruntime` uses to access all I/O channels (digital in/out, analog in/out, serial ports). Drivers register themselves with the HAL at startup. PLC programs never call drivers directly — they go through the HAL.

### `drivers/`
Hardware-specific driver implementations:
- `gpio/` — Linux GPIO (character device and sysfs)
- `spi/` — SPI bus (ADC, DAC, expansion I/O)
- `i2c/` — I2C sensors and I/O expanders
- `uart/` — Serial ports (RS-232, RS-485)
- `fieldbus/` — Low-level drivers for fieldbus adapters (used by the gateways layer)

### `emulated/`
Fully software-emulated I/O subsystem. Allows running and testing PLC programs on a development machine without any physical hardware. Emulated channels can be driven by scripts, test fixtures, or the hardware-in-loop test suite.

### `realtime-proxy/`
A real-time I/O proxy that exposes local physical I/O channels over the network to remote OSOlogic nodes. Enables distributed architectures where I/O modules are remote from the CPU running the PLC program. Uses a low-latency UDP-based protocol with deterministic timing guarantees.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
