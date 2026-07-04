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
import shutil
import socket
import subprocess
import tempfile
import threading
import time
from collections import deque
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

LOGS = deque(maxlen=5000)          # in-memory log ring (osoadmin Logs module)
LOG_CFG = {"level": "info", "max_lines": 2000, "persistent": False}
_LEVELS = {"debug": 0, "info": 1, "warn": 2, "error": 3}


def log(level, msg):
    LOGS.append({"time": time.strftime("%H:%M:%S"), "level": level, "msg": msg})
    print(f"[{level}] {msg}", flush=True)


# ---- alarms (osoadmin Alarms & Events module) --------------
ALARM_RULES = [
    {"id": "a1", "tag": "hass.sensor.tank_level",  "op": ">=", "value": 48, "severity": "warn",     "label": "Tank level HIGH"},
    {"id": "a2", "tag": "hass.sensor.tank_level",  "op": "<=", "value": 36, "severity": "warn",     "label": "Tank level LOW"},
    {"id": "a3", "tag": "hass.sensor.temperature", "op": ">=", "value": 27, "severity": "critical", "label": "Ambient temp HIGH"},
    {"id": "a4", "tag": "hass.binary_sensor.jam",  "op": "==", "value": 1,  "severity": "critical", "label": "Jam detected"},
]
ALARM_ACTIVE = {}
ALARM_EVENTS = deque(maxlen=500)
_arc = 4

# ---- historian (osoadmin Historian / time-series module) ----
HISTORY = {}   # tag -> deque[[t, value]]  (in-memory trend buffer)
HIST_CFG = {"backend": "in-memory", "host": "localhost", "port": 8086,
            "database": "osodb_hist", "token": "", "sample_ms": 1000, "retention_s": 600,
            "tags": ["hass.sensor.tank_level", "hass.sensor.temperature", "hass.climate.hvac", "2.5"]}
_hist_last = 0.0


# ---- users & roles (osoadmin — maps to MariaDB users + GRANTs) ----
ROLES = {
    "viewer":   {"label": "Viewer",   "privileges": ["SELECT"],                               "scope": "osodb.*",
                 "about": "read tags, dashboards, trends"},
    "operator": {"label": "Operator", "privileges": ["SELECT", "UPDATE"],                      "scope": "osodb.tags",
                 "about": "read + write set-points"},
    "engineer": {"label": "Engineer", "privileges": ["SELECT", "INSERT", "UPDATE", "DELETE"],  "scope": "osodb.*",
                 "about": "edit tags, config, programs"},
    "admin":    {"label": "Admin",    "privileges": ["ALL PRIVILEGES"],                        "scope": "*.*",
                 "about": "full control"},
}
USERS = [
    {"user": "root",     "host": "localhost", "role": "admin",    "privileges": ["ALL PRIVILEGES"],           "scope": "*.*"},
    {"user": "osoapp",   "host": "%",         "role": "operator", "privileges": ["SELECT", "INSERT", "UPDATE"], "scope": "osodb.*"},
    {"user": "plc_view", "host": "%",         "role": "viewer",   "privileges": ["SELECT"],                    "scope": "osodb.*"},
]


def grant_sql(u):
    return f"GRANT {', '.join(u['privileges'])} ON {u['scope']} TO '{u['user']}'@'{u['host']}';"


# ---- Home Assistant compatibility-gateway config (osoadmin) ----
HASS_CFG = {"enabled": False, "url": "http://homeassistant.local:8123", "token": "",
            "poll_ms": 1000, "domains": ["light", "switch", "sensor", "binary_sensor", "climate", "cover", "lock"]}


def hass_drivers():
    out = []
    for path in sorted(glob.glob(os.path.join(UI_DIR, "gateways", "home-assistant", "drivers", "*.json"))):
        try:
            d = json.load(open(path))
            out.append({"domain": d.get("domain"), "label": d.get("label", d.get("domain")),
                        "icon": d.get("icon", "🔧"), "access": d.get("osodb", {}).get("access", "")})
        except Exception:
            pass
    return out


def _cmp(v, op, val):
    try:
        v = float(v)
    except Exception:
        return False
    return {">": v > val, "<": v < val, ">=": v >= val, "<=": v <= val,
            "==": v == val, "!=": v != val}.get(op, False)


def eval_alarms():
    now = time.strftime("%H:%M:%S")
    for rule in ALARM_RULES:
        r = CACHE.get(rule["tag"])
        if not r:
            continue
        cond = _cmp(r.get("value"), rule["op"], float(rule["value"]))
        rid = rule["id"]
        if cond and rid not in ALARM_ACTIVE:
            ALARM_ACTIVE[rid] = {"rule": rule, "since": now, "value": r.get("value"), "acked": False}
            ALARM_EVENTS.append({"time": now, "type": "raised", "label": rule["label"], "severity": rule["severity"], "tag": rule["tag"]})
            log("warn", f"ALARM raised — {rule['label']} ({rule['tag']}={r.get('value')})")
        elif not cond and rid in ALARM_ACTIVE:
            del ALARM_ACTIVE[rid]
            ALARM_EVENTS.append({"time": now, "type": "cleared", "label": rule["label"], "severity": rule["severity"], "tag": rule["tag"]})
            log("info", f"ALARM cleared — {rule['label']}")
        elif cond and rid in ALARM_ACTIVE:
            ALARM_ACTIVE[rid]["value"] = r.get("value")


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
    global CYCLES, _hist_last
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
        try:
            eval_alarms()
        except Exception:
            pass
        # historian sampling
        if (time.time() - _hist_last) * 1000 >= HIST_CFG["sample_ms"]:
            _hist_last = time.time()
            for tid in HIST_CFG["tags"]:
                r = CACHE.get(tid)
                if r is not None:
                    HISTORY.setdefault(tid, deque(maxlen=1000)).append([round(t, 1), r.get("value")])
        time.sleep(SCAN_MS / 1000.0)


# ---- script exec (osoadmin Script editor) ------------------
# SANDBOX/DEV ONLY: runs code on the device. In production this endpoint MUST be
# authenticated, authorised per user/role, sandboxed (restricted user, cgroups,
# no host escalation) and audited. Toggle with OSO_EXEC_ENABLE=0.
EXEC_ENABLE = os.environ.get("OSO_EXEC_ENABLE", "1") == "1"
INTERP = {"bash": ["bash", "-c"], "sh": ["sh", "-c"], "python": ["python3", "-c"], "python3": ["python3", "-c"],
          "node": ["node", "-e"], "javascript": ["node", "-e"], "php": ["php", "-r"], "ruby": ["ruby", "-e"],
          "perl": ["perl", "-e"], "lua": ["lua", "-e"], "r": ["Rscript", "-e"]}
COMPILE = {
    "cpp":    {"file": "main.cpp",  "build": lambda d, s: ["g++", s, "-O0", "-o", d + "/a.out"], "run": lambda d, s: [d + "/a.out"]},
    "rust":   {"file": "main.rs",   "build": lambda d, s: ["rustc", s, "-o", d + "/a.out"],      "run": lambda d, s: [d + "/a.out"]},
    "go":     {"file": "main.go",   "build": None,                                               "run": lambda d, s: ["go", "run", s]},
    "java":   {"file": "Main.java", "build": lambda d, s: ["javac", s],                          "run": lambda d, s: ["java", "-cp", d, "Main"]},
    "csharp": {"file": "main.cs",   "build": lambda d, s: ["mcs", s, "-out:" + d + "/a.exe"],    "run": lambda d, s: ["mono", d + "/a.exe"]},
}
TOOLCHAINS = {
    "bash":   {"check": "bash",    "apt": "bash",      "docker": "bash",                 "kind": "interpreted"},
    "python": {"check": "python3", "apt": "python3",   "docker": "python:3",             "kind": "interpreted"},
    "node":   {"check": "node",    "apt": "nodejs",    "docker": "node:20",              "kind": "interpreted"},
    "php":    {"check": "php",     "apt": "php-cli",   "docker": "php:8",                "kind": "interpreted"},
    "ruby":   {"check": "ruby",    "apt": "ruby",      "docker": "ruby:3",               "kind": "interpreted"},
    "perl":   {"check": "perl",    "apt": "perl",      "docker": "perl",                 "kind": "interpreted"},
    "lua":    {"check": "lua",     "apt": "lua5.4",    "docker": "nickblah/lua",         "kind": "interpreted"},
    "r":      {"check": "Rscript", "apt": "r-base",    "docker": "r-base",               "kind": "interpreted"},
    "cpp":    {"check": "g++",     "apt": "g++",       "docker": "gcc",                  "kind": "compiled"},
    "rust":   {"check": "rustc",   "apt": "rustc",     "docker": "rust",                 "kind": "compiled"},
    "go":     {"check": "go",      "apt": "golang-go", "docker": "golang",               "kind": "compiled"},
    "java":   {"check": "javac",   "apt": "default-jdk", "docker": "eclipse-temurin:21", "kind": "compiled"},
    "csharp": {"check": "mcs",     "apt": "mono-mcs",  "docker": "mono",                 "kind": "compiled"},
}


def run_script(lang, code, debug=False):
    # Pseudo-debug: with debug=True we export OSO_DEBUG=1 so a script can gate its own
    # trace output ("hidden flags"), and we report elapsed time + the exact command run.
    if not EXEC_ENABLE:
        return {"error": "script execution is disabled on this instance"}
    env = dict(os.environ)
    if debug:
        env["OSO_DEBUG"] = "1"
    t0 = time.monotonic()

    def done(res):
        res["elapsed_ms"] = int((time.monotonic() - t0) * 1000)
        res["debug"] = debug
        return res

    if lang in INTERP:
        cmd = INTERP[lang]
        try:
            p = subprocess.run(cmd + [code], capture_output=True, text=True, timeout=20, env=env)
            return done({"lang": lang, "cmd": " ".join(cmd), "stdout": p.stdout, "stderr": p.stderr, "code": p.returncode})
        except FileNotFoundError:
            return {"error": f"{cmd[0]} not installed — install its toolchain"}
        except subprocess.TimeoutExpired:
            return {"error": "script timed out (20s limit)"}
        except Exception as e:
            return {"error": str(e)}
    if lang in COMPILE:
        spec = COMPILE[lang]
        try:
            with tempfile.TemporaryDirectory() as d:
                src = os.path.join(d, spec["file"])
                open(src, "w").write(code)
                if spec["build"]:
                    b = subprocess.run(spec["build"](d, src), capture_output=True, text=True, timeout=45, cwd=d, env=env)
                    if b.returncode != 0:
                        return done({"lang": lang, "stdout": b.stdout, "stderr": "[compile]\n" + b.stderr, "code": b.returncode})
                run_cmd = spec["run"](d, src)
                r = subprocess.run(run_cmd, capture_output=True, text=True, timeout=20, cwd=d, env=env)
                return done({"lang": lang, "cmd": " ".join(run_cmd), "stdout": r.stdout, "stderr": r.stderr, "code": r.returncode})
        except FileNotFoundError as e:
            return {"error": f"toolchain missing ({e}) — install it in Toolchains"}
        except subprocess.TimeoutExpired:
            return {"error": "compile/run timed out"}
        except Exception as e:
            return {"error": str(e)}
    return {"error": f"'{lang}' has no runner configured"}


def toolchain_status():
    out = []
    for lang, tc in TOOLCHAINS.items():
        path = shutil.which(tc["check"])
        ver = ""
        if path:
            try:
                lines = (subprocess.run([tc["check"], "--version"], capture_output=True, text=True, timeout=4).stdout or "").splitlines()
                ver = lines[0][:70] if lines else ""
            except Exception:
                ver = ""
        out.append({"lang": lang, "check": tc["check"], "present": bool(path), "version": ver,
                    "kind": tc["kind"], "apt": tc["apt"], "docker": tc["docker"]})
    return out


def toolchain_install(lang, method):
    tc = TOOLCHAINS.get(lang)
    if not tc:
        return {"error": "unknown toolchain"}
    cmd = ["docker", "pull", tc["docker"]] if method == "docker" else ["sudo", "-n", "apt-get", "install", "-y", tc["apt"]]
    log("info", f"toolchain install {lang} via {method}: {' '.join(cmd)}")
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
        return {"cmd": " ".join(cmd), "stdout": p.stdout[-4000:], "stderr": p.stderr[-4000:], "code": p.returncode}
    except FileNotFoundError:
        return {"cmd": " ".join(cmd), "error": f"{cmd[0]} not available on this device"}
    except subprocess.TimeoutExpired:
        return {"cmd": " ".join(cmd), "error": "install timed out"}
    except Exception as e:
        return {"cmd": " ".join(cmd), "error": str(e)}


# ---- fail2ban (osoadmin) -----------------------------------
def fail2ban_status():
    if not shutil.which("fail2ban-client"):
        return {"installed": False, "demo": True, "jails": [
            {"name": "sshd", "banned": 3, "total": 47, "ips": ["203.0.113.5", "198.51.100.9", "192.0.2.7"]},
            {"name": "nginx-http-auth", "banned": 1, "total": 12, "ips": ["203.0.113.5"]},
            {"name": "recidive", "banned": 0, "total": 5, "ips": []}]}
    try:
        st = subprocess.run(["fail2ban-client", "status"], capture_output=True, text=True, timeout=6).stdout
        names = []
        for line in st.splitlines():
            if "Jail list" in line:
                names = [x.strip() for x in line.split(":", 1)[1].split(",") if x.strip()]
        jails = []
        for n in names:
            s = subprocess.run(["fail2ban-client", "status", n], capture_output=True, text=True, timeout=6).stdout
            j = {"name": n, "banned": 0, "total": 0, "ips": []}
            for ln in s.splitlines():
                if "Currently banned" in ln:
                    j["banned"] = int((ln.split(":")[-1].strip() or "0"))
                elif "Total banned" in ln:
                    j["total"] = int((ln.split(":")[-1].strip() or "0"))
                elif "Banned IP list" in ln:
                    j["ips"] = ln.split(":", 1)[1].split()
            jails.append(j)
        return {"installed": True, "jails": jails}
    except Exception as e:
        return {"installed": True, "error": str(e), "jails": []}


# ---- firewall / iptables (osoadmin) ------------------------
def firewall_rules():
    demo = {"available": False, "demo": True, "rules": [
        "-P INPUT DROP", "-P FORWARD DROP", "-P OUTPUT ACCEPT",
        "-A INPUT -i lo -j ACCEPT",
        "-A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT",
        "-A INPUT -p tcp --dport 22 -j ACCEPT",
        "-A INPUT -p tcp --dport 8080 -j ACCEPT",
        "-A INPUT -p tcp --dport 4840 -j ACCEPT",
        "-A INPUT -p tcp --dport 502 -s 192.168.0.0/16 -j ACCEPT"]}
    if not shutil.which("iptables"):
        return demo
    try:
        p = subprocess.run(["sudo", "-n", "iptables", "-S"], capture_output=True, text=True, timeout=6)
        if p.returncode != 0:
            p = subprocess.run(["iptables", "-S"], capture_output=True, text=True, timeout=6)
        if p.returncode != 0:   # present but no privilege → demo
            return demo
        rules = [ln for ln in p.stdout.splitlines() if ln.startswith(("-A", "-P"))]
        return {"available": True, "rules": rules}
    except Exception:
        return demo


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
        if path == "/api/v1/logs":
            q = parse_qs(self.path.split("?", 1)[1] if "?" in self.path else "")
            n = int((q.get("n", ["300"])[0]) or 300)
            thr = _LEVELS.get(q.get("level", [""])[0], 0)
            lines = [ln for ln in LOGS if _LEVELS.get(ln["level"], 1) >= thr]
            return self._json({"config": LOG_CFG, "lines": lines[-n:]})
        if path == "/api/v1/alarms":
            active = [{"id": k, "label": v["rule"]["label"], "severity": v["rule"]["severity"],
                       "tag": v["rule"]["tag"], "since": v["since"], "value": v["value"], "acked": v["acked"]}
                      for k, v in ALARM_ACTIVE.items()]
            return self._json({"rules": ALARM_RULES, "active": active, "events": list(ALARM_EVENTS)[-100:]})
        if path == "/api/v1/historian/config":
            return self._json(HIST_CFG)
        if path == "/api/v1/history":
            q = parse_qs(self.path.split("?", 1)[1] if "?" in self.path else "")
            tag = q.get("tag", [""])[0]
            n = int((q.get("n", ["400"])[0]) or 400)
            return self._json({"tag": tag, "samples": list(HISTORY.get(tag, []))[-n:]})
        if path == "/api/v1/users":
            return self._json({"users": USERS, "roles": ROLES})
        if path == "/api/v1/hass/config":
            return self._json({"config": HASS_CFG, "drivers": hass_drivers()})
        if path == "/api/v1/toolchains":
            return self._json({"toolchains": toolchain_status()})
        if path == "/api/v1/fail2ban":
            return self._json(fail2ban_status())
        if path == "/api/v1/firewall":
            return self._json(firewall_rules())
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
            log("warn", f"scan cycle {'started' if RUNNING else 'stopped'}")
            return self._json({"state": "running" if RUNNING else "stopped", "ok": True})
        if path == "/api/v1/logs/clear":
            LOGS.clear()
            return self._json({"ok": True})
        if path.startswith("/api/v1/alarms/"):
            global _arc
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "rule":
                _arc += 1
                rule = {"id": "a" + str(_arc), "tag": body.get("tag", ""), "op": body.get("op", ">"),
                        "value": float(body.get("value", 0) or 0), "severity": body.get("severity", "warn"),
                        "label": body.get("label", "alarm")}
                ALARM_RULES.append(rule)
                return self._json({"ok": True, "rule": rule})
            if act == "delete":
                rid = body.get("id")
                ALARM_RULES[:] = [r for r in ALARM_RULES if r["id"] != rid]
                ALARM_ACTIVE.pop(rid, None)
                return self._json({"ok": True})
            if act == "ack":
                rid = body.get("id")
                if rid in ALARM_ACTIVE:
                    ALARM_ACTIVE[rid]["acked"] = True
                return self._json({"ok": True})
        if path.startswith("/api/v1/users/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]

            def _find(u, h):
                return next((x for x in USERS if x["user"] == u and x["host"] == h), None)

            if act == "create":
                role = body.get("role", "viewer")
                rp = ROLES.get(role, ROLES["viewer"])
                u = {"user": body.get("user", ""), "host": body.get("host", "%"), "role": role,
                     "privileges": body.get("privileges", rp["privileges"]), "scope": body.get("scope", rp["scope"])}
                if not u["user"]:
                    return self._json({"error": "user required"}, 400)
                USERS[:] = [x for x in USERS if not (x["user"] == u["user"] and x["host"] == u["host"])]
                USERS.append(u)
                sql = f"CREATE USER IF NOT EXISTS '{u['user']}'@'{u['host']}' IDENTIFIED BY '****';\n" + grant_sql(u)
                if _conn:
                    try:
                        db_exec(f"CREATE USER IF NOT EXISTS '{u['user']}'@'{u['host']}' IDENTIFIED BY %s", (body.get("password", ""),))
                        db_exec(f"GRANT {', '.join(u['privileges'])} ON {u['scope']} TO '{u['user']}'@'{u['host']}'")
                    except Exception as e:
                        log("warn", f"grant failed: {e}")
                log("info", f"user {u['user']}@{u['host']} → {role}")
                return self._json({"ok": True, "sql": sql, "user": u})
            if act == "role":
                u = _find(body.get("user"), body.get("host"))
                if not u:
                    return self._json({"error": "not found"}, 404)
                role = body.get("role")
                if role in ROLES:
                    u["role"] = role
                    u["privileges"] = ROLES[role]["privileges"]
                    u["scope"] = ROLES[role]["scope"]
                sql = f"REVOKE ALL PRIVILEGES, GRANT OPTION FROM '{u['user']}'@'{u['host']}';\n" + grant_sql(u)
                if _conn:
                    try:
                        db_exec(f"REVOKE ALL PRIVILEGES, GRANT OPTION FROM '{u['user']}'@'{u['host']}'")
                        db_exec(f"GRANT {', '.join(u['privileges'])} ON {u['scope']} TO '{u['user']}'@'{u['host']}'")
                    except Exception as e:
                        log("warn", f"regrant failed: {e}")
                return self._json({"ok": True, "sql": sql, "user": u})
            if act == "delete":
                USERS[:] = [x for x in USERS if not (x["user"] == body.get("user") and x["host"] == body.get("host"))]
                sql = f"DROP USER IF EXISTS '{body.get('user')}'@'{body.get('host')}';"
                if _conn:
                    try:
                        db_exec(f"DROP USER IF EXISTS '{body.get('user')}'@'{body.get('host')}'")
                    except Exception as e:
                        log("warn", f"drop failed: {e}")
                return self._json({"ok": True, "sql": sql})
        if path == "/api/v1/hass/test":
            url = HASS_CFG["url"].rstrip("/")
            try:
                import urllib.request
                req = urllib.request.Request(url + "/api/", headers={"Authorization": "Bearer " + HASS_CFG["token"]})
                with urllib.request.urlopen(req, timeout=4) as r:
                    ok = r.status == 200
                return self._json({"ok": ok, "message": f"connected to {url}" if ok else f"HTTP {r.status}"})
            except Exception as e:
                return self._json({"ok": False, "message": f"could not reach {url}: {e}"})
        if path in ("/api/v1/exec", "/api/admin/exec"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            lang = body.get("lang", "bash")
            dbg = bool(body.get("debug"))
            log("info", f"exec {lang} script ({len(body.get('code',''))} chars){' [debug]' if dbg else ''}")
            return self._json(run_script(lang, body.get("code", ""), debug=dbg))
        if path == "/api/v1/toolchains/install":
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            return self._json(toolchain_install(body.get("lang", ""), body.get("method", "apt")))
        if path == "/api/v1/fail2ban/unban":
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            if shutil.which("fail2ban-client"):
                try:
                    subprocess.run(["fail2ban-client", "set", body.get("jail", ""), "unbanip", body.get("ip", "")],
                                   capture_output=True, text=True, timeout=6)
                except Exception:
                    pass
            log("info", f"unban {body.get('ip')} from {body.get('jail')}")
            return self._json({"ok": True})
        if path == "/api/v1/firewall/rule":
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            flag = "-A" if body.get("action", "add") == "add" else "-D"
            spec = body.get("spec", "")
            if shutil.which("iptables") and spec:
                try:
                    subprocess.run(["sudo", "-n", "iptables", flag, "INPUT"] + spec.split(),
                                   capture_output=True, text=True, timeout=6)
                except Exception:
                    pass
            log("info", f"firewall {body.get('action', 'add')} INPUT {spec}")
            return self._json({"ok": True})
        if path.startswith("/api/v1/projects/"):   # activate / etc — accepted stub
            return self._json({"ok": True})
        return self._json({"error": "not found", "path": path}, 404)

    def do_PUT(self):
        path = self.path.split("?", 1)[0]
        if path == "/api/v1/logs/config":
            global LOGS
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            for k in ("level", "max_lines", "persistent"):
                if k in body:
                    LOG_CFG[k] = body[k]
            LOGS = deque(LOGS, maxlen=int(LOG_CFG["max_lines"]))
            log("info", "log configuration updated")
            return self._json({"config": LOG_CFG, "ok": True})
        if path == "/api/v1/historian/config":
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            for k in list(HIST_CFG.keys()):
                if k in body:
                    HIST_CFG[k] = body[k]
            log("info", f"historian → {HIST_CFG['backend']} ({len(HIST_CFG['tags'])} tags @ {HIST_CFG['sample_ms']}ms)")
            return self._json({"config": HIST_CFG, "ok": True})
        if path == "/api/v1/hass/config":
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            for k in list(HASS_CFG.keys()):
                if k in body:
                    HASS_CFG[k] = body[k]
            log("info", f"Home Assistant gateway {'enabled' if HASS_CFG['enabled'] else 'disabled'} ({HASS_CFG['url']})")
            return self._json({"config": HASS_CFG, "ok": True})
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
        log("info", f"set-point {key} = {num}")
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
    log("info", f"OSOLogic sandbox core up — {len(CACHE)} tags, db {'up' if _conn else 'seed'}, scan {SCAN_MS}ms")
    if OPC_ENABLE:
        run_opc()
    else:
        while True:
            time.sleep(3600)


if __name__ == "__main__":
    main()
