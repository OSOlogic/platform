# Certified Protocol Gateways (PROFINET, EtherNet/IP, OPC-DA) — *Enterprise add-on*

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOlogic](https://github.com/OSOlogic/platform)

> **This is an Enterprise module.** The interface and this documentation are part of the
> Community Edition; the implementation ships with **OSOlogic Enterprise**.

---

## What it adds

Production-grade connectivity to protocols that require certified, licensed stacks:

- **PROFINET** — IO-Device and IO-Controller, conformance class B/C, with GSDML support.
- **EtherNet/IP** — adapter and scanner, CIP objects, implicit/explicit messaging.
- **OPC-DA (legacy)** — bridge to classic OPC/DCOM systems still in the field.

The Community Edition already ships **Modbus RTU/TCP**, **MQTT**, and the **OPC-UA** server,
which cover most integrations. These certified gateways are for plants standardized on
PROFINET/EtherNet-IP or that must interoperate with legacy OPC-DA.

## How it connects

Each gateway maps its process data onto the same `osodb` hub, so it appears through the
standard OPC-UA / REST / MQTT interfaces like any other point — no special client needed.
A PROFINET slot/subslot or an EtherNet/IP assembly instance becomes an OPC-UA Variable
under the corresponding device Object, addressable with the usual reversible NodeId.

## Why Enterprise

These stacks carry **conformance certification costs** (PI, ODVA) and licensed protocol
implementations, delivered and supported as an Enterprise add-on.

**Get it:** osologic.team@gmail.com · [osologic.com](https://osologic.com)
