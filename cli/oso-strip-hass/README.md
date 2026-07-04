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

## Why this is public (licensing & transparency)

This tool reads only public integration **metadata** (`manifest.json`) and produces OSOLogic driver
*candidates* — it does **not** import, copy or vendor Home Assistant code. HA core is **Apache-2.0**,
one-way compatible with our **AGPL-3.0**; reading metadata and generating our own drivers is
interoperability/analysis, not a derivative work. Keeping it open is the cleaner, more honest
position: we interoperate with and **credit** the project, and hide nothing.

> **Deeper porting** — bringing an integration's actual client code into a native driver — is handled
> **per integration**: keep the upstream Apache-2.0 `LICENSE`/`NOTICE` and attribution, and review any
> integration that pulls GPL/proprietary vendor libraries case by case. That review step is where
> caution belongs — not in this manifest-level sweep.
