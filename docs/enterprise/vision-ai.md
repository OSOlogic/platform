# Computer Vision / Advanced AI — *Enterprise add-on*

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOlogic](https://github.com/OSOlogic/platform)

> **This is an Enterprise module.** The interface and this documentation are part of the
> Community Edition; the implementation ships with **OSOlogic Enterprise**. You can build
> and test your integration against the open interfaces below using the CE OPC-UA gateway.

---

## What it does

On-edge **computer vision and AI** integrated with the real-time control loop:

- **Inline inspection & defect detection** — classify/segment product on a line-scan or
  area camera and act within the scan cycle.
- **Sorting / ejection** — vision decisions drive I/O (ejectors, diverters) deterministically
  through the `osodb` hub, with timing aligned to the runtime.
- **Model training pipeline** — dataset capture from live I/O events, training (CNN / YOLO),
  and versioned model deployment to the edge.
- **Accelerated inference** — CPU, iGPU, NPU, or discrete GPU (e.g. NVIDIA A2/A10) back-ends.

## How it connects (open interfaces)

The Enterprise Vision service publishes its results as **standard OPC-UA nodes** under a
`Vision` object, and consumes commands the same way — so any Community Edition component
integrates without lock-in:

```
Object Vision                          [ns=2;s=vision]
  Folder Results
    LastClass        String   Read     ns=2;s=vision.class
    Confidence       Float    Read     ns=2;s=vision.conf
    DefectCount      UInt32   Read     ns=2;s=vision.defects
    EjectDecision    Boolean  Read     ns=2;s=vision.eject
  Folder Control
    ModelName        String   ReadWrite ns=2;s=vision.model
    Enabled          Boolean  ReadWrite ns=2;s=vision.enabled
```

### Example — react to vision decisions from CE

```python
# pip install asyncua
import asyncio
from asyncua import Client, ua

URL = "opc.tcp://plc.local:4840/osologic/server/"

async def main():
    async with Client(url=URL) as c:
        ns = await c.get_namespace_index("urn:osologic:platform")
        eject = c.get_node(ua.NodeId("vision.eject", ns, ua.NodeIdType.String))
        conf  = c.get_node(ua.NodeId("vision.conf",  ns, ua.NodeIdType.String))

        # subscribe to eject decisions and mirror them to a physical output
        out = c.get_node(ua.NodeId("1.0", ns, ua.NodeIdType.String))   # module 1, output 0
        async def on_change(node, val, data):
            if node == eject and val:
                print(f"defect (conf={await conf.read_value():.2f}) -> firing ejector")
                await out.write_value(ua.DataValue(ua.Variant(True, ua.VariantType.Boolean)))
        sub = await c.create_subscription(50, type("H", (), {"datachange_notification": on_change})())
        await sub.subscribe_data_change(eject)
        await asyncio.sleep(3600)

asyncio.run(main())
```

The exact same pattern works over **REST/WebSocket** ([`api/`](../../api/)) and **MQTT**.

## Why Enterprise

Vision/AI carries model IP, accelerated-inference back-ends, a training pipeline, and
per-customer tuning — packaged, supported, and certified as an Enterprise add-on.

**Get it:** osologic.team@gmail.com · [osologic.com](https://osologic.com)
