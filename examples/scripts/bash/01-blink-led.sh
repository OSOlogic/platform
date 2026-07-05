#!/usr/bin/env bash
# Blink LED — bash + curl. Toggle an osodb tag once per second.
BASE="${OSO_URL:-http://127.0.0.1:8080}"; TAG="${OSO_TAG:-hass.switch.led}"; s=0
while true; do
  s=$((1-s))
  curl -fsS -X PUT -H 'Content-Type: application/json' -d "{\"value\":$s}" "$BASE/var/$TAG" >/dev/null
  echo "$TAG = $s"; sleep 1
done
