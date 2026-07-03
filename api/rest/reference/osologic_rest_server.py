#!/usr/bin/env python3
"""
OSOlogic — REST + WebSocket API (reference implementation)
=========================================================

The `/api/v1` HTTP + WebSocket contract that the platform's web front-ends use:
the webmin-oso Cockpit modules (I/O tags, gateways, runtime, projects) and the
osoLadder editor. It is backed by the shared in-memory hub (osodb) and the same
reversible NodeId scheme used across the platform:

    ns=2;s=<module_id>.<io_definition_id>

Endpoints
    GET    /api/v1/tags                 list tags
    GET    /api/v1/tags/{id}            read one tag
    PUT    /api/v1/tags/{id}            write a tag value
    GET    /api/v1/devices              list devices
    GET    /api/v1/gateways             list gateways
    POST   /api/v1/gateways             add a gateway
    PUT    /api/v1/gateways/{id}        update a gateway
    DELETE /api/v1/gateways/{id}        remove a gateway
    POST   /api/v1/gateways/{id}/restart
    GET    /api/v1/runtime/status       scan-cycle and health
    POST   /api/v1/runtime/{action}     start | stop | reset
    GET    /api/v1/projects             list IEC 61131-3 projects
    POST   /api/v1/projects             upload a project
    POST   /api/v1/projects/{id}/activate
    DELETE /api/v1/projects/{id}
    GET    /api/v1/status               runtime status (osoLadder compat)
    GET/PUT/api/v1/var/{name}           read/write a tag by name (osoLadder compat)
    WS     /ws                          live tag updates (tag_update / tag_batch)

Interactive documentation is served at /docs (OpenAPI at /openapi.json).

Run:  pip install "fastapi" "uvicorn[standard]"
      python osologic_rest_server.py            # http://0.0.0.0:8080
"""
import asyncio
import time

from fastapi import Body, FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

# Hub data type (OPC-UA built-in) -> IEC 61131-3 type used by the tag browser
IEC_TYPE = {
    "Boolean": "BOOL", "UInt16": "WORD", "UInt32": "DWORD", "UInt64": "LWORD",
    "Int16": "INT", "Int32": "DINT", "Int64": "LINT", "Float": "REAL", "Double": "LREAL",
}


def now_us():
    return int(time.time() * 1_000_000)


# ---------------------------------------------------------------------------
# In-memory hub (osodb stand-in; replace with the osodb adapter in production)
# ---------------------------------------------------------------------------
class Hub:
    def __init__(self):
        self.devices = [
            {"module_id": 1, "name": "Plant_IO_2", "model": "Aggregated_Plant_IO_2",
             "protocol": "aggregated", "connected": True},
            {"module_id": 2, "name": "Press_Sensor_1", "model": "Borrell_AI_1",
             "protocol": "modbus-rtu", "connected": True},
        ]
        self.tags = {}          # id -> tag dict
        self._add(1, "Plant_IO_2", "Config", 32, "WDT_Timeout", "UInt16", True, 1000, "ms")
        for i in range(16):
            self._add(1, "Plant_IO_2", "Plant_IO_2", i, f"Output_{i + 1}", "Boolean", True, False)
        self._add(2, "Press_Sensor_1", "Press_Sensor_1", 0, "Pressure", "Float", False, 3.14, "bar")

    def _add(self, mid, dev, group, io, name, dtype, writable, value, units=""):
        tid = f"ns=2;s={mid}.{io}"
        self.tags[tid] = {"id": tid, "module_id": mid, "io": io, "name": name,
                          "group": group, "device": dev, "type": IEC_TYPE.get(dtype, "STRING"),
                          "access": "rw" if writable else "ro", "value": value,
                          "units": units, "quality": "good", "ts_us": now_us()}

    def connected(self, mid):
        return next((d["connected"] for d in self.devices if d["module_id"] == mid), False)

    def public(self, t):
        return {"id": t["id"], "name": t["name"], "group": t["group"], "device": t["device"],
                "type": t["type"], "access": t["access"], "value": t["value"],
                "units": t["units"], "quality": "good" if self.connected(t["module_id"]) else "bad",
                "ts_us": t["ts_us"]}

    def list(self):
        return [self.public(t) for t in self.tags.values()]

    def get(self, tid):
        t = self.tags.get(tid)
        return self.public(t) if t else None

    def by_name(self, name):
        return next((t for t in self.tags.values() if t["name"] == name), None)

    def write(self, tid, value):
        t = self.tags.get(tid)
        if not t:
            raise KeyError(tid)
        if t["access"] != "rw":
            raise PermissionError(tid)
        t["value"] = value
        t["ts_us"] = now_us()
        return self.public(t)


hub = Hub()

gateways = [
    {"id": "opcua-1", "name": "OPC-UA Server", "protocol": "opc-ua", "status": "running",
     "endpoint_url": "opc.tcp://0.0.0.0:4840/osologic/server/", "points": len(hub.tags)},
    {"id": "modbus-1", "name": "Line A RTU", "protocol": "modbus-rtu", "status": "running",
     "interface": "/dev/ttyUSB0", "bitrate": 19200, "slot": 5, "points": 33},
    {"id": "mqtt-1", "name": "Telemetry", "protocol": "mqtt", "status": "stopped",
     "host": "localhost", "port": 1883, "topic_prefix": "/plc/data", "points": 0},
]

runtime = {"device": "BorrellPLC", "firmware": "a-1.0.0", "platform": "OSOlogic Linux (PREEMPT_RT)",
           "state": "running", "cycle_ms": 10, "last_cycle_us": 7100, "cycle_count": 0,
           "error_msg": "", "tasks": [{"name": "scan", "period_ms": 10, "state": "running"},
                                      {"name": "databaseSync", "period_ms": 100, "state": "running"}]}

projects = [
    {"id": "proj-1", "name": "line_a.osol", "active": True, "version": "3",
     "uploaded_at": "2026-07-01T09:00:00Z", "checksum": "a1b2c3d4", "size_bytes": 20480},
]

app = FastAPI(title="OSOlogic REST API", version="1.0.0",
              description="Reference REST + WebSocket API for the OSOlogic platform.")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])
V = "/api/v1"


# ── Tags ───────────────────────────────────────────────────────────────────
@app.get(V + "/tags")
def list_tags():
    return hub.list()


@app.get(V + "/tags/{tag_id:path}")
def read_tag(tag_id: str):
    t = hub.get(tag_id)
    if not t:
        raise HTTPException(404, "unknown tag")
    return t


@app.put(V + "/tags/{tag_id:path}")
async def write_tag(tag_id: str, body: dict = Body(...)):
    try:
        updated = hub.write(tag_id, body.get("value"))
    except KeyError:
        raise HTTPException(404, "unknown tag")
    except PermissionError:
        raise HTTPException(403, "read-only tag")
    await bus.publish(updated["name"], updated["value"], updated["ts_us"])
    return updated


# ── Devices ────────────────────────────────────────────────────────────────
@app.get(V + "/devices")
def list_devices():
    return hub.devices


# ── Gateways ───────────────────────────────────────────────────────────────
@app.get(V + "/gateways")
def list_gateways():
    return gateways


@app.post(V + "/gateways")
def add_gateway(gw: dict = Body(...)):
    gw.setdefault("status", "stopped")
    gw.setdefault("points", 0)
    gateways.append(gw)
    return gw


@app.put(V + "/gateways/{gid}")
def update_gateway(gid: str, patch: dict = Body(...)):
    for gw in gateways:
        if gw["id"] == gid:
            gw.update(patch)
            return gw
    raise HTTPException(404, "unknown gateway")


@app.delete(V + "/gateways/{gid}")
def delete_gateway(gid: str):
    before = len(gateways)
    gateways[:] = [g for g in gateways if g["id"] != gid]
    if len(gateways) == before:
        raise HTTPException(404, "unknown gateway")
    return {"deleted": gid}


@app.post(V + "/gateways/{gid}/restart")
def restart_gateway(gid: str):
    for gw in gateways:
        if gw["id"] == gid:
            gw["status"] = "running"
            return gw
    raise HTTPException(404, "unknown gateway")


# ── Runtime ────────────────────────────────────────────────────────────────
@app.get(V + "/runtime/status")
@app.get(V + "/status")           # osoLadder compatibility
def runtime_status():
    return runtime


@app.post(V + "/runtime/{action}")
def runtime_action(action: str):
    if action not in ("start", "stop", "reset"):
        raise HTTPException(400, "unknown action")
    runtime["state"] = {"start": "running", "stop": "stopped", "reset": "running"}[action]
    return runtime


# ── Projects ───────────────────────────────────────────────────────────────
@app.get(V + "/projects")
def list_projects():
    return projects


@app.post(V + "/projects")
def add_project(p: dict = Body(...)):
    p.setdefault("active", False)
    projects.append(p)
    return p


@app.post(V + "/projects/{pid}/activate")
def activate_project(pid: str):
    found = False
    for p in projects:
        p["active"] = (p["id"] == pid)
        found = found or p["active"]
    if not found:
        raise HTTPException(404, "unknown project")
    return {"active": pid}


@app.delete(V + "/projects/{pid}")
def delete_project(pid: str):
    projects[:] = [p for p in projects if p["id"] != pid]
    return {"deleted": pid}


# ── osoLadder compatibility (read/write by name) ────────────────────────────
@app.get(V + "/var/{name}")
def read_var(name: str):
    t = hub.by_name(name)
    if not t:
        raise HTTPException(404, "unknown variable")
    return hub.public(t)


@app.put(V + "/var/{name}")
async def write_var(name: str, body: dict = Body(...)):
    t = hub.by_name(name)
    if not t:
        raise HTTPException(404, "unknown variable")
    updated = hub.write(t["id"], body.get("value"))
    await bus.publish(updated["name"], updated["value"], updated["ts_us"])
    return updated


# ── WebSocket live updates ──────────────────────────────────────────────────
class Bus:
    def __init__(self):
        self.clients = set()

    async def publish(self, name, value, ts_us):
        dead = []
        for ws in self.clients:
            try:
                await ws.send_json({"type": "tag_update", "tag": name, "value": value, "ts_us": ts_us})
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.clients.discard(ws)


bus = Bus()


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    bus.clients.add(ws)
    try:
        while True:
            await ws.receive_text()          # subscribe / keepalive messages
    except WebSocketDisconnect:
        bus.clients.discard(ws)


async def _simulate():
    """Drive a couple of tags so the live view shows activity (example only)."""
    n = 0
    while True:
        await asyncio.sleep(2.0)
        n += 1
        updates = []
        t = hub.tags["ns=2;s=1.0"]                       # toggle Output_1
        t["value"] = not t["value"]; t["ts_us"] = now_us()
        updates.append({"tag": t["name"], "value": t["value"], "ts_us": t["ts_us"]})
        p = hub.tags["ns=2;s=2.0"]                        # drift Pressure
        p["value"] = round(3.0 + (n % 20) * 0.05, 3); p["ts_us"] = now_us()
        updates.append({"tag": p["name"], "value": p["value"], "ts_us": p["ts_us"]})
        runtime["cycle_count"] += 200
        for ws in list(bus.clients):
            try:
                await ws.send_json({"type": "tag_batch", "updates": updates})
            except Exception:
                bus.clients.discard(ws)


@app.on_event("startup")
async def _startup():
    asyncio.create_task(_simulate())


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
