# Fleet Management — *Enterprise add-on*

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOlogic](https://github.com/OSOlogic/platform)

> **This is an Enterprise module.** The interface and this documentation are part of the
> Community Edition; the implementation ships with **OSOlogic Enterprise**.

---

## What it adds

Operate many OSOlogic controllers as one estate:

- **Central inventory** — every device, model, firmware version, and health in one console.
- **Fleet-wide OTA** — staged rollouts of OS images and application updates with rollback.
- **Provisioning** — zero-touch onboarding of new controllers from templates.
- **Remote configuration & backup** — push configuration, collect diagnostics, restore.

## How it connects

Each controller runs a lightweight agent that registers with the fleet server and exposes
its status through the standard interfaces. A CE controller integrates by pointing the agent
at your fleet endpoint; management then flows over the same authenticated channels:

```
# per-controller agent config (example)
[fleet]
server   = https://fleet.example.com
group    = line-A
enrolment_token = <issued-by-fleet>
```

## Why Enterprise

Fleet inventory, staged OTA, and provisioning are operational services delivered and
supported as an Enterprise add-on.

**Get it:** osologic.team@gmail.com · [osologic.com](https://osologic.com)
