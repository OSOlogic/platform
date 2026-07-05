"""
osoLogic — IEC 61131-3 ST Compiler REST API
compiler-java/server.py — Flask wrapper around STLite.jar

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

Endpoints / Endpoints:
  POST /compile        — Compile ST source code or project
  POST /compile/file   — Upload and compile a .st or .prj file
  POST /lex            — Tokenize ST source (Python lexer, no Java required)
  POST /parse          — Parse ST source to AST JSON (Python parser, no Java required)
  GET  /health         — Health check
  GET  /version        — Compiler version info

Usage / Uso:
  pip install flask
  python server.py [--host 0.0.0.0] [--port 8090] [--jar STLite.jar]

  # From the web editor or CLI:
  curl -X POST http://localhost:8090/compile \
       -H "Content-Type: application/json" \
       -d '{"source": "procedure main()\\nbegin\\n  debug(42);\\nend;"}'

Response (success) / Respuesta (éxito):
  {
    "ok": true,
    "hex": "<base64-encoded Intel HEX>",
    "hex_raw": ":10000000...",
    "size_bytes": 1234,
    "warnings": []
  }

Response (error) / Respuesta (error):
  {
    "ok": false,
    "errors": ["line 3: unexpected token 'end'"],
    "warnings": []
  }
"""

import argparse
import base64
import json
import os
import subprocess
import sys
import tempfile
import time
import traceback
from pathlib import Path

from flask import Flask, request, jsonify

# ── Configuration / Configuración ─────────────────────────────────

DEFAULT_JAR  = Path(__file__).parent / "STLite.jar"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8090
COMPILE_TIMEOUT = 30  # seconds / segundos

app = Flask(__name__)

# ── Internal helpers / Helpers internos ──────────────────────────

def find_java() -> str:
    """Locate the java executable. / Localizar el ejecutable java."""
    for candidate in ("java", "/usr/bin/java", "/usr/local/bin/java"):
        try:
            subprocess.run([candidate, "-version"], capture_output=True, timeout=5)
            return candidate
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    raise RuntimeError("java not found — install JDK 11+ and add it to PATH. "
                       "java no encontrado — instalar JDK 11+ y añadirlo al PATH.")


def compile_st(source: str, jar_path: Path, java_bin: str) -> dict:
    """
    Compile ST source code using STLite.jar.
    Returns a dict with 'ok', 'hex'/'errors', and 'warnings'.

    Compila código fuente ST usando STLite.jar.
    Devuelve un dict con 'ok', 'hex'/'errors' y 'warnings'.
    """
    with tempfile.TemporaryDirectory(prefix="osost_") as tmpdir:
        src_path = Path(tmpdir) / "program.st"
        prj_path = Path(tmpdir) / "program.prj"
        hex_path = Path(tmpdir) / "dist" / "program.hex"

        src_path.write_text(source, encoding="utf-8")
        prj_path.write_text(f"FILE=program.st\n", encoding="utf-8")

        t0 = time.monotonic()
        try:
            result = subprocess.run(
                [java_bin, "-jar", str(jar_path), "-compile", str(prj_path)],
                cwd=tmpdir,
                capture_output=True,
                text=True,
                timeout=COMPILE_TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            return {"ok": False, "errors": [f"Compiler timeout after {COMPILE_TIMEOUT}s"],
                    "warnings": []}

        elapsed = time.monotonic() - t0
        stderr  = result.stderr or ""
        stdout  = result.stdout or ""

        # Parse errors / Parsear errores
        errors   = [l for l in (stdout + "\n" + stderr).splitlines()
                    if "error" in l.lower() or "Error" in l]
        warnings = [l for l in (stdout + "\n" + stderr).splitlines()
                    if "warning" in l.lower() or "Warning" in l]

        if result.returncode != 0 or not hex_path.exists():
            if not errors:
                errors = [(stdout + stderr).strip() or "Unknown compile error"]
            return {"ok": False, "errors": errors, "warnings": warnings}

        hex_raw  = hex_path.read_text(encoding="ascii")
        hex_b64  = base64.b64encode(hex_path.read_bytes()).decode("ascii")

        return {
            "ok":        True,
            "hex":       hex_b64,
            "hex_raw":   hex_raw,
            "size_bytes": len(hex_path.read_bytes()),
            "compile_ms": int(elapsed * 1000),
            "warnings":  warnings,
        }


def compile_project(prj_content: str, files: dict, jar_path: Path, java_bin: str) -> dict:
    """
    Compile a multi-file project.
    'files' is a dict { filename: source_code }.

    Compila un proyecto multi-fichero.
    'files' es un dict { nombre_fichero: código_fuente }.
    """
    with tempfile.TemporaryDirectory(prefix="osost_") as tmpdir:
        prj_path = Path(tmpdir) / "project.prj"
        prj_path.write_text(prj_content, encoding="utf-8")

        for name, src in files.items():
            (Path(tmpdir) / name).write_text(src, encoding="utf-8")

        hex_path = Path(tmpdir) / "dist" / "project.hex"

        try:
            result = subprocess.run(
                [java_bin, "-jar", str(jar_path), "-compile", str(prj_path)],
                cwd=tmpdir,
                capture_output=True,
                text=True,
                timeout=COMPILE_TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            return {"ok": False, "errors": [f"Timeout after {COMPILE_TIMEOUT}s"], "warnings": []}

        stderr  = result.stderr or ""
        stdout  = result.stdout or ""
        errors  = [l for l in (stdout+"\n"+stderr).splitlines()
                   if "error" in l.lower()]
        warnings= [l for l in (stdout+"\n"+stderr).splitlines()
                   if "warning" in l.lower()]

        if result.returncode != 0 or not hex_path.exists():
            return {"ok": False, "errors": errors or [stderr.strip()], "warnings": warnings}

        hex_raw = hex_path.read_text(encoding="ascii")
        return {
            "ok":        True,
            "hex":       base64.b64encode(hex_path.read_bytes()).decode("ascii"),
            "hex_raw":   hex_raw,
            "size_bytes": len(hex_path.read_bytes()),
            "warnings":  warnings,
        }

# ── Flask routes / Rutas Flask ───────────────────────────────────

@app.route("/health", methods=["GET"])
def health():
    """Health check. / Comprobación de estado."""
    return jsonify({"ok": True, "service": "osoST compiler", "version": "1.0"})


@app.route("/version", methods=["GET"])
def version():
    """Return Java and STLite version info. / Devolver info de versión."""
    try:
        java = find_java()
        r = subprocess.run([java, "-version"], capture_output=True, text=True, timeout=5)
        jv = (r.stderr or r.stdout).splitlines()[0] if (r.stderr or r.stdout) else "unknown"
        return jsonify({"ok": True, "java": jv, "jar": str(app.config["JAR"])})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500


@app.route("/compile", methods=["POST"])
def compile_endpoint():
    """
    Compile ST source from JSON body.
    Compilar fuente ST desde body JSON.

    Body (single file):
      { "source": "procedure main() begin ... end;" }

    Body (multi-file project):
      {
        "project": "FILE=main.st\\nFILE=math.st",
        "files": { "main.st": "...", "math.st": "..." }
      }
    """
    try:
        data     = request.get_json(force=True)
        jar      = app.config["JAR"]
        java_bin = app.config["JAVA"]

        if "project" in data and "files" in data:
            result = compile_project(data["project"], data["files"], jar, java_bin)
        elif "source" in data:
            result = compile_st(data["source"], jar, java_bin)
        else:
            return jsonify({"ok": False,
                            "errors": ["Body must contain 'source' or 'project'+'files'"]}), 400

        status = 200 if result["ok"] else 422
        return jsonify(result), status

    except Exception:
        return jsonify({"ok": False, "errors": [traceback.format_exc()]}), 500


def compile_python(source: str) -> dict:
    """
    Compile ST with the pure-Python compiler (ostc) — the no-Java alternative backend.
    Same response shape as compile_st() so the editor renders both identically.

    Compila ST con el compilador Python puro (ostc) — backend alternativo sin Java.
    """
    here = Path(__file__).parent.parent / "compiler-python"
    with tempfile.TemporaryDirectory(prefix="osost_py_") as tmpdir:
        src_path = Path(tmpdir) / "program.st"
        hex_path = Path(tmpdir) / "program.hex"
        src_path.write_text(source, encoding="utf-8")
        t0 = time.monotonic()
        try:
            result = subprocess.run(
                [sys.executable, "-m", "ostc", str(src_path), "-o", str(hex_path)],
                cwd=str(here), capture_output=True, text=True, timeout=COMPILE_TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            return {"ok": False, "errors": [f"Compiler timeout after {COMPILE_TIMEOUT}s"],
                    "warnings": [], "backend": "python"}
        elapsed = time.monotonic() - t0
        if result.returncode != 0 or not hex_path.exists():
            msg = (result.stderr or result.stdout or "compile failed").strip()
            errs = [ln for ln in msg.splitlines() if ln.strip()] or ["compile failed"]
            return {"ok": False, "errors": errs, "warnings": [], "backend": "python"}
        hex_bytes = hex_path.read_bytes()
        return {
            "ok": True,
            "hex": base64.b64encode(hex_bytes).decode("ascii"),
            "hex_raw": hex_bytes.decode("ascii"),
            "size_bytes": len(hex_bytes),
            "compile_ms": int(elapsed * 1000),
            "warnings": [],
            "backend": "python",
        }


@app.route("/compile/python", methods=["POST"])
def compile_python_endpoint():
    """Compile ST via the pure-Python backend (no Java). Body: { "source": "..." }"""
    try:
        data   = request.get_json(force=True)
        source = data.get("source", "")
        if not source:
            return jsonify({"ok": False, "errors": ["Body must contain 'source'"]}), 400
        result = compile_python(source)
        return jsonify(result), (200 if result["ok"] else 422)
    except Exception:
        return jsonify({"ok": False, "errors": [traceback.format_exc()]}), 500


@app.route("/compile/file", methods=["POST"])
def compile_file_endpoint():
    """
    Upload a .st or .prj file and compile it.
    Subir un fichero .st o .prj y compilarlo.
    """
    if "file" not in request.files:
        return jsonify({"ok": False, "errors": ["No file part in request"]}), 400

    f    = request.files["file"]
    name = f.filename or "upload.st"
    src  = f.read().decode("utf-8", errors="replace")
    jar  = app.config["JAR"]
    java = app.config["JAVA"]

    if name.endswith(".prj"):
        result = compile_project(src, {}, jar, java)
    else:
        result = compile_st(src, jar, java)

    return jsonify(result), (200 if result["ok"] else 422)

@app.route("/lex", methods=["POST"])
def lex_endpoint():
    """
    Tokenize ST source using the Python lexer (no Java required).
    Tokenizar fuente ST usando el lexer Python (sin Java necesario).

    Body: { "source": "..." }
    Response: { "ok": true, "tokens": [{"kind":"IF","value":"IF","line":1,"col":1}, ...] }
    """
    try:
        import sys, os
        # Add compiler-python to path so ostc is importable
        # Añadir compiler-python al path para que ostc sea importable
        here = Path(__file__).parent.parent / "compiler-python"
        if str(here) not in sys.path:
            sys.path.insert(0, str(here))
        from ostc.lexer import tokenize, LexError

        data   = request.get_json(force=True)
        source = data.get("source", "")
        tokens = tokenize(source)
        return jsonify({
            "ok": True,
            "tokens": [
                {"kind": t.kind, "value": t.value, "line": t.line, "col": t.col}
                for t in tokens
            ],
        })
    except Exception as e:
        return jsonify({"ok": False, "errors": [str(e)]}), 422


@app.route("/parse", methods=["POST"])
def parse_endpoint():
    """
    Parse ST source to AST JSON using the Python parser.
    Parsear fuente ST a JSON de AST usando el parser Python.

    Body: { "source": "..." }
    Response: { "ok": true, "ast": {...} }
    """
    try:
        import sys, dataclasses
        here = Path(__file__).parent.parent / "compiler-python"
        if str(here) not in sys.path:
            sys.path.insert(0, str(here))
        from ostc.lexer import tokenize, LexError
        from ostc.parser import Parser, ParseError

        data   = request.get_json(force=True)
        source = data.get("source", "")
        tokens = tokenize(source)
        ast    = Parser(tokens).parse()

        def _to_dict(node):
            if dataclasses.is_dataclass(node) and not isinstance(node, type):
                d = {"_type": type(node).__name__}
                for f in dataclasses.fields(node):
                    val = getattr(node, f.name)
                    d[f.name] = _to_dict(val)
                return d
            if isinstance(node, (list, tuple)):
                return [_to_dict(v) for v in node]
            return node

        return jsonify({"ok": True, "ast": _to_dict(ast)})
    except Exception as e:
        return jsonify({"ok": False, "errors": [str(e)]}), 422


# ── Entry point / Punto de entrada ───────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="osoST Compiler REST API")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--jar",  default=str(DEFAULT_JAR))
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    jar_path = Path(args.jar)
    if not jar_path.exists():
        print(f"ERROR: STLite.jar not found at {jar_path}", file=sys.stderr)
        print("Copy STLite.jar to this directory or use --jar <path>", file=sys.stderr)
        sys.exit(1)

    try:
        java_bin = find_java()
        print(f"[osoST] Java found: {java_bin}")
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    app.config["JAR"]  = jar_path
    app.config["JAVA"] = java_bin

    print(f"[osoST] Compiler REST API starting on http://{args.host}:{args.port}")
    print(f"[osoST] Using JAR: {jar_path}")
    app.run(host=args.host, port=args.port, debug=args.debug)


if __name__ == "__main__":
    main()
