# api/ — OSOlogic® API Layer

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · AGPL-3.0

---

REST, GraphQL, WebSocket, gRPC, MCP and OpenAPI endpoints exposed by the platform.

All interfaces here sit on top of `osodb` and `osoruntime`, providing access to process variables, alarms, device configuration, and control operations from external clients, UIs, and AI agents.

## Directory Structure

```
api/
├── rest/           # RESTful HTTP endpoints (process data, config, alarms)
├── graphql/        # GraphQL schema and resolvers for flexible querying
├── websocket/      # WebSocket server for real-time streaming of process data
├── grpc/           # gRPC service definitions and generated stubs
├── mcp/            # Model Context Protocol integration (AI/LLM tooling)
└── openapi/        # OpenAPI 3.x specifications and generated docs
```

### `rest/`
Standard RESTful HTTP API for reading and writing process variables, managing devices, handling alarms, and system configuration. Primary integration point for SCADA, MES, and third-party applications.

### `graphql/`
GraphQL interface allowing clients to query exactly the data they need. Useful for dashboards, mobile clients, and any UI consuming heterogeneous process data.

### `websocket/`
Real-time push API over WebSocket. Clients subscribe to process variables or alarms and receive updates as they change in `osodb`. Used by the HMI and dashboard UIs.

### `grpc/`
High-performance binary RPC interface using Protocol Buffers. Intended for inter-service communication and high-throughput integrations (edge analytics, cloud pipelines).

### `mcp/`
Model Context Protocol server exposing OSOlogic tools and resources to AI/LLM agents. Enables natural-language interaction with the automation platform.

### `openapi/`
OpenAPI 3.x specifications for the REST API. Used to generate client SDKs, interactive documentation (Swagger UI), and validation schemas.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
