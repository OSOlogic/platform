#!/usr/bin/env python3
"""
OSOlogic — OPC-UA gateway (reference implementation)
====================================================

Exposes the OSOlogic in-memory data hub (osodb) as an OPC-UA address space,
following OPC-UA information-model conventions, WITHOUT changing the core
data model:

  * NodeId is deterministic and reversible:  ns=<idx>;s=<module_id>.<io_def_id>
  * Reads come from a *view* over the hub (see sql/opcua_views.sql for the
    MariaDB adapter equivalent).
  * Writes are resolved back from NodeId -> (module_id, io_definition_id) and
    handed to the hub's existing write path — the view is never written to.

The gateway talks to a `HubSource` abstraction. Two adapters are provided:

  * InMemorySource  — the osodb-style in-memory hub (self-contained example).
  * MariaDBSource   — adapter that reads the opcua_* SQL views.

This mirrors the platform architecture: the in-memory database is the canonical
hub; MariaDB and other databases are adapters behind it.

Run the example:  python3 osologic_opcua_server.py --example
Endpoint:         opc.tcp://0.0.0.0:4840/osologic/server/
Namespace:        urn:osologic:platform
"""
import argparse
import asyncio
import logging
from datetime import datetime, timezone

from asyncua import Server, ua

NAMESPACE_URI = "urn:osologic:platform"
ENDPOINT = "opc.tcp://0.0.0.0:4840/osologic/server/"

# OPC-UA built-in DataType name -> asyncua VariantType
DATATYPE_MAP = {
    "Boolean": ua.VariantType.Boolean,
    "UInt16": ua.VariantType.UInt16,
    "UInt32": ua.VariantType.UInt32,
    "UInt64": ua.VariantType.UInt64,
    "Int16": ua.VariantType.Int16,
    "Int32": ua.VariantType.Int32,
    "Float": ua.VariantType.Float,
    "Double": ua.VariantType.Double,
}

log = logging.getLogger("osologic.opcua")


# ---------------------------------------------------------------------------
# Hub source abstraction (the canonical in-memory hub, or an adapter over it)
# ---------------------------------------------------------------------------
class HubSource:
    """Interface every adapter implements. Shapes match sql/opcua_views.sql."""

    def list_objects(self):
        raise NotImplementedError

    def list_variables(self):
        raise NotImplementedError

    def read(self, module_id, io_definition_id):
        """Return (value, status_good: bool, source_timestamp)."""
        raise NotImplementedError

    def write_setpoint(self, module_id, io_definition_id, value):
        """Write path: NodeId already resolved back to ids. Return True on ok."""
        raise NotImplementedError


class InMemorySource(HubSource):
    """osodb-style in-memory hub, populated with a representative configuration."""

    def __init__(self):
        self._objects = []
        self._vars = {}          # (mid, iod) -> dict(meta + live)

    def add_object(self, module_id, name, model_name, protocol, connected=True):
        self._objects.append(dict(
            module_id=module_id, node_id=f"ns=2;s={module_id}",
            browse_name=name, display_name=name, model_name=model_name,
            protocol=protocol, is_connected=connected,
            status_code="Good" if connected else "Bad_NotConnected"))

    def add_variable(self, module_id, io_def_id, label, folder, data_type,
                     writable, units="", value=0):
        self._vars[(module_id, io_def_id)] = dict(
            module_id=module_id, io_definition_id=io_def_id,
            node_id=f"ns=2;s={module_id}.{io_def_id}",
            browse_name=label, display_name=label, folder=folder,
            data_type=data_type,
            access_level="CurrentRead|CurrentWrite" if writable else "CurrentRead",
            engineering_units=units, value=value,
            source_timestamp=datetime.now(timezone.utc))

    def _connected(self, module_id):
        return next((o["is_connected"] for o in self._objects
                     if o["module_id"] == module_id), False)

    def list_objects(self):
        return list(self._objects)

    def list_variables(self):
        return list(self._vars.values())

    def read(self, module_id, io_definition_id):
        v = self._vars[(module_id, io_definition_id)]
        return v["value"], self._connected(module_id), v["source_timestamp"]

    def write_setpoint(self, module_id, io_definition_id, value):
        key = (module_id, io_definition_id)
        if key not in self._vars:
            return False
        self._vars[key]["value"] = value
        self._vars[key]["source_timestamp"] = datetime.now(timezone.utc)
        log.info("WRITE resolved  NodeId ns=2;s=%s.%s  ->  module=%s io=%s  value=%s",
                 module_id, io_definition_id, module_id, io_definition_id, value)
        return True


class MariaDBSource(HubSource):
    """Adapter that reads the opcua_* SQL views (see sql/opcua_views.sql).

    Reads are served from the views; writes bypass the views and update the
    setpoint through the runtime write path."""

    def __init__(self, **conn):
        import mariadb  # MariaDB connector, required by this adapter
        self._cx = mariadb.connect(**conn)

    def _q(self, sql, args=()):
        cur = self._cx.cursor(dictionary=True)
        cur.execute(sql, args)
        rows = cur.fetchall()
        cur.close()
        return rows

    def list_objects(self):
        return self._q("SELECT * FROM opcua_objects")

    def list_variables(self):
        return self._q("SELECT * FROM opcua_variables")

    def read(self, module_id, io_definition_id):
        r = self._q("SELECT value, status_code, source_timestamp FROM opcua_variables "
                    "WHERE module_id=? AND io_definition_id=?",
                    (module_id, io_definition_id))
        if not r:
            return None, False, None
        row = r[0]
        return row["value"], row["status_code"] == "Good", row["source_timestamp"]

    def write_setpoint(self, module_id, io_definition_id, value):
        cur = self._cx.cursor()
        cur.execute("UPDATE rtmirror SET required_value=? "
                    "WHERE fk_module_id=? AND fk_io_definition_id=?",
                    (value, module_id, io_definition_id))
        self._cx.commit()
        cur.close()
        return cur.rowcount > 0


# ---------------------------------------------------------------------------
# Gateway
# ---------------------------------------------------------------------------
class OpcUaGateway:
    def __init__(self, source: HubSource):
        self.source = source
        self.server = Server()
        self.idx = None
        self._writables = {}     # asyncua Node -> (module_id, io_def_id)

    async def build(self):
        await self.server.init()
        self.server.set_endpoint(ENDPOINT)
        self.server.set_server_name("OSOlogic OPC-UA Gateway")
        self.idx = await self.server.register_namespace(NAMESPACE_URI)
        # NodeId strings assume OSOlogic is the first custom namespace (ns=2).
        if self.idx != 2:
            log.warning("OSOlogic namespace got index %s (expected 2); "
                        "NodeId strings in SQL views assume ns=2", self.idx)

        objects_node = self.server.nodes.objects
        # index device Objects by module_id
        obj_nodes = {}
        for o in self.source.list_objects():
            nid = ua.NodeId(str(o["module_id"]), self.idx, ua.NodeIdType.String)
            dev = await objects_node.add_object(nid, o["browse_name"])
            obj_nodes[o["module_id"]] = dev
            log.info("Object  %-24s  %s  (%s/%s)", o["browse_name"],
                     f"ns={self.idx};s={o['module_id']}", o["model_name"], o["protocol"])

        # folders per (module, folder-name)
        folders = {}
        for v in self.source.list_variables():
            mid = v["module_id"]
            fkey = (mid, v["folder"])
            if fkey not in folders:
                dev = obj_nodes.get(mid)
                if dev is None:
                    continue
                folders[fkey] = await dev.add_folder(
                    ua.NodeId(f"{mid}.{v['folder']}", self.idx, ua.NodeIdType.String),
                    v["folder"])
            folder = folders[fkey]

            vt = DATATYPE_MAP.get(v["data_type"], ua.VariantType.Variant)
            nid = ua.NodeId(f"{mid}.{v['io_definition_id']}", self.idx, ua.NodeIdType.String)
            var = await folder.add_variable(nid, v["browse_name"], v["value"], varianttype=vt)
            await self._write(var, v["value"], connected=True)

            if "CurrentWrite" in v["access_level"]:
                await var.set_writable()
                self._writables[var] = (mid, v["io_definition_id"])

            if v.get("engineering_units"):
                await var.add_property(
                    ua.NodeId(f"{mid}.{v['io_definition_id']}.EU", self.idx, ua.NodeIdType.String),
                    "EngineeringUnits", str(v["engineering_units"]))

    async def _write(self, var, value, connected):
        """Write a value as a full DataValue (StatusCode + SourceTimestamp)."""
        dv = ua.DataValue(ua.Variant(value, await var.read_data_type_as_variant_type()))
        dv.SourceTimestamp = datetime.now(timezone.utc)
        if not connected:
            dv.StatusCode = ua.StatusCode(ua.StatusCodes.BadNotConnected)
        await var.write_value(dv)

    async def _refresh_loop(self):
        """Poll: push hub values into the address space; resolve client writes
        back to the hub via the reversible NodeId."""
        last = {}
        while True:
            # 1) client -> hub (writes): detect changed writable nodes
            for var, (mid, iod) in self._writables.items():
                cur = await var.read_value()
                if last.get(var) is not None and cur != last[var]:
                    self.source.write_setpoint(mid, iod, cur)
                last[var] = cur
            # 2) hub -> client (reads): refresh from source (skip writables we own)
            for v in self.source.list_variables():
                node = self.server.get_node(
                    ua.NodeId(f"{v['module_id']}.{v['io_definition_id']}", self.idx,
                              ua.NodeIdType.String))
                if node in self._writables:
                    continue
                value, good, ts = self.source.read(v["module_id"], v["io_definition_id"])
                dv = ua.DataValue(ua.Variant(value, DATATYPE_MAP.get(v["data_type"],
                                                                     ua.VariantType.Variant)))
                dv.SourceTimestamp = ts or datetime.now(timezone.utc)
                if not good:
                    dv.StatusCode = ua.StatusCode(ua.StatusCodes.BadNotConnected)
                await node.write_value(dv)
            await asyncio.sleep(1.0)

    async def run(self):
        await self.build()
        async with self.server:
            log.info("OSOlogic OPC-UA gateway up at %s (ns=%s)", ENDPOINT, self.idx)
            await self._refresh_loop()


def seed_example() -> InMemorySource:
    """Representative configuration for the in-memory example
    (Aggregated Plant I/O: 16 outputs + 8 safe states + watchdog config,
    plus a read-only measured value with engineering units)."""
    s = InMemorySource()
    s.add_object(1, "Plant_IO_2", "Aggregated_Plant_IO_2", "aggregated", connected=True)
    for i in range(16):                                   # standard outputs (bit, rw)
        s.add_variable(1, i, f"Output_{i+1}", "IO", "Boolean", writable=True, value=False)
    for i in range(16, 24):                               # safe states (bit, rw)
        s.add_variable(1, i, f"SafeState_{i-15}", "SafeState", "Boolean", writable=True, value=False)
    s.add_variable(1, 32, "WDT_Timeout", "Config", "UInt16", writable=True, units="ms", value=1000)
    # a read-only measured value with engineering units
    s.add_object(2, "Press_Sensor_1", "Borrell_AI_1", "modbus-rtu", connected=True)
    s.add_variable(2, 0, "Pressure", "IO", "Float", writable=False, units="bar", value=3.14)
    return s


async def _amain(args):
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    if args.example:
        source = seed_example()
    else:
        source = MariaDBSource(host=args.db_host, port=args.db_port, user=args.db_user,
                               password=args.db_pass, database=args.db_name)
    await OpcUaGateway(source).run()


def main():
    p = argparse.ArgumentParser(description="OSOlogic OPC-UA gateway (reference)")
    p.add_argument("--example", action="store_true",
                   help="run the self-contained in-memory example")
    p.add_argument("--db-host", default="127.0.0.1")
    p.add_argument("--db-port", type=int, default=3306)
    p.add_argument("--db-user", default="plc")
    p.add_argument("--db-pass", default="")
    p.add_argument("--db-name", default="PLC")
    args = p.parse_args()
    try:
        asyncio.run(_amain(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
