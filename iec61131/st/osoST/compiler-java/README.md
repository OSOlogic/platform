<div align="center">
  <img src="../../../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>osoST Java Compiler Backend</h1>
  <p><strong>Flask REST wrapper around STLite.jar — reference ST compiler for OsoLogic®</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-Structured_Text-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_A.M._Zúñiga_·_J._Roig_Borrell-Roig_Borrell_S.L._·_Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Requirements / Requisitos

- Java 11+
- Python 3.8+ with Flask (`pip install flask`)
- `STLite.jar` — copy to this directory

---

## Setup / Configuración

```bash
cd compiler-java/
pip install flask
cp /path/to/STLite.jar .
python server.py --port 8090
# API at http://localhost:8090
```

---

## REST API

| Method | Endpoint        | Backend | Description                                  |
|--------|-----------------|---------|----------------------------------------------|
| POST   | `/compile`      | Java    | Compile ST source → Intel HEX                |
| POST   | `/compile/file` | Java    | Upload .st/.prj file and compile             |
| POST   | `/lex`          | Python  | Tokenize ST source (no Java required)        |
| POST   | `/parse`        | Python  | Parse ST source → AST JSON (no Java)         |
| GET    | `/health`       | —       | Health check                                 |
| GET    | `/version`      | —       | Java and JAR version info                    |

### Compile example / Ejemplo de compilación

```bash
curl -X POST http://localhost:8090/compile \
     -H "Content-Type: application/json" \
     -d '{"source": "PROCEDURE main()\n  debug(42);\nEND_PROCEDURE"}'
```

**Success / Éxito:**
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

**Error / Error:**
```json
{
  "ok": false,
  "errors": ["line 2: syntax error near 'debug'"],
  "warnings": []
}
```

### Lex / Parse (Python, no Java)

```bash
curl -X POST http://localhost:8090/lex \
     -H "Content-Type: application/json" \
     -d '{"source": "IF x > 0 THEN x := x - 1; END_IF;"}'

curl -X POST http://localhost:8090/parse \
     -H "Content-Type: application/json" \
     -d '{"source": "PROCEDURE main()\n  debug(1);\nEND_PROCEDURE"}'
```

---

## CLI wrapper / Wrapper CLI

```bash
./compile.sh examples/counter.st
# Produces: examples/counter.hex
```

---

## Related / Relacionado

- [`../`](../) — osoST project root
- [`../compiler-python/`](../compiler-python/) — Python compiler (ostc)
- [`../runtime/`](../runtime/) — P-code VM (pcodevm.c)

---

<div align="center">
  <sub>(C) Angel Miguel Zúñiga Schmemund &lt;miguel@ibercomp.com&gt; · Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
