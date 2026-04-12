# osoST — IEC 61131-3 Structured Text for OsoLogic®

> Part of the **OsoLogic® Open Industrial Automation Platform** — [osologic.org](https://osologic.org)

**osoST** is the Structured Text (ST) sub-project of OsoLogic®.
It provides a complete, open-source toolchain for writing, editing, compiling, and running
IEC 61131-3 ST programs on osoLogic PLCs — from bare-metal MCUs to Linux ARM/x64 systems.

**osoST** es el subproyecto de Texto Estructurado (ST) de OsoLogic®.
Proporciona una cadena de herramientas completa y de código abierto para escribir, editar,
compilar y ejecutar programas IEC 61131-3 ST en PLCs osoLogic — desde MCUs bare-metal
hasta sistemas Linux ARM/x64.

---

## Project structure / Estructura del proyecto

```
osoST/
├── compiler-java/      # Java backend (STLite.jar) + Flask REST wrapper
│   ├── STLite.jar      # (copy here — not included in repo)
│   ├── server.py       # REST API: POST /compile  /lex  /parse  GET /health /version
│   ├── compile.sh      # CLI wrapper for quick testing
│   ├── requirements.txt
│   └── README.md
│
├── compiler-python/    # Pure-Python ST compiler (no Java, no dependencies)
│   ├── ostc/
│   │   ├── tokens.py   # Keyword table and token name list
│   │   ├── lexer.py    # Hand-written lexer → Token list
│   │   ├── parser.py   # Recursive-descent parser → AST
│   │   ├── ast_nodes.py# Frozen dataclass AST node definitions
│   │   ├── codegen.py  # AST → P-code + Intel HEX
│   │   ├── hex_writer.py # Intel HEX emitter with STLite header
│   │   └── cli.py      # python -m ostc <file.st>
│   ├── requirements.txt
│   └── README.md
│
├── editor/             # Monaco-based web editor for ST
│   ├── index.html      # Single-page app (no build step required)
│   ├── osost.css       # Styles following OsoLogic design system
│   └── osost.js        # Editor logic, compile/lex/parse REST calls
│
├── runtime/            # P-code virtual machine in C99
│   ├── pcodevm.h       # VM struct, opcode enum, public API
│   ├── pcodevm.c       # Stack-machine interpreter
│   ├── osoruntime.c    # main() + scan cycle + Intel HEX loader
│   ├── hardware_linux.c# HAL for Linux (GPIO via libgpiod, Modbus TCP)
│   ├── hardware_bare.c # HAL template for STM32 / RP2040 / ESP32
│   ├── hardware_demo.c # Demo HAL (Mandelbrot, BACnet stub)
│   ├── Makefile
│   └── README.md
│
└── examples/           # Example ST programs / Programas ST de ejemplo
    ├── blink.st        # LED blink via GPIO trap
    ├── counter.st      # Up counter with debug output
    └── pid.st          # Discrete PID controller
```

---

## Quick start / Inicio rápido

### 1 — Web editor / Editor web

Open `editor/index.html` in any modern browser. Compilation requires the REST server.

Abra `editor/index.html` en cualquier navegador moderno. La compilación requiere el servidor REST.

```bash
# Serve locally / Servir localmente
python3 -m http.server 8080 --directory osoST/editor/
# Open http://localhost:8080
```

### 2 — Java compiler REST server / Servidor REST compilador Java

```bash
cd compiler-java/
pip install flask
# Copy STLite.jar from STLiteOsoLogic/ to this directory first
python server.py --port 8090
# API available at http://localhost:8090
```

Requires: Java 11+, STLite.jar.

### 3 — Python compiler (CLI) / Compilador Python (CLI)

```bash
cd compiler-python/
python -m ostc examples/counter.st --lex   # tokenise only / solo tokenizar
python -m ostc examples/counter.st --ast   # print AST / imprimir AST
python -m ostc examples/counter.st --asm   # P-code disassembly / desensamblado P-code
python -m ostc examples/counter.st          # compile → counter.hex
```

No third-party dependencies required. Python 3.10+ only.
Sin dependencias externas. Solo Python 3.10+.

### 4 — Runtime (Linux) / Runtime (Linux)

```bash
cd runtime/
make                                       # build osoruntime binary
./osoruntime counter.hex                   # free-run (no fixed cycle)
./osoruntime counter.hex --scan=10         # 10 ms fixed scan cycle
./osoruntime counter.hex --scan=10 --debug --metrics
```

---

## Architecture / Arquitectura

```
 ST source
     │
     ▼
 Lexer (ostc/lexer.py)
     │  token list
     ▼
 Parser (ostc/parser.py)
     │  AST (frozen dataclasses)
     ▼
 CodeGen (ostc/codegen.py)
     │  P-code bytes
     ▼
 HexWriter → Intel HEX (.hex)
     │
     ▼
 osoruntime  ──  scan loop  ──  pcodevm.c interpreter
                                     │
                              hardware_*.c  ←── GPIO / Modbus / MQTT / etc.
```

### Compiler backends / Backends de compilación

| Backend       | Language | Requires | Maturity      |
|---------------|----------|----------|---------------|
| STLite.jar    | Java     | JDK 11+  | Production    |
| ostc (Python) | Python   | None     | Beta          |

Both backends produce compatible Intel HEX files for pcodevm.c.
Ambos backends producen archivos Intel HEX compatibles con pcodevm.c.

### Deployment targets / Destinos de despliegue

| Target      | HAL file            | Scan cycle            | Notes                        |
|-------------|---------------------|-----------------------|------------------------------|
| Linux ARM   | `hardware_linux.c`  | `timerfd_create`      | Raspberry Pi, AM335x, i.MX   |
| Linux x64   | `hardware_linux.c`  | `timerfd_create`      | Industrial PC, BorrellPLC    |
| STM32       | `hardware_bare.c`   | Timer ISR / RTOS task | Cortex-M0+/M3/M4/M7         |
| RP2040      | `hardware_bare.c`   | Timer IRQ             | Raspberry Pi Pico            |
| ESP32       | `hardware_bare.c`   | FreeRTOS task         | Wi-Fi PLC                    |

---

## Integration in OsoLogic® / Integración en OsoLogic®

osoST is a core component of the OsoLogic® platform stack:

```
  webmin-oso (web panel)
       │  embed.js
       ▼
  osoST editor/  ←── this project
       │  REST /compile
       ▼
  compiler-java/server.py  or  python -m ostc
       │  .hex file
       ▼
  osoruntime  +  pcodevm.c
       │  TRAP calls
       ▼
  hardware_linux.c  →  GPIO / Modbus / osodb process tags
```

The REST API endpoints `/lex`, `/parse`, `/compile` are used by:
- The web editor (editor/index.html) for browser-side feedback
- `ui/st-editor/` integration layer in webmin-oso
- CI pipelines and automated tests

---

## License / Licencia

[AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.html)

---

## Copyright / Derechos de autor

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund \<miguel@ibercomp.com\>  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Jose Roig Borrell, Roig Borrell SL, Ibercomp SL

Part of the **OsoLogic®** open-source PLC project — [osologic.org](https://osologic.org)
