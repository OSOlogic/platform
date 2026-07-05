#!/usr/bin/env bash
# Poll tags — REST. Read a set of tags every 2 s (a tiny dashboard in a terminal).
BASE="${OSO_URL:-http://127.0.0.1:8080}"
TAGS=(hass.sensor.tank_level hass.switch.pump hass.switch.led)
while true; do
  printf '%(%H:%M:%S)T  ' -1
  for t in "${TAGS[@]}"; do
    v=$(curl -fsS "$BASE/var/$t" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("value"))' 2>/dev/null)
    printf '%s=%s  ' "$t" "$v"
  done; echo
  sleep 2
done
