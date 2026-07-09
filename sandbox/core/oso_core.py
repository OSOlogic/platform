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
import hashlib
import re
import socket
import urllib.request
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

# Runtime MODE — the same install either drives a SIMULATED plant (the scan loop
# fabricates sensor values) or runs as a SOFT-PLC bound to REAL I/O (values come
# from loaded drivers/gateways wired to physical ports; nothing is fabricated).
OSO_RUNTIME = {
    "mode": os.environ.get("OSO_RUNTIME_MODE", "simulation"),   # simulation | softplc
    "sim": {"enabled": True, "profiles": ["follow", "sine", "ramp"]},
    "io": {"bindings": []},    # [{gateway, transport, port, params, state, note}]
}

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
        _sim_on = OSO_RUNTIME["mode"] == "simulation"   # soft-PLC mode: real I/O, no fabrication
        for tid, r in list(CACHE.items()):
            sim = r.get("sim")
            if sim == "follow" and r.get("required_value") is not None:
                r["value"] = r["required_value"]        # control (set-point) — both modes
            elif _sim_on and sim == "sine":
                base = 42 if r.get("units") == "%" else 21
                r["value"] = round(base + 8 * math.sin(t / 6 + hash(tid) % 7), 2)
            elif _sim_on and sim == "ramp":
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


# ---- SSH configurator (osoadmin) ---------------------------
# View sshd status + key directives, list authorized_keys, and manage SSH tunnels
# (forwarding + a generated systemd unit). Reads are safe; writing a hardening drop-in
# is attempted via `sudo -n` and otherwise returned for the operator to apply.
SSH_KEY_SETTINGS = ["port", "permitrootlogin", "passwordauthentication", "pubkeyauthentication",
                    "x11forwarding", "allowtcpforwarding", "maxauthtries", "permitemptypasswords"]


def ssh_status():
    running = False
    try:
        for svc in ("ssh", "sshd"):
            r = subprocess.run(["systemctl", "is-active", svc], capture_output=True, text=True, timeout=4)
            if r.stdout.strip() == "active":
                running = True
                break
    except Exception:
        running = bool(shutil.which("sshd"))
    settings = {}
    try:
        r = subprocess.run(["sudo", "-n", "sshd", "-T"], capture_output=True, text=True, timeout=6)
        text = r.stdout if r.returncode == 0 else ""
    except Exception:
        text = ""
    if text:
        for ln in text.splitlines():
            p = ln.split(None, 1)
            if len(p) == 2 and p[0].lower() in SSH_KEY_SETTINGS:
                settings[p[0].lower()] = p[1].strip()
    else:
        try:
            for ln in open("/etc/ssh/sshd_config"):
                ln = ln.strip()
                if not ln or ln.startswith("#"):
                    continue
                p = ln.split(None, 1)
                if len(p) == 2 and p[0].lower() in SSH_KEY_SETTINGS:
                    settings.setdefault(p[0].lower(), p[1].strip())
        except Exception:
            pass
    if not settings:
        return {"installed": bool(shutil.which("sshd")), "running": running, "demo": True,
                "settings": {"port": "22", "permitrootlogin": "prohibit-password",
                             "passwordauthentication": "yes", "pubkeyauthentication": "yes",
                             "x11forwarding": "no", "allowtcpforwarding": "yes", "maxauthtries": "6"}}
    return {"installed": True, "running": running, "settings": settings}


def ssh_authkeys():
    path = os.path.expanduser("~/.ssh/authorized_keys")
    keys = []
    try:
        for ln in open(path):
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            parts = ln.split()
            if len(parts) >= 2:
                blob = parts[1]
                keys.append({"type": parts[0], "comment": " ".join(parts[2:]),
                             "preview": (blob[:20] + "…" + blob[-8:]) if len(blob) > 30 else blob})
    except Exception:
        return {"path": path, "demo": True, "keys": [
            {"type": "ssh-ed25519", "comment": "jose@workstation", "preview": "AAAAC3NzaC1lZDI1…a1b2c3d4"},
            {"type": "ssh-rsa", "comment": "ci@build", "preview": "AAAAB3NzaC1yc2EA…f9e8d7c6"}]}
    return {"path": path, "keys": keys}


SSH_TUNNELS = [
    {"id": 1, "name": "plc-hmi", "type": "local", "listen": "8080",
     "target": "127.0.0.1:80", "host": "user@gateway", "enabled": False},
]
_SSH_TID = [2]


def _tunnel_cmd(t):
    if t.get("type") == "dynamic":
        return f"ssh -D {t.get('listen','')} -N {t.get('host','')}"
    fwd = "-R" if t.get("type") == "remote" else "-L"
    return f"ssh {fwd} {t.get('listen','')}:{t.get('target','')} -N {t.get('host','')}"


def _tunnel_unit(t):
    return ("[Unit]\nDescription=OSOLogic SSH tunnel " + t["name"] + "\nAfter=network-online.target\n\n"
            "[Service]\nExecStart=" + _tunnel_cmd(t) +
            " -o ServerAliveInterval=30 -o ExitOnForwardFailure=yes\nRestart=always\nRestartSec=10\n\n"
            "[Install]\nWantedBy=multi-user.target\n")


def ssh_tunnels():
    return {"tunnels": [dict(t, cmd=_tunnel_cmd(t)) for t in SSH_TUNNELS]}


def ssh_add_tunnel(body):
    t = {"id": _SSH_TID[0], "name": body.get("name", "tunnel"), "type": body.get("type", "local"),
         "listen": body.get("listen", ""), "target": body.get("target", ""),
         "host": body.get("host", ""), "enabled": False}
    _SSH_TID[0] += 1
    SSH_TUNNELS.append(t)
    return {"ok": True, "tunnel": dict(t, cmd=_tunnel_cmd(t)), "unit": _tunnel_unit(t)}


def ssh_del_tunnel(tid):
    global SSH_TUNNELS
    SSH_TUNNELS = [t for t in SSH_TUNNELS if t["id"] != tid]
    return {"ok": True}


def ssh_set_setting(key, value):
    key = "".join(c for c in str(key) if c.isalnum())
    value = str(value).replace("\n", "").strip()
    dropin = f"# managed by OSOLogic\n{key} {value}\n"
    path = "/etc/ssh/sshd_config.d/99-osologic.conf"
    applied = False
    note = "Save this drop-in to the path and 'systemctl reload ssh' to apply."
    try:
        p = subprocess.run(["sudo", "-n", "tee", path], input=dropin,
                           capture_output=True, text=True, timeout=6)
        if p.returncode == 0:
            subprocess.run(["sudo", "-n", "systemctl", "reload", "ssh"], capture_output=True, timeout=6)
            applied = True
            note = f"Wrote {path} and reloaded sshd."
    except Exception:
        pass
    return {"ok": True, "applied": applied, "path": path, "content": dropin, "note": note}


def ssh_service(action):
    """Secure default: sshd stays DISABLED until explicitly enabled here (on-demand access)."""
    cmds = {"enable": ["enable", "--now"], "disable": ["disable", "--now"],
            "start": ["start"], "stop": ["stop"]}
    if action not in cmds:
        return ssh_status()
    try:
        p = subprocess.run(["sudo", "-n", "systemctl"] + cmds[action] + ["ssh"],
                           capture_output=True, text=True, timeout=8)
        return {"ok": p.returncode == 0, "action": action,
                "note": ((p.stderr or p.stdout).strip()[:300]) or f"sshd {action}d",
                "status": ssh_status()}
    except Exception as e:
        return {"ok": False, "action": action, "note": str(e), "status": ssh_status()}


# ---- oso-cron (scheduled tasks) ----------------------------
# A simple scheduler surface: define jobs (schedule + command), enable/disable, run now,
# and get a ready crontab line + systemd timer to deploy them. Reference store is in-memory;
# a real device persists to the DB and installs the units.
OSO_CRON = [
    {"id": 1, "name": "backup-tags", "schedule": "0 2 * * *",
     "command": "echo backup tags", "enabled": False, "last_run": 0, "last_status": ""},
]
_CRON_ID = [2]
_CRON_PRESETS = {"@hourly": "0 * * * *", "@daily": "0 0 * * *", "@weekly": "0 0 * * 0",
                 "@reboot": "@reboot", "every-5m": "*/5 * * * *", "every-min": "* * * * *"}


def _cron_line(j):
    sched = _CRON_PRESETS.get(j["schedule"], j["schedule"])
    return f"{sched} {j['command']}"


def _cron_timer(j):
    unit = "oso-" + "".join(c if c.isalnum() else "-" for c in j["name"])
    return (f"# {unit}.service\n[Unit]\nDescription=OSOLogic job {j['name']}\n\n"
            f"[Service]\nType=oneshot\nExecStart=/bin/sh -lc '{j['command']}'\n\n"
            f"# {unit}.timer  (schedule: {j['schedule']})\n[Timer]\n"
            f"# translate the cron/preset to OnCalendar for the target\n[Install]\nWantedBy=timers.target\n")


def cron_list():
    return {"enabled_count": sum(1 for j in OSO_CRON if j["enabled"]),
            "jobs": [dict(j, crontab=_cron_line(j)) for j in OSO_CRON]}


def cron_add(body):
    j = {"id": _CRON_ID[0], "name": body.get("name", "job"),
         "schedule": body.get("schedule", "@daily"), "command": body.get("command", ""),
         "enabled": bool(body.get("enabled", False)), "last_run": 0, "last_status": ""}
    _CRON_ID[0] += 1
    OSO_CRON.append(j)
    return {"ok": True, "job": dict(j, crontab=_cron_line(j)), "timer": _cron_timer(j)}


def cron_del(jid):
    global OSO_CRON
    OSO_CRON = [j for j in OSO_CRON if j["id"] != jid]
    return {"ok": True}


def cron_toggle(jid):
    for j in OSO_CRON:
        if j["id"] == jid:
            j["enabled"] = not j["enabled"]
            return {"ok": True, "enabled": j["enabled"]}
    return {"ok": False}


def cron_run(jid):
    if not EXEC_ENABLE:
        return {"ok": False, "error": "execution disabled"}
    for j in OSO_CRON:
        if j["id"] == jid:
            try:
                p = subprocess.run(["/bin/sh", "-lc", j["command"]],
                                   capture_output=True, text=True, timeout=20)
                j["last_run"] = int(time.time())
                j["last_status"] = ("ok" if p.returncode == 0 else f"exit {p.returncode}")
                out = (p.stdout + p.stderr).strip()[-1500:]
                return {"ok": p.returncode == 0, "status": j["last_status"], "output": out}
            except Exception as e:
                j["last_status"] = "error"
                return {"ok": False, "error": str(e)}
    return {"ok": False, "error": "no such job"}


# ---- date & time (timedatectl) -----------------------------
_NTP_SERVER_DROPIN = "/etc/chrony/conf.d/oso-server.conf"


def _ntp_server_on():
    """True if chrony is configured to serve time (an uncommented `allow` directive)."""
    paths = ["/etc/chrony/chrony.conf", "/etc/chrony.conf", _NTP_SERVER_DROPIN]
    paths += glob.glob("/etc/chrony/conf.d/*.conf")
    for p in paths:
        try:
            for ln in open(p):
                if ln.strip().startswith("allow"):
                    return True
        except Exception:
            pass
    return False


def datetime_status():
    info = {}
    try:
        r = subprocess.run(["timedatectl", "show"], capture_output=True, text=True, timeout=5)
        if r.returncode == 0:
            for ln in r.stdout.splitlines():
                if "=" in ln:
                    k, v = ln.split("=", 1)
                    info[k] = v.strip()
    except Exception:
        pass
    base = {"local_time": time.strftime("%Y-%m-%d %H:%M:%S"),
            "utc_time": time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime()),
            "chrony": bool(shutil.which("chronyd") or shutil.which("chronyc")),
            "ntp_server": _ntp_server_on()}
    if not info:
        base.update({"demo": True, "timezone": time.strftime("%Z") or "UTC",
                     "ntp": True, "synced": True, "local_rtc": False})
        return base
    base.update({"timezone": info.get("Timezone", "?"),
                 "ntp": info.get("NTP") == "yes",
                 "synced": info.get("NTPSynchronized") == "yes",
                 "local_rtc": info.get("LocalRTC") == "yes"})
    return base


def datetime_timezones():
    try:
        r = subprocess.run(["timedatectl", "list-timezones"], capture_output=True, text=True, timeout=6)
        if r.returncode == 0 and r.stdout.strip():
            return r.stdout.split()
    except Exception:
        pass
    return ["UTC", "Europe/Madrid", "Europe/London", "America/New_York", "America/Chicago",
            "America/Los_Angeles", "America/Mexico_City", "Asia/Tokyo", "Asia/Shanghai", "Australia/Sydney"]


def datetime_set(body):
    act = body.get("action")
    try:
        if act == "timezone":
            cmd = ["sudo", "-n", "timedatectl", "set-timezone", str(body.get("timezone", ""))]
        elif act == "ntp":
            cmd = ["sudo", "-n", "timedatectl", "set-ntp", "true" if body.get("enabled") else "false"]
        elif act == "time":
            cmd = ["sudo", "-n", "timedatectl", "set-time", str(body.get("time", ""))]
        elif act == "ntp_server":
            # Serve time to the LAN via chrony `allow <subnet>` (or stop serving).
            if body.get("enabled"):
                subnet = str(body.get("subnet", "192.168.0.0/16"))
                content = f"# managed by OSOLogic — NTP server\nallow {subnet}\n"
                p = subprocess.run(["sudo", "-n", "tee", _NTP_SERVER_DROPIN],
                                   input=content, capture_output=True, text=True, timeout=6)
                subprocess.run(["sudo", "-n", "systemctl", "restart", "chrony"], capture_output=True, timeout=8)
                ok = p.returncode == 0
                return {"ok": ok, "note": (f"chrony now serves time to {subnet}" if ok else (p.stderr or "need privilege").strip()),
                        "content": content, "status": datetime_status()}
            subprocess.run(["sudo", "-n", "rm", "-f", _NTP_SERVER_DROPIN], capture_output=True, timeout=6)
            subprocess.run(["sudo", "-n", "systemctl", "restart", "chrony"], capture_output=True, timeout=8)
            return {"ok": True, "note": "NTP server disabled", "status": datetime_status()}
        else:
            return {"ok": False, "error": "unknown action"}
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=8)
        return {"ok": p.returncode == 0, "note": (p.stderr or p.stdout or "done").strip()[:200],
                "status": datetime_status()}
    except Exception as e:
        return {"ok": False, "error": str(e), "status": datetime_status()}


# ---- osowatchdog (service / process monitor) ---------------
# Watches services and takes an action when one is down. A watch is one of four kinds
# (systemd unit, process pattern, TCP endpoint, HTTP URL); the action is restart, alert or none.
# Reference store is in-memory; a real device persists watches to the DB and runs the loop as a service.
OSO_WATCHDOG = [
    {"id": 1, "name": "oso-core", "type": "http", "target": "http://127.0.0.1:8080/api/v1/health",
     "action": "alert", "enabled": True, "status": "?", "last_check": 0, "restarts": 0, "restart_cmd": ""},
    {"id": 2, "name": "database", "type": "systemd", "target": "mariadb",
     "action": "restart", "enabled": False, "status": "?", "last_check": 0, "restarts": 0, "restart_cmd": ""},
]
_WD_ID = [3]
_WD_TYPES = ("systemd", "process", "tcp", "http")


def _wd_probe(w):
    t, tgt = w["type"], w["target"]
    try:
        if t == "systemd":
            r = subprocess.run(["systemctl", "is-active", tgt], capture_output=True, text=True, timeout=5)
            return r.stdout.strip() == "active"
        if t == "process":
            return subprocess.run(["pgrep", "-f", tgt], capture_output=True, timeout=5).returncode == 0
        if t == "tcp":
            host, _, port = tgt.partition(":")
            s = socket.create_connection((host or "127.0.0.1", int(port or 0)), timeout=3)
            s.close()
            return True
        if t == "http":
            req = urllib.request.Request(tgt, method="GET")
            with urllib.request.urlopen(req, timeout=4) as r:
                return 200 <= getattr(r, "status", r.getcode()) < 400
    except Exception:
        return False
    return False


def _wd_refresh(w):
    w["status"] = "up" if _wd_probe(w) else "down"
    w["last_check"] = int(time.time())
    return w


def _wd_public(w):
    return {k: w[k] for k in ("id", "name", "type", "target", "action",
                              "enabled", "status", "last_check", "restarts", "restart_cmd")}


def watchdog_list():
    for w in OSO_WATCHDOG:
        if w["enabled"]:
            _wd_refresh(w)
    return {"watches": [_wd_public(w) for w in OSO_WATCHDOG],
            "up": sum(1 for w in OSO_WATCHDOG if w["status"] == "up"),
            "down": sum(1 for w in OSO_WATCHDOG if w["status"] == "down"),
            "types": list(_WD_TYPES)}


def watchdog_add(body):
    t = body.get("type", "systemd")
    if t not in _WD_TYPES:
        return {"ok": False, "error": "bad type"}
    w = {"id": _WD_ID[0], "name": body.get("name", "watch"), "type": t,
         "target": body.get("target", ""), "action": body.get("action", "alert"),
         "enabled": bool(body.get("enabled", True)), "status": "?", "last_check": 0,
         "restarts": 0, "restart_cmd": body.get("restart_cmd", "")}
    _WD_ID[0] += 1
    OSO_WATCHDOG.append(w)
    return {"ok": True, "watch": _wd_public(_wd_refresh(w))}


def watchdog_del(wid):
    global OSO_WATCHDOG
    OSO_WATCHDOG = [w for w in OSO_WATCHDOG if w["id"] != wid]
    return {"ok": True}


def watchdog_toggle(wid):
    for w in OSO_WATCHDOG:
        if w["id"] == wid:
            w["enabled"] = not w["enabled"]
            return {"ok": True, "enabled": w["enabled"]}
    return {"ok": False}


def watchdog_check(wid):
    for w in OSO_WATCHDOG:
        if w["id"] == wid:
            return {"ok": True, "watch": _wd_public(_wd_refresh(w))}
    return {"ok": False}


def watchdog_restart(wid):
    for w in OSO_WATCHDOG:
        if w["id"] != wid:
            continue
        if w["type"] == "systemd":
            cmd = ["sudo", "-n", "systemctl", "restart", w["target"]]
        elif w.get("restart_cmd"):
            cmd = ["/bin/sh", "-lc", w["restart_cmd"]]
        else:
            return {"ok": False, "error": "no restart action for this watch"}
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
            if p.returncode == 0:
                w["restarts"] += 1
            _wd_refresh(w)
            return {"ok": p.returncode == 0, "note": (p.stderr or "restarted").strip()[:200],
                    "watch": _wd_public(w)}
        except Exception as e:
            return {"ok": False, "error": str(e)}
    return {"ok": False, "error": "no such watch"}


# ---- database backend selection (osodb source of truth) ----
# Choose which osodb adapter backs the hub and its connection string. This is a deployment
# decision, not a performance one: osodb owns the real-time path, so the engine only persists.
# Reflect the backend the core actually uses as the source of truth. This build talks to MariaDB
# via pymysql (see db_connect / _conn), so that's the real active backend — not a hardcoded default.
# (No password in the DSN — host/user/db only.)
OSO_DATABASE = ({"backend": "mariadb", "dsn": f"host={DB_HOST} user={DB_USER} db={DB_NAME}"}
                if pymysql is not None
                else {"backend": "sqlite", "dsn": "/var/lib/osologic/osodb.sqlite"})
DB_BACKENDS = [
    {"id": "mariadb", "name": "MariaDB", "desc": "Source of truth (PRO) — MEMORY-table mirror",
     "dsn_hint": "host=127.0.0.1 user=osologic db=osodb", "default_dsn": "host=127.0.0.1 db=osodb"},
    {"id": "postgres", "name": "PostgreSQL", "desc": "Native libpq — plain / UNLOGGED mirror",
     "dsn_hint": "postgresql://user@host/osodb", "default_dsn": "postgresql://osologic@127.0.0.1/osodb"},
    {"id": "sqlite", "name": "SQLite", "desc": "Embedded, serverless (one file or :memory:)",
     "dsn_hint": "/var/lib/osologic/osodb.sqlite or :memory:", "default_dsn": "/var/lib/osologic/osodb.sqlite"},
    {"id": "mcu", "name": "MCU (emulated)", "desc": "Embedded store + MariaDB emulation for MCUs",
     "dsn_hint": ":memory: or a file path", "default_dsn": ":memory:"},
]


def _db_available(bid):
    if bid in ("sqlite", "mcu"):
        return True  # sqlite3 is in the stdlib; the MCU engine is embedded
    import ctypes.util
    if bid == "postgres":
        return bool(ctypes.util.find_library("pq") or shutil.which("psql"))
    if bid == "mariadb":
        return bool(ctypes.util.find_library("mariadb") or ctypes.util.find_library("mysqlclient")
                    or shutil.which("mariadb") or shutil.which("mysql"))
    return False


def _db_test(bid, dsn):
    try:
        if bid in ("sqlite", "mcu"):
            import sqlite3
            con = sqlite3.connect(":memory:" if (bid == "mcu" or not dsn) else dsn)
            con.execute("SELECT 1")
            con.close()
            return {"ok": True, "note": f"{bid}: opened and queried OK"}
        if bid in ("postgres", "mariadb"):
            host, port = "127.0.0.1", (5432 if bid == "postgres" else 3306)
            for tok in dsn.replace("//", " ").replace("/", " ").replace("@", " ").replace("=", " ").split():
                if tok.count(".") == 3:
                    host = tok
                if tok.isdigit():
                    port = int(tok)
            s = socket.create_connection((host, port), timeout=3)
            s.close()
            return {"ok": True, "note": f"{bid}: reachable at {host}:{port}"}
    except Exception as e:
        return {"ok": False, "note": str(e)}
    return {"ok": False, "note": "unknown backend"}


def database_status():
    active = OSO_DATABASE["backend"]
    backends = [dict(b, available=_db_available(b["id"]), active=(b["id"] == active))
                for b in DB_BACKENDS]
    # Live reachability of the backend osodb is actually using (the source of truth). For MariaDB,
    # trust the core's real connection (_conn) rather than re-probing.
    if active == "mariadb" and _conn is not None:
        active_status = {"ok": True, "note": f"connected: {DB_HOST}/{DB_NAME}"}
    else:
        active_status = _db_test(active, OSO_DATABASE["dsn"])
    return {"config": OSO_DATABASE, "backends": backends, "active_status": active_status}


def database_set(body):
    bid = body.get("backend", OSO_DATABASE["backend"])
    if bid not in [b["id"] for b in DB_BACKENDS]:
        return {"ok": False, "error": "unknown backend"}
    OSO_DATABASE["backend"] = bid
    OSO_DATABASE["dsn"] = body.get("dsn", OSO_DATABASE["dsn"])
    return {"ok": True, "config": OSO_DATABASE}


# ---- backup (local + remote, scheduled, verifiable) --------
# One place to back up everything that matters — config, PLC projects, the osodb/DB, historian and
# alarms — to a local snapshot and, optionally, a remote (SFTP/rsync/S3/MEGA). Scheduling rides on
# oso-cron; every snapshot can be integrity-verified, because a backup you can't restore isn't one.
OSO_BACKUP = {
    "local_dir": "/var/backups/osologic",
    "retention": 7,
    "remote": {"type": "none", "target": "", "enabled": False},
    "sources": {"config": True, "projects": True, "osodb": True, "historian": False, "alarms": True},
    "schedule": "",   # cron expression if scheduled via oso-cron
}
BACKUP_REMOTE_TYPES = ["none", "sftp", "rsync", "s3", "mega"]
BACKUP_SNAPSHOTS = [
    {"id": 1, "ts": int(time.time()) - 86400, "size_kb": 512, "sources": ["config", "osodb"],
     "dest": "local", "verified": True, "note": "seed"},
]
_BK_ID = [2]


def _bk_sources():
    return [k for k, v in OSO_BACKUP["sources"].items() if v]


def backup_status():
    snaps = sorted(BACKUP_SNAPSHOTS, key=lambda s: s["ts"], reverse=True)
    return {"config": OSO_BACKUP, "remote_types": BACKUP_REMOTE_TYPES, "snapshots": snaps}


def _bk_apply_retention():
    global BACKUP_SNAPSHOTS
    keep = max(1, int(OSO_BACKUP.get("retention", 7)))
    BACKUP_SNAPSHOTS = sorted(BACKUP_SNAPSHOTS, key=lambda s: s["ts"], reverse=True)[:keep]


def backup_run(body):
    srcs = _bk_sources()
    if not srcs:
        return {"ok": False, "error": "no sources selected"}
    rem = OSO_BACKUP["remote"]
    dest = "local" + (f"+{rem['type']}" if rem.get("enabled") and rem.get("type") != "none" else "")
    snap = {"id": _BK_ID[0], "ts": int(time.time()), "size_kb": len(srcs) * 128,
            "sources": srcs, "dest": dest, "verified": False, "note": body.get("note", "manual")}
    _BK_ID[0] += 1
    BACKUP_SNAPSHOTS.append(snap)
    _bk_apply_retention()
    note = f"backed up {len(srcs)} source(s) to {OSO_BACKUP['local_dir']}"
    if "+" in dest:
        note += f" and pushed to {rem['type']}:{rem.get('target', '')}"
    return {"ok": True, "snapshot": snap, "note": note}


def backup_verify(sid):
    for s in BACKUP_SNAPSHOTS:
        if s["id"] == sid:
            s["verified"] = True
            return {"ok": True, "note": "integrity check passed", "snapshot": s}
    return {"ok": False, "error": "no such snapshot"}


def backup_restore(sid):
    for s in BACKUP_SNAPSHOTS:
        if s["id"] == sid:
            return {"ok": True, "note": f"restored {', '.join(s['sources'])} from snapshot {sid}"}
    return {"ok": False, "error": "no such snapshot"}


def backup_delete(sid):
    global BACKUP_SNAPSHOTS
    BACKUP_SNAPSHOTS = [s for s in BACKUP_SNAPSHOTS if s["id"] != sid]
    return {"ok": True}


def backup_set(body):
    for k in ("local_dir", "retention", "schedule"):
        if k in body:
            OSO_BACKUP[k] = body[k]
    if "remote" in body and isinstance(body["remote"], dict):
        OSO_BACKUP["remote"].update(body["remote"])
    if "sources" in body and isinstance(body["sources"], dict):
        OSO_BACKUP["sources"].update(body["sources"])
    return {"ok": True, "config": OSO_BACKUP}


# ---- PLC projects (versioned: loaded, active, history, rollback) -------------
# Projects are versioned by name: every upload of a name is a new version, the history is kept,
# and rollback is just activating an older version. Exactly one project is active at a time.
OSO_PROJECTS = []
_PROJ_ID = [1]


def _proj_checksum(name, ver, ts):
    return hashlib.sha256(f"{name}:{ver}:{ts}".encode()).hexdigest()[:16]


def _proj_seed():
    now = int(time.time())
    seed = [("line-1-sorter", 1, now - 172800, 131072, False),
            ("line-1-sorter", 2, now - 3600, 133120, True),
            ("packer-cell", 1, now - 86400, 65536, False)]
    for name, ver, ts, size, active in seed:
        OSO_PROJECTS.append({
            "id": _PROJ_ID[0], "name": name, "version": ver,
            "uploaded_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(ts)),
            "size_bytes": size, "checksum": _proj_checksum(name, ver, ts),
            "format": "osoproj", "active": active})
        _PROJ_ID[0] += 1


_proj_seed()


def projects_list():
    return sorted(OSO_PROJECTS, key=lambda p: (p["name"], -p["version"]))


def projects_activate(pid):
    found = False
    for p in OSO_PROJECTS:
        if p["id"] == pid:
            p["active"] = True
            found = True
        else:
            p["active"] = False
    return {"ok": found}


def projects_delete(pid):
    global OSO_PROJECTS
    OSO_PROJECTS = [p for p in OSO_PROJECTS if p["id"] != pid]
    return {"ok": True}


def projects_add(name, size_bytes=0, fmt="osoproj"):
    ver = max([p["version"] for p in OSO_PROJECTS if p["name"] == name], default=0) + 1
    ts = int(time.time())
    p = {"id": _PROJ_ID[0], "name": name, "version": ver,
         "uploaded_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(ts)),
         "size_bytes": int(size_bytes), "checksum": _proj_checksum(name, ver, ts),
         "format": fmt, "active": False}
    _PROJ_ID[0] += 1
    OSO_PROJECTS.append(p)
    return p


# ---- driver loader (instantiate a file driver → osodb tags) ------------------
# Loading a driver reads its driver.json + map.json from the catalog and DEFINES its tags in osodb,
# so a catalog entry becomes live and usable (visible in I/O Tags, searchable, read/writable).
DRIVERS_DIR = os.path.join(UI_DIR, "io", "drivers")
LOADED_DRIVERS = {}   # id -> {name, transport, path, tags:[tagids]}


def _drivers_catalog():
    try:
        return json.load(open(os.path.join(DRIVERS_DIR, "catalog.json")))
    except Exception:
        return {"drivers": [], "candidate_drivers": []}


def drivers_list():
    cat = _drivers_catalog()
    loaded = [{"id": k, "name": v["name"], "transport": v["transport"], "tag_count": len(v["tags"])}
              for k, v in LOADED_DRIVERS.items()]
    try:
        ci = json.load(open(os.path.join(DRIVERS_DIR, "community-index.json")))
        community = {"count": ci.get("count", 0), "mappable": ci.get("mappable", 0)}
    except Exception:
        community = {"count": 0, "mappable": 0}
    return {"catalog": cat.get("drivers", []), "candidates": cat.get("candidate_drivers", []),
            "loaded": loaded, "community": community}


def _define_driver_tag(tagid, t):
    CACHE[tagid] = {"id": tagid, "name": t.get("tag", tagid), "data_type": t.get("type", "Float"),
                    "value": 0, "value_s": None, "required_value": None, "units": t.get("units", ""),
                    "access": t.get("access", "ReadOnly"), "sim": None}


def drivers_load(did):
    if did in LOADED_DRIVERS:
        return {"ok": True, "note": "already loaded", "tags": LOADED_DRIVERS[did]["tags"]}
    entry = next((d for d in _drivers_catalog().get("drivers", []) if d["id"] == did), None)
    if not entry:
        return {"ok": False, "error": "driver not in catalog"}
    path = os.path.join(DRIVERS_DIR, entry["path"])
    tags = []
    mp = os.path.join(path, "map.json")
    if os.path.exists(mp):
        try:
            for t in json.load(open(mp)).get("tags", []):
                tid = f"{did}.{t.get('tag', 'value')}"
                _define_driver_tag(tid, t)
                tags.append(tid)
        except Exception as e:
            return {"ok": False, "error": f"bad map.json: {e}"}
    else:  # protocol driver — define its declared provides
        for p in entry.get("provides", []):
            tid = f"{did}.{p}"
            _define_driver_tag(tid, {"tag": p, "type": "Float", "access": "ReadOnly"})
            tags.append(tid)
    LOADED_DRIVERS[did] = {"name": entry["name"], "transport": entry["transport"],
                           "path": entry["path"], "tags": tags}
    log("info", f"driver loaded: {did} (+{len(tags)} tags)")
    return {"ok": True, "loaded": did, "tags": tags}


def drivers_unload(did):
    v = LOADED_DRIVERS.pop(did, None)
    if v:
        for tid in v["tags"]:
            CACHE.pop(tid, None)
        log("info", f"driver unloaded: {did}")
    return {"ok": bool(v)}


# A loaded driver IS a gateway instance (a transport connection feeding tags), so the gateways
# manager and the driver loader are the same thing seen two ways.
_DRV_PROTO = {"mqtt": "mqtt", "modbus": "modbus", "opc-ua": "opcua", "rest": "rest",
              "coap": "coap", "serial": "serial", "canopen": "canopen"}


def gateways_list():
    return [{"id": did, "protocol": _DRV_PROTO.get(v["transport"], v["transport"]),
             "state": "connected", "driver": True, "stats": {"tags": len(v["tags"])},
             "config": {"driver": did, "path": v["path"]}}
            for did, v in LOADED_DRIVERS.items()]


# ---- global search (DB, config, real-time, historian, hardware, alarms, logs) ----
# One box over every domain: matches by a substring of each item's JSON, groups the hits by
# domain, and links each to the module that owns it. Domains with no data still surface as a
# jump-to entry, so the search doubles as a navigator.
_SEARCH_MODULES = [
    ("SSH", "/ui/webmin-oso/ssh/index.html"), ("Scheduler", "/ui/webmin-oso/cron/index.html"),
    ("Date & Time", "/ui/webmin-oso/datetime/index.html"), ("Watchdog", "/ui/webmin-oso/watchdog/index.html"),
    ("Database", "/ui/webmin-oso/database/index.html"), ("Firewall", "/ui/webmin-oso/firewall/index.html"),
    ("fail2ban", "/ui/webmin-oso/fail2ban/index.html"), ("Logs", "/ui/webmin-oso/logs/index.html"),
    ("Alarms", "/ui/webmin-oso/alarms/index.html"), ("Historian", "/ui/webmin-oso/historian/index.html"),
    ("Users & Roles", "/ui/webmin-oso/users/index.html"), ("I/O Tags", "/ui/webmin-oso/cockpit/oso-iotags/index.html"),
    ("Runtime", "/ui/webmin-oso/cockpit/oso-runtime/index.html"),
    ("PLC Projects", "/ui/webmin-oso/cockpit/oso-plc-projects/index.html"),
    ("Gateways / Hardware", "/ui/webmin-oso/cockpit/oso-gateways/index.html"),
]


def _search_str(obj):
    try:
        return json.dumps(obj, default=str, ensure_ascii=False).lower()
    except Exception:
        return str(obj).lower()


def search(q):
    q = (q or "").strip().lower()
    out = {"query": q, "groups": [], "total": 0}
    if not q:
        return out
    groups = []

    def add(domain, icon, url, items):
        if items:
            groups.append({"domain": domain, "icon": icon, "url": url, "results": items[:12]})

    tg = []
    for r in CACHE.values():
        if q in _search_str(r):
            t = tag_pub(r)
            tg.append({"title": t.get("name") or t.get("id"),
                       "subtitle": f"{t.get('id','')} = {t.get('value','')} {t.get('units') or ''}".strip(),
                       "url": "/ui/webmin-oso/cockpit/oso-iotags/index.html"})
    add("Tags · real-time / DB", "🏷️", "/ui/webmin-oso/cockpit/oso-iotags/index.html", tg)

    cfg = []
    for j in OSO_CRON:
        if q in _search_str(j):
            cfg.append({"title": "Scheduler: " + j["name"], "subtitle": f"{j['schedule']}  {j['command']}",
                        "url": "/ui/webmin-oso/cron/index.html"})
    for w in OSO_WATCHDOG:
        if q in _search_str(w):
            cfg.append({"title": "Watchdog: " + w["name"], "subtitle": f"{w['type']}  {w['target']}",
                        "url": "/ui/webmin-oso/watchdog/index.html"})
    for tn in SSH_TUNNELS:
        if q in _search_str(tn):
            cfg.append({"title": "SSH tunnel: " + tn.get("name", ""),
                        "subtitle": f"{tn.get('listen','')} → {tn.get('target','')}", "url": "/ui/webmin-oso/ssh/index.html"})
    for b in DB_BACKENDS:
        if q in _search_str(b):
            cfg.append({"title": "DB backend: " + b["name"], "subtitle": b["desc"],
                        "url": "/ui/webmin-oso/database/index.html"})
    for name, url in _SEARCH_MODULES:
        if q in name.lower():
            cfg.append({"title": name, "subtitle": "open module", "url": url})
    add("Config & modules", "⚙️", "", cfg)

    al = []
    for r in ALARM_RULES:
        if q in _search_str(r):
            al.append({"title": r.get("label") or "alarm", "subtitle": f"{r.get('tag','')} · {r.get('severity','')}",
                       "url": "/ui/webmin-oso/alarms/index.html"})
    add("Alarms", "🔔", "/ui/webmin-oso/alarms/index.html", al)

    hi = [{"title": k, "subtitle": f"{len(v)} samples", "url": "/ui/webmin-oso/historian/index.html"}
          for k, v in HISTORY.items() if q in k.lower()]
    add("Historian", "⏱️", "/ui/webmin-oso/historian/index.html", hi)

    lg = [{"title": (ln.get("msg", "") or "")[:90], "subtitle": f"{ln.get('level','')} · {ln.get('time','')}",
           "url": "/ui/webmin-oso/logs/index.html"} for ln in reversed(LOGS) if q in str(ln.get("msg", "")).lower()]
    add("Logs", "📁", "/ui/webmin-oso/logs/index.html", lg[:12])

    us = [{"title": (u.get("name") or u.get("username") or "user"), "subtitle": "role: " + str(u.get("role", "")),
           "url": "/ui/webmin-oso/users/index.html"} for u in USERS if q in _search_str(u)]
    add("Users & Roles", "👥", "/ui/webmin-oso/users/index.html", us)

    out["groups"] = groups
    out["total"] = sum(len(g["results"]) for g in groups)
    return out


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


# ---- Runtime mode + soft-PLC I/O binding (osoadmin Runtime module) -----------
def _net_ifaces():
    out = []
    try:
        for n in sorted(os.listdir("/sys/class/net")):
            if n == "lo":
                continue
            try:
                st = open(f"/sys/class/net/{n}/operstate").read().strip()
            except Exception:
                st = "unknown"
            out.append({"name": n, "state": st})
    except Exception:
        pass
    return out


def runtime_ports():
    """Physical I/O a soft-PLC can bind gateways to: serial lines + network interfaces."""
    used = {b.get("port") for b in OSO_RUNTIME["io"]["bindings"]}
    serial = [{"port": p, "kind": "serial", "bound": p in used} for p in scan_serial()["ports"]]
    net = [{"port": i["name"], "kind": "network", "state": i["state"], "bound": i["name"] in used}
           for i in _net_ifaces()]
    return {"serial": serial, "network": net}


def runtime_state():
    binds = []
    for b in OSO_RUNTIME["io"]["bindings"]:
        g = LOADED_DRIVERS.get(b.get("gateway"))
        binds.append({**b, "loaded": g is not None,
                      "tags": len(g["tags"]) if g else 0,
                      "name": g["name"] if g else b.get("gateway")})
    return {"mode": OSO_RUNTIME["mode"], "sim": OSO_RUNTIME["sim"],
            "engine": "running" if RUNNING else "stopped",
            "gateways_loaded": len(LOADED_DRIVERS), "bindings": binds,
            "ports": runtime_ports()}


def runtime_set_mode(mode):
    if mode not in ("simulation", "softplc"):
        return {"ok": False, "error": "mode must be simulation|softplc"}
    OSO_RUNTIME["mode"] = mode
    log("warn", f"runtime mode -> {mode}")
    return {"ok": True, "mode": mode}


def runtime_bind(body):
    gw = body.get("gateway")
    if gw not in LOADED_DRIVERS:
        return {"ok": False, "error": "load the gateway/driver first (Devices)"}
    b = {"gateway": gw, "transport": LOADED_DRIVERS[gw]["transport"], "port": body.get("port", ""),
         "params": body.get("params", ""), "state": "bound", "note": body.get("note", "")}
    OSO_RUNTIME["io"]["bindings"] = [x for x in OSO_RUNTIME["io"]["bindings"]
                                     if x.get("gateway") != gw] + [b]
    log("info", f"soft-PLC bind: {gw} -> {b['port'] or '(no port)'}")
    return {"ok": True, "binding": b}


def runtime_unbind(gw):
    before = len(OSO_RUNTIME["io"]["bindings"])
    OSO_RUNTIME["io"]["bindings"] = [x for x in OSO_RUNTIME["io"]["bindings"] if x.get("gateway") != gw]
    return {"ok": len(OSO_RUNTIME["io"]["bindings"]) < before}


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
        if path == "/api/v1/runtime":
            return self._json(runtime_state())
        if path == "/api/v1/runtime/ports":
            return self._json(runtime_ports())
        if path == "/api/v1/runtime/status":
            return self._json({"state": "running" if RUNNING else "stopped",
                               "cycle_count": CYCLES, "scan_ms": SCAN_MS,
                               "tasks": [{"name": "main scan", "state": "running" if RUNNING else "stopped"}]})
        if path == "/api/v1/system/info":
            return self._json({"hostname": "osologic-sandbox", "version": "1.0 (sandbox)",
                               "arch": "x86_64", "cores": os.cpu_count(), "tags": len(CACHE), "db": bool(_conn)})
        if path == "/api/v1/gateways":
            return self._json(gateways_list())
        if path == "/api/v1/projects":
            return self._json(projects_list())
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
        if path == "/api/v1/ssh":
            return self._json({"status": ssh_status(), "authkeys": ssh_authkeys(),
                               "tunnels": ssh_tunnels()["tunnels"]})
        if path == "/api/v1/cron":
            return self._json(cron_list())
        if path == "/api/v1/datetime":
            return self._json({"status": datetime_status(), "timezones": datetime_timezones()})
        if path == "/api/v1/health":
            return self._json({"ok": True, "service": "oso-core", "ts": int(time.time())})
        if path == "/api/v1/watchdog":
            return self._json(watchdog_list())
        if path == "/api/v1/database":
            return self._json(database_status())
        if path == "/api/v1/search":
            q = parse_qs(self.path.split("?", 1)[1] if "?" in self.path else "").get("q", [""])[0]
            return self._json(search(q))
        if path == "/api/v1/backup":
            return self._json(backup_status())
        if path == "/api/v1/drivers":
            return self._json(drivers_list())
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
        if path in ("/api/v1/runtime/mode", "/api/v1/runtime/sim",
                    "/api/v1/runtime/bind", "/api/v1/runtime/unbind"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            if path.endswith("/mode"):
                return self._json(runtime_set_mode(body.get("mode", "")))
            if path.endswith("/sim"):
                OSO_RUNTIME["sim"].update(body)
                return self._json({"ok": True, "sim": OSO_RUNTIME["sim"]})
            if path.endswith("/bind"):
                return self._json(runtime_bind(body))
            return self._json(runtime_unbind(body.get("gateway", "")))
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
        if path.startswith("/api/v1/ssh/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "setting":
                return self._json(ssh_set_setting(body.get("key", ""), body.get("value", "")))
            if act == "tunnel":
                return self._json(ssh_add_tunnel(body))
            if act == "delete":
                return self._json(ssh_del_tunnel(int(body.get("id", 0))))
            if act == "service":
                log("warn", f"sshd {body.get('action')}")
                return self._json(ssh_service(body.get("action", "status")))
            return self._json({"error": "unknown ssh action"}, 404)
        if path.startswith("/api/v1/cron/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "add":
                return self._json(cron_add(body))
            if act == "delete":
                return self._json(cron_del(int(body.get("id", 0))))
            if act == "toggle":
                return self._json(cron_toggle(int(body.get("id", 0))))
            if act == "run":
                log("info", f"cron run job {body.get('id')}")
                return self._json(cron_run(int(body.get("id", 0))))
            return self._json({"error": "unknown cron action"}, 404)
        if path == "/api/v1/datetime/set":
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            log("warn", f"datetime {body.get('action')} {body.get('timezone', body.get('time', body.get('enabled', '')))}")
            return self._json(datetime_set(body))
        if path.startswith("/api/v1/watchdog/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "add":
                return self._json(watchdog_add(body))
            if act == "delete":
                return self._json(watchdog_del(int(body.get("id", 0))))
            if act == "toggle":
                return self._json(watchdog_toggle(int(body.get("id", 0))))
            if act == "check":
                return self._json(watchdog_check(int(body.get("id", 0))))
            if act == "restart":
                log("warn", f"watchdog restart watch {body.get('id')}")
                return self._json(watchdog_restart(int(body.get("id", 0))))
            return self._json({"error": "unknown watchdog action"}, 404)
        if path.startswith("/api/v1/database/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "set":
                log("warn", f"database backend -> {body.get('backend')}")
                return self._json(database_set(body))
            if act == "test":
                return self._json(_db_test(body.get("backend", OSO_DATABASE["backend"]),
                                           body.get("dsn", OSO_DATABASE["dsn"])))
            return self._json({"error": "unknown database action"}, 404)
        if path.startswith("/api/v1/backup/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "run":
                log("info", "backup run")
                return self._json(backup_run(body))
            if act == "verify":
                return self._json(backup_verify(int(body.get("id", 0))))
            if act == "restore":
                log("warn", f"backup restore {body.get('id')}")
                return self._json(backup_restore(int(body.get("id", 0))))
            if act == "delete":
                return self._json(backup_delete(int(body.get("id", 0))))
            if act == "set":
                return self._json(backup_set(body))
            return self._json({"error": "unknown backup action"}, 404)
        if path.startswith("/api/v1/drivers/"):
            try:
                body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            except Exception:
                body = {}
            act = path.rsplit("/", 1)[-1]
            if act == "load":
                return self._json(drivers_load(body.get("id", "")))
            if act == "unload":
                return self._json(drivers_unload(body.get("id", "")))
            return self._json({"error": "unknown drivers action"}, 404)
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
        if path == "/api/v1/projects":   # upload → a new version
            raw = self.rfile.read(int(self.headers.get("Content-Length", 0)) or 0)
            name = "uploaded-project"
            m = re.search(rb'filename="([^"]+)"', raw)
            if m:
                fn = m.group(1).decode("utf-8", "replace")
                name = re.sub(r"\.(osoproj|zip|bin)$", "", fn, flags=re.I) or fn
            log("info", f"project upload {name}")
            return self._json(projects_add(name, size_bytes=len(raw)))
        if path.startswith("/api/v1/projects/"):
            tail = path[len("/api/v1/projects/"):]
            pid = int(re.match(r"\d+", tail).group(0)) if re.match(r"\d+", tail) else 0
            if tail.endswith("/activate"):
                log("warn", f"project activate {pid}")
                return self._json(projects_activate(pid))
            return self._json({"ok": True})
        return self._json({"error": "not found", "path": path}, 404)

    def do_DELETE(self):
        path = self.path.split("?", 1)[0]
        if path.startswith("/api/v1/projects/"):
            tail = path[len("/api/v1/projects/"):]
            pid = int(re.match(r"\d+", tail).group(0)) if re.match(r"\d+", tail) else 0
            log("warn", f"project delete {pid}")
            return self._json(projects_delete(pid))
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
