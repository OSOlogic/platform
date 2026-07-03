#!/usr/bin/env python3
"""End-to-end check of the OSOlogic OPC-UA gateway: browse, read (value +
DataType + StatusCode + SourceTimestamp), write, and confirm the NodeId
resolves back to the hub."""
import asyncio
from asyncua import Client, ua

URL = "opc.tcp://127.0.0.1:4840/osologic/server/"


async def main():
    async with Client(url=URL) as c:
        ns = await c.get_namespace_index("urn:osologic:platform")
        print(f"connected — OSOlogic namespace index = {ns}\n")

        print("Address space (browse):")
        objects = c.nodes.objects
        for dev in await objects.get_children():
            if dev.nodeid.NamespaceIndex != ns:
                continue
            bn = (await dev.read_browse_name()).Name
            print(f"  Object {bn}  [{dev.nodeid.to_string()}]")
            for folder in await dev.get_children():
                if folder.nodeid.NamespaceIndex != ns:
                    continue
                fbn = (await folder.read_browse_name()).Name
                print(f"    Folder {fbn}")
                for var in await folder.get_children():
                    if var.nodeid.NamespaceIndex != ns:
                        continue
                    vbn = (await var.read_browse_name()).Name
                    dv = await var.read_data_value()
                    dtype = (await var.read_data_type_as_variant_type()).name
                    st = dv.StatusCode.name if dv.StatusCode else "Good"
                    print(f"      {vbn:16} = {str(dv.Value.Value):8} "
                          f"[{dtype:8} {st}]  {var.nodeid.to_string()}")

        # Read a specific node by its reversible string NodeId
        wdt = c.get_node(ua.NodeId("1.32", ns, ua.NodeIdType.String))
        before = await wdt.read_value()
        print(f"\nWDT_Timeout (ns={ns};s=1.32) before write = {before}")

        # Write through OPC-UA -> gateway must resolve NodeId back to (module 1, io 32)
        await wdt.write_value(ua.DataValue(ua.Variant(2500, ua.VariantType.UInt16)))
        await asyncio.sleep(1.5)                      # let the refresh loop resolve it
        after = await wdt.read_value()
        print(f"WDT_Timeout after client write        = {after}")
        print("\nOK — write resolved back to the hub" if after == 2500
              else "\nWARN — value did not round-trip")


if __name__ == "__main__":
    asyncio.run(main())
