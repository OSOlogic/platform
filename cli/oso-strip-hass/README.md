# oso-strip-hass — Home Assistant integration → OSOLogic driver candidates

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

Implements the *strip* process of [RFC 0002](../../standard/rfcs/0002-osologic-driver-model.md): sweep
HA integration `manifest.json` files, classify by **transport** (inferred from `requirements` +
discovery hints) and **iot_class**, score the **easily mappable** ones (local + standard transport:
MQTT · Modbus · REST · CoAP · zeroconf-local) → native OSOLogic driver candidates; the rest stay
bridged via the [compatibility gateway](../../gateways/home-assistant/).

```bash
python3 strip.py --samples                 # classify the bundled samples
python3 strip.py --samples --out ./out     # + write candidate driver.json stubs
python3 strip.py --path /path/to/homeassistant/components   # a real HA tree
```

Reads manifests only — it never imports Home Assistant. Output stubs are `kind: bridge-adapter`
candidates to be reviewed and completed with a `map.json` (entity→tag). HA is Apache-2.0 →
one-way compatible with our AGPL-3.0; keep upstream notices.
