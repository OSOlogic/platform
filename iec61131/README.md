# iec61131/ — OSOlogic® IEC 61131-3 Language Engines

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · AGPL-3.0

---

Language engines for all five IEC 61131-3 programming languages — compiled and interpreted.

These engines parse, compile, and execute PLC programs written by users in standard industrial languages. All engines integrate with `osoruntime` for scan-cycle execution and share process data through `osodb`.

> **Note:** The legacy `IEC 61131-3/` directory at the repo root is archived. All active code lives here.

## Directory Structure

```
iec61131/
├── ladder/         # Ladder Diagram (LD) engine — osoLadder
├── st/             # Structured Text (ST) compiler and interpreter
├── fbd/            # Function Block Diagram (FBD) engine
├── sfc/            # Sequential Function Chart (SFC) engine
├── il/             # Instruction List (IL) interpreter (legacy support)
├── runtime-bridge/ # Interface between language engines and osoruntime
└── tests/          # IEC 61131-3 conformance and language-specific tests
```

### `ladder/`
The **osoLadder** engine — the primary OSOlogic Ladder Diagram implementation. Supports contacts, coils, timers, counters, comparison and math blocks, and custom function blocks. Compiles to an internal IL bytecode executed by `osoruntime`.

### `st/`
Structured Text compiler and interpreter. Supports the full IEC 61131-3 ST grammar: variables, expressions, control flow (IF, CASE, FOR, WHILE, REPEAT), functions, and function blocks.

### `fbd/`
Function Block Diagram engine. Allows graphical dataflow-style programming using standard and custom function blocks. Internally compiled to the same execution model as Ladder and ST.

### `sfc/`
Sequential Function Chart engine for implementing state-machine-style programs. Manages steps, transitions, and actions with full IEC 61131-3 SFC semantics.

### `il/`
Instruction List interpreter for legacy program support. IL is officially deprecated in IEC 61131-3 ed. 3, but retained here for compatibility with existing automation code.

### `runtime-bridge/`
The interface layer between all language engines and `osoruntime`. Handles program instantiation, variable binding to `osodb` tags, and the execution callback interface used by the scan cycle.

### `tests/`
Conformance tests, language-specific unit tests, and regression suites for all five language engines.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
