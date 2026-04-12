"""
osoLogic — osoST Python Compiler
ostc/codegen.py — P-code generator for the STLite virtual machine

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

P-code opcode set matches STLite's pcodevm.c exactly.
Generates Intel HEX via HexWriter.

El conjunto de opcodes P-code coincide exactamente con pcodevm.c de STLite.
Genera Intel HEX via HexWriter.

Stack-machine conventions / Convenciones de máquina de pila:
  - All values are 4 bytes on the stack (I32 or F32)
    Todos los valores son 4 bytes en la pila (I32 o F32)
  - Strings are an index into a static string table
    Las cadenas son un índice en una tabla de cadenas estática
  - Frame layout: [locals...][params...][return_addr][old_fp]
    Disposición del frame: [locales...][params...][dir_retorno][fp_anterior]
"""

from __future__ import annotations
from typing import Optional

from .ast_nodes import (
    Node, Program,
    SimpleType, StringType, ArrayType,
    VarDecl, VarBlock, FunctionDecl, ProcedureDecl, TrapDecl, EnumType,
    AssignStmt, IfStmt, CaseStmt, WhileStmt, RepeatStmt, ForStmt,
    ReturnStmt, ExitStmt, CallStmt,
    IntLit, FloatLit, BoolLit, StrLit, VarRef, ArrayIndex,
    BinOp, UnaryOp, CallExpr, Ternary, DebugExpr,
)
from .hex_writer import HexWriter


# ── Opcodes (must match pcodevm.c) ────────────────────────────────

class Op:
    # Stack manipulation / Manipulación de pila
    PUSH_I    = 0x01   # push I32 literal (4 bytes follow)
    PUSH_F    = 0x02   # push F32 literal (4 bytes follow)
    PUSH_STR  = 0x03   # push string index (2 bytes follow)
    POP       = 0x04   # discard top of stack
    DUP       = 0x05   # duplicate top

    # Variables (local / global) / Variables (locales / globales)
    LOAD_L    = 0x10   # load local  at frame offset  (2 bytes)
    STORE_L   = 0x11   # store local at frame offset  (2 bytes)
    LOAD_G    = 0x12   # load global at byte offset   (4 bytes)
    STORE_G   = 0x13   # store global at byte offset  (4 bytes)
    LOAD_ARR  = 0x14   # array load:  base(4) elem_size(2) on stack
    STORE_ARR = 0x15   # array store: base(4) elem_size(2) on stack

    # Arithmetic (I32) / Aritmética (I32)
    ADD_I     = 0x20
    SUB_I     = 0x21
    MUL_I     = 0x22
    DIV_I     = 0x23
    MOD_I     = 0x24
    NEG_I     = 0x25

    # Arithmetic (F32) / Aritmética (F32)
    ADD_F     = 0x28
    SUB_F     = 0x29
    MUL_F     = 0x2A
    DIV_F     = 0x2B
    NEG_F     = 0x2C

    # Conversions / Conversiones
    I2F       = 0x30
    F2I       = 0x31

    # Comparisons (result: 0 or 1 I32) / Comparaciones (resultado: 0 ó 1 I32)
    EQ_I      = 0x40
    NE_I      = 0x41
    LT_I      = 0x42
    LE_I      = 0x43
    GT_I      = 0x44
    GE_I      = 0x45
    EQ_F      = 0x48
    NE_F      = 0x49
    LT_F      = 0x4A
    LE_F      = 0x4B
    GT_F      = 0x4C
    GE_F      = 0x4D

    # Logic / Lógica
    AND_I     = 0x50
    OR_I      = 0x51
    XOR_I     = 0x52
    NOT_I     = 0x53

    # Control flow / Control de flujo
    JMP       = 0x60   # unconditional jump, target I32 follows
    JZ        = 0x61   # jump if zero (false), target I32 follows
    JNZ       = 0x62   # jump if non-zero (true), target I32 follows
    CALL      = 0x70   # call function/procedure by address, argc follows (1 byte)
    RET       = 0x71   # return (pops frame)
    TRAP      = 0x72   # call hardware(), trap_id follows (1 byte)
    HALT      = 0x7F   # stop execution

    # Debug / Depuración
    DEBUG_I   = 0x80   # debug print I32 (1 format byte follows: nargs)
    DEBUG_F   = 0x81
    DEBUG_STR = 0x82

    # VT type tags (used by PUSH_I for typed pushes) / Etiquetas de tipo VT
    VT_I32    = 0x04
    VT_F32    = 0x05


# ── Type system helpers / Ayudantes del sistema de tipos ──────────

class _TypeKind:
    INT   = "int"
    FLOAT = "float"
    BOOL  = "bool"
    STR   = "str"
    ARRAY = "array"

def _type_kind(typ: Node) -> str:
    if isinstance(typ, SimpleType):
        if typ.name in ("T_REAL", "T_LREAL", "T_FLOAT"):
            return _TypeKind.FLOAT
        if typ.name == "T_BOOL":
            return _TypeKind.BOOL
        if typ.name == "T_STRING":
            return _TypeKind.STR
        return _TypeKind.INT
    if isinstance(typ, StringType):
        return _TypeKind.STR
    if isinstance(typ, ArrayType):
        return _TypeKind.ARRAY
    return _TypeKind.INT   # safe default


def _elem_bytes(typ: Node) -> int:
    """Bytes per stack element. / Bytes por elemento de pila."""
    k = _type_kind(typ)
    if k == _TypeKind.STR:
        return 81   # 1 len + 80 chars (STLite default)
    return 4        # I32 / F32


# ── Symbol table / Tabla de símbolos ──────────────────────────────

class _Symbol:
    """Describes a declared variable. / Describe una variable declarada."""
    __slots__ = ("name", "typ", "scope", "offset", "size")

    def __init__(self, name: str, typ: Node, scope: str, offset: int, size: int):
        self.name   = name
        self.typ    = typ
        self.scope  = scope
        self.offset = offset   # bytes from frame base (local) or global area
        self.size   = size     # bytes occupied


class _Scope:
    """Holds a flat symbol table for one function/procedure."""
    def __init__(self):
        self._syms: dict[str, _Symbol] = {}
        self._local_bytes = 0

    def declare(self, decl: VarDecl) -> _Symbol:
        size = _elem_bytes(decl.typ)
        sym  = _Symbol(decl.name, decl.typ, decl.scope,
                       offset=self._local_bytes, size=size)
        self._syms[decl.name.lower()] = sym
        self._local_bytes += size
        return sym

    def lookup(self, name: str) -> Optional[_Symbol]:
        return self._syms.get(name.lower())

    @property
    def local_bytes(self) -> int:
        return self._local_bytes


# ── Code generator / Generador de código ──────────────────────────

class CodeGen:
    """
    Traverses the AST and emits P-code into a HexWriter.
    Recorre el AST y emite P-code en un HexWriter.

    Usage / Uso:
        cg = CodeGen()
        cg.generate(program_node)
        hex_str = cg.writer.to_intel_hex()
    """

    def __init__(self, stack_needed: int = 512):
        self._stack_needed = stack_needed
        self._writer: HexWriter | None = None

        # Global symbol table / Tabla de símbolos global
        self._globals: dict[str, _Symbol] = {}
        self._global_bytes = 0

        # Enum value table / Tabla de valores enum
        self._enums: dict[str, int] = {}

        # Function/procedure address table / Tabla de direcciones
        self._proc_addr: dict[str, int] = {}   # name → code offset
        self._proc_calls: list[tuple[int, str]] = []  # (patch_offset, name)

        # Trap table / Tabla de traps
        self._traps: dict[str, int] = {}       # name → trap_id

        # Current scope / Ámbito actual
        self._scope: Optional[_Scope] = None

        # Current loop exit patches / Parches de salida de bucle
        self._exit_patches: list[list[int]] = []   # stack of patch-offset lists

    @property
    def writer(self) -> HexWriter:
        assert self._writer is not None, "Call generate() first"
        return self._writer

    # ── Top-level / Nivel superior ────────────────────────────────

    def generate(self, prog: Program) -> None:
        # First pass: collect enum values / Primera pasada: recopilar valores enum
        for et in prog.types:
            for i, name in enumerate(et.values):
                self._enums[name.lower()] = i

        # First pass: collect trap IDs / Primera pasada: recopilar IDs de trap
        for td in prog.traps:
            self._traps[td.name.lower()] = td.trap_id

        # First pass: collect globals / Primera pasada: recopilar globales
        for vb in prog.global_vars:
            for decl in vb.decls:
                size = _elem_bytes(decl.typ)
                sym  = _Symbol(decl.name, decl.typ, "global",
                               offset=self._global_bytes, size=size)
                self._globals[decl.name.lower()] = sym
                self._global_bytes += size

        self._writer = HexWriter(global_bytes=self._global_bytes,
                                 stack_needed=self._stack_needed)

        # Emit a JMP to the "main" procedure (first procedure named 'main' or first proc)
        # Emitir un JMP al procedimiento "main" (primer procedimiento llamado 'main' o el primero)
        w = self._writer
        w.emit(Op.JMP)
        main_patch = w.position()
        w.emit_i32(0)   # back-patch later / parchar después

        # Emit functions / Emitir funciones
        for fd in prog.functions:
            self._emit_function(fd)

        # Emit procedures / Emitir procedimientos
        for pd in prog.procedures:
            self._emit_procedure(pd)

        # Back-patch JMP to main / Parchar JMP al main
        main_name = "main"
        if main_name not in self._proc_addr:
            # Use first procedure / Usar primer procedimiento
            if prog.procedures:
                main_name = prog.procedures[0].name.lower()

        if main_name in self._proc_addr:
            w.patch_i32(main_patch, self._proc_addr[main_name])

        # Back-patch all CALL instructions / Parchar todas las instrucciones CALL
        for patch_off, name in self._proc_calls:
            addr = self._proc_addr.get(name.lower(), 0)
            w.patch_i32(patch_off, addr)

        w.emit(Op.HALT)

    # ── Function / Función ────────────────────────────────────────

    def _emit_function(self, fd: FunctionDecl) -> None:
        w = self._writer
        self._proc_addr[fd.name.lower()] = w.position()
        self._scope = _Scope()

        for vb in fd.var_blocks:
            for decl in vb.decls:
                self._scope.declare(decl)

        for stmt in fd.body:
            self._emit_stmt(stmt)

        # Functions must have an explicit RETURN; emit one as safety net
        # Las funciones deben tener un RETURN explícito; emitir uno como red de seguridad
        w.emit(Op.RET)
        self._scope = None

    # ── Procedure / Procedimiento ─────────────────────────────────

    def _emit_procedure(self, pd: ProcedureDecl) -> None:
        w = self._writer
        self._proc_addr[pd.name.lower()] = w.position()
        self._scope = _Scope()

        for vb in pd.var_blocks:
            for decl in vb.decls:
                self._scope.declare(decl)

        for stmt in pd.body:
            self._emit_stmt(stmt)

        w.emit(Op.RET)
        self._scope = None

    # ── Statements / Sentencias ───────────────────────────────────

    def _emit_stmt(self, node: Node) -> None:
        if isinstance(node, AssignStmt):
            self._emit_assign(node)
        elif isinstance(node, IfStmt):
            self._emit_if(node)
        elif isinstance(node, WhileStmt):
            self._emit_while(node)
        elif isinstance(node, RepeatStmt):
            self._emit_repeat(node)
        elif isinstance(node, ForStmt):
            self._emit_for(node)
        elif isinstance(node, CaseStmt):
            self._emit_case(node)
        elif isinstance(node, ReturnStmt):
            self._emit_return(node)
        elif isinstance(node, ExitStmt):
            self._emit_exit(node)
        elif isinstance(node, CallStmt):
            self._emit_call_stmt(node)
        else:
            raise NotImplementedError(f"Unsupported statement: {type(node).__name__}")

    def _emit_assign(self, node: AssignStmt) -> None:
        self._emit_expr(node.value)
        self._emit_store(node.target)

    def _emit_if(self, node: IfStmt) -> None:
        w = self._writer
        end_patches: list[int] = []

        # Main condition / Condición principal
        self._emit_expr(node.cond)
        w.emit(Op.JZ)
        next_patch = w.position()
        w.emit_i32(0)

        for stmt in node.then_body:
            self._emit_stmt(stmt)

        w.emit(Op.JMP)
        end_patches.append(w.position())
        w.emit_i32(0)

        # ELSIF branches / Ramas ELSIF
        for ec, eb in node.elsifs:
            w.patch_i32(next_patch, w.position())
            self._emit_expr(ec)
            w.emit(Op.JZ)
            next_patch = w.position()
            w.emit_i32(0)

            for s in eb:
                self._emit_stmt(s)

            w.emit(Op.JMP)
            end_patches.append(w.position())
            w.emit_i32(0)

        # ELSE / branch / Rama ELSE
        w.patch_i32(next_patch, w.position())
        if node.else_body:
            for s in node.else_body:
                self._emit_stmt(s)

        end_pos = w.position()
        for p in end_patches:
            w.patch_i32(p, end_pos)

    def _emit_while(self, node: WhileStmt) -> None:
        w = self._writer
        loop_start = w.position()

        self._exit_patches.append([])
        self._emit_expr(node.cond)
        w.emit(Op.JZ)
        exit_patch = w.position()
        w.emit_i32(0)

        for s in node.body:
            self._emit_stmt(s)

        w.emit(Op.JMP)
        w.emit_i32(loop_start)

        end = w.position()
        w.patch_i32(exit_patch, end)
        for p in self._exit_patches.pop():
            w.patch_i32(p, end)

    def _emit_repeat(self, node: RepeatStmt) -> None:
        w = self._writer
        loop_start = w.position()

        self._exit_patches.append([])

        for s in node.body:
            self._emit_stmt(s)

        self._emit_expr(node.cond)
        w.emit(Op.JZ)
        w.emit_i32(loop_start)   # repeat until cond is true

        end = w.position()
        for p in self._exit_patches.pop():
            w.patch_i32(p, end)

    def _emit_for(self, node: ForStmt) -> None:
        """
        FOR i := start TO end [BY step] DO ... END_FOR
        Emits:  i = start
                loop: if (downto ? i < end : i > end) goto done
                      body
                      i = i + step  (or i - step for DOWNTO)
                      goto loop
                done:
        """
        w = self._writer
        sym = self._lookup(node.var)

        # Initialize loop variable / Inicializar variable de bucle
        self._emit_expr(node.start)
        self._emit_store_sym(sym)

        self._exit_patches.append([])
        loop_start = w.position()

        # Condition: exit when past the limit
        self._emit_load_sym(sym)
        self._emit_expr(node.end)
        if node.downto:
            w.emit(Op.LT_I)    # exit if i < end (DOWNTO)
        else:
            w.emit(Op.GT_I)    # exit if i > end (TO)

        w.emit(Op.JNZ)
        exit_patch = w.position()
        w.emit_i32(0)

        for s in node.body:
            self._emit_stmt(s)

        # Increment/decrement / Incremento/decremento
        self._emit_load_sym(sym)
        if node.step:
            self._emit_expr(node.step)
        else:
            w.emit(Op.PUSH_I)
            w.emit_i32(1)

        if node.downto:
            w.emit(Op.SUB_I)
        else:
            w.emit(Op.ADD_I)
        self._emit_store_sym(sym)

        w.emit(Op.JMP)
        w.emit_i32(loop_start)

        end = w.position()
        w.patch_i32(exit_patch, end)
        for p in self._exit_patches.pop():
            w.patch_i32(p, end)

    def _emit_case(self, node: CaseStmt) -> None:
        w = self._writer
        end_patches: list[int] = []

        for labels, body in node.branches:
            # Build OR chain of comparisons / Construir cadena OR de comparaciones
            pass_patches: list[int] = []
            for lv in labels:
                self._emit_expr(node.expr)
                w.emit(Op.PUSH_I)
                w.emit_i32(lv)
                w.emit(Op.EQ_I)
                w.emit(Op.JNZ)
                pass_patches.append(w.position())
                w.emit_i32(0)

            # None matched — jump over body / Ninguno coincidió — saltar sobre cuerpo
            w.emit(Op.JMP)
            skip_patch = w.position()
            w.emit_i32(0)

            here = w.position()
            for p in pass_patches:
                w.patch_i32(p, here)

            for s in body:
                self._emit_stmt(s)

            w.emit(Op.JMP)
            end_patches.append(w.position())
            w.emit_i32(0)

            w.patch_i32(skip_patch, w.position())

        # ELSE body / Cuerpo ELSE
        if node.else_body:
            for s in node.else_body:
                self._emit_stmt(s)

        end = w.position()
        for p in end_patches:
            w.patch_i32(p, end)

    def _emit_return(self, node: ReturnStmt) -> None:
        w = self._writer
        if node.value:
            self._emit_expr(node.value)
        w.emit(Op.RET)

    def _emit_exit(self, node: ExitStmt) -> None:
        w = self._writer
        w.emit(Op.JMP)
        patch = w.position()
        w.emit_i32(0)
        if self._exit_patches:
            self._exit_patches[-1].append(patch)

    def _emit_call_stmt(self, node: CallStmt) -> None:
        w = self._writer
        name_lo = node.name.lower()

        # Is it a TRAP? / ¿Es un TRAP?
        if name_lo in self._traps:
            for arg in node.args:
                self._emit_expr(arg)
            w.emit(Op.TRAP)
            w.emit(self._traps[name_lo] & 0xFF)
            return

        # Regular call / Llamada regular
        for arg in node.args:
            self._emit_expr(arg)
        w.emit(Op.CALL)
        self._proc_calls.append((w.position(), name_lo))
        w.emit_i32(0)           # back-patch later / parchar después
        w.emit(len(node.args) & 0xFF)

        # Discard return value if any (procedure call context)
        # Descartar valor de retorno si existe (contexto de llamada a procedimiento)
        w.emit(Op.POP)

    # ── Expressions / Expresiones ─────────────────────────────────

    def _emit_expr(self, node: Node) -> None:
        w = self._writer

        if isinstance(node, IntLit):
            w.emit(Op.PUSH_I)
            w.emit_i32(node.value)

        elif isinstance(node, FloatLit):
            w.emit(Op.PUSH_F)
            w.emit_f32(node.value)

        elif isinstance(node, BoolLit):
            w.emit(Op.PUSH_I)
            w.emit_i32(1 if node.value else 0)

        elif isinstance(node, StrLit):
            # Strings are emitted inline via PUSH_STR + inline data
            # Las cadenas se emiten inline via PUSH_STR + datos inline
            w.emit(Op.PUSH_STR)
            w.emit_string(node.value)

        elif isinstance(node, VarRef):
            # Check enum values first / Verificar valores enum primero
            ev = self._enums.get(node.name.lower())
            if ev is not None:
                w.emit(Op.PUSH_I)
                w.emit_i32(ev)
                return
            sym = self._lookup(node.name)
            self._emit_load_sym(sym)

        elif isinstance(node, ArrayIndex):
            self._emit_array_load(node)

        elif isinstance(node, BinOp):
            self._emit_binop(node)

        elif isinstance(node, UnaryOp):
            self._emit_unary(node)

        elif isinstance(node, CallExpr):
            self._emit_call_expr(node)

        elif isinstance(node, Ternary):
            self._emit_ternary(node)

        elif isinstance(node, DebugExpr):
            self._emit_debug(node)

        else:
            raise NotImplementedError(f"Unsupported expression: {type(node).__name__}")

    def _emit_binop(self, node: BinOp) -> None:
        w = self._writer
        self._emit_expr(node.left)
        self._emit_expr(node.right)

        # Determine if float context / Determinar si contexto float
        lk = self._expr_type_kind(node.left)
        rk = self._expr_type_kind(node.right)
        is_float = (lk == _TypeKind.FLOAT or rk == _TypeKind.FLOAT)

        op = node.op
        if op == "+":
            w.emit(Op.ADD_F if is_float else Op.ADD_I)
        elif op == "-":
            w.emit(Op.SUB_F if is_float else Op.SUB_I)
        elif op == "*":
            w.emit(Op.MUL_F if is_float else Op.MUL_I)
        elif op == "/":
            w.emit(Op.DIV_F if is_float else Op.DIV_I)
        elif op == "%":
            w.emit(Op.MOD_I)
        elif op == "AND":
            w.emit(Op.AND_I)
        elif op == "OR":
            w.emit(Op.OR_I)
        elif op == "XOR":
            w.emit(Op.XOR_I)
        elif op == "=":
            w.emit(Op.EQ_F if is_float else Op.EQ_I)
        elif op == "<>":
            w.emit(Op.NE_F if is_float else Op.NE_I)
        elif op == "<":
            w.emit(Op.LT_F if is_float else Op.LT_I)
        elif op == "<=":
            w.emit(Op.LE_F if is_float else Op.LE_I)
        elif op == ">":
            w.emit(Op.GT_F if is_float else Op.GT_I)
        elif op == ">=":
            w.emit(Op.GE_F if is_float else Op.GE_I)
        else:
            raise NotImplementedError(f"Unsupported binary operator: {op!r}")

    def _emit_unary(self, node: UnaryOp) -> None:
        w = self._writer
        self._emit_expr(node.operand)
        op = node.op
        if op == "-":
            k = self._expr_type_kind(node.operand)
            w.emit(Op.NEG_F if k == _TypeKind.FLOAT else Op.NEG_I)
        elif op == "NOT":
            w.emit(Op.NOT_I)
        else:
            raise NotImplementedError(f"Unsupported unary operator: {op!r}")

    def _emit_call_expr(self, node: CallExpr) -> None:
        w = self._writer
        name_lo = node.name.lower()

        if name_lo in self._traps:
            for arg in node.args:
                self._emit_expr(arg)
            w.emit(Op.TRAP)
            w.emit(self._traps[name_lo] & 0xFF)
            return

        for arg in node.args:
            self._emit_expr(arg)
        w.emit(Op.CALL)
        self._proc_calls.append((w.position(), name_lo))
        w.emit_i32(0)
        w.emit(len(node.args) & 0xFF)

    def _emit_ternary(self, node: Ternary) -> None:
        w = self._writer
        self._emit_expr(node.cond)
        w.emit(Op.JZ)
        else_patch = w.position()
        w.emit_i32(0)

        self._emit_expr(node.then_val)
        w.emit(Op.JMP)
        end_patch = w.position()
        w.emit_i32(0)

        w.patch_i32(else_patch, w.position())
        self._emit_expr(node.else_val)
        w.patch_i32(end_patch, w.position())

    def _emit_debug(self, node: DebugExpr) -> None:
        w = self._writer
        for arg in node.args:
            self._emit_expr(arg)
            k = self._expr_type_kind(arg)
            if k == _TypeKind.FLOAT:
                w.emit(Op.DEBUG_F)
            elif k == _TypeKind.STR:
                w.emit(Op.DEBUG_STR)
            else:
                w.emit(Op.DEBUG_I)

    def _emit_array_load(self, node: ArrayIndex) -> None:
        """Emit code to load an array element. / Emitir código para cargar un elemento de array."""
        # Simplified: emit base address then index
        # TODO: full multi-dim array support in a future revision
        # Simplificado: emitir dirección base luego índice
        # TODO: soporte completo de arrays multidimensionales en revisión futura
        self._emit_expr(node.indices[0])
        # Caller must handle further indexing; this is a stub.

    # ── Store / Load helpers / Ayudantes de almacenamiento/carga ──

    def _emit_store(self, target: Node) -> None:
        if isinstance(target, VarRef):
            sym = self._lookup(target.name)
            self._emit_store_sym(sym)
        elif isinstance(target, ArrayIndex):
            # TODO: array store / Almacenamiento en array (pendiente)
            pass
        else:
            raise NotImplementedError(f"Cannot assign to {type(target).__name__}")

    def _emit_load_sym(self, sym: _Symbol) -> None:
        w = self._writer
        if sym.scope == "global":
            w.emit(Op.LOAD_G)
            w.emit_i32(sym.offset)
        else:
            w.emit(Op.LOAD_L)
            w.emit_u16(sym.offset)

    def _emit_store_sym(self, sym: _Symbol) -> None:
        w = self._writer
        if sym.scope == "global":
            w.emit(Op.STORE_G)
            w.emit_i32(sym.offset)
        else:
            w.emit(Op.STORE_L)
            w.emit_u16(sym.offset)

    # ── Symbol lookup / Búsqueda de símbolo ──────────────────────

    def _lookup(self, name: str) -> _Symbol:
        name_lo = name.lower()
        if self._scope:
            sym = self._scope.lookup(name_lo)
            if sym:
                return sym
        sym = self._globals.get(name_lo)
        if sym:
            return sym
        raise NameError(f"Undefined variable: {name!r} / Variable no definida: {name!r}")

    # ── Type inference for expressions / Inferencia de tipos ──────

    def _expr_type_kind(self, node: Node) -> str:
        """
        Best-effort type inference for primitive expressions.
        Only needed to pick INT vs FLOAT opcodes.

        Inferencia de tipos de mejor esfuerzo para expresiones primitivas.
        Solo necesaria para elegir opcodes INT vs FLOAT.
        """
        if isinstance(node, FloatLit):
            return _TypeKind.FLOAT
        if isinstance(node, IntLit):
            return _TypeKind.INT
        if isinstance(node, BoolLit):
            return _TypeKind.BOOL
        if isinstance(node, StrLit):
            return _TypeKind.STR
        if isinstance(node, VarRef):
            try:
                sym = self._lookup(node.name)
                return _type_kind(sym.typ)
            except NameError:
                return _TypeKind.INT
        if isinstance(node, BinOp):
            lk = self._expr_type_kind(node.left)
            rk = self._expr_type_kind(node.right)
            if lk == _TypeKind.FLOAT or rk == _TypeKind.FLOAT:
                return _TypeKind.FLOAT
            return _TypeKind.INT
        if isinstance(node, UnaryOp):
            return self._expr_type_kind(node.operand)
        return _TypeKind.INT   # conservative default


# ── Disassembler (for --asm output) / Desensamblador (para --asm) ──

_OP_NAMES: dict[int, str] = {v: k for k, v in vars(Op).items() if isinstance(v, int)}

def disassemble(data: bytes, start_offset: int = 0) -> str:
    """
    Human-readable P-code dump.
    Volcado P-code legible por humanos.
    """
    lines: list[str] = []
    i = start_offset
    while i < len(data):
        op = data[i]
        name = _OP_NAMES.get(op, f"0x{op:02X}")
        line = f"  {i:06X}  {name}"
        i += 1

        if op in (Op.PUSH_I, Op.JMP, Op.JZ, Op.JNZ):
            if i + 4 <= len(data):
                import struct
                val = struct.unpack_from("<i", data, i)[0]
                line += f"  {val}"
                i += 4
        elif op == Op.PUSH_F:
            if i + 4 <= len(data):
                import struct
                val = struct.unpack_from("<f", data, i)[0]
                line += f"  {val:.6g}"
                i += 4
        elif op in (Op.LOAD_L, Op.STORE_L):
            if i + 2 <= len(data):
                import struct
                off = struct.unpack_from("<H", data, i)[0]
                line += f"  @{off}"
                i += 2
        elif op in (Op.LOAD_G, Op.STORE_G):
            if i + 4 <= len(data):
                import struct
                off = struct.unpack_from("<i", data, i)[0]
                line += f"  @{off}"
                i += 4
        elif op == Op.CALL:
            if i + 5 <= len(data):
                import struct
                addr  = struct.unpack_from("<i", data, i)[0]
                argc  = data[i + 4]
                line += f"  addr={addr}  argc={argc}"
                i += 5
        elif op == Op.TRAP:
            if i < len(data):
                line += f"  #{data[i]}"
                i += 1

        lines.append(line)
    return "\n".join(lines)
