"""
osoLogic — osoST Python Compiler
ostc/parser.py — Recursive-descent parser for IEC 61131-3 ST
                 (STLite dialect with TRAP, ternary ? :, and DEBUG)

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

Grammar overview / Resumen de gramática:
  program        = (var_global | type_decl | trap_decl
                   | function_decl | procedure_decl)*
  function_decl  = FUNCTION id COLON type var_block* stmt* END_FUNCTION
  procedure_decl = PROCEDURE id [params] var_block* stmt* END_PROCEDURE
  stmt           = assign_stmt | if_stmt | case_stmt | while_stmt
                 | repeat_stmt | for_stmt | return_stmt | exit_stmt
                 | call_stmt
  expr           = ternary | or_expr
  or_expr        = and_expr (OR and_expr)*
  and_expr       = not_expr (AND not_expr)*
  not_expr       = NOT not_expr | cmp_expr
  cmp_expr       = add_expr (('='|'<>'|'<'|'<='|'>'|'>=') add_expr)*
  add_expr       = mul_expr (('+'|'-') mul_expr)*
  mul_expr       = unary (('*'|'/'|'%'|MOD|XOR) unary)*
  unary          = '-' unary | primary
  primary        = literal | var_or_call | '(' expr ')'
                 | DEBUG '(' args ')' | ARRAY_INDEX
"""

from __future__ import annotations
from typing import Optional

from .lexer import TokenStream, LexError, Token
from .ast_nodes import (
    Node, SimpleType, StringType, ArrayType,
    VarDecl, VarBlock, Param, TrapDecl,
    FunctionDecl, ProcedureDecl, EnumType, Program,
    AssignStmt, IfStmt, CaseStmt, WhileStmt, RepeatStmt,
    ForStmt, ReturnStmt, ExitStmt, CallStmt,
    IntLit, FloatLit, BoolLit, StrLit, VarRef, ArrayIndex,
    BinOp, UnaryOp, CallExpr, Ternary, DebugExpr,
)


class ParseError(Exception):
    """Raised on syntax error. / Lanzada en error de sintaxis."""
    def __init__(self, msg: str, line: int = 0, col: int = 0):
        super().__init__(f"Parse error at {line}:{col}: {msg} / "
                         f"Error sintáctico en {line}:{col}: {msg}")
        self.line = line
        self.col  = col


class Parser:
    """
    Recursive-descent parser.
    Returns a Program AST node.

    Parser de descenso recursivo.
    Devuelve un nodo AST Program.
    """

    def __init__(self, tokens: list[Token]):
        self._ts = TokenStream(tokens)

    # ── Public entry / Entrada pública ────────────────────────────

    def parse(self) -> Program:
        """Parse the full token stream and return a Program node."""
        global_vars: list[VarBlock] = []
        types:       list[EnumType] = []
        traps:       list[TrapDecl] = []
        functions:   list[FunctionDecl]   = []
        procedures:  list[ProcedureDecl]  = []

        while not self._ts.eof:
            k = self._ts.peek_kind()
            if k == "VAR_GLOBAL":
                global_vars.append(self._var_block("global"))
            elif k == "TYPE":
                types.extend(self._type_section())
            elif k == "TRAP":
                traps.append(self._trap_decl())
            elif k == "FUNCTION":
                functions.append(self._function_decl())
            elif k == "PROCEDURE":
                procedures.append(self._procedure_decl())
            else:
                tok = self._ts.next()
                self._err(f"Unexpected top-level token: {tok.kind!r} ({tok.value!r})",
                          tok)

        return Program(
            line=0, col=0,
            global_vars=tuple(global_vars),
            types=tuple(types),
            traps=tuple(traps),
            functions=tuple(functions),
            procedures=tuple(procedures),
        )

    # ── Error helper / Ayudante de error ─────────────────────────

    def _err(self, msg: str, tok: Token | None = None) -> ParseError:
        line = tok.line if tok else 0
        col  = tok.col  if tok else 0
        raise ParseError(msg, line, col)

    # ── Type parsing / Análisis de tipos ─────────────────────────

    _BUILTIN_TYPES = {
        "T_BOOL", "T_SINT", "T_USINT", "T_INT", "T_UINT",
        "T_DINT", "T_UDINT", "T_LONG", "T_ULONG",
        "T_REAL", "T_LREAL", "T_FLOAT", "T_BYTE", "T_WORD", "T_DWORD",
    }

    def _type_ref(self) -> Node:
        """Parse a type reference. / Analizar una referencia de tipo."""
        k = self._ts.peek_kind()

        if k == "ARRAY":
            return self._array_type()

        if k == "T_STRING":
            self._ts.next()
            max_len = 80
            if self._ts.peek_kind() == "LBRACKET":
                self._ts.next()
                tok = self._ts.expect("INT_LIT")
                max_len = int(tok.value)
                self._ts.expect("RBRACKET")
            return StringType(line=0, col=0, max_len=max_len)

        if k in self._BUILTIN_TYPES or k == "ID":
            tok = self._ts.next()
            name = tok.value if k == "ID" else k[2:]   # strip T_ prefix for display
            return SimpleType(line=tok.line, col=tok.col, name=tok.value if k == "ID" else k)

        tok = self._ts.peek()
        self._err(f"Expected type, got {k!r}", tok)

    def _array_type(self) -> ArrayType:
        """ARRAY [ lo..hi , ... ] OF type"""
        t0 = self._ts.expect("ARRAY")
        self._ts.expect("LBRACKET")
        dims: list[tuple[int, int]] = []
        while True:
            lo_tok = self._ts.expect("INT_LIT")
            self._ts.expect("DOTDOT")
            hi_tok = self._ts.expect("INT_LIT")
            dims.append((int(lo_tok.value), int(hi_tok.value)))
            if not self._ts.match("COMMA"):
                break
        self._ts.expect("RBRACKET")
        self._ts.expect("OF")
        elem = self._type_ref()
        return ArrayType(line=t0.line, col=t0.col,
                         dimensions=tuple(dims), element_type=elem)

    # ── Variable blocks / Bloques de variables ────────────────────

    _VAR_SECTION_START = {
        "VAR": "local", "VAR_GLOBAL": "global",
        "VAR_INPUT": "input", "VAR_OUTPUT": "output",
        "VAR_IN_OUT": "inout",
    }
    _VAR_SECTION_END = "END_VAR"

    def _var_block(self, scope: str) -> VarBlock:
        """Parse VAR ... END_VAR (the opening keyword has already been consumed or passed)."""
        # Consume the opening keyword
        tok = self._ts.next()
        line, col = tok.line, tok.col
        decls: list[VarDecl] = []

        while self._ts.peek_kind() != self._VAR_SECTION_END:
            if self._ts.eof:
                self._err("Unterminated VAR block / Bloque VAR sin cerrar")
            d = self._var_decl(scope)
            decls.append(d)

        self._ts.expect(self._VAR_SECTION_END)
        return VarBlock(line=line, col=col, scope=scope, decls=tuple(decls))

    def _var_decl(self, scope: str) -> VarDecl:
        """name : type [:= init_expr] ;"""
        name_tok = self._ts.expect("ID")
        self._ts.expect("COLON")
        typ = self._type_ref()
        init: Optional[Node] = None
        if self._ts.match("ASSIGN"):
            init = self._expr()
        self._ts.expect("SEMICOLON")
        return VarDecl(line=name_tok.line, col=name_tok.col,
                       name=str(name_tok.value), typ=typ,
                       scope=scope, init=init)

    def _var_blocks_until(self, stop_kinds: set[str]) -> list[VarBlock]:
        """Collect zero or more VAR* blocks until a stop keyword."""
        blocks: list[VarBlock] = []
        while self._ts.peek_kind() in self._VAR_SECTION_START:
            k = self._ts.peek_kind()
            scope = self._VAR_SECTION_START[k]
            blocks.append(self._var_block(scope))
        return blocks

    # ── TYPE section / Sección TYPE ───────────────────────────────

    def _type_section(self) -> list[EnumType]:
        """TYPE name : ( A, B, C ); END_TYPE"""
        self._ts.expect("TYPE")
        enums: list[EnumType] = []
        while self._ts.peek_kind() != "END_TYPE":
            if self._ts.eof:
                self._err("Unterminated TYPE section / Sección TYPE sin cerrar")
            name_tok = self._ts.expect("ID")
            self._ts.expect("COLON")
            self._ts.expect("LPAREN")
            values: list[str] = []
            while True:
                v = self._ts.expect("ID")
                values.append(str(v.value))
                if not self._ts.match("COMMA"):
                    break
            self._ts.expect("RPAREN")
            self._ts.expect("SEMICOLON")
            enums.append(EnumType(line=name_tok.line, col=name_tok.col,
                                   name=str(name_tok.value), values=tuple(values)))
        self._ts.expect("END_TYPE")
        return enums

    # ── TRAP declaration / Declaración TRAP ───────────────────────

    def _trap_decl(self) -> TrapDecl:
        """
        PROCEDURE name ( params ) TRAP # N ;
        (STLite extension — maps ST symbol to hardware() trap ID)
        (Extensión STLite — mapea símbolo ST a ID de trap en hardware())
        """
        self._ts.expect("TRAP")           # keyword TRAP used as declaration prefix
        name_tok = self._ts.expect("ID")
        params   = self._param_list()

        ret: Optional[Node] = None
        if self._ts.match("COLON"):
            ret = self._type_ref()

        self._ts.expect("TRAP")           # keyword TRAP again before #N
        self._ts.expect("HASH")
        id_tok = self._ts.expect("INT_LIT")

        self._ts.expect("SEMICOLON")
        return TrapDecl(line=name_tok.line, col=name_tok.col,
                        name=str(name_tok.value), params=params,
                        return_type=ret, trap_id=int(id_tok.value))

    # ── FUNCTION declaration / Declaración de FUNCTION ───────────

    def _function_decl(self) -> FunctionDecl:
        """FUNCTION name : type  var_block*  stmt*  END_FUNCTION"""
        self._ts.expect("FUNCTION")
        name_tok = self._ts.expect("ID")
        self._ts.expect("COLON")
        ret_type = self._type_ref()

        var_blocks = self._var_blocks_until({"END_FUNCTION"})
        body       = self._stmt_list({"END_FUNCTION"})
        self._ts.expect("END_FUNCTION")

        return FunctionDecl(line=name_tok.line, col=name_tok.col,
                            name=str(name_tok.value), params=(),
                            return_type=ret_type,
                            var_blocks=tuple(var_blocks), body=tuple(body))

    # ── PROCEDURE declaration / Declaración de PROCEDURE ─────────

    def _procedure_decl(self) -> ProcedureDecl:
        """PROCEDURE name [( params )]  var_block*  stmt*  END_PROCEDURE"""
        self._ts.expect("PROCEDURE")
        name_tok = self._ts.expect("ID")

        params: tuple[Param, ...] = ()
        if self._ts.peek_kind() == "LPAREN":
            params = self._param_list()

        var_blocks = self._var_blocks_until({"END_PROCEDURE"})
        body       = self._stmt_list({"END_PROCEDURE"})
        self._ts.expect("END_PROCEDURE")

        return ProcedureDecl(line=name_tok.line, col=name_tok.col,
                             name=str(name_tok.value), params=params,
                             var_blocks=tuple(var_blocks), body=tuple(body))

    def _param_list(self) -> tuple[Param, ...]:
        """( name : type , ... ) — formal parameter list"""
        self._ts.expect("LPAREN")
        params: list[Param] = []
        if self._ts.peek_kind() != "RPAREN":
            while True:
                n = self._ts.expect("ID")
                self._ts.expect("COLON")
                t = self._type_ref()
                params.append(Param(line=n.line, col=n.col,
                                    name=str(n.value), typ=t))
                # IEC declarations separate with ';'; also accept ',' for convenience.
                if not (self._ts.match("SEMICOLON") or self._ts.match("COMMA")):
                    break
        self._ts.expect("RPAREN")
        return tuple(params)

    # ── Statement list / Lista de sentencias ──────────────────────

    def _stmt_list(self, stop_kinds: set[str]) -> list[Node]:
        """Parse statements until one of stop_kinds is peeked."""
        stmts: list[Node] = []
        while not self._ts.eof and self._ts.peek_kind() not in stop_kinds:
            # Skip bare semicolons (trailing ';' after END_IF, END_WHILE, etc.)
            # Saltar punto y coma sueltos (';' tras END_IF, END_WHILE, etc.)
            if self._ts.peek_kind() == "SEMICOLON":
                self._ts.next()
                continue
            stmts.append(self._stmt())
        return stmts

    def _stmt(self) -> Node:
        """Dispatch to the right statement parser."""
        k = self._ts.peek_kind()

        if k == "IF":
            return self._if_stmt()
        if k == "CASE":
            return self._case_stmt()
        if k == "WHILE":
            return self._while_stmt()
        if k == "REPEAT":
            return self._repeat_stmt()
        if k == "FOR":
            return self._for_stmt()
        if k == "RETURN":
            return self._return_stmt()
        if k == "EXIT":
            tok = self._ts.next()
            self._ts.expect("SEMICOLON")
            return ExitStmt(line=tok.line, col=tok.col)
        if k == "ID":
            return self._assign_or_call()

        if k == "DEBUG":
            # debug(...) used as a statement / debug(...) usado como sentencia
            expr = self._debug_expr()
            self._ts.expect("SEMICOLON")
            return CallStmt(line=expr.line, col=expr.col,
                            name="debug", args=expr.args)

        tok = self._ts.next()
        self._err(f"Unexpected token in statement: {tok.kind!r} ({tok.value!r})", tok)

    # ── Individual statements / Sentencias individuales ───────────

    def _assign_or_call(self) -> Node:
        """
        name := expr ;          → AssignStmt
        name [ idx ] := expr ;  → AssignStmt (array element)
        name ( args ) ;         → CallStmt
        """
        name_tok = self._ts.expect("ID")
        name = str(name_tok.value)

        if self._ts.peek_kind() == "LPAREN":
            # Procedure call / Llamada a procedimiento
            args = self._call_args()
            self._ts.expect("SEMICOLON")
            return CallStmt(line=name_tok.line, col=name_tok.col,
                            name=name, args=args)

        # Build LHS (may include array indexing)
        lhs: Node = VarRef(line=name_tok.line, col=name_tok.col, name=name)
        while self._ts.peek_kind() == "LBRACKET":
            self._ts.next()
            idx = self._expr_list()
            self._ts.expect("RBRACKET")
            lhs = ArrayIndex(line=name_tok.line, col=name_tok.col,
                             base=lhs, indices=idx)

        self._ts.expect("ASSIGN")
        value = self._expr()
        self._ts.expect("SEMICOLON")
        return AssignStmt(line=name_tok.line, col=name_tok.col,
                          target=lhs, value=value)

    def _if_stmt(self) -> IfStmt:
        """IF cond THEN stmts [ELSIF cond THEN stmts]* [ELSE stmts] END_IF"""
        tok = self._ts.expect("IF")
        cond = self._expr()
        self._ts.expect("THEN")
        then_body = self._stmt_list({"ELSIF", "ELSE", "END_IF"})

        elsifs: list[tuple[Node, tuple[Node, ...]]] = []
        while self._ts.match("ELSIF"):
            ec = self._expr()
            self._ts.expect("THEN")
            eb = self._stmt_list({"ELSIF", "ELSE", "END_IF"})
            elsifs.append((ec, tuple(eb)))

        else_body: Optional[tuple[Node, ...]] = None
        if self._ts.match("ELSE"):
            else_body = tuple(self._stmt_list({"END_IF"}))

        self._ts.expect("END_IF")
        return IfStmt(line=tok.line, col=tok.col,
                      cond=cond, then_body=tuple(then_body),
                      elsifs=tuple(elsifs), else_body=else_body)

    def _case_stmt(self) -> CaseStmt:
        """CASE expr OF int_val: stmts ... [ELSE stmts] END_CASE"""
        tok = self._ts.expect("CASE")
        expr = self._expr()
        self._ts.expect("OF")

        branches: list[tuple[tuple[int, ...], tuple[Node, ...]]] = []
        stop = {"ELSE", "END_CASE"}

        while self._ts.peek_kind() not in stop and not self._ts.eof:
            # Collect one or more integer labels / Recolectar uno o más etiquetas enteras
            labels: list[int] = []
            while True:
                lv = self._ts.expect("INT_LIT")
                labels.append(int(lv.value))
                if not self._ts.match("COMMA"):
                    break
            self._ts.expect("COLON")
            body = self._stmt_list({"INT_LIT", "ELSE", "END_CASE"})
            branches.append((tuple(labels), tuple(body)))

        else_body: Optional[tuple[Node, ...]] = None
        if self._ts.match("ELSE"):
            else_body = tuple(self._stmt_list({"END_CASE"}))

        self._ts.expect("END_CASE")
        return CaseStmt(line=tok.line, col=tok.col,
                        expr=expr, branches=tuple(branches), else_body=else_body)

    def _while_stmt(self) -> WhileStmt:
        """WHILE cond DO stmts END_WHILE"""
        tok = self._ts.expect("WHILE")
        cond = self._expr()
        self._ts.expect("DO")
        body = self._stmt_list({"END_WHILE"})
        self._ts.expect("END_WHILE")
        return WhileStmt(line=tok.line, col=tok.col,
                         cond=cond, body=tuple(body))

    def _repeat_stmt(self) -> RepeatStmt:
        """REPEAT stmts UNTIL cond END_REPEAT"""
        tok = self._ts.expect("REPEAT")
        body = self._stmt_list({"UNTIL"})
        self._ts.expect("UNTIL")
        cond = self._expr()
        self._ts.expect("END_REPEAT")
        return RepeatStmt(line=tok.line, col=tok.col,
                          body=tuple(body), cond=cond)

    def _for_stmt(self) -> ForStmt:
        """FOR var := start TO|DOWNTO end [BY step] DO stmts END_FOR"""
        tok = self._ts.expect("FOR")
        var_tok = self._ts.expect("ID")
        self._ts.expect("ASSIGN")
        start = self._expr()

        downto = False
        if self._ts.peek_kind() == "DOWNTO":
            self._ts.next()
            downto = True
        else:
            self._ts.expect("TO")

        end = self._expr()

        step: Optional[Node] = None
        if self._ts.match("BY"):
            step = self._expr()

        self._ts.expect("DO")
        body = self._stmt_list({"END_FOR"})
        self._ts.expect("END_FOR")

        return ForStmt(line=tok.line, col=tok.col,
                       var=str(var_tok.value), start=start, end=end,
                       step=step, downto=downto, body=tuple(body))

    def _return_stmt(self) -> ReturnStmt:
        """RETURN [expr] ;"""
        tok = self._ts.expect("RETURN")
        value: Optional[Node] = None
        if self._ts.peek_kind() != "SEMICOLON":
            value = self._expr()
        self._ts.expect("SEMICOLON")
        return ReturnStmt(line=tok.line, col=tok.col, value=value)

    # ── Expression / Expresión ────────────────────────────────────
    # Operator precedence (low → high) / Precedencia de operadores:
    #   ternary → OR → AND → NOT → comparison → add → mul → unary → primary

    def _expr(self) -> Node:
        """Top-level expression: may be a ternary."""
        return self._ternary()

    def _ternary(self) -> Node:
        """cond ? then_val : else_val (STLite extension)"""
        node = self._or_expr()
        if self._ts.match("QUESTION"):
            then_val = self._or_expr()
            self._ts.expect("COLON")
            else_val = self._or_expr()
            node = Ternary(line=node.line, col=node.col,
                           cond=node, then_val=then_val, else_val=else_val)
        return node

    def _or_expr(self) -> Node:
        left = self._and_expr()
        while self._ts.match("OR"):
            right = self._and_expr()
            left  = BinOp(line=left.line, col=left.col,
                          op="OR", left=left, right=right)
        return left

    def _and_expr(self) -> Node:
        left = self._xor_expr()
        while self._ts.match("AND"):
            right = self._xor_expr()
            left  = BinOp(line=left.line, col=left.col,
                          op="AND", left=left, right=right)
        return left

    def _xor_expr(self) -> Node:
        left = self._not_expr()
        while self._ts.match("XOR"):
            right = self._not_expr()
            left  = BinOp(line=left.line, col=left.col,
                          op="XOR", left=left, right=right)
        return left

    def _not_expr(self) -> Node:
        if self._ts.peek_kind() == "NOT":
            tok = self._ts.next()
            operand = self._not_expr()
            return UnaryOp(line=tok.line, col=tok.col, op="NOT", operand=operand)
        return self._cmp_expr()

    _CMP_OPS = {"EQ": "=", "NE": "<>", "LT": "<", "LE": "<=", "GT": ">", "GE": ">="}

    def _cmp_expr(self) -> Node:
        left = self._add_expr()
        while self._ts.peek_kind() in self._CMP_OPS:
            tok = self._ts.next()
            op  = self._CMP_OPS[tok.kind]
            right = self._add_expr()
            left  = BinOp(line=left.line, col=left.col,
                          op=op, left=left, right=right)
        return left

    def _add_expr(self) -> Node:
        left = self._mul_expr()
        while self._ts.peek_kind() in ("PLUS", "MINUS"):
            tok = self._ts.next()
            op  = "+" if tok.kind == "PLUS" else "-"
            right = self._mul_expr()
            left  = BinOp(line=left.line, col=left.col,
                          op=op, left=left, right=right)
        return left

    def _mul_expr(self) -> Node:
        left = self._unary()
        while self._ts.peek_kind() in ("STAR", "SLASH", "PERCENT", "MOD"):
            tok = self._ts.next()
            op_map = {"STAR": "*", "SLASH": "/", "PERCENT": "%", "MOD": "%"}
            op    = op_map[tok.kind]
            right = self._unary()
            left  = BinOp(line=left.line, col=left.col,
                          op=op, left=left, right=right)
        return left

    def _unary(self) -> Node:
        if self._ts.peek_kind() == "MINUS":
            tok = self._ts.next()
            operand = self._unary()
            return UnaryOp(line=tok.line, col=tok.col, op="-", operand=operand)
        return self._primary()

    def _primary(self) -> Node:
        k = self._ts.peek_kind()

        if k == "INT_LIT":
            tok = self._ts.next()
            return IntLit(line=tok.line, col=tok.col, value=int(tok.value))

        if k == "FLOAT_LIT":
            tok = self._ts.next()
            return FloatLit(line=tok.line, col=tok.col, value=float(tok.value))

        if k == "BOOL_LIT":
            tok = self._ts.next()
            return BoolLit(line=tok.line, col=tok.col, value=bool(tok.value))

        if k == "STRING_LIT":
            tok = self._ts.next()
            return StrLit(line=tok.line, col=tok.col, value=str(tok.value))

        if k == "LPAREN":
            self._ts.next()
            node = self._expr()
            self._ts.expect("RPAREN")
            return node

        if k == "DEBUG":
            return self._debug_expr()

        if k == "ID":
            return self._var_or_call()

        tok = self._ts.peek()
        self._err(f"Expected expression, got {k!r}", tok)

    def _var_or_call(self) -> Node:
        """
        name            → VarRef
        name ( args )   → CallExpr
        name [ idx ]    → ArrayIndex
        """
        name_tok = self._ts.expect("ID")
        name = str(name_tok.value)

        if self._ts.peek_kind() == "LPAREN":
            args = self._call_args()
            return CallExpr(line=name_tok.line, col=name_tok.col,
                            name=name, args=args)

        node: Node = VarRef(line=name_tok.line, col=name_tok.col, name=name)
        while self._ts.peek_kind() == "LBRACKET":
            self._ts.next()
            indices = self._expr_list()
            self._ts.expect("RBRACKET")
            node = ArrayIndex(line=name_tok.line, col=name_tok.col,
                              base=node, indices=indices)
        return node

    def _debug_expr(self) -> DebugExpr:
        """DEBUG ( arg, ... )"""
        tok = self._ts.expect("DEBUG")
        args = self._call_args()
        return DebugExpr(line=tok.line, col=tok.col, args=args)

    def _call_args(self) -> tuple[Node, ...]:
        """( [expr , ...] )"""
        self._ts.expect("LPAREN")
        args: list[Node] = []
        if self._ts.peek_kind() != "RPAREN":
            args.append(self._expr())
            while self._ts.match("COMMA"):
                args.append(self._expr())
        self._ts.expect("RPAREN")
        return tuple(args)

    def _expr_list(self) -> tuple[Node, ...]:
        """expr , expr , ... (for array indices without surrounding parens)"""
        items = [self._expr()]
        while self._ts.match("COMMA"):
            items.append(self._expr())
        return tuple(items)
