#!/usr/bin/env python3
"""Tank control — Python. Read a level tag, drive a pump with hysteresis."""
import os, time, json, urllib.request
BASE = os.environ.get("OSO_URL", "http://127.0.0.1:8080")
LEVEL, PUMP = "hass.sensor.tank_level", "hass.switch.pump"
LOW, HIGH = 20.0, 80.0

def get(tag):
    with urllib.request.urlopen(f"{BASE}/var/{tag}", timeout=3) as r:
        return float(json.load(r).get("value", 0))
def put(tag, v):
    req = urllib.request.Request(f"{BASE}/var/{tag}", method="PUT",
          data=json.dumps({"value": v}).encode(), headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=3).read()

pump = 0
while True:
    lvl = get(LEVEL)
    if lvl <= LOW:  pump = 1
    elif lvl >= HIGH: pump = 0
    put(PUMP, pump)
    print(f"level={lvl:.0f}%  pump={'ON' if pump else 'off'}")
    time.sleep(2)
