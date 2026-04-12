# osoST Java Compiler Backend

Flask REST wrapper around STLite.jar — the reference Java-based ST compiler.
Wrapper Flask REST sobre STLite.jar — el compilador ST de referencia basado en Java.

## Requirements / Requisitos

- Java 11 or later / Java 11 o superior
- Python 3.8+ with Flask / Python 3.8+ con Flask
- STLite.jar (copy to this directory) / STLite.jar (copiar en este directorio)

## Setup / Configuración

```bash
cd osoST/compiler-java/
pip install flask
cp /path/to/STLite.jar .
python server.py
# API at http://localhost:8090
```

## REST API

| Method | Endpoint        | Description                                      |
|--------|-----------------|--------------------------------------------------|
| POST   | `/compile`      | Compile ST source → Intel HEX                    |
| POST   | `/compile/file` | Upload .st/.prj file and compile                 |
| POST   | `/lex`          | Tokenize ST source (Python lexer, no Java)       |
| POST   | `/parse`        | Parse ST source to AST JSON (Python parser)      |
| GET    | `/health`       | Health check                                     |
| GET    | `/version`      | Java and JAR version info                        |

### Compile example / Ejemplo de compilación

```bash
curl -X POST http://localhost:8090/compile \
     -H "Content-Type: application/json" \
     -d '{"source": "procedure main()\n  debug(42);\nend_procedure"}'
```

Response (success):

```json
{
  "ok": true,
  "hex": "<base64>",
  "hex_raw": ":10000000...\n",
  "size_bytes": 48,
  "compile_ms": 320,
  "warnings": []
}
```

Response (error):

```json
{
  "ok": false,
  "errors": ["line 2: syntax error near 'debug'"],
  "warnings": []
}
```

### Lex / Parse endpoints (Python, no Java required)

```bash
# Tokenize / Tokenizar
curl -X POST http://localhost:8090/lex \
     -H "Content-Type: application/json" \
     -d '{"source": "IF x > 0 THEN x := x - 1; END_IF;"}'

# Parse to AST / Parsear a AST
curl -X POST http://localhost:8090/parse \
     -H "Content-Type: application/json" \
     -d '{"source": "PROCEDURE main()\n  debug(1);\nEND_PROCEDURE"}'
```

## CLI wrapper / Wrapper CLI

```bash
./compile.sh examples/counter.st
# Produces: examples/counter.hex
```

---

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund \<miguel@ibercomp.com\>  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Jose Roig Borrell, Roig Borrell SL, Ibercomp SL  
Part of **OsoLogic®** — [osologic.org](https://osologic.org)  
SPDX-License-Identifier: AGPL-3.0-or-later
