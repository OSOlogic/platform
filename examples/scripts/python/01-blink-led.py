#!/usr/bin/env python3
"""Blink LED — Python. Toggle an osodb tag once per second over REST."""
import os, time, json, urllib.request

BASE = os.environ.get("OSO_URL", "http://127.0.0.1:8080")
TAG  = os.environ.get("OSO_TAG", "hass.switch.led")

def put(tag, value):
    req = urllib.request.Request(f"{BASE}/var/{tag}", method="PUT",
                                 data=json.dumps({"value": value}).encode(),
                                 headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=3).read()

state = 0
while True:
    state ^= 1                      # flip 0/1
    put(TAG, state)
    print(f"{TAG} = {state}")
    time.sleep(1)
