# Connectivity & device support

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

What OSOLogic can talk to. Everything lands in the same place — the
[`osodb`](../core/osodb) data hub — so once a device is connected, every surface
(Ladder/ST logic, SQL, OPC-UA, MQTT, REST, MCP, the HMI) can use it.

**Status legend**

| | Meaning |
|---|---|
| ✅ **Native** | Implemented in the Community Edition (reference or better) |
| 🔜 **Roadmap** | Planned native support — scaffolded, to be developed |
| 🔌 **Via gateway** | Reached through an optional third-party compatibility gateway |
| 🧩 **Enterprise** | Certified / hardened variants offered as Enterprise add-ons |

---

## Native — industrial protocols & fieldbus

| Protocol / bus | Status | Where |
|---|---|---|
| **Modbus RTU** (RS-485) | ✅ Native | [`gateways/modbus`](../gateways/modbus/) · [`core/`](../core/) |
| **Modbus TCP** | ✅ Native | [`gateways/modbus`](../gateways/modbus/) |
| **SPI** on-board I/O | ✅ Native | [`core/`](../core/) · [`io/`](../io/) |
| **OPC-UA** (server, basic) | ✅ Native | [`gateways/opc-ua`](../gateways/opc-ua/) (Python + C++ refs) |
| **MQTT** | ✅ Native | [`gateways/mqtt`](../gateways/mqtt/) |
| **CANopen** | 🔜 Roadmap | [`gateways/canopen`](../gateways/canopen/) |
| **EtherNet/IP** | 🔜 Roadmap | [`gateways/ethernetip`](../gateways/ethernetip/) |
| **PROFINET** | 🔜 Roadmap · 🧩 | [`gateways/profinet`](../gateways/profinet/) |
| **OPC-DA** (legacy) | 🔜 Roadmap | [`gateways/opcda-legacy`](../gateways/opcda-legacy/) |
| **BACnet · KNX · DNP3** | 🔜 Roadmap | — |
| OPC-UA security / HA / A&C / Historian | 🧩 Enterprise | [`docs/enterprise`](enterprise/) |

## Native — data & application interfaces

| Interface | Status | Where |
|---|---|---|
| **SQL / MariaDB** direct control | ✅ Native | [`core/osodb`](../core/osodb/) · read `SELECT`, control `UPDATE required_value` |
| **REST + WebSocket** | ✅ Native | [`api/rest`](../api/rest/) |
| **MCP** (Model Context Protocol, AI) | ✅ Native | [`api/mcp`](../api/mcp/) |
| **GraphQL · gRPC** | 🔜 Roadmap | [`api/`](../api/) |
| **Node-RED** flows | ✅ Native | [`ui/node-red`](../ui/node-red/) |
| **IEC 61131-3** (Ladder, ST) | ✅ Native *(prototype)* | [`iec61131/`](../iec61131/) |

## Via gateway — third-party ecosystems (compatibility)

For the broad world of consumer / smart-home / IoT devices, OSOLogic **interoperates**
with existing platforms through optional gateways, rather than duplicating their work.

| Ecosystem | Reached via | Brings |
|---|---|---|
| **Home Assistant** | [`gateways/home-assistant`](../gateways/home-assistant/) compatibility gateway | Zigbee, Z-Wave, Matter/Thread, KNX, Wi-Fi & the many devices HA already supports — mapped to typed osodb tags |
| **Node-RED** community nodes | [`ui/node-red`](../ui/node-red/) | Any protocol/service with a Node-RED node |

> Gateways use each project's **public APIs** and honour its licence — they complement
> those projects, they don't replace or repackage them.

## Add support

- **Native driver** — implement under [`gateways/<protocol>`](../gateways/) against the osodb hub.
- **Home Assistant device** — usually already covered; add a [driver mapping](../gateways/home-assistant/drivers/)
  if a new HA domain needs one.

---

*This matrix evolves as drivers land. Native industrial protocols are the priority; gateways cover
the long tail today.*
