#!/usr/bin/env bash
# Blink LED — pure REST. The osodb hub exposes every tag at /var/<id>.
# (REST calls run like scripts through the runtime — the tag ACL still applies.)
BASE="${OSO_URL:-http://127.0.0.1:8080}"; TAG="${OSO_TAG:-hass.switch.led}"; s=0
while true; do
  s=$((1-s))
  # write the set-point
  curl -fsS -X PUT "$BASE/var/$TAG" -H 'Content-Type: application/json' -d "{\"value\":$s}" >/dev/null
  # read the current value
  curl -fsS "$BASE/var/$TAG"; echo
  sleep 1
done
