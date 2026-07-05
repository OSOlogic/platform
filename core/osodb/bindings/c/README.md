# osodb — C binding

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0-or-later

A thin `extern "C"` API over the C++ [`osodb`](../../) hub (`MemoryHub`), so C code — notably the
IEC 61131-3 runtime HAL ([`iec61131/st/osoST/runtime`](../../../../iec61131/st/osoST/runtime/)) — can
read/write tags **natively and in-process** (nanosecond) instead of over REST. osodb owns the
write-through / write-back sync to **MariaDB**, so callers never touch the database directly.

## API (`osodb_c.h`)

| Function | Meaning |
|---|---|
| `osodb_c_read(tag_id, &out)` | current value (scan **input** image) |
| `osodb_c_write(tag_id, raw)` | current value (scan **output** image) — **ACL: ReadOnly tags rejected** |
| `osodb_c_take_required(tag_id, &out)` | one pending set-point (external control, e.g. SQL `UPDATE required_value`) |
| `osodb_c_define(tag_id, access)` | declare a tag (`0`=ReadOnly, `1`=ReadWrite) |

`tag_id` is osodb's canonical `"<module_id>.<io_definition_id>"` (e.g. `"1.5"`), matching
`TagKey::to_string()`. The **ACL is osodb's own per-tag `Access`** — this is the same permission the
gateways honour, so the PLC, SQL, OPC-UA, MQTT and REST all agree.

## Build / test

```sh
g++ -std=c++17 -I. osodb_c.cpp ../../src/osodb.cpp your_prog.c -o your_prog
```

The runtime HAL selects this path with `-DUSE_OSODB_NATIVE` (see the runtime `osodb_tags.h`),
falling back to the REST bridge (`-DUSE_OSODB`, libcurl) or an in-memory reference store otherwise.

Verified: RW round-trip (`write 42 → read 42`), ACL denies a write to a ReadOnly tag, malformed ids
rejected.
