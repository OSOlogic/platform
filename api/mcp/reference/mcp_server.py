#!/usr/bin/env python3
"""
OSOlogic — Model Context Protocol server (reference implementation)
==================================================================

Exposes an OSOlogic system to AI agents and LLM applications as a small set of
typed MCP tools and resources, backed by the in-memory hub (osodb) and the same
reversible NodeId scheme used across the platform:

    ns=2;s=<module_id>.<io_definition_id>

This reference ships with a self-contained in-memory example hub so it runs on
its own. In a deployment the same tools are wired to osodb, or to the OPC-UA
gateway, which shares the `HubSource` abstraction.

Run:  pip install "mcp[cli]"
      python mcp_server.py          # serves MCP over stdio
"""
from datetime import datetime, timezone

from mcp.server.fastmcp import FastMCP

mcp = FastMCP("OSOlogic")


# ---------------------------------------------------------------------------
# In-memory example hub (replace with the osodb / OPC-UA HubSource adapter)
# ---------------------------------------------------------------------------
class ExampleHub:
    def __init__(self):
        self.devices = [
            {"module_id": 1, "name": "Plant_IO_2", "model": "Aggregated_Plant_IO_2",
             "protocol": "aggregated", "connected": True},
            {"module_id": 2, "name": "Press_Sensor_1", "model": "Borrell_AI_1",
             "protocol": "modbus-rtu", "connected": True},
        ]
        self.tags = {
            "2.0": {"module_id": 2, "io": 0, "name": "Pressure", "folder": "IO",
                    "data_type": "Float", "access": "read", "units": "bar", "value": 3.14},
            "1.32": {"module_id": 1, "io": 32, "name": "WDT_Timeout", "folder": "Config",
                     "data_type": "UInt16", "access": "readwrite", "units": "ms", "value": 1000},
        }
        for i in range(16):
            self.tags[f"1.{i}"] = {"module_id": 1, "io": i, "name": f"Output_{i + 1}",
                                   "folder": "IO", "data_type": "Boolean",
                                   "access": "readwrite", "units": "", "value": False}

    @staticmethod
    def node_id(key):
        return f"ns=2;s={key}"

    @staticmethod
    def _key(node_id):
        # accepts "ns=2;s=1.32" or "1.32"
        return node_id.split("s=")[-1]

    def read(self, node_id):
        t = self.tags[self._key(node_id)]
        status = "Good" if next(d["connected"] for d in self.devices
                                if d["module_id"] == t["module_id"]) else "Bad_NotConnected"
        return {"node_id": self.node_id(self._key(node_id)), "name": t["name"],
                "value": t["value"], "data_type": t["data_type"], "units": t["units"],
                "status": status, "source_timestamp": datetime.now(timezone.utc).isoformat()}

    def write(self, node_id, value):
        key = self._key(node_id)
        t = self.tags[key]
        if t["access"] != "readwrite":
            raise ValueError(f"{self.node_id(key)} is read-only")
        # NodeId resolves back to (module_id, io) — the hub write path is called here
        t["value"] = value
        return {"node_id": self.node_id(key), "module_id": t["module_id"], "io": t["io"],
                "written": value, "status": "Good"}


hub = ExampleHub()


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
@mcp.tool()
def list_devices() -> list[dict]:
    """Enumerate configured devices: module id, name, model, protocol, connection status."""
    return hub.devices


@mcp.tool()
def browse(device_id: int | None = None) -> list[dict]:
    """List variables, optionally filtered to one device. Returns node_id, name, folder,
    data type, access, current value, units and status for each tag."""
    out = []
    for key, t in hub.tags.items():
        if device_id is not None and t["module_id"] != device_id:
            continue
        out.append({"node_id": hub.node_id(key), "name": t["name"], "folder": t["folder"],
                    "data_type": t["data_type"], "access": t["access"],
                    "value": t["value"], "units": t["units"]})
    return out


@mcp.tool()
def read_tag(node_id: str) -> dict:
    """Read a single tag by NodeId (e.g. 'ns=2;s=2.0'): value, data type, units, status,
    and source timestamp."""
    return hub.read(node_id)


@mcp.tool()
def write_tag(node_id: str, value: float | bool | int) -> dict:
    """Write a setpoint to a writable tag by NodeId. The NodeId is resolved back to its
    (module, point) and handed to the hub write path."""
    return hub.write(node_id, value)


# ---------------------------------------------------------------------------
# Resources
# ---------------------------------------------------------------------------
@mcp.resource("osologic://devices")
def devices_resource() -> str:
    """Snapshot of the device inventory."""
    import json
    return json.dumps(hub.devices, indent=2)


@mcp.resource("osologic://tags")
def tags_resource() -> str:
    """Snapshot of all visible tags."""
    import json
    return json.dumps(browse(), indent=2)


if __name__ == "__main__":
    mcp.run()
