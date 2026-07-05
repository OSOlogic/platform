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
    """Opcodes as the C VM (runtime/pcodevm.c) executes them — decimal, typed.

    Encoding conventions the VM expects:
      - Typed ops (LOAD/STORE/arith/cmp/logic) carry a value-type byte `t` (VT_*).
      - LOAD_G/STORE_G/LOAD_L/STORE_L: addr/offset u16, then t (u8).
      - PUSH_I: t (u8), then the literal sized by t (I32 → 4 bytes).
      - Jumps (JMP/JMPF) take a *relative* signed i16 offset from the byte after it.
      - CALL takes an absolute u16 address; RET pops the 2-byte return address.
    ostc keeps every value 4-byte, so it only ever emits VT_I32 / VT_F32.
    """
    # Stack manipulation
    PUSH_I    = 1    # t(u8) + value(width by t)
    PUSH_F    = 2    # f32 (4 bytes)
    PUSH_S    = 3    # string index (u16)

    # Variables — addr/off(u16) + type(u8)
    LOAD_G    = 10
    STORE_G   = 11
    LOAD_L    = 12
    STORE_L   = 13

    # Arithmetic — type(u8)
    ADD       = 20
    SUB       = 21
    MUL       = 22
    DIV       = 23
    NEG       = 24

    # Comparisons — type(u8)
    LT        = 30
    LE        = 31
    GT        = 32
    GE        = 33
    EQ        = 34
    NE        = 35

    # Logic — type(u8)
    AND       = 40
    OR        = 41
    NOT       = 42
    XOR       = 43

    # Control flow — jumps are RELATIVE i16
    JMP       = 50   # pc += i16
    JMPF      = 51   # pop; if not truthy: pc += i16
    CALL      = 60   # push return pc; pc = u16 addr
    RET       = 61   # pc = pop2()
    HALT      = 62
    LINK      = 63   # frame(u16) pbytes(u16)  — enter frame (locals/params)
    UNLINK    = 64   # leave frame
    LEAVE     = 65   # retbytes(u8) — function return-value path

    # Debug / math / trap
    DEBUG     = 70
    MATH      = 73   # subop(u8)
    TRAP      = 80   # trap_id(u8)

    # Conversions
    CAST_I32  = 107
    CAST_F32  = 108

    # Value-type tags (must match pcodevm.h VT_*)
    VT_S8  = 0
    VT_U8  = 1
    VT_S16 = 2
    VT_U16 = 3
    VT_I32 = 4
    VT_F32 = 5
    VT_STR = 6

    # MATH subops
    MATH_MOD_I = 106


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
        self._func_ret: Optional[_Symbol] = None   # return slot of the function being emitted

        # Current loop exit patches / Parches de salida de bucle
        self._exit_patches: list[list[int]] = []   # stack of patch-offset lists

    @property
    def writer(self) -> HexWriter:
        assert self._writer is not None, "Call generate() first"
        return self._writer

    # ── Emit helpers for the typed VM / Ayudantes de emisión ──────

    @staticmethod
    def _vt(kind: str) -> int:
        """ostc keeps values 4-byte → VT_F32 for floats, VT_I32 for everything else."""
        return Op.VT_F32 if kind == _TypeKind.FLOAT else Op.VT_I32

    def _vt_of(self, node: Node) -> int:
        return self._vt(self._expr_type_kind(node))

    # Relative jumps (signed i16 from the byte after the operand).
    def _emit_jmp(self, op: int) -> int:
        """Emit a forward jump with a placeholder; return the operand offset to patch."""
        w = self._writer
        w.emit(op)
        p = w.position()
        w.emit_i16(0)
        return p

    def _patch_jmp(self, patch: int) -> None:
        """Patch a forward jump so it lands at the current position."""
        w = self._writer
        w.patch_i16(patch, w.position() - (patch + 2))

    def _emit_jmp_to(self, op: int, target: int) -> None:
        """Emit a jump (usually backward) to a known absolute target."""
        w = self._writer
        w.emit(op)
        p = w.position()
        w.emit_i16(target - (p + 2))

    def _emit_jnz(self) -> int:
        """Jump-if-true placeholder: the VM only has JMPF, so invert with NOT then JMPF."""
        w = self._writer
        w.emit(Op.NOT); w.emit_u8(Op.VT_I32)
        return self._emit_jmp(Op.JMPF)

    def _emit_push_int(self, v: int) -> None:
        w = self._writer
        w.emit(Op.PUSH_I); w.emit_u8(Op.VT_I32); w.emit_i32(v)

    def _emit_push_float(self, v: float) -> None:
        w = self._writer
        w.emit(Op.PUSH_F); w.emit_f32(v)

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
        # Global initializers. The runtime clears RAM before every scan (vm_reset), so
        # emit the declared initial values at entry — they re-apply each scan.
        for vb in prog.global_vars:
            for decl in vb.decls:
                if getattr(decl, "init", None) is not None:
                    self._emit_expr(decl.init)
                    self._emit_store_sym(self._globals[decl.name.lower()])

        # Entry: CALL main; HALT. run_vm restarts at pc=0 every scan; main RETurns to
        # the HALT, which returns from run_vm back to the runtime's scan loop.
        w.emit(Op.CALL)
        main_patch = w.position()
        w.emit_u16(0)      # back-patched to main's address / parchado a la dir de main
        w.emit(Op.HALT)

        # Emit functions / Emitir funciones
        for fd in prog.functions:
            self._emit_function(fd)

        # Emit procedures / Emitir procedimientos
        for pd in prog.procedures:
            self._emit_procedure(pd)

        # Back-patch CALL main / Parchar CALL al main
        main_name = "main"
        if main_name not in self._proc_addr and prog.procedures:
            main_name = prog.procedures[0].name.lower()
        if main_name in self._proc_addr:
            w.patch_u16(main_patch, self._proc_addr[main_name])

        # Back-patch all CALL instructions / Parchar todas las instrucciones CALL
        for patch_off, name in self._proc_calls:
            w.patch_u16(patch_off, self._proc_addr.get(name.lower(), 0))

    # ── Function / Función ────────────────────────────────────────

    def _emit_function(self, fd: FunctionDecl) -> None:
        w = self._writer
        self._proc_addr[fd.name.lower()] = w.position()
        self._scope = _Scope()

        # Formal parameters are locals (inputs), declared before body vars.
        for p in fd.params:
            self._scope.declare(VarDecl(line=p.line, col=p.col, name=p.name, typ=p.typ, scope="input"))

        for vb in fd.var_blocks:
            for decl in vb.decls:
                self._scope.declare(decl)

        # IEC 61131-3: a FUNCTION returns its value by assigning to a variable
        # named after the function. Declare that return slot so `fname := expr` resolves.
        ret_sym = self._scope.declare(
            VarDecl(line=fd.line, col=fd.col, name=fd.name, typ=fd.return_type, scope="output"))
        prev_ret, self._func_ret = self._func_ret, ret_sym

        for stmt in fd.body:
            self._emit_stmt(stmt)

        # Fall-through return: push the return variable's value, then RET.
        self._emit_load_sym(ret_sym)
        w.emit(Op.RET)
        self._func_ret = prev_ret
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
        end_patches: list[int] = []

        # Main condition / Condición principal
        self._emit_expr(node.cond)
        next_patch = self._emit_jmp(Op.JMPF)   # skip THEN if false

        for stmt in node.then_body:
            self._emit_stmt(stmt)
        end_patches.append(self._emit_jmp(Op.JMP))

        # ELSIF branches / Ramas ELSIF
        for ec, eb in node.elsifs:
            self._patch_jmp(next_patch)
            self._emit_expr(ec)
            next_patch = self._emit_jmp(Op.JMPF)
            for s in eb:
                self._emit_stmt(s)
            end_patches.append(self._emit_jmp(Op.JMP))

        # ELSE / branch / Rama ELSE
        self._patch_jmp(next_patch)
        if node.else_body:
            for s in node.else_body:
                self._emit_stmt(s)

        for p in end_patches:
            self._patch_jmp(p)

    def _emit_while(self, node: WhileStmt) -> None:
        w = self._writer
        loop_start = w.position()

        self._exit_patches.append([])
        self._emit_expr(node.cond)
        exit_patch = self._emit_jmp(Op.JMPF)

        for s in node.body:
            self._emit_stmt(s)

        self._emit_jmp_to(Op.JMP, loop_start)

        self._patch_jmp(exit_patch)
        for p in self._exit_patches.pop():
            self._patch_jmp(p)

    def _emit_repeat(self, node: RepeatStmt) -> None:
        w = self._writer
        loop_start = w.position()

        self._exit_patches.append([])

        for s in node.body:
            self._emit_stmt(s)

        self._emit_expr(node.cond)
        self._emit_jmp_to(Op.JMPF, loop_start)   # loop back while cond is false

        for p in self._exit_patches.pop():
            self._patch_jmp(p)

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
        w.emit(Op.LT if node.downto else Op.GT)   # exit if past the limit
        w.emit_u8(Op.VT_I32)
        exit_patch = self._emit_jnz()

        for s in node.body:
            self._emit_stmt(s)

        # Increment/decrement / Incremento/decremento
        self._emit_load_sym(sym)
        if node.step:
            self._emit_expr(node.step)
        else:
            self._emit_push_int(1)

        w.emit(Op.SUB if node.downto else Op.ADD)
        w.emit_u8(Op.VT_I32)
        self._emit_store_sym(sym)

        self._emit_jmp_to(Op.JMP, loop_start)

        self._patch_jmp(exit_patch)
        for p in self._exit_patches.pop():
            self._patch_jmp(p)

    def _emit_case(self, node: CaseStmt) -> None:
        w = self._writer
        end_patches: list[int] = []

        for labels, body in node.branches:
            # Build OR chain of comparisons / Construir cadena OR de comparaciones
            pass_patches: list[int] = []
            for lv in labels:
                self._emit_expr(node.expr)
                self._emit_push_int(lv)
                w.emit(Op.EQ); w.emit_u8(Op.VT_I32)
                pass_patches.append(self._emit_jnz())

            # None matched — jump over body / Ninguno coincidió — saltar sobre cuerpo
            skip_patch = self._emit_jmp(Op.JMP)

            for p in pass_patches:
                self._patch_jmp(p)

            for s in body:
                self._emit_stmt(s)

            end_patches.append(self._emit_jmp(Op.JMP))
            self._patch_jmp(skip_patch)

        # ELSE body / Cuerpo ELSE
        if node.else_body:
            for s in node.else_body:
                self._emit_stmt(s)

        for p in end_patches:
            self._patch_jmp(p)

    def _emit_return(self, node: ReturnStmt) -> None:
        w = self._writer
        if node.value:
            self._emit_expr(node.value)
        elif self._func_ret is not None:
            # Bare RETURN inside a function still yields the return-slot value.
            self._emit_load_sym(self._func_ret)
        w.emit(Op.RET)

    def _emit_exit(self, node: ExitStmt) -> None:
        patch = self._emit_jmp(Op.JMP)
        if self._exit_patches:
            self._exit_patches[-1].append(patch)

    def _arg_vt(self, node: Node) -> int:
        k = self._expr_type_kind(node)
        if k == _TypeKind.STR:
            return Op.VT_STR
        return Op.VT_F32 if k == _TypeKind.FLOAT else Op.VT_I32

    def _emit_debug_call(self, args) -> None:
        """VM DEBUG: push args, then DEBUG opcode + n + one value-type byte per arg."""
        w = self._writer
        for arg in args:
            self._emit_expr(arg)
        w.emit(Op.DEBUG)
        w.emit_u8(len(args) & 0xFF)
        for arg in args:
            w.emit_u8(self._arg_vt(arg))

    def _emit_call_stmt(self, node: CallStmt) -> None:
        w = self._writer
        name_lo = node.name.lower()

        # Built-in debug print / Impresión de depuración incorporada
        if name_lo == "debug":
            self._emit_debug_call(node.args)
            return

        # Is it a TRAP? / ¿Es un TRAP?
        if name_lo in self._traps:
            for arg in node.args:
                self._emit_expr(arg)
            w.emit(Op.TRAP)
            w.emit(self._traps[name_lo] & 0xFF)
            return

        # Regular procedure call: VM CALL takes a u16 absolute address.
        # (Arguments / frames via LINK are a later milestone.)
        w.emit(Op.CALL)
        self._proc_calls.append((w.position(), name_lo))
        w.emit_u16(0)           # back-patched to the callee's address

    # ── Expressions / Expresiones ─────────────────────────────────

    def _emit_expr(self, node: Node) -> None:
        w = self._writer

        if isinstance(node, IntLit):
            self._emit_push_int(node.value)

        elif isinstance(node, FloatLit):
            self._emit_push_float(node.value)

        elif isinstance(node, BoolLit):
            self._emit_push_int(1 if node.value else 0)

        elif isinstance(node, StrLit):
            # Strings: PUSH_S + inline data (string support is a later milestone).
            w.emit(Op.PUSH_S)
            w.emit_string(node.value)

        elif isinstance(node, VarRef):
            # Check enum values first / Verificar valores enum primero
            ev = self._enums.get(node.name.lower())
            if ev is not None:
                self._emit_push_int(ev)
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

    # ST operator → VM opcode (all typed: a value-type byte follows).
    _BINOP = {
        "+": Op.ADD, "-": Op.SUB, "*": Op.MUL, "/": Op.DIV,
        "AND": Op.AND, "OR": Op.OR, "XOR": Op.XOR,
        "=": Op.EQ, "<>": Op.NE, "<": Op.LT, "<=": Op.LE, ">": Op.GT, ">=": Op.GE,
    }

    def _emit_binop(self, node: BinOp) -> None:
        w = self._writer
        self._emit_expr(node.left)
        self._emit_expr(node.right)

        lk = self._expr_type_kind(node.left)
        rk = self._expr_type_kind(node.right)
        vt = Op.VT_F32 if (lk == _TypeKind.FLOAT or rk == _TypeKind.FLOAT) else Op.VT_I32

        op = node.op
        if op == "%":
            w.emit(Op.MATH); w.emit_u8(Op.MATH_MOD_I)   # integer modulo
            return
        vm_op = self._BINOP.get(op)
        if vm_op is None:
            raise NotImplementedError(f"Unsupported binary operator: {op!r}")
        w.emit(vm_op); w.emit_u8(vt)

    def _emit_unary(self, node: UnaryOp) -> None:
        w = self._writer
        self._emit_expr(node.operand)
        op = node.op
        if op == "-":
            w.emit(Op.NEG); w.emit_u8(self._vt_of(node.operand))
        elif op == "NOT":
            w.emit(Op.NOT); w.emit_u8(Op.VT_I32)
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

        w.emit(Op.CALL)
        self._proc_calls.append((w.position(), name_lo))
        w.emit_u16(0)

    def _emit_ternary(self, node: Ternary) -> None:
        self._emit_expr(node.cond)
        else_patch = self._emit_jmp(Op.JMPF)
        self._emit_expr(node.then_val)
        end_patch = self._emit_jmp(Op.JMP)
        self._patch_jmp(else_patch)
        self._emit_expr(node.else_val)
        self._patch_jmp(end_patch)

    def _emit_debug(self, node: DebugExpr) -> None:
        self._emit_debug_call(node.args)

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
        vt = self._vt(_type_kind(sym.typ))
        w.emit(Op.LOAD_G if sym.scope == "global" else Op.LOAD_L)
        w.emit_u16(sym.offset)
        w.emit_u8(vt)

    def _emit_store_sym(self, sym: _Symbol) -> None:
        w = self._writer
        vt = self._vt(_type_kind(sym.typ))
        w.emit(Op.STORE_G if sym.scope == "global" else Op.STORE_L)
        w.emit_u16(sym.offset)
        w.emit_u8(vt)

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
    """Human-readable P-code dump matching the VM encoding."""
    import struct
    lines: list[str] = []
    i = start_offset
    TYPED = {Op.ADD, Op.SUB, Op.MUL, Op.DIV, Op.NEG, Op.LT, Op.LE, Op.GT, Op.GE,
             Op.EQ, Op.NE, Op.AND, Op.OR, Op.NOT, Op.XOR}
    MEM = {Op.LOAD_G, Op.STORE_G, Op.LOAD_L, Op.STORE_L}
    while i < len(data):
        op = data[i]
        name = _OP_NAMES.get(op, f"0x{op:02X}")
        line = f"  {i:06X}  {name}"
        i += 1
        if op == Op.PUSH_I:
            t = data[i]; val = struct.unpack_from("<i", data, i + 1)[0]
            line += f"  t={t} {val}"; i += 5
        elif op == Op.PUSH_F:
            line += f"  {struct.unpack_from('<f', data, i)[0]:.6g}"; i += 4
        elif op == Op.PUSH_S:
            ln = data[i]; line += f"  str[{ln}]"; i += 1 + ln
        elif op in MEM:
            addr = struct.unpack_from("<H", data, i)[0]; t = data[i + 2]
            line += f"  @{addr} t={t}"; i += 3
        elif op in TYPED:
            line += f"  t={data[i]}"; i += 1
        elif op in (Op.JMP, Op.JMPF):
            off = struct.unpack_from("<h", data, i)[0]
            line += f"  {off:+d} -> {i + 2 + off:#06x}"; i += 2
        elif op == Op.CALL:
            line += f"  ->{struct.unpack_from('<H', data, i)[0]}"; i += 2
        elif op == Op.LINK:
            line += f"  frame={struct.unpack_from('<H', data, i)[0]} pbytes={struct.unpack_from('<H', data, i + 2)[0]}"; i += 4
        elif op == Op.LEAVE:
            line += f"  ret={data[i]}"; i += 1
        elif op == Op.TRAP:
            line += f"  #{data[i]}"; i += 1
        elif op == Op.MATH:
            line += f"  sub={data[i]}"; i += 1
        elif op == Op.DEBUG:
            n = data[i]; types = list(data[i + 1:i + 1 + n]); line += f"  n={n} {types}"; i += 1 + n
        lines.append(line)
    return "\n".join(lines)
