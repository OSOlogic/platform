#!/usr/bin/env python3
# ============================================================
# OSOLogic — Home Assistant bridge (reference / prototype)
#
# Mirrors Home Assistant entities into osodb (via the osoLogic REST API) and
# turns osodb set-points into HA service calls — a compatibility gateway so a
# device HA already supports (Zigbee, Z-Wave, Matter, MQTT, ...) can appear as an
# OSOLogic tag. Uses HA's public API; complements HA, doesn't replace it.
#
# HA -> osodb : subscribe to state_changed over the HA WebSocket API, write each
#               entity's state/attributes to osodb tags `hass.<domain>.<id>[.<attr>]`.
# osodb -> HA : call HA services (light.turn_on, switch.turn_off, ...) to apply
#               commands (see apply_command()).
#
# Env: HASS_URL, HASS_TOKEN (long-lived), OSO_REST (osoLogic REST base).
#
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
import argparse
import asyncio
import json
import os

import aiohttp  # pip install aiohttp

HASS_URL   = os.environ.get("HASS_URL", "http://homeassistant.local:8123")
HASS_TOKEN = os.environ.get("HASS_TOKEN", "")
OSO_REST   = os.environ.get("OSO_REST", "http://localhost:8080/api").rstrip("/")


def load_mapping(path):
    if not path:
        return {"domains": None, "units": {}, "writable": [], "rename": {}}
    with open(path) as f:
        return json.load(f)


def tag_key(entity_id, attr=None):
    # light.kitchen -> hass.light.kitchen ; + .brightness for attributes
    return "hass." + entity_id + (("." + attr) if attr else "")


def coerce(state):
    """HA state string -> a typed value for osodb."""
    if state in ("on", "true", "home", "open", "unlocked"):
        return True
    if state in ("off", "false", "away", "closed", "locked"):
        return False
    try:
        f = float(state)
        return int(f) if f.is_integer() else f
    except (TypeError, ValueError):
        return state  # keep as string (e.g. "idle", "heat")


async def osodb_write(session, key, value):
    """Upsert a tag value into osodb through the osoLogic REST API."""
    try:
        async with session.put(f"{OSO_REST}/var/{key}", json={"value": value}) as r:
            if r.status >= 400:
                print(f"[osodb] PUT {key} -> HTTP {r.status}")
    except aiohttp.ClientError as e:
        print(f"[osodb] {key}: {e}")


def should_bridge(entity_id, mapping):
    domain = entity_id.split(".", 1)[0]
    doms = mapping.get("domains")
    return (doms is None) or (domain in doms)


async def mirror_state(session, mapping, entity_id, st):
    if not should_bridge(entity_id, mapping):
        return
    key = tag_key(mapping.get("rename", {}).get(entity_id, entity_id))
    await osodb_write(session, key, coerce(st.get("state")))
    # selected attributes as sibling tags
    for attr, val in (st.get("attributes") or {}).items():
        if isinstance(val, (int, float, bool, str)):
            await osodb_write(session, tag_key(entity_id, attr), val)


# osodb set-point -> HA service call. A minimal, extensible command map.
def command_for(entity_id, value):
    domain = entity_id.split(".", 1)[0]
    on = value in (True, 1, "1", "on", "true")
    table = {
        "light":  ("light.turn_on" if on else "light.turn_off"),
        "switch": ("switch.turn_on" if on else "switch.turn_off"),
        "fan":    ("fan.turn_on" if on else "fan.turn_off"),
        "cover":  ("cover.open_cover" if on else "cover.close_cover"),
        "lock":   ("lock.unlock" if on else "lock.lock"),
    }
    service = table.get(domain)
    return (service, {"entity_id": entity_id}) if service else (None, None)


async def apply_command(session, entity_id, value):
    service, data = command_for(entity_id, value)
    if not service:
        print(f"[hass] no command mapping for {entity_id}")
        return
    dom, svc = service.split(".", 1)
    url = f"{HASS_URL}/api/services/{dom}/{svc}"
    headers = {"Authorization": f"Bearer {HASS_TOKEN}"}
    async with session.post(url, headers=headers, json=data) as r:
        print(f"[hass] {service} {entity_id} -> HTTP {r.status}")


async def run(mapping):
    ws_url = HASS_URL.replace("http", "ws", 1) + "/api/websocket"
    async with aiohttp.ClientSession() as session:
        async with session.ws_connect(ws_url) as ws:
            _id = 0

            def nid():
                nonlocal _id
                _id += 1
                return _id

            async for msg in ws:
                data = json.loads(msg.data)
                mtype = data.get("type")

                if mtype == "auth_required":
                    await ws.send_json({"type": "auth", "access_token": HASS_TOKEN})

                elif mtype == "auth_ok":
                    print("[hass] authenticated — priming states + subscribing")
                    await ws.send_json({"id": nid(), "type": "get_states"})
                    await ws.send_json({"id": nid(), "type": "subscribe_events",
                                        "event_type": "state_changed"})

                elif mtype == "auth_invalid":
                    print("[hass] auth failed — check HASS_TOKEN")
                    return

                elif mtype == "result" and isinstance(data.get("result"), list):
                    for st in data["result"]:               # initial snapshot
                        await mirror_state(session, mapping, st["entity_id"], st)
                    print(f"[hass] primed {len(data['result'])} entities into osodb")

                elif mtype == "event":
                    ev = data["event"]["data"]
                    ns = ev.get("new_state")
                    if ns:
                        await mirror_state(session, mapping, ev["entity_id"], ns)


def main():
    ap = argparse.ArgumentParser(description="OSOLogic ↔ Home Assistant bridge (reference)")
    ap.add_argument("--map", help="mapping JSON (domains/units/writable/rename)")
    args = ap.parse_args()
    if not HASS_TOKEN:
        raise SystemExit("set HASS_TOKEN (a Home Assistant long-lived access token)")
    asyncio.run(run(load_mapping(args.map)))


if __name__ == "__main__":
    main()
