# standard/ — OSOlogic® Open Standard

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · Apache-2.0

---

Open standard definitions for the OSOlogic platform: schemas, canonical data model, protocol specification, and RFCs.

The goal of the OSOlogic Standard is to create an open, vendor-neutral specification that any implementation can conform to — enabling interoperability between OSOlogic nodes, third-party tools, and the broader industrial automation ecosystem.

## Directory Structure

```
standard/
├── schema/         # JSON and XML schemas for OSOlogic data structures
├── data-model/     # Canonical data model: tags, alarms, events, devices
├── protocol/       # OSOlogic inter-node protocol specification
├── rfcs/           # OSOlogic Request for Comments — design proposals and decisions
└── compliance/     # Compliance test definitions for conformance validation
```

### `schema/`
Formal schemas for all OSOlogic data structures serialized over the API, inter-node protocol, and storage. Includes JSON Schema and XML Schema (XSD) definitions for process variable tags, alarm records, event logs, device descriptors, and configuration files.

### `data-model/`
The OSOlogic canonical data model. Defines the structure of the process variable namespace, tag addressing conventions, data types, engineering units, alarm thresholds, and device descriptors. This is the reference model that `osodb` implements.

### `protocol/`
Specification for the OSOlogic inter-node communication protocol: message framing, addressing, synchronization, and the real-time I/O proxy wire format. Written as a formal protocol document with BNF grammar where applicable.

### `rfcs/`
OSOlogic Requests for Comments — the design proposal and decision process for the standard. Each RFC proposes a specific change or addition, goes through a review period, and is either accepted, rejected, or superseded. Accepted RFCs become part of the standard.

### `compliance/`
Compliance test definitions used to validate that an OSOlogic implementation conforms to the standard. Includes test cases for the data model, protocol, and API contracts.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
