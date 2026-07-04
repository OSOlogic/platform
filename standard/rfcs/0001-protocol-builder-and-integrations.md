# RFC 0001 — Protocol & Integrations Manager

**Status:** Draft · **Author:** José Roig Borrell · **© 2026 Roig Borrell S.L. · Ibercomp S.L.** · AGPL-3.0-or-later

> Design capture. Not yet implemented — this RFC defines the model so we can build it next.

## Summary

A single place in **osowebmin** — the **Protocol & Integrations Manager** — to connect OSOlogic to
things it doesn't have a native driver for, in two complementary ways:

1. **Integration adapters** — bring third-party ecosystems (starting with **Home Assistant**
   integrations) into OSOlogic as tags.
2. **Custom protocol builder** — define *any* serial or network protocol declaratively (a web editor
   **and** a JSON/XML file), with no code for the common cases and an optional per-frame script for
   the exotic ones.

Everything it connects lands in [`osodb`](../../core/osodb) as tags, like every other source.

## Motivation

Industrial reality is a long tail of RS-232/RS-485 and ad-hoc TCP/IP devices with bespoke framing and
CRCs — scales, drives, meters, controllers — that no standard driver covers. Today each one needs C
code. Instead, let an engineer **describe** the protocol (polling commands, byte layout, bit
decoding, CRC) in a form or a file, and OSOlogic speaks it.

**Scope / non-goals.** A custom protocol is for **low-cadence polling and request/response** (seconds,
not microseconds). It is explicitly **not** for hard real-time or ultra-low-latency control — those
stay on native drivers. But it is invaluable for talking to devices that otherwise couldn't be reached
at all.

## Part A — Integrations: **bridge** vs. **import** (two distinct things)

These are complementary and must not be conflated:

### A1 · Runtime bridge — *talk to* a live instance (built)

Speak to a **running** Home Assistant over its public API; HA keeps running and does the device work.
Instant, zero-port — but depends on a separate HA process. This is the shipped
[compatibility gateway](../../gateways/home-assistant/).

### A2 · Import integrations — *bring them home* (the goal here)

Adapt an integration so it runs **natively inside OSOlogic**, with **no HA process** — we bring the
code/knowledge over. Two acceptable paths:

- **Systematic (tooled)** — a converter/adapter that reads an integration's structure
  (`manifest.json`, its entity platforms, config flow, its Python client lib) and generates an
  **OSOlogic integration skeleton + a driver mapping**, semi-automating the port. Do the long tail
  this way.
- **Manual** — hand-port high-value integrations straight against the osodb hub.

Both paths reuse the [driver library](../../gateways/home-assistant/drivers/) (domain → osodb tag) and
the [mapper](../../gateways/home-assistant/reference/hass_mapper.py); the manager lets you choose
**bridge or import per integration**.

**Licensing (why we can).** Home Assistant core and most integrations are **Apache-2.0**, which is
one-way compatible with our **AGPL-3.0** core — adapting/importing is permitted provided we **keep the
notices and attribution** (upstream `LICENSE`/`NOTICE`, credit the authors). Integrations that pull
GPL/proprietary vendor libraries are handled case by case. This is *bringing open code home, with
credit* — not repackaging someone's running product.

## Part B — Custom protocol builder

### Model

A protocol definition is a document (JSON or XML; the web editor is a view over it):

```
protocol
├── transport      serial (RS-232/485) | tcp | udp  — port/baud/parity | host/port
├── framing        delimiter | fixed-length | length-prefixed | silence-gap (Modbus-style)
├── crc            modbus16 | crc8 | xor | checksum-sum | none | script   (poly/init/reflect/order)
├── requests[]     polling read commands: tx frame template + rx field map + poll interval
├── writes[]       set-point commands: tx frame template with a tag-bound field
└── hooks          optional per-frame script (custom CRC / decode) — sandboxed
```

### Field mapping (bytes → values → bits)

Each rx field pulls a value out of the response frame and writes it to an osodb tag:

- **offset / size / type** — `uint8/16/32`, `int*`, `float32`, `bcd`, `ascii`, `bits`
- **endian** — big / little (and word order for 32-bit)
- **scale / offset / units** — engineering conversion (`raw × 0.1 → %`)
- **bits** — decode a byte/word into individual boolean tags by bit position (status words)

### CRC

Selectable algorithm with parameters (polynomial, init, reflect in/out, append order). When a device
uses something non-standard, set `crc.type = "script"` and point at a small **sandboxed script** that
receives the frame bytes and returns the check value — the escape hatch for truly custom framing.

### Per-frame script hook

Optional `hooks.on_frame` script runs on each received frame *before* field mapping — for oddball
decoding, variable-length payloads, or vendor quirks. Sandboxed, time-boxed, low-cadence only.

### Worked example

See [`../schema/json/examples/protocol-definition.example.json`](../schema/json/examples/protocol-definition.example.json)
— a custom RS-485 tank controller: a 1 s poll reads a `uint16` level (scaled to %) and a status byte
decoded into `high`/`low`/`fault` bit tags, with a Modbus CRC-16; plus a valve set-point write.

## Where it lives

- **UI** — `ui/webmin-oso` → *Protocol & Integrations Manager*: list/add integrations; a form-based
  **protocol editor** (transport, framing, CRC, request/field builder with a live frame preview) that
  reads/writes the same JSON/XML.
- **Runtime** — a `gateways/custom-protocol` engine executes definitions on a low-rate scheduler and
  maps results to osodb; native drivers keep the fast path.
- **Storage** — definitions live as files (versionable) and/or in the DB, like any other config.

## Open questions

- Script sandbox: language & limits (JS via a small VM? WASM? restricted Python?).
- Definition schema versioning and a shared library of community protocol definitions (`contrib/`).
- How the manager surfaces "native vs custom vs integration" — ties into the
  [connectivity matrix](../../docs/connectivity.md).
