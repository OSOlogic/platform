"""
osoLogic — osoST Python Compiler
ostc/lexer.py — Hand-written lexer for IEC 61131-3 ST
                (no external dependencies / sin dependencias externas)

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

Produces a flat list of Token objects from ST source text.
Produces una lista plana de objetos Token desde texto fuente ST.
"""

from __future__ import annotations
import re
from dataclasses import dataclass
from typing import Iterator

from .tokens import KEYWORDS

# ── Token dataclass ───────────────────────────────────────────────

@dataclass(slots=True)
class Token:
    kind:   str    # token type name (e.g. 'ID', 'INT_LIT', 'IF')
    value:  object # raw value (str, int, float, bool)
    line:   int    # 1-based source line
    col:    int    # 1-based column


# ── Lexer errors ─────────────────────────────────────────────────

class LexError(Exception):
    def __init__(self, msg: str, line: int, col: int):
        super().__init__(f"Lex error at {line}:{col}: {msg}")
        self.line = line
        self.col  = col


# ── Token patterns (order matters — longer patterns first) ────────

_RULES: list[tuple[str, str]] = [
    # Comments / Comentarios
    ("COMMENT_BLOCK", r"\(\*[\s\S]*?\*\)"),    # (* ... *)
    ("COMMENT_LINE",  r"//[^\n]*"),             # // ...
    # String literals / Literales de cadena
    ("STRING_LIT",    r'"(?:[^"\\]|\\.)*"'),
    # Number literals / Literales numéricos
    ("FLOAT_LIT",     r"\d+\.\d+([eE][+-]?\d+)?"),
    ("INT_LIT",       r"\d+"),
    # Two-char operators
    ("ASSIGN",        r":="),
    ("NE",            r"<>"),
    ("LE",            r"<="),
    ("GE",            r">="),
    ("DOTDOT",        r"\.\."),
    # Single-char operators / Operadores un carácter
    ("EQ",            r"="),
    ("LT",            r"<"),
    ("GT",            r">"),
    ("PLUS",          r"\+"),
    ("MINUS",         r"-"),
    ("STAR",          r"\*"),
    ("SLASH",         r"/"),
    ("PERCENT",       r"%"),
    ("HASH",          r"#"),
    ("QUESTION",      r"\?"),
    # Delimiters / Delimitadores
    ("LPAREN",        r"\("),
    ("RPAREN",        r"\)"),
    ("LBRACKET",      r"\["),
    ("RBRACKET",      r"\]"),
    ("COMMA",         r","),
    ("SEMICOLON",     r";"),
    ("COLON",         r":"),
    ("DOT",           r"\."),
    # Identifiers & keywords / Identificadores y keywords
    ("ID",            r"[A-Za-z_][A-Za-z0-9_]*"),
    # Whitespace (skip) / Espacios (saltar)
    ("WS",            r"[ \t\r\n]+"),
]

_MASTER_RE = re.compile(
    "|".join(f"(?P<{name}>{pattern})" for name, pattern in _RULES),
    re.IGNORECASE,
)


# ── Lexer function ────────────────────────────────────────────────

def tokenize(source: str) -> list[Token]:
    """
    Tokenize ST source code. Returns list of Token objects.
    Comments and whitespace are discarded.

    Tokeniza código fuente ST. Devuelve lista de objetos Token.
    Los comentarios y espacios en blanco se descartan.

    Raises LexError on unknown character.
    Lanza LexError ante carácter desconocido.
    """
    tokens: list[Token] = []
    line = 1
    line_start = 0

    for m in _MASTER_RE.finditer(source):
        kind  = m.lastgroup
        raw   = m.group()
        start = m.start()
        col   = start - line_start + 1

        # Update line counter
        nl_count = raw.count("\n")
        if nl_count:
            line      += nl_count
            line_start = start + raw.rfind("\n") + 1

        if kind in ("WS", "COMMENT_BLOCK", "COMMENT_LINE"):
            continue  # skip / saltar

        if kind == "ID":
            kw = KEYWORDS.get(raw.lower())
            if kw:
                kind = kw
                if kind in ("TRUE", "FALSE"):
                    tokens.append(Token("BOOL_LIT", raw.lower() == "true", line, col))
                    continue

        # Coerce values / Convertir valores
        if kind == "INT_LIT":
            value: object = int(raw)
        elif kind == "FLOAT_LIT":
            value = float(raw)
        elif kind == "STRING_LIT":
            value = raw[1:-1].replace('\\"', '"').replace("\\n", "\n")
        else:
            value = raw

        tokens.append(Token(kind, value, line, col))

    # Verify all source consumed (detect unknown chars)
    covered = sum(m.end() - m.start() for m in _MASTER_RE.finditer(source))
    if covered < len(source):
        # Find first unconsumed position
        pos = 0
        for m in _MASTER_RE.finditer(source):
            if m.start() > pos:
                col = pos - source.rfind("\n", 0, pos)
                ln  = source[:pos].count("\n") + 1
                raise LexError(f"Unexpected character: {source[pos]!r}", ln, col)
            pos = m.end()

    return tokens


# ── Token stream helper ───────────────────────────────────────────

class TokenStream:
    """
    Peekable token stream used by the parser.
    Flujo de tokens con peek usado por el parser.
    """
    __slots__ = ("_tokens", "_pos")

    def __init__(self, tokens: list[Token]):
        self._tokens = tokens
        self._pos    = 0

    @property
    def eof(self) -> bool:
        return self._pos >= len(self._tokens)

    def peek(self, offset: int = 0) -> Token | None:
        idx = self._pos + offset
        return self._tokens[idx] if idx < len(self._tokens) else None

    def peek_kind(self, offset: int = 0) -> str:
        t = self.peek(offset)
        return t.kind if t else "EOF"

    def next(self) -> Token:
        if self.eof:
            last = self._tokens[-1] if self._tokens else Token("EOF", "", 0, 0)
            raise LexError("Unexpected end of input / Fin de entrada inesperado",
                           last.line, last.col)
        tok = self._tokens[self._pos]
        self._pos += 1
        return tok

    def expect(self, kind: str) -> Token:
        tok = self.next()
        if tok.kind != kind:
            raise LexError(
                f"Expected {kind!r} but got {tok.kind!r} ({tok.value!r}). "
                f"Se esperaba {kind!r} pero se obtuvo {tok.kind!r}",
                tok.line, tok.col
            )
        return tok

    def match(self, *kinds: str) -> Token | None:
        if self.peek_kind() in kinds:
            return self.next()
        return None
