# osoST Python Compiler (ostc)

Pure-Python IEC 61131-3 ST compiler — no Java, no third-party dependencies.
Compilador IEC 61131-3 ST en Python puro — sin Java, sin dependencias externas.

Targets the same P-code VM as the Java STLite backend (pcodevm.c).
Apunta al mismo VM P-code que el backend Java STLite (pcodevm.c).

## Installation / Instalación

```bash
# No install needed — run directly from source
# Sin instalación — ejecutar directamente desde el fuente
cd osoST/compiler-python/
python -m ostc --help
```

Optional development dependencies:

```bash
pip install pytest
```

## Usage / Uso

```bash
python -m ostc <file.st>               # compile → <file.hex>
python -m ostc <file.st> -o out.hex    # specify output path
python -m ostc <file.st> --lex         # print token list and exit
python -m ostc <file.st> --ast         # print AST and exit
python -m ostc <file.st> --asm         # print P-code disassembly
python -m ostc <file.st> -v            # verbose output
```

## Module structure / Estructura del módulo

```
ostc/
├── __init__.py   — Public package exports
├── tokens.py     — Keyword table and token name list
├── lexer.py      — Tokenizer: source → list[Token]
│                   TokenStream class for parser use
├── parser.py     — Recursive-descent parser: tokens → Program AST
├── ast_nodes.py  — Frozen dataclass AST node definitions
├── codegen.py    — AST → P-code bytes via HexWriter
│                   disassemble() for --asm output
└── hex_writer.py — Intel HEX emitter with STLite 24-byte header
```

## Implementation status / Estado de implementación

| Component       | Status       | Notes                                        |
|-----------------|--------------|----------------------------------------------|
| Lexer           | ✅ Complete  | Full ST token set, bilingual error messages  |
| Parser          | ✅ Complete  | Recursive descent, full ST grammar           |
| AST nodes       | ✅ Complete  | Frozen dataclasses, all ST constructs        |
| Code generator  | ✅ Scaffold  | Core ops implemented; array indexing partial |
| HEX writer      | ✅ Complete  | STLite-compatible header + Intel HEX records |
| CLI             | ✅ Complete  | All flags wired to pipeline stages           |

Array multi-dimension indexing and full type checking are planned for v0.2.

## ST language features supported / Características del lenguaje ST soportadas

- Data types: BOOL, SINT, INT, DINT, LONG, REAL, LREAL, FLOAT, STRING, arrays
- Declarations: VAR, VAR_GLOBAL, VAR_INPUT, VAR_OUTPUT, VAR_IN_OUT
- Control flow: IF/ELSIF/ELSE, CASE, WHILE, REPEAT, FOR (TO/DOWNTO/BY)
- Statements: assignment `:=`, RETURN, EXIT
- Operators: arithmetic, logical (AND OR NOT XOR MOD), comparison, ternary `?:`
- Calls: functions (return value), procedures, TRAP (hardware dispatch)
- STLite extensions: TRAP declaration, DEBUG(), ternary operator

---

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund \<miguel@ibercomp.com\>  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Jose Roig Borrell, Roig Borrell SL, Ibercomp SL  
Part of **OsoLogic®** — [osologic.org](https://osologic.org)  
SPDX-License-Identifier: AGPL-3.0-or-later
