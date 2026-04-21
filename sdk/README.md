# sdk/ — OSOlogic® Developer SDKs

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · Apache-2.0

---

Developer SDKs for integrating with the OSOlogic platform from external applications.

Each SDK wraps the OSOlogic REST and WebSocket APIs in an idiomatic client library for its target language and runtime. All SDKs provide the same core capabilities: reading and writing process variables, subscribing to real-time updates, managing alarms, and executing control commands.

## Directory Structure

```
sdk/
├── c/              # C SDK (libosologic) — embedded and system integrations
├── cpp/            # C++ SDK — modern C++17, suitable for HMI and edge apps
├── dotnet/         # .NET SDK (NuGet) — Windows SCADA, WPF, MAUI clients
├── php/            # PHP SDK — web dashboards and intranet portals
├── python/         # Python SDK — data science, scripting, testing
├── node/           # Node.js SDK — web backends, Node-RED, serverless
└── examples/       # Usage examples for all SDKs
```

### `c/`
`libosologic` — C library providing low-level access to `osodb` and `osoruntime` via shared memory (local) or the REST/WebSocket API (remote). Intended for embedded system integrations and performance-critical applications.

### `cpp/`
Modern C++17 SDK. Wraps `libosologic` with RAII resource management, async/await (via coroutines), and type-safe tag access. Suitable for HMI applications, edge analytics, and native desktop tools.

### `dotnet/`
.NET SDK distributed as a NuGet package. Provides async/await access to all OSOlogic APIs. Targeted at Windows SCADA integrations, WPF dashboards, and cross-platform MAUI clients.

### `php/`
PHP SDK for web-based dashboards, intranet portals, and lightweight automation web applications. Supports Composer installation.

### `python/`
Python SDK for scripting, data analysis, testing, and integration. Works with standard Python tooling (pip, virtual environments). Supports both sync and async (asyncio) usage patterns.

### `node/`
Node.js SDK (ESM and CommonJS). Used for web backends, Node-RED custom nodes, serverless functions, and TypeScript applications. Distributed via npm.

### `examples/`
Annotated usage examples for each SDK: connecting to osodb, reading process variables, writing outputs, subscribing to real-time events, and handling alarms.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
