# OSOlogic Licensing

This document explains the licensing model of the OSOlogic project in detail.

## Why a multi-license model?

OSOlogic is an open platform at its core, but developing and maintaining
industrial-grade software requires a sustainable funding model. Our
approach follows the pattern used by MongoDB, MariaDB Corporation, Grafana
Labs and others:

- The **core** is strongly copyleft (AGPL-3.0) to ensure that the
  commons remains open, even in network deployments.
- **Client libraries and tooling** are permissive (Apache-2.0) so that
  any application — open or proprietary — can integrate with OSOlogic
  without legal friction.
- **Enterprise features** fund the project through commercial licensing.

## Component-by-component breakdown

| Directory | License | Rationale |
|-----------|---------|-----------|
| `core/` | AGPL-3.0 | RT kernel, osodb, scan cycle — must stay open |
| `iec61131/` | AGPL-3.0 | Language engines (Ladder, ST, FBD, SFC, IL) |
| `gateways/` | AGPL-3.0 | Protocol connectors (OPC-UA, Modbus, PROFINET…) |
| `io/` | AGPL-3.0 | HAL and hardware drivers |
| `os-dist/` | AGPL-3.0 | OSOlogic Linux distribution and baremetal builds |
| `bsp/` | AGPL-3.0 | Board support packages |
| `ui/` | AGPL-3.0 | Web interfaces and editors (served over network) |
| `api/` | AGPL-3.0 | API endpoints and specifications |
| `cli/` | AGPL-3.0 | Command-line tools |
| `sdk/` | Apache-2.0 | Client libraries and language bindings |
| `standard/` | Apache-2.0 | Open schemas, data model, protocol spec |
| `docs/` | CC-BY-4.0 | Documentation |
| `hardware/` | CERN-OHL-S-2.0 | Open hardware designs (PCB, BOM) |

### AGPL-3.0 components (`core/`, `iec61131/`, `gateways/`, `io/`, `ui/`, `api/`, `cli/`)

Under AGPL-3.0 you have the four freedoms to use, study, modify and
redistribute the code. In addition, Section 13 requires that if you
**make the modified software available over a network**, you must also
make the corresponding source code available to all network users.

This protects the project from closed-source cloud forks and ensures
that improvements benefit the entire community.

### Apache-2.0 components (`sdk/`, `standard/`)

Permissive license with an explicit patent grant. You can use these
SDKs and schemas in **any** software, including closed-source commercial
products, without triggering any copyleft obligation on your own code.

**Your proprietary SCADA, MES, ERP or IoT application can link to the
OSOlogic SDK freely. Your code stays yours.**

### CC-BY-4.0 (`docs/`)

Documentation is published under Creative Commons Attribution 4.0.
You may share and adapt it freely, provided you give appropriate credit.

### CERN-OHL-S-2.0 (`hardware/`)

Hardware designs are published under the CERN Open Hardware Licence
Strong v2 (CERN-OHL-S-2.0), which applies copyleft to hardware
modifications, consistent with the spirit of AGPL-3.0 for software.

---

## Commercial licensing

Roig Borrell S.L. holds the copyright of the original OSOlogic code
and, through the Contributor License Agreement (CLA), of all accepted
contributions. This allows us to offer OSOlogic under a **commercial
license** to organizations that:

- Cannot or do not want to comply with AGPL-3.0.
- Wish to embed OSOlogic in proprietary hardware or software products.
- Require enterprise support, SLA or indemnification.
- Operate regulated environments requiring custom licensing terms.

Commercial licensing inquiries: **licensing@osologic.com**

---

## Contributions & CLA

By contributing to OSOlogic you agree to our Contributor License
Agreement (`CLA.md`). This grants Roig Borrell S.L. the rights necessary to:

- Distribute your contribution under the applicable open-source license.
- Sublicense your contribution under commercial terms.
- Relicense the project in the future if the community and
  sustainability of the project require it.

You retain full copyright over your contribution.

See [CONTRIBUTING.md](./CONTRIBUTING.md) and [CLA.md](./CLA.md) for details.

---

## Trademark

"OSOlogic", "Borrell Automation", "BorrellPLC", "XPLC" and
"PlantManager" are registered trademarks of Roig Borrell S.L. and
Ibercomp S.L. The open-source license does **not** grant trademark rights.
Forks and derivative works must use a different name.

---

## FAQ

**Can I use OSOlogic in my factory?**
Yes, freely. Internal use never triggers AGPL distribution obligations.

**Can I sell a PLC with OSOlogic pre-installed?**
Yes, under AGPL-3.0 (you must provide source and license notices).
Or under a commercial license if you prefer to keep your additions closed.

**Can I connect my proprietary SCADA to OSOlogic?**
Yes. Use the Apache-2.0 SDK. Your SCADA code is unaffected.

**Can I offer OSOlogic as a cloud service?**
Yes, but AGPL-3.0 requires you to publish all your modifications
to the core. Or buy a commercial license.

**Can I fork OSOlogic and rename it?**
Yes, under AGPL-3.0. You cannot reuse the OSOlogic trademark.

**Is OSOlogic OSI-approved open source?**
Yes. AGPL-3.0 and Apache-2.0 are both OSI-approved licenses.
