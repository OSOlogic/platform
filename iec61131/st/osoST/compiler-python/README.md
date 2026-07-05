<div align="center">
  <img src="../../../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>ostc — osoST Python Compiler</h1>
  <p><strong>Pure-Python IEC 61131-3 ST compiler — no Java, no third-party dependencies</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-Structured_Text-800000?style=flat-square">
    <img src="https://img.shields.io/badge/Python-3.10+-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_A.M._Zúñiga_·_J._Roig_Borrell-Roig_Borrell_S.L._·_Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Usage / Uso

```bash
python -m ostc <file.st>               # compile → <file.hex>
python -m ostc <file.st> -o out.hex    # specify output
python -m ostc <file.st> --lex         # print token list and exit
python -m ostc <file.st> --ast         # print AST and exit
python -m ostc <file.st> --asm         # print P-code disassembly
python -m ostc <file.st> -v            # verbose output
```

No installation required — runs directly from source. Python 3.10+, stdlib only.

---

## Runs on the runtime (no Java) / Ejecuta en el runtime (sin Java)

`ostc` emits P-code **byte-compatible with the C VM** ([`../runtime/pcodevm.c`](../runtime/)),
so its output runs directly on `osoruntime` — the free, no-Java backend for the OSOLogic PLC:

```
Ladder / ST  →  ostc  →  Intel HEX  →  osoruntime (VM)  →  osodb tags (ACL) → MariaDB
```

**Supported today (verified running):** globals + initializers, INT/DINT/REAL/BOOL, arithmetic,
comparisons, boolean logic, `IF/ELSIF/ELSE`, `WHILE`/`REPEAT`/`FOR`/`CASE`, `PROCEDURE` and
`FUNCTION` with parameters and locals (LINK/UNLINK/LEAVE frames), `TRAP` hardware/`osodb` calls,
and `debug()`. See [`RETARGET_VM.md`](RETARGET_VM.md) for the opcode/encoding map and milestone
status (M1–M3 done; M4 = strings/casts/arrays, not needed for relay/analog logic).

> The Java [STLite](../compiler-java/) compiler remains the authoritative reference; `ostc` is the
> dependency-free alternative for hosts without a JVM. The editor's backend selector picks either.

---

## Module structure / Estructura del módulo

```
ostc/
├── __init__.py    ← package exports
├── __main__.py    ← enables  python -m ostc
├── tokens.py      ← keyword table and token name list
├── lexer.py       ← tokenizer: source → list[Token]  +  TokenStream
├── parser.py      ← recursive-descent parser: tokens → Program AST
├── ast_nodes.py   ← frozen dataclass AST node definitions
├── codegen.py     ← AST → P-code bytes via HexWriter + disassemble()
└── hex_writer.py  ← Intel HEX emitter with STLite 24-byte header
```

---

## Implementation status / Estado de implementación

| Component    | Status        | Notes                                          |
|--------------|---------------|------------------------------------------------|
| Lexer        | ✅ Complete   | Full ST token set, bilingual error messages    |
| Parser       | ✅ Complete   | Recursive descent, full ST + STLite grammar    |
| AST nodes    | ✅ Complete   | Frozen dataclasses, all ST constructs          |
| Code gen     | ✅ Scaffold   | Core ops implemented; multi-dim arrays pending |
| HEX writer   | ✅ Complete   | STLite-compatible header + Intel HEX records   |
| CLI          | ✅ Complete   | All pipeline stages wired                      |

---

## ST language features / Características del lenguaje ST

- **Types** — BOOL, SINT/USINT, INT/UINT, DINT/UDINT, LONG/ULONG, REAL/LREAL, FLOAT, STRING, arrays
- **Declarations** — VAR, VAR_GLOBAL, VAR_INPUT, VAR_OUTPUT, VAR_IN_OUT
- **Control flow** — IF/ELSIF/ELSE, CASE, WHILE, REPEAT, FOR (TO/DOWNTO/BY)
- **Operators** — arithmetic, logical (AND OR NOT XOR MOD), comparison, ternary `?:`
- **Calls** — functions (return value), procedures, TRAP (hardware dispatch)
- **STLite extensions** — TRAP declaration, DEBUG(), ternary operator `?:`

---

## Related / Relacionado

- [`../`](../) — osoST project root
- [`../runtime/`](../runtime/) — P-code VM (pcodevm.c)
- [`../compiler-java/`](../compiler-java/) — Java backend REST wrapper

---

<div align="center">
  <sub>(C) Angel Miguel Zúñiga Schmemund &lt;miguel@ibercomp.com&gt; · Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
