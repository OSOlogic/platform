# High Availability / Redundancy — *Enterprise add-on*

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOlogic](https://github.com/OSOlogic/platform)

> **This is an Enterprise module.** The interface and this documentation are part of the
> Community Edition; the implementation ships with **OSOlogic Enterprise**.

---

## What it adds

Continuous operation for critical processes:

- **Hot-standby runtime** — a primary and a backup controller run in lockstep; the backup
  takes over on failure with bumpless transfer of outputs.
- **Redundant I/O and networks** — dual fieldbus paths with automatic switchover.
- **State replication** — the `osodb` hub and the runtime scan state are mirrored to the
  standby so it holds an up-to-date process image at all times.
- **Health arbitration** — heartbeat, watchdog, and split-brain protection.

## How it connects

The redundant pair presents a **single logical endpoint** to clients: the same OPC-UA /
REST / MQTT interface and NodeId space regardless of which controller is active. A
`Redundancy` object exposes role and health so operators and SCADA can observe failover:

```
Object Redundancy      [ns=2;s=ha]
  Role       String  Read   ns=2;s=ha.role     # primary | standby
  Partner    String  Read   ns=2;s=ha.partner  # reachable | lost
  LastSwitch DateTime Read  ns=2;s=ha.lastswitch
```

## Why Enterprise

Deterministic failover and state replication are safety-relevant and validated per
deployment, delivered and supported as an Enterprise add-on.

**Get it:** licensing@osologic.com · [osologic.com](https://osologic.com)
