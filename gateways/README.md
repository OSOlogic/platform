# gateways/ — OSOlogic® Protocol Gateways

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · AGPL-3.0

---

Protocol connectors bridging legacy and modern industrial protocols to `osodb`.

Each gateway translates between a specific protocol and the OSOlogic data bus, making field devices and external systems accessible as process variables regardless of their native communication standard.

## Directory Structure

```
gateways/
├── opc-ua/         # OPC Unified Architecture — server and client
├── modbus/         # Modbus TCP and Modbus RTU (RS-485)
├── profinet/       # PROFINET IO controller and device stack
├── ethernetip/     # EtherNet/IP (CIP) adapter and scanner
├── canopen/        # CANopen master stack
├── mqtt/           # MQTT broker client and topic mapper
├── opcda-legacy/   # OPC Data Access (classic COM/DCOM) legacy bridge
└── common/         # Shared gateway utilities: tag mapper, polling engine, reconnect logic
```

### `opc-ua/`
Full OPC-UA server and client implementation. Exposes the `osodb` tag space as an OPC-UA address space and allows subscribing to external OPC-UA servers. The primary integration protocol for modern industrial systems and SCADA platforms.

### `modbus/`
Modbus TCP and Modbus RTU (over RS-485) master and slave implementations. Supports polling of coils, discrete inputs, holding registers, and input registers, with automatic mapping into `osodb`.

### `profinet/`
PROFINET IO controller and device stack. Enables integration with PROFINET-based field devices such as Siemens ET200 distributed I/O, variable-frequency drives, and safety modules.

### `ethernetip/`
EtherNet/IP (Common Industrial Protocol) adapter and scanner. Connects to Allen-Bradley PLCs, distributed I/O, and other CIP-compatible devices.

### `canopen/`
CANopen master stack for connecting servo drives, encoders, sensors, and distributed I/O over CAN bus. Supports SDO/PDO communication and NMT management.

### `mqtt/`
MQTT broker client with configurable topic-to-tag mapping. Supports MQTT 3.1.1 and 5.0. Used for IoT integration, cloud connectivity, and Node-RED pipelines.

### `opcda-legacy/`
Bridge for legacy OPC Data Access (OPC-DA) servers using COM/DCOM. Enables integration with older SCADA and DCS systems during migration to OPC-UA.

### `common/`
Shared utilities used across all gateways: tag mapping engine, polling scheduler, connection watchdog and reconnect logic, data-type conversion, and gateway health reporting.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
