#!/usr/bin/env python3
"""
OSOlogic — MCP client (reference example)
=========================================

Connects to the OSOlogic MCP server over stdio, lists the available tools, then
exercises the plant: enumerate devices, read a tag, write a setpoint, and read
it back.

Run:  pip install "mcp[cli]"
      python mcp_client.py
"""
import asyncio
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

SERVER = StdioServerParameters(command=sys.executable, args=["mcp_server.py"])


async def main():
    async with stdio_client(SERVER) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            tools = await session.list_tools()
            print("tools:", ", ".join(t.name for t in tools.tools))

            devices = await session.call_tool("list_devices", {})
            print("\ndevices:", devices.content[0].text)

            before = await session.call_tool("read_tag", {"node_id": "ns=2;s=1.32"})
            print("\nWDT_Timeout before:", before.content[0].text)

            await session.call_tool("write_tag", {"node_id": "ns=2;s=1.32", "value": 2500})
            after = await session.call_tool("read_tag", {"node_id": "ns=2;s=1.32"})
            print("WDT_Timeout after :", after.content[0].text)


if __name__ == "__main__":
    asyncio.run(main())
