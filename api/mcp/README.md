# api/mcp — Model Context Protocol interface

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — The Modern & Open Automation Platform · AGPL-3.0

---

The **MCP** interface lets AI agents and LLM applications interact with an OSOlogic system
through a small, well-typed set of tools and resources, backed by the same in-memory hub
(`osodb`) and NodeId scheme as the other interfaces.

It turns the plant into something an agent can safely reason about: *list the devices, read a
tag, write a setpoint, browse the address space* — with the same reversible NodeId
(`ns=2;s=<module_id>.<io_definition_id>`) used across the platform.

## Tools

| Tool | Description |
|---|---|
| `list_devices()` | Enumerate configured devices (module, model, protocol, connection status). |
| `browse(device_id?)` | List variables as `{node_id, name, folder, data_type, access, value, units, status}`. |
| `read_tag(node_id)` | Read a single tag: value, status, source timestamp, engineering units. |
| `write_tag(node_id, value)` | Resolve the NodeId back to a point and write a setpoint. |

## Resources

| Resource | Description |
|---|---|
| `osologic://devices` | Snapshot of the device inventory. |
| `osologic://tags` | Snapshot of all visible tags. |

## Reference implementation

[`reference/mcp_server.py`](reference/mcp_server.py) is a self-contained MCP server built on
the Python MCP SDK. It ships with an in-memory example hub so it runs on its own; in a
deployment the same tools are wired to `osodb` (or to the OPC-UA gateway, which shares the
`HubSource` abstraction — see [`gateways/opc-ua`](../../gateways/opc-ua/)).

```bash
pip install "mcp[cli]"
python reference/mcp_server.py            # serves MCP over stdio
python reference/mcp_client.py            # example: connect, list devices, read/write a tag
```

## Status

Reference/skeleton for the Community Edition MCP interface. Access control, subscriptions,
and streaming updates are part of the OSOlogic Enterprise security and AI add-ons.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
