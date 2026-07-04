#!/usr/bin/env python3
# ============================================================
# OSOLogic — Home Assistant discovery crawler + sniffer (prototype)
#
# Crawl a Home Assistant instance to enumerate everything it exposes, then
# auto-generate a bridge mapping — so you don't hand-write which entities to
# mirror into osodb. Also a live "sniffer" that tails state_changed events.
#
#   crawl : GET /api/states + /api/services -> summary + mapping.json
#   sniff : WebSocket subscribe state_changed -> live event tail
#
# Env: HASS_URL, HASS_TOKEN (long-lived access token).
#
#   python3 hass_discover.py                      # crawl + print summary
#   python3 hass_discover.py --out mapping.json   # + write a bridge mapping
#   python3 hass_discover.py --sniff              # live event tail
#
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
import argparse
import asyncio
import json
import os
from collections import defaultdict

import aiohttp  # pip install aiohttp

HASS_URL   = os.environ.get("HASS_URL", "http://homeassistant.local:8123").rstrip("/")
HASS_TOKEN = os.environ.get("HASS_TOKEN", "")

# Domains that are typically writable, and the service that toggles them.
WRITABLE_HINTS = {"light", "switch", "fan", "cover", "lock", "climate", "media_player",
                  "input_boolean", "input_number", "select", "number", "button", "scene"}


def headers():
    return {"Authorization": f"Bearer {HASS_TOKEN}", "Content-Type": "application/json"}


async def crawl(out_path):
    async with aiohttp.ClientSession(headers=headers()) as s:
        async with s.get(f"{HASS_URL}/api/states") as r:
            r.raise_for_status()
            states = await r.json()
        services = []
        try:
            async with s.get(f"{HASS_URL}/api/services") as r:
                if r.status == 200:
                    services = await r.json()
        except aiohttp.ClientError:
            pass

    by_domain = defaultdict(list)
    units = {}
    for st in states:
        eid = st["entity_id"]
        domain = eid.split(".", 1)[0]
        by_domain[domain].append(st)
        u = (st.get("attributes") or {}).get("unit_of_measurement")
        if u:
            units[eid] = u

    svc_domains = {sv.get("domain") for sv in services}

    # ---- human summary ----
    print(f"Home Assistant @ {HASS_URL}")
    print(f"  {len(states)} entities across {len(by_domain)} domains, "
          f"{len(services)} service domains\n")
    print(f"  {'domain':<18}{'count':>6}   sample")
    print(f"  {'-'*18}{'-'*6}   {'-'*30}")
    for domain in sorted(by_domain, key=lambda d: -len(by_domain[d])):
        ents = by_domain[domain]
        writable = "  (writable)" if domain in svc_domains and domain in WRITABLE_HINTS else ""
        sample = ", ".join(e["entity_id"].split(".", 1)[1] for e in ents[:3])
        print(f"  {domain:<18}{len(ents):>6}   {sample}{writable}")

    # ---- generated mapping ----
    if out_path:
        writable = sorted(
            st["entity_id"] for st in states
            if st["entity_id"].split(".", 1)[0] in (svc_domains & WRITABLE_HINTS)
        )
        mapping = {
            "_generated_by": "hass_discover.py",
            "domains": sorted(by_domain),
            "units": units,
            "writable": writable,
            "rename": {},
        }
        with open(out_path, "w") as f:
            json.dump(mapping, f, indent=2, ensure_ascii=False)
        print(f"\nwrote {out_path}: {len(mapping['domains'])} domains, "
              f"{len(units)} with units, {len(writable)} writable entities")


async def sniff():
    ws_url = HASS_URL.replace("http", "ws", 1) + "/api/websocket"
    print(f"sniffing state_changed @ {ws_url}  (Ctrl-C to stop)\n")
    async with aiohttp.ClientSession() as s:
        async with s.ws_connect(ws_url) as ws:
            _id = 0
            async for msg in ws:
                data = json.loads(msg.data)
                t = data.get("type")
                if t == "auth_required":
                    await ws.send_json({"type": "auth", "access_token": HASS_TOKEN})
                elif t == "auth_ok":
                    _id += 1
                    await ws.send_json({"id": _id, "type": "subscribe_events",
                                        "event_type": "state_changed"})
                elif t == "auth_invalid":
                    print("auth failed — check HASS_TOKEN")
                    return
                elif t == "event":
                    ev = data["event"]["data"]
                    old = (ev.get("old_state") or {}).get("state", "∅")
                    new = (ev.get("new_state") or {}).get("state", "∅")
                    if old != new:
                        print(f"  {ev['entity_id']:<40} {old!s:>12}  →  {new!s}")


def main():
    ap = argparse.ArgumentParser(description="Home Assistant discovery crawler + sniffer")
    ap.add_argument("--out", help="write a generated bridge mapping to this file")
    ap.add_argument("--sniff", action="store_true", help="live-tail state_changed events")
    args = ap.parse_args()
    if not HASS_TOKEN:
        raise SystemExit("set HASS_TOKEN (a Home Assistant long-lived access token)")
    asyncio.run(sniff() if args.sniff else crawl(args.out))


if __name__ == "__main__":
    main()
