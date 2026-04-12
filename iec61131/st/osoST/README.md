<div align="center">
  <img src="../../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>osoST</h1>
  <p><strong>IEC 61131-3 Structured Text toolchain — editor, compiler, and P-code runtime</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-Structured_Text-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_A.M._Zúñiga_·_J._Roig_Borrell-Roig_Borrell_S.L._·_Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Project structure / Estructura del proyecto

```
iec61131/st/osoST/
├── editor/             ← Monaco-based web editor (no build step)
│   ├── index.html
│   ├── osost.css
│   └── osost.js
│
├── compiler-java/      ← Java backend (STLite.jar) + Flask REST wrapper
│   ├── server.py       #   POST /compile  /lex  /parse  GET /health /version
│   ├── compile.sh
│   ├── requirements.txt
│   └── README.md
│
├── compiler-python/    ← Pure-Python ST compiler (no Java, no dependencies)
│   └── ostc/
│       ├── tokens.py   #   keyword table
│       ├── lexer.py    #   tokenizer
│       ├── parser.py   #   recursive-descent parser → AST
│       ├── ast_nodes.py#   frozen dataclass AST nodes
│       ├── codegen.py  #   AST → P-code + Intel HEX
│       ├── hex_writer.py#  Intel HEX emitter with STLite header
│       └── cli.py      #   python -m ostc <file.st>
│
├── runtime/            ← P-code virtual machine in C99
│   ├── pcodevm.h       #   VM struct, opcodes, public API
│   ├── pcodevm.c       #   stack-machine interpreter
│   ├── osoruntime.c    #   main() + scan cycle + HEX loader
│   ├── hardware_linux.c#   HAL: GPIO (libgpiod), Modbus TCP (libmodbus)
│   ├── hardware_bare.c #   HAL template: STM32 / RP2040 / ESP32
│   ├── hardware_demo.c #   demo HAL (no real hardware needed)
│   ├── Makefile
│   └── README.md
│
└── examples/
    ├── blink.st        ← LED blink via GPIO trap
    ├── counter.st      ← up counter with IF/ELSE
    └── pid.st          ← discrete PID controller
```

---

## Quick start / Inicio rápido

### Web editor / Editor web

```bash
python3 -m http.server 8080 --directory osoST/editor/
# Open http://localhost:8080
```

### Java compiler REST server

```bash
cd compiler-java/
pip install flask
# Copy STLite.jar to this directory first
python server.py --port 8090
```

### Python compiler (CLI)

```bash
cd compiler-python/
python -m ostc examples/counter.st --lex    # tokenise
python -m ostc examples/counter.st --ast    # print AST
python -m ostc examples/counter.st --asm    # P-code disassembly
python -m ostc examples/counter.st          # compile → counter.hex
```

No third-party dependencies. Python 3.10+.

### Runtime (Linux)

```bash
cd runtime/ && make
./osoruntime counter.hex --scan=10 --debug
```

---

## Compiler backends / Backends de compilación

| Backend        | Language | Requires | Status      |
|----------------|----------|----------|-------------|
| STLite.jar     | Java     | JDK 11+  | Production  |
| ostc (Python)  | Python   | None     | Beta        |

Both produce compatible Intel HEX files for `pcodevm.c`.

---

## Deployment targets / Destinos de despliegue

| Target     | HAL file             | Scan cycle            |
|------------|----------------------|-----------------------|
| Linux ARM  | `hardware_linux.c`   | `timerfd_create`      |
| Linux x64  | `hardware_linux.c`   | `timerfd_create`      |
| STM32      | `hardware_bare.c`    | Timer ISR / RTOS      |
| RP2040     | `hardware_bare.c`    | Timer IRQ             |
| ESP32      | `hardware_bare.c`    | FreeRTOS task         |

---

## Architecture / Arquitectura

```
┌──────────────────────────────────────────────────────────┐
│  Browser / CLI                                           │
│                                                          │
│  osoST editor (editor/)     ← this folder               │
│    └── compiles via ──────────────────────────────┐     │
│                                                    ▼     │
│              compiler-java/server.py               │     │
│              compiler-python/ostc                  │     │
│                                                    │     │
│    └── produces ──────────────────────────────┐   │     │
│                                                ▼   │     │
│              Intel HEX (.hex)                  │   │     │
│                                                │   │     │
│    └── runs on ───────────────────────────┐   │   │     │
│                                            ▼   │   │     │
│              runtime/osoruntime            │   │   │     │
│              runtime/pcodevm.c             │   │   │     │
│                                            │   │   │     │
│    └── hardware via ──────────────────┐   │   │   │     │
│                                        ▼   │   │   │     │
│              hardware_linux.c          │   │   │   │     │
│              hardware_bare.c           │   │   │   │     │
└──────────────────────────────────────────────────────────┘
```

---

## Related / Relacionado

- [`iec61131/ladder/osoLadder/`](../ladder/osoLadder/) — Ladder Diagram editor
- [`ui/st-editor/`](../../../ui/st-editor/) — webmin-oso integration layer for osoST
- [`core/osoruntime/`](../../../core/osoruntime/) — real-time scan cycle engine
- [`api/openapi/osologic-admin-api.yaml`](../../../api/openapi/osologic-admin-api.yaml) — REST API contract

---

<div align="center">
  <sub>(C) Angel Miguel Zúñiga Schmemund &lt;miguel@ibercomp.com&gt; · Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
