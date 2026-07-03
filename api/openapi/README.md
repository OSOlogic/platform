# OSOlogic Admin API — OpenAPI Spec

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · AGPL-3.0

---

`osologic-admin-api.yaml` defines the shared contract for both admin frontends:

| Frontend | Target | File |
|---|---|---|
| `ui/webmin-oso/cockpit/` | Linux (RPi4, CM4, x86_64) | Cockpit React modules |
| `ui/webmin-oso/embedded/` | Baremetal (RP2040, STM32) | mongoose.ws + vanilla JS |

## Endpoint tiers

- **[CORE]** — must be implemented on all targets including baremetal.
- **[LINUX]** — only required on Linux targets (Cockpit modules).

## WebSocket protocol

See `../websocket/protocol.md`.

## Validate / render

```bash
# Render docs locally (requires npx)
npx @redocly/cli preview-docs osologic-admin-api.yaml

# Validate
npx @redocly/cli lint osologic-admin-api.yaml
```
