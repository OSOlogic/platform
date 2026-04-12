# OSOlogic WebSocket Protocol

Endpoint: `ws://{host}/ws`

Both frontends (Cockpit modules and embedded UI) use the same protocol.

---

## Connection

Connect to `ws://{host}/ws`. No subprotocol negotiation required.

On connect, the server sends a `hello` frame immediately:

```json
{ "type": "hello", "firmware": "1.0.0-alpha", "platform": "rp2040" }
```

---

## Frame format

All frames are JSON objects with a mandatory `type` field.

```json
{ "type": "<frame-type>", ...payload }
```

---

## Client → Server

### subscribe — subscribe to tag updates

```json
{ "type": "subscribe", "tags": ["PLC.Q0.0", "PLC.I0.1", "sensor.temp"] }
```

- Server will push `tag_update` frames for each listed tag whenever its value changes.
- Send an empty array to unsubscribe from all.
- Wildcards: `"PLC.*"` subscribes to all tags in the PLC namespace.

### write — write a tag value

```json
{ "type": "write", "tag": "PLC.Q0.0", "value": true }
```

Server replies with `ack` or `error`.

### ping

```json
{ "type": "ping" }
```

Server replies with `pong`. Use to keep connection alive (send every 10 s on embedded targets).

---

## Server → Client

### tag_update — tag value changed

```json
{
  "type": "tag_update",
  "tag":  "PLC.Q0.0",
  "value": true,
  "ts_us": 1234567890
}
```

Sent only for subscribed tags. Batching: the server MAY batch multiple updates in a single frame:

```json
{
  "type": "tag_batch",
  "updates": [
    { "tag": "PLC.Q0.0", "value": true,  "ts_us": 1234567890 },
    { "tag": "PLC.I0.1", "value": false, "ts_us": 1234567891 }
  ]
}
```

### runtime_event — scan cycle state change

```json
{ "type": "runtime_event", "state": "running", "cycle_count": 360100 }
```

Sent on any transition: stopped → running, running → error, etc.

### ack — write acknowledged

```json
{ "type": "ack", "tag": "PLC.Q0.0", "value": true }
```

### error — operation failed

```json
{ "type": "error", "code": "TAG_NOT_FOUND", "msg": "Tag PLC.Q9.9 does not exist" }
```

Error codes:

| Code              | Meaning                          |
|-------------------|----------------------------------|
| TAG_NOT_FOUND     | Tag name unknown                 |
| TAG_READ_ONLY     | Write attempted on read-only tag |
| TYPE_MISMATCH     | Value type does not match tag    |
| RUNTIME_STOPPED   | Write rejected: scan not running |
| UNAUTHORIZED      | Authentication required          |

### pong

```json
{ "type": "pong" }
```

---

## Embedded targets — constraints

On RP2040 / STM32:

- Max simultaneous subscriptions: **32 tags**
- Max frame size: **512 bytes** (fits in one TCP segment on lwIP default config)
- Batching interval: **50 ms** (flush buffer every scan cycle or 50 ms, whichever first)
- No compression (no zlib on baremetal)
- No TLS on RP2040 (add mbedTLS on STM32H7 if flash allows)
