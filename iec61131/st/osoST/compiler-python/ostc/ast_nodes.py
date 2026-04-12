"""
osoLogic — osoST Python Compiler
ostc/ast_nodes.py — Abstract Syntax Tree node definitions

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

All nodes are frozen dataclasses for immutability.
All type annotations are strings to avoid forward-reference issues.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Optional


# ── Base ──────────────────────────────────────────────────────────

@dataclass(frozen=True, kw_only=True)
class Node:
    """Base AST node. / Nodo AST base."""
    line: int = field(default=0, compare=False, hash=False)
    col:  int = field(default=0, compare=False, hash=False)


# ── Type references / Referencias de tipo ─────────────────────────

@dataclass(frozen=True)
class SimpleType(Node):
    """A built-in or user-defined type name. / Nombre de tipo built-in o definido."""
    name: str                   # e.g. "LONG", "BOOL", "MyEnum"

@dataclass(frozen=True)
class StringType(Node):
    """STRING[n] or STRING. / STRING[n] o STRING."""
    max_len: int = 80           # default 80 as per STLite

@dataclass(frozen=True)
class ArrayType(Node):
    """ARRAY[lo..hi, ...] OF base_type"""
    dimensions: tuple           # tuple of (lo, hi) pairs
    element_type: Node          # element type node

TypeNode = SimpleType | StringType | ArrayType


# ── Declarations / Declaraciones ─────────────────────────────────

@dataclass(frozen=True)
class VarDecl(Node):
    """Single variable declaration. / Declaración de variable."""
    name:     str
    typ:      TypeNode
    scope:    str               # 'local' | 'global' | 'input' | 'output'
    init:     Optional[Node] = None   # optional initializer

@dataclass(frozen=True)
class VarBlock(Node):
    """VAR ... END_VAR block. / Bloque VAR ... END_VAR."""
    scope: str                  # 'local' | 'global' | 'input' | 'output'
    decls: tuple[VarDecl, ...]


@dataclass(frozen=True)
class Param(Node):
    """Function/procedure parameter. / Parámetro de función/procedimiento."""
    name: str
    typ:  TypeNode

@dataclass(frozen=True)
class TrapDecl(Node):
    """
    TRAP declaration: procedure foo(...) trap #N
    Links ST symbols to hardware() trap IDs.
    Vincula símbolos ST con IDs de trap en hardware().
    """
    name:      str
    params:    tuple[Param, ...]
    return_type: Optional[TypeNode]
    trap_id:   int


@dataclass(frozen=True)
class FunctionDecl(Node):
    """
    Function declaration (returns a value).
    Declaración de función (devuelve un valor).
    """
    name:        str
    params:      tuple[Param, ...]
    return_type: TypeNode
    var_blocks:  tuple[VarBlock, ...]
    body:        tuple[Node, ...]   # statements

@dataclass(frozen=True)
class ProcedureDecl(Node):
    """
    Procedure declaration (no return value).
    Declaración de procedimiento (sin valor de retorno).
    """
    name:       str
    params:     tuple[Param, ...]
    var_blocks: tuple[VarBlock, ...]
    body:       tuple[Node, ...]

@dataclass(frozen=True)
class EnumType(Node):
    """TYPE name : (A, B, C); END_TYPE"""
    name:   str
    values: tuple[str, ...]

@dataclass(frozen=True)
class Program(Node):
    """
    Top-level compilation unit.
    Unidad de compilación de nivel superior.
    """
    global_vars:  tuple[VarBlock, ...]
    types:        tuple[EnumType, ...]
    traps:        tuple[TrapDecl, ...]
    functions:    tuple[FunctionDecl, ...]
    procedures:   tuple[ProcedureDecl, ...]


# ── Statements / Sentencias ───────────────────────────────────────

@dataclass(frozen=True)
class AssignStmt(Node):
    """lhs := rhs"""
    target: Node
    value:  Node

@dataclass(frozen=True)
class IfStmt(Node):
    """IF cond THEN ... [ELSIF cond THEN ...]* [ELSE ...] END_IF"""
    cond:      Node
    then_body: tuple[Node, ...]
    elsifs:    tuple[tuple[Node, tuple[Node, ...]], ...]  # (cond, body)*
    else_body: Optional[tuple[Node, ...]]

@dataclass(frozen=True)
class CaseStmt(Node):
    """CASE expr OF val: body ... [ELSE body] END_CASE"""
    expr:     Node
    branches: tuple[tuple[tuple[int, ...], tuple[Node, ...]], ...]
    else_body: Optional[tuple[Node, ...]]

@dataclass(frozen=True)
class WhileStmt(Node):
    """WHILE cond DO body END_WHILE"""
    cond: Node
    body: tuple[Node, ...]

@dataclass(frozen=True)
class RepeatStmt(Node):
    """REPEAT body UNTIL cond END_REPEAT"""
    body: tuple[Node, ...]
    cond: Node

@dataclass(frozen=True)
class ForStmt(Node):
    """FOR var := start TO|DOWNTO end [BY step] DO body END_FOR"""
    var:      str
    start:    Node
    end:      Node
    step:     Optional[Node]
    downto:   bool
    body:     tuple[Node, ...]

@dataclass(frozen=True)
class ReturnStmt(Node):
    """RETURN [expr]"""
    value: Optional[Node]

@dataclass(frozen=True)
class ExitStmt(Node):
    """EXIT (from loop)"""

@dataclass(frozen=True)
class CallStmt(Node):
    """Procedure call as a statement. / Llamada a procedimiento como sentencia."""
    name: str
    args: tuple[Node, ...]


# ── Expressions / Expresiones ─────────────────────────────────────

@dataclass(frozen=True)
class IntLit(Node):
    value: int

@dataclass(frozen=True)
class FloatLit(Node):
    value: float

@dataclass(frozen=True)
class BoolLit(Node):
    value: bool

@dataclass(frozen=True)
class StrLit(Node):
    value: str

@dataclass(frozen=True)
class VarRef(Node):
    """Reference to a variable. / Referencia a una variable."""
    name: str

@dataclass(frozen=True)
class ArrayIndex(Node):
    """arr[i] or arr[i, j]. / Indexado de array."""
    base:    Node
    indices: tuple[Node, ...]

@dataclass(frozen=True)
class BinOp(Node):
    """Binary operation. / Operación binaria."""
    op:    str   # '+' '-' '*' '/' '%' 'AND' 'OR' 'XOR' '=' '<>' '<' '<=' '>' '>='
    left:  Node
    right: Node

@dataclass(frozen=True)
class UnaryOp(Node):
    """Unary operation. / Operación unaria."""
    op:      str  # 'NOT' '-'
    operand: Node

@dataclass(frozen=True)
class CallExpr(Node):
    """Function call as expression. / Llamada a función como expresión."""
    name: str
    args: tuple[Node, ...]

@dataclass(frozen=True)
class Ternary(Node):
    """cond ? then_val : else_val (STLite extension)"""
    cond:      Node
    then_val:  Node
    else_val:  Node

@dataclass(frozen=True)
class DebugExpr(Node):
    """debug(...) built-in. / debug(...) incorporado."""
    args: tuple[Node, ...]
