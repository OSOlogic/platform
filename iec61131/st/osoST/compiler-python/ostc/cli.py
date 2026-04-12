"""
osoLogic — osoST Python Compiler
ostc/cli.py — Command-line entry point

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

Usage / Uso:
  python -m ostc <file.st|file.prj> [options]

  Options / Opciones:
    -o <output.hex>   Output HEX file (default: <input>.hex)
    --lex             Print token list and exit
    --ast             Print AST and exit
    --asm             Print P-code assembly and exit
    -v, --verbose     Verbose output
"""

from __future__ import annotations
import argparse
import sys
from pathlib import Path

from .lexer import tokenize, LexError
from .parser import Parser, ParseError
from .codegen import CodeGen, disassemble


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="ostc",
        description="osoLogic ST Compiler — osologic.org"
    )
    parser.add_argument("input",  help="Source file (.st) or project (.prj)")
    parser.add_argument("-o",     dest="output", default=None,
                        help="Output Intel HEX file")
    parser.add_argument("--lex",  action="store_true",
                        help="Print token list and exit / Imprimir tokens y salir")
    parser.add_argument("--ast",  action="store_true",
                        help="Print AST and exit / Imprimir AST y salir")
    parser.add_argument("--asm",  action="store_true",
                        help="Print P-code assembly / Imprimir ensamblado P-code")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    src_path = Path(args.input)
    if not src_path.exists():
        print(f"ERROR: file not found: {src_path}", file=sys.stderr)
        return 1

    out_path = Path(args.output) if args.output else src_path.with_suffix(".hex")

    # Read source / Leer fuente
    source = src_path.read_text(encoding="utf-8")
    if args.verbose:
        print(f"[ostc] Compiling: {src_path}")

    # ── Lexer ──────────────────────────────────────────────────────
    try:
        tokens = tokenize(source)
    except LexError as e:
        print(f"Lex error / Error léxico: {e}", file=sys.stderr)
        return 1

    if args.lex:
        for tok in tokens:
            print(f"{tok.line:4}:{tok.col:<4} {tok.kind:<20} {tok.value!r}")
        return 0

    if args.verbose:
        print(f"[ostc] Tokens: {len(tokens)}")

    # ── Parser ─────────────────────────────────────────────────────
    try:
        ast = Parser(tokens).parse()
    except ParseError as e:
        print(f"Parse error / Error sintáctico: {e}", file=sys.stderr)
        return 1

    if args.ast:
        import pprint
        pprint.pprint(ast)
        return 0

    if args.verbose:
        print(f"[ostc] Parsed successfully / Análisis completado")

    # ── Code generation ────────────────────────────────────────────
    try:
        cg = CodeGen()
        cg.generate(ast)
    except Exception as e:
        print(f"Codegen error / Error de generación: {e}", file=sys.stderr)
        return 1

    if args.asm:
        from .hex_writer import HEADER_SIZE
        raw = cg.writer.to_bytes()
        print(disassemble(raw, start_offset=HEADER_SIZE))
        return 0

    writer = cg.writer
    out_path.write_text(writer.to_intel_hex(), encoding="ascii")
    print(f"[ostc] Written / Escrito: {out_path}  ({len(writer.to_bytes())} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
