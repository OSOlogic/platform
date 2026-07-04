#!/usr/bin/env python3
# ============================================================
# OSOLogic sandbox core (x86_64) — reference, not the C++ runtime.
#
# Fronts the MariaDB `tags` table as the osodb hub and exposes it three ways:
#   • REST   — GET /tags, GET /var/{key}, PUT /var/{key}   (what the web UIs use)
#   • OPC-UA — every tag as a node under ns "urn:osologic:sandbox"  (if asyncua present)
#   • static — serves the ui/ folder so the whole frontend runs from one port
# A scan loop applies set-points (required_value -> value) and simulates a few
# sensors so the interfaces show live movement. Control anything over the network:
# PUT a value, UPDATE a row, or write an OPC-UA node — it all lands in the same place.
#
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
import glob
import json
import math
import os
import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote

try:
    import pymysql
except Exception:
    pymysql = None

DB_HOST = os.environ.get("OSO_DB_HOST", "db")
DB_USER = os.environ.get("OSO_DB_USER", "osoapp")
DB_PASS = os.environ.get("OSO_DB_PASS", "osoapp")
DB_NAME = os.environ.get("OSO_DB_NAME", "osodb")
HTTP_PORT = int(os.environ.get("OSO_HTTP_PORT", "8080"))
OPC_PORT = int(os.environ.get("OSO_OPC_PORT", "4840"))
OPC_ENABLE = os.environ.get("OSO_OPC_ENABLE", "1") == "1"
UI_DIR = os.environ.get("OSO_UI_DIR", "/ui")
SCAN_MS = int(os.environ.get("OSO_SCAN_MS", "500"))

CACHE = {}                 # id -> tag dict  (the in-memory hub)
LOCK = threading.Lock()
_conn = None
_t0 = time.time()
RUNNING = True             # scan-cycle state (osoadmin Runtime module)
CYCLES = 0                 # scan cycle counter


def tag_pub(r):
    """Public tag shape for REST (works for /tags and /api/v1/tags — includes `type` alias)."""
    return {"id": r["id"], "name": r.get("name"), "data_type": r.get("data_type"),
            "type": r.get("data_type"), "value": tag_value(r), "units": r.get("units"),
            "access": r.get("access")}


# ---- DB -----------------------------------------------------
def db_connect():
    global _conn
    if not pymysql:
        return None
    for attempt in range(60):
        try:
            _conn = pymysql.connect(host=DB_HOST, user=DB_USER, password=DB_PASS,
                                    database=DB_NAME, autocommit=True, connect_timeout=4,
                                    cursorclass=pymysql.cursors.DictCursor)
            print(f"[core] connected to MariaDB {DB_HOST}/{DB_NAME}", flush=True)
            return _conn
        except Exception as e:
            print(f"[core] DB not ready ({e}); retry {attempt+1}/60…", flush=True)
            time.sleep(2)
    print("[core] WARNING: no DB — running with an in-memory seed only", flush=True)
    return None


def db_query(sql, args=None):
    with LOCK:
        if not _conn:
            return []
        with _conn.cursor() as cur:
            cur.execute(sql, args or ())
            return cur.fetchall()


def db_exec(sql, args=None):
    with LOCK:
        if not _conn:
            return
        with _conn.cursor() as cur:
            cur.execute(sql, args or ())


def load_cache():
    rows = db_query("SELECT * FROM tags")
    if not rows:   # no DB (e.g. quick test without Docker): seed the demo plant so the UIs still work
        def T(i, n, dt, v, u, ac, sim):
            return {"id": i, "name": n, "data_type": dt, "value": v, "value_s": None,
                    "required_value": None, "units": u, "access": ac, "sim": sim}
        rows = [
            T("2.1", "Machine run", "Boolean", 0, None, "ReadWrite", "follow"),
            T("2.5", "Motor speed", "Float", 0, "rpm", "ReadWrite", "follow"),
            T("hass.switch.pump", "Water Pump", "Boolean", 0, None, "ReadWrite", "follow"),
            T("hass.light.hall", "Hall Light", "Boolean", 0, None, "ReadWrite", "follow"),
            T("hass.cover.gate", "Loading Gate", "Boolean", 0, None, "ReadWrite", "follow"),
            T("hass.lock.door", "Access Door", "Boolean", 0, None, "ReadWrite", "follow"),
            T("hass.sensor.tank_level", "Tank Level", "Float", 42, "%", "ReadOnly", "sine"),
            T("hass.sensor.temperature", "Ambient Temp", "Float", 21, "°C", "ReadOnly", "sine"),
            T("hass.climate.hvac", "HVAC", "Float", 22, "°C", "ReadOnly", "ramp"),
            T("hass.binary_sensor.jam", "Jam Detector", "Boolean", 0, None, "ReadOnly", None),
        ]
    for r in rows:
        CACHE[r["id"]] = r


def tag_value(r):
    return r["value_s"] if r.get("data_type") == "String" else r["value"]


# ---- scan loop ---------------------------------------------
def scan_loop():
    global CYCLES
    while True:
        CYCLES += 1
        t = time.time() - _t0
        for tid, r in list(CACHE.items()):
            sim = r.get("sim")
            if sim == "follow" and r.get("required_value") is not None:
                r["value"] = r["required_value"]
            elif sim == "sine":
                base = 42 if r.get("units") == "%" else 21
                r["value"] = round(base + 8 * math.sin(t / 6 + hash(tid) % 7), 2)
            elif sim == "ramp":
                r["value"] = round(20 + 4 * ((t / 20) % 1), 2)
            # persist current value back to the DB (source of truth)
            if _conn:
                try:
                    db_exec("UPDATE tags SET value=%s WHERE id=%s", (float(r["value"] or 0), tid))
                except Exception:
                    pass
        time.sleep(SCAN_MS / 1000.0)


# ---- network / serial discovery ----------------------------
PORT_NAMES = {502: "Modbus TCP", 102: "S7comm", 44818: "EtherNet/IP", 4840: "OPC-UA",
              1883: "MQTT", 8883: "MQTT/TLS", 20000: "DNP3", 47808: "BACnet",
              80: "HTTP", 443: "HTTPS", 22: "SSH", 23: "Telnet", 161: "SNMP",
              8080: "HTTP-alt", 1880: "Node-RED", 8123: "Home Assistant",
              3306: "MariaDB", 5432: "PostgreSQL", 6379: "Redis"}


def scan_serial():
    ports = []
    for pat in ("/dev/ttyUSB*", "/dev/ttyACM*", "/dev/ttyS[0-9]*", "/dev/ttyAMA*", "/dev/serial/by-id/*"):
        ports += glob.glob(pat)
    return {"ports": sorted(set(ports))}


def _probe(ip, port, timeout=0.35):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        return s.connect_ex((ip, port)) == 0
    except Exception:
        return False
    finally:
        s.close()


def scan_tcp(subnet, ports_str):
    try:
        plist = [int(p) for p in ports_str.split(",") if p.strip()][:16]
    except Exception:
        plist = [502, 4840, 1883, 80]
    base = subnet.strip().rstrip(".")
    if base.count(".") != 2:
        return {"error": "subnet must be a /24 prefix like 192.168.1"}
    targets = [(f"{base}.{h}", p) for h in range(1, 255) for p in plist]
    found = {}
    with ThreadPoolExecutor(max_workers=128) as ex:
        for (ip, port), ok in zip(targets, ex.map(lambda t: _probe(*t), targets)):
            if ok:
                found.setdefault(ip, []).append({"port": port, "proto": PORT_NAMES.get(port, "?")})
    hosts = [{"ip": ip, "ports": found[ip]} for ip in
             sorted(found, key=lambda x: tuple(int(o) for o in x.split(".")))]
    return {"subnet": base + ".0/24", "scanned_ports": plist, "hosts": hosts}


# ---- REST + static -----------------------------------------
class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,PUT,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type,Authorization")

    def _json(self, obj, code=200):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self._cors()
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204); self._cors(); self.end_headers()

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path in ("/tags", "/api/tags", "/api/v1/tags"):
            return self._json([tag_pub(r) for r in CACHE.values()])
        if path == "/api/v1/runtime/status":
            return self._json({"state": "running" if RUNNING else "stopped",
                               "cycle_count": CYCLES, "scan_ms": SCAN_MS,
                               "tasks": [{"name": "main scan", "state": "running" if RUNNING else "stopped"}]})
        if path == "/api/v1/system/info":
            return self._json({"hostname": "osologic-sandbox", "version": "1.0 (sandbox)",
                               "arch": "x86_64", "cores": os.cpu_count(), "tags": len(CACHE), "db": bool(_conn)})
        if path == "/api/v1/gateways":
            return self._json([])
        if path == "/api/v1/projects":
            return self._json([])
        if path == "/api/v1/scan/serial":
            return self._json(scan_serial())
        if path == "/api/v1/scan/tcp":
            q = parse_qs(self.path.split("?", 1)[1] if "?" in self.path else "")
            return self._json(scan_tcp(q.get("subnet", ["192.168.1"])[0],
                                       q.get("ports", ["502,102,44818,4840,1883,80"])[0]))
        if path.startswith("/var/") or path.startswith("/api/tags/") or path.startswith("/api/v1/tags/"):
            key = unquote(path.split("/var/", 1)[-1] if "/var/" in path else path.rsplit("/", 1)[-1])
            r = CACHE.get(key)
            if not r:
                return self._json({"error": "unknown tag", "key": key}, 404)
            return self._json(tag_pub(r))
        if path in ("/healthz", "/api/health"):
            return self._json({"ok": True, "tags": len(CACHE), "db": bool(_conn), "running": RUNNING})
        return self._static(path)

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        global RUNNING
        if path.startswith("/api/v1/runtime/"):
            action = path.rsplit("/", 1)[-1]
            if action in ("start", "restart", "resume"):
                RUNNING = True
            elif action in ("stop", "pause"):
                RUNNING = False
            return self._json({"state": "running" if RUNNING else "stopped", "ok": True})
        if path.startswith("/api/v1/projects/"):   # activate / etc — accepted stub
            return self._json({"ok": True})
        return self._json({"error": "not found", "path": path}, 404)

    def do_PUT(self):
        path = self.path.split("?", 1)[0]
        if not (path.startswith("/var/") or path.startswith("/api/tags/") or path.startswith("/api/v1/tags/")):
            return self._json({"error": "not found"}, 404)
        key = unquote(path.split("/var/", 1)[-1] if "/var/" in path else path.rsplit("/", 1)[-1])
        r = CACHE.get(key)
        if not r:
            return self._json({"error": "unknown tag", "key": key}, 404)
        if r.get("access") == "ReadOnly":
            return self._json({"error": "read-only tag"}, 403)
        try:
            n = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(n) or b"{}")
        except Exception:
            return self._json({"error": "bad body"}, 400)
        val = body.get("value", body.get("required_value"))
        try:
            num = float(val if not isinstance(val, bool) else (1 if val else 0))
        except Exception:
            num = 1.0 if str(val).lower() in ("1", "true", "on") else 0.0
        r["required_value"] = num
        db_exec("UPDATE tags SET required_value=%s WHERE id=%s", (num, key))
        return self._json({"id": key, "required_value": num, "ok": True})

    def _static(self, path):
        if path in ("/", ""):
            path = "/index.html"
        # serve the mounted ui/ tree; default landing is a small index
        full = os.path.realpath(os.path.join(UI_DIR, path.lstrip("/")))
        if not full.startswith(os.path.realpath(UI_DIR)) or not os.path.isfile(full):
            # serve the sandbox landing at "/" — works whether UI_DIR is the repo root
            # (dev / bare-metal) or has the landing bind-mounted at /index.html (docker)
            if path == "/index.html":
                for cand in (os.path.join(UI_DIR, "sandbox", "web", "index.html"),
                             os.path.join(UI_DIR, "_sandbox.html")):
                    if os.path.isfile(cand):
                        full = cand
                        break
                else:
                    return self._json({"error": "not found", "path": path}, 404)
            else:
                return self._json({"error": "not found", "path": path}, 404)
        ctype = {
            ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
            ".json": "application/json", ".svg": "image/svg+xml", ".png": "image/png",
        }.get(os.path.splitext(full)[1], "application/octet-stream")
        with open(full, "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self._cors()
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def run_http():
    srv = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    print(f"[core] REST + UI on http://0.0.0.0:{HTTP_PORT}", flush=True)
    srv.serve_forever()


# ---- OPC-UA -------------------------------------------------
def run_opc():
    try:
        import asyncio
        from asyncua import Server, ua
    except Exception as e:
        print(f"[core] OPC-UA disabled (asyncua missing: {e})", flush=True)
        return

    async def main():
        server = Server()
        await server.init()
        server.set_endpoint(f"opc.tcp://0.0.0.0:{OPC_PORT}/osologic/")
        server.set_server_name("OSOLogic Sandbox")
        idx = await server.register_namespace("urn:osologic:sandbox")
        objs = server.nodes.objects
        folder = await objs.add_folder(idx, "Tags")
        nodes = {}
        for tid, r in CACHE.items():
            var = await folder.add_variable(idx, tid, float(r.get("value") or 0))
            if r.get("access") == "ReadWrite":
                await var.set_writable()
            nodes[tid] = var
        print(f"[core] OPC-UA on opc.tcp://0.0.0.0:{OPC_PORT}/osologic/ ({len(nodes)} nodes)", flush=True)
        async with server:
            while True:
                for tid, var in nodes.items():
                    r = CACHE.get(tid)
                    if r is not None:
                        try:
                            await var.write_value(float(r.get("value") or 0))
                        except Exception:
                            pass
                await asyncio.sleep(SCAN_MS / 1000.0)

    import asyncio
    asyncio.run(main())


def main():
    db_connect()
    load_cache()
    threading.Thread(target=scan_loop, daemon=True).start()
    threading.Thread(target=run_http, daemon=True).start()
    print(f"[core] osologic sandbox up — {len(CACHE)} tags", flush=True)
    if OPC_ENABLE:
        run_opc()
    else:
        while True:
            time.sleep(3600)


if __name__ == "__main__":
    main()
