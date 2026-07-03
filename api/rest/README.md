# api/rest — REST + WebSocket API

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — The Modern & Open Automation Platform · AGPL-3.0

---

The `/api/v1` HTTP + WebSocket contract that the platform's web front-ends consume — the
**webmin-oso** Cockpit modules (I/O tags, gateways, runtime, projects) and the **osoLadder**
editor. It is backed by the in-memory hub (`osodb`) and the same reversible NodeId scheme used
across the platform: `ns=2;s=<module_id>.<io_definition_id>`.

## Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/api/v1/tags` | List tags `{id, name, group, device, type, access, value, units, quality, ts_us}` |
| GET | `/api/v1/tags/{id}` | Read one tag |
| PUT | `/api/v1/tags/{id}` | Write a tag value `{value}` |
| GET | `/api/v1/devices` | List devices |
| GET | `/api/v1/gateways` | List gateways |
| POST | `/api/v1/gateways` | Add a gateway |
| PUT/DELETE | `/api/v1/gateways/{id}` | Update / remove a gateway |
| POST | `/api/v1/gateways/{id}/restart` | Restart a gateway |
| GET | `/api/v1/runtime/status` | Scan-cycle and health |
| POST | `/api/v1/runtime/{action}` | `start` \| `stop` \| `reset` |
| GET | `/api/v1/projects` | List IEC 61131-3 projects |
| POST | `/api/v1/projects` | Upload a project |
| POST | `/api/v1/projects/{id}/activate` | Activate a project |
| DELETE | `/api/v1/projects/{id}` | Remove a project |
| GET/PUT | `/api/v1/var/{name}` | Read/write a tag by name (osoLadder compatibility) |
| GET | `/api/v1/status` | Runtime status (osoLadder compatibility) |
| WS | `/ws` | Live tag updates |

Tag `type` uses IEC 61131-3 names (`BOOL`, `WORD`, `DWORD`, `INT`, `DINT`, `REAL`, `LREAL`, …),
mapped from the hub's OPC-UA data types.

### WebSocket messages

Client subscribes with `{"type":"subscribe","tags":["*"]}`. The server pushes:

```json
{ "type": "tag_update", "tag": "WDT_Timeout", "value": 2500, "ts_us": 1783109942730725 }
{ "type": "tag_batch",  "updates": [ { "tag": "Output_1", "value": true, "ts_us": ... } ] }
```

## Reference implementation

[`reference/osologic_rest_server.py`](reference/osologic_rest_server.py) implements the full
contract on FastAPI, backed by an in-memory example hub (replace with the `osodb` adapter in a
deployment). It powers the Cockpit modules under [`ui/webmin-oso`](../../ui/webmin-oso/) and the
[`osoLadder`](../../iec61131/ladder/osoLadder/) editor without changes.

```bash
pip install "fastapi" "uvicorn[standard]"
python reference/osologic_rest_server.py     # http://0.0.0.0:8080  (docs at /docs)
```

Point a front-end at the server:

```bash
# webmin-oso Cockpit modules call BASE = /api/v1 on the same host
# osoLadder: set the REST base URL to http://<host>:8080/api/v1
```

## Status

Reference implementation of the Community Edition API. Authentication, fine-grained access
control, and historical/aggregated queries are part of the OSOlogic Enterprise add-ons.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
