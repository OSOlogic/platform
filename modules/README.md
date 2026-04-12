# modules/ — DEPRECATED

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

> **This directory is deprecated and will be removed.**

The `modules/` directory represented an early attempt at organizing the OSOlogic codebase into functional modules. This structure has been fully superseded by the current scaffold.

All functionality previously intended for this directory is now organized under the proper top-level modules:

| Old location | New location |
|---|---|
| `modules/core/` | [`core/`](../core/) — osodb, osoruntime, osokernel |
| `modules/main/` | [`iec61131/`](../iec61131/), [`gateways/`](../gateways/), [`io/`](../io/) |
| `modules/experimental/` | [`contrib/`](../contrib/) |

Do not add new code here.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
