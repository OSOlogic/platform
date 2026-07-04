#!/usr/bin/env python3
# ============================================================
# oso-strip-hass — sweep Home Assistant integration manifests, classify by
# transport/iot_class, score the easily-mappable ones and emit candidate
# OSOLogic driver stubs (RFC 0002). Reads manifest.json files; no HA import.
#
#   python3 strip.py --path <ha>/homeassistant/components   # a real HA tree
#   python3 strip.py --samples                               # the bundled samples
#   python3 strip.py --samples --out ./out                  # + write driver stubs
#
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
import argparse
import glob
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# requirement substring -> OSOLogic transport we already have (or can declare via RFC 0001)
LIB_TRANSPORT = [
    ("paho-mqtt", "mqtt"), ("aiomqtt", "mqtt"),
    ("pymodbus", "modbus"),
    ("aiocoap", "coap"),
    ("aiohttp", "rest/http"), ("requests", "rest/http"), ("jsonpath", "rest/http"), ("xmltodict", "rest/http"),
]
LOCAL = {"local_push", "local_polling"}
STD_TRANSPORTS = {"mqtt", "modbus", "rest/http", "coap", "zeroconf-local"}


def classify(m):
    reqs = [r.lower() for r in m.get("requirements", [])]
    transport = None
    if "mqtt" in m:
        transport = "mqtt"
    for lib, t in LIB_TRANSPORT:
        if transport:
            break
        if any(lib in r for r in reqs):
            transport = t
    if not transport and ("zeroconf" in m or "ssdp" in m or "dhcp" in m):
        transport = "zeroconf-local"
    iot = m.get("iot_class", "unknown")
    local = iot in LOCAL
    easy = local and transport in STD_TRANSPORTS
    reason = []
    reason.append("local" if local else f"iot_class={iot}")
    reason.append(f"transport={transport or 'unknown'}")
    if not easy:
        if not local:
            reason.append("→ bridge (cloud/assumed)")
        elif transport not in STD_TRANSPORTS:
            reason.append("→ bridge (non-standard transport)")
    return {"domain": m.get("domain"), "name": m.get("name", m.get("domain")),
            "iot_class": iot, "transport": transport, "local": local,
            "easy": easy, "reason": " · ".join(reason)}


def driver_stub(c):
    return {
        "id": f"hass-{c['domain']}", "name": c["name"], "version": "0.1.0-candidate",
        "kind": "bridge-adapter", "transport": c["transport"] or "bridge",
        "cadence": "low", "source": "home-assistant", "domain": c["domain"],
        "note": "generated candidate — review + attach map.json (RFC 0002)",
        "license": "AGPL-3.0-or-later",
    }


def main():
    ap = argparse.ArgumentParser(description="Strip HA integrations into OSOLogic driver candidates")
    ap.add_argument("--path", help="HA components dir (contains <domain>/manifest.json)")
    ap.add_argument("--samples", action="store_true", help="use the bundled samples/")
    ap.add_argument("--out", help="write candidate driver.json stubs here")
    args = ap.parse_args()

    root = os.path.join(HERE, "samples") if args.samples else args.path
    if not root:
        ap.error("give --path <ha components> or --samples")
    files = glob.glob(os.path.join(root, "*", "manifest.json"))
    results = [classify(json.load(open(f))) for f in files]
    results.sort(key=lambda c: (not c["easy"], c["domain"]))

    easy = [c for c in results if c["easy"]]
    print(f"\nSwept {len(results)} integrations — {len(easy)} easily mappable → native driver, "
          f"{len(results) - len(easy)} stay bridged\n")
    print(f"  {'STATUS':10} {'DOMAIN':14} {'TRANSPORT':16} {'IOT_CLASS':14} REASON")
    for c in results:
        print(f"  {'✓ native' if c['easy'] else '· bridged':10} {c['domain']:14} "
              f"{str(c['transport']):16} {c['iot_class']:14} {c['reason']}")

    if args.out:
        os.makedirs(args.out, exist_ok=True)
        for c in easy:
            d = os.path.join(args.out, f"hass-{c['domain']}")
            os.makedirs(d, exist_ok=True)
            json.dump(driver_stub(c), open(os.path.join(d, "driver.json"), "w"), indent=2)
        print(f"\n  wrote {len(easy)} candidate driver stubs to {args.out}/")
    print()


if __name__ == "__main__":
    main()
