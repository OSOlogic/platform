# Advanced Real-Time Multi-Axis CNC — *Enterprise add-on*

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOlogic](https://github.com/OSOlogic/platform)

> **This is an Enterprise module.** The interface and this documentation are part of the
> Community Edition; the implementation ships with **OSOlogic Enterprise**. Build and test
> your integration against the open interfaces below using the CE OPC-UA gateway.

---

## What it does

Deterministic **multi-axis motion control** running on the OSOlogic real-time core:

- **Coordinated motion** across N axes with trajectory interpolation (linear, circular,
  helical, spline) and **look-ahead** for smooth high-speed contouring.
- **Kinematics** — Cartesian, gantry, and custom transforms; tool-length/radius compensation.
- **G-code runtime** — parse and execute standard G/M-code, with jog, MDI, and program modes.
- **Cycle-accurate** — motion planning bound to the PREEMPT_RT scan cycle for repeatable timing.
- **Safety interlocks** — coordinated safe-state with the base `secure_state` mechanism (CE).

## How it connects (open interfaces)

The Enterprise CNC service exposes axes and program control as **standard OPC-UA nodes**
under a `CNC` object:

```
Object CNC                             [ns=2;s=cnc]
  Folder Axes
    X.Position       Double  Read      ns=2;s=cnc.x.pos
    X.Velocity       Double  Read      ns=2;s=cnc.x.vel
    Y.Position       Double  Read      ns=2;s=cnc.y.pos
    Z.Position       Double  Read      ns=2;s=cnc.z.pos
  Folder Program
    State            String  Read      ns=2;s=cnc.state      # idle|running|feed-hold|alarm
    Feedrate         Double  ReadWrite ns=2;s=cnc.feed
    Load             String  ReadWrite ns=2;s=cnc.load       # program name
    Command          String  ReadWrite ns=2;s=cnc.cmd        # start|hold|resume|stop|home
```

### Example — supervise and drive a job from CE

```python
# pip install asyncua
import asyncio
from asyncua import Client, ua

URL = "opc.tcp://cnc.local:4840/osologic/server/"

async def main():
    async with Client(url=URL) as c:
        ns = await c.get_namespace_index("urn:osologic:platform")
        def n(s): return c.get_node(ua.NodeId(s, ns, ua.NodeIdType.String))

        await n("cnc.load").write_value(ua.DataValue(ua.Variant("part_042.nc", ua.VariantType.String)))
        await n("cnc.cmd").write_value(ua.DataValue(ua.Variant("start", ua.VariantType.String)))

        while (state := await n("cnc.state").read_value()) == "running":
            x = await n("cnc.x.pos").read_value(); y = await n("cnc.y.pos").read_value()
            print(f"running  X={x:.3f} Y={y:.3f}")
            await asyncio.sleep(0.5)
        print("final state:", state)

asyncio.run(main())
```

## Why Enterprise

Real-time multi-axis interpolation, kinematics, and G-code runtime are high-value,
safety-relevant, and tuned per machine — delivered and supported as an Enterprise add-on.

**Get it:** licensing@osologic.com · [osologic.com](https://osologic.com)
