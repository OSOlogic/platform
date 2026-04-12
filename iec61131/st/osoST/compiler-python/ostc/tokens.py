"""
osoLogic — osoST Python Compiler
ostc/tokens.py — Token definitions for the IEC 61131-3 ST lexer

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later
"""

# ── Reserved keywords (case-insensitive in ST) ────────────────────
KEYWORDS: dict[str, str] = {
    # Program structure / Estructura del programa
    "program":    "PROGRAM",
    "end_program":"END_PROGRAM",
    "function":   "FUNCTION",
    "end_function":"END_FUNCTION",
    "function_block":"FUNCTION_BLOCK",
    "end_function_block":"END_FUNCTION_BLOCK",
    "procedure":       "PROCEDURE",
    "end_procedure":   "END_PROCEDURE",
    # Variable declarations / Declaraciones de variables
    "var":        "VAR",
    "var_global": "VAR_GLOBAL",
    "var_input":  "VAR_INPUT",
    "var_output": "VAR_OUTPUT",
    "var_in_out": "VAR_IN_OUT",
    "end_var":    "END_VAR",
    # Types / Tipos
    "type":       "TYPE",
    "end_type":   "END_TYPE",
    "array":      "ARRAY",
    "of":         "OF",
    "struct":     "STRUCT",
    "end_struct": "END_STRUCT",
    # Control flow / Control de flujo
    "if":         "IF",
    "then":       "THEN",
    "elsif":      "ELSIF",
    "else":       "ELSE",
    "end_if":     "END_IF",
    "case":       "CASE",
    "of":         "OF",
    "end_case":   "END_CASE",
    "while":      "WHILE",
    "do":         "DO",
    "end_while":  "END_WHILE",
    "repeat":     "REPEAT",
    "until":      "UNTIL",
    "end_repeat": "END_REPEAT",
    "for":        "FOR",
    "to":         "TO",
    "by":         "BY",
    "downto":     "DOWNTO",
    "end_for":    "END_FOR",
    "return":     "RETURN",
    "exit":       "EXIT",
    # Block delimiters / Delimitadores de bloque
    "begin":      "BEGIN",
    "end":        "END",
    # Logical operators / Operadores lógicos
    "and":        "AND",
    "or":         "OR",
    "not":        "NOT",
    "xor":        "XOR",
    "mod":        "MOD",
    # Literals / Literales
    "true":       "TRUE",
    "false":      "FALSE",
    # Builtin types / Tipos built-in
    "bool":       "T_BOOL",
    "sint":       "T_SINT",
    "usint":      "T_USINT",
    "int":        "T_INT",
    "uint":       "T_UINT",
    "dint":       "T_DINT",
    "udint":      "T_UDINT",
    "long":       "T_LONG",
    "ulong":      "T_ULONG",
    "real":       "T_REAL",
    "lreal":      "T_LREAL",
    "float":      "T_FLOAT",
    "string":     "T_STRING",
    "byte":       "T_BYTE",
    "word":       "T_WORD",
    "dword":      "T_DWORD",
    # STLite extensions
    "debug":      "DEBUG",
    "trap":       "TRAP",
}

# ── All token names ───────────────────────────────────────────────
TOKENS: tuple[str, ...] = (
    # Literals
    "INT_LIT", "FLOAT_LIT", "STRING_LIT", "BOOL_LIT",
    # Identifiers
    "ID",
    # Operators / Operadores
    "ASSIGN",       # :=
    "EQ", "NE",     # = <>
    "LT", "LE",     # < <=
    "GT", "GE",     # > >=
    "PLUS", "MINUS", "STAR", "SLASH", "PERCENT",
    "HASH",         # #  (trap number)
    "QUESTION",     # ?  (ternary)
    # Delimiters / Delimitadores
    "LPAREN", "RPAREN",
    "LBRACKET", "RBRACKET",
    "COMMA", "SEMICOLON", "COLON", "DOT", "DOTDOT",
    # Keywords (generated from KEYWORDS dict)
    *sorted(set(KEYWORDS.values())),
)

# Remove duplicates preserving order
_seen: set[str] = set()
TOKENS = tuple(t for t in TOKENS if not (t in _seen or _seen.add(t)))  # type: ignore
