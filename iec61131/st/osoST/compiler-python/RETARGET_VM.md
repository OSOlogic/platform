# ostc → pcodevm retarget spec

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · AGPL-3.0-or-later

> **Status: M1–M4 DONE and verified running on `osoruntime`.** The pure-Python compiler `ostc`
> now emits bytecode the C VM (`runtime/pcodevm.c`) executes — the no-Java backend is **deployable**
> for relay logic, arithmetic, timers/counters/PID, sub-routines, the osodb tag path, **casts &
> mixed INT/REAL math, arrays, and strings**. This document is the map that got us there; the VM is
> authoritative.

Historically `ostc` emitted its **own** bytecode scheme the VM didn't run (`opcode desconocido …`),
so only Java **STLite** targeted the VM. The retarget below aligned `ostc`'s codegen to the VM's
opcodes/encoding byte-for-byte (only `codegen.py` + `hex_writer` jump patching changed).

The runtime **container/header** format was already fine (the loader reads ostc HEX: `code=… globals=…`);
the gap was purely the **instruction encoding**.

## Value types (operand byte `t`, from `pcodevm.h`)

`VT_S8=0  VT_U8=1  VT_S16=2  VT_U16=3  VT_I32=4  VT_F32=5  VT_STR=6`. `tbytes`: 1/1/2/2/4/4/2.
Map ostc types → VT: BOOL→`VT_U8` (or S16; pick one and be consistent), INT/SINT→S16, DINT→I32,
UINT→U16, REAL→F32, STRING→STR.

## Opcode + encoding map (ostc emit → VM)

Operand readers in the VM: `cu8` (1B), `cu16` (2B), `ci16` (signed 2B, **relative** for jumps),
`ci32` (4B). Stack slots are typed; `push_ram/store_ram(addr,t)`.

| Operation | VM opcode | # | Operands the VM reads | Notes |
|---|---|---|---|---|
| push int literal | `PUSH_I` | 1 | `t`(u8) then value sized by t (S8→i8, S16→i16, I32→i32…) | **variable width by type** |
| push float | `PUSH_F` | 2 | f32 (4B) | |
| push string idx | `PUSH_S` | 3 | u16 | |
| load global | `LOAD_G` | 10 | `addr`(u16), `t`(u8) | not i32 offset; **add type byte** |
| store global | `STORE_G` | 11 | `addr`(u16), `t`(u8) | |
| load local | `LOAD_L` | 12 | `off`(u16), `t`(u8) | frame-relative via `local_addr` |
| store local | `STORE_L` | 13 | `off`(u16), `t`(u8) | |
| add/sub/mul/div/neg | `ADD..NEG` | 20-24 | `t`(u8) | typed; F32 handled inside |
| lt/le/gt/ge/eq/ne | `LT..NE` | 30-35 | `t`(u8) | `cmp_op(t, code)` |
| and/or/not/xor | `AND..XOR` | 40-43 | `t`(u8) | truthiness by t |
| jump | `JMP` | 50 | `ci16` **relative** to next pc | offset = target − (pc after operand) |
| jump if false | `JMPF` | 51 | `ci16` relative | pops truthy |
| call | `CALL` | 60 | `addr`(u16) **absolute** | pushes return pc (2B); no argc byte |
| return | `RET` | 61 | — | pops 2B return addr |
| halt | `HALT` | 62 | — | program entry runs from pc=0 |
| link frame | `LINK` | 63 | `frame`(u16), `pbytes`(u16) | see calling convention |
| unlink | `UNLINK` | 64 | — | |
| leave (ret value) | `LEAVE` | 65 | `retbytes`(u8) | function return-value path |
| trap | `TRAP` | 80 | `trap_id`(u8) | our tag_read/#30, tag_write/#31, millis/#12 |
| mov global imm32 | `MOV_GI32` | 91 | `addr`(u16), `i32` | peephole (optional) |

ostc extras with **no VM equivalent** (must be lowered, not emitted): `POP`, `DUP`, `I2F/F2I`
(use VM `CAST_*` 103-108), `MOD_I` (VM `MATH` subop `MATH_MOD_I=106`), array ops (VM `*_GA/*_LA`).

## Calling convention (`FRAME_HEADER_BYTES = 6`)

- Program starts at **pc=0** (osoruntime drives `run_vm` from 0). Emit the entry (call `main`, then
  `HALT`) at pc=0, or place `main` first.
- A procedure/function with locals: caller pushes args (typed), emits `CALL addr`. Callee begins with
  `LINK frame_bytes, pbytes` (copies pbytes of args above the 6-byte header, zeroes the rest of the
  frame). Access params/locals via `LOAD_L/STORE_L off,t`. Return: `UNLINK` then `RET` (procedure), or
  `LEAVE retbytes` (function leaves the return value). `fp`/`sp` are byte offsets into RAM.
- A procedure with **no locals and no params** (e.g. a global-only `main`): no `LINK` needed — body +
  `RET`/`HALT`. **Start here to prove the pipeline** (counter.st is exactly this shape).

## Jump patching

VM jumps are **relative i16**. Emit a placeholder `ci16=0`, remember the operand offset, and on
patch write `target − (operand_offset + 2)`. (ostc currently patches absolute i32 — change
`patch_i32` sites to `patch_i16_relative`.)

## Milestones (verified by running on `osoruntime_demo`)

1. ✅ **Single `main`, globals only** (commit 99dc6db) — PUSH_I(t), LOAD_G/STORE_G(addr,t), typed
   ADD/GE/LT, relative JMP/JMPF, HALT, entry `CALL main; HALT`, global initializers, `debug` builtin.
   *counter.st runs, prints the counter each scan, HALTs clean.*
2. ✅ **Traps / osodb** (verified with M1) — TRAP #30/#31 fire; a Ladder tag program runs and the
   **ACL is enforced** end-to-end (`tag_write` to a read-only binding denied).
3. ✅ **Procedures/functions with params & locals** (commit 5445782) — LINK/UNLINK/LEAVE calling
   convention, frame-relative LOAD_L/STORE_L (offset + 6), params-first layout, args pushed by the
   caller, functions LEAVE their return value. **BOOL is 1 byte (VT_U8)** so bare-bool conditions
   work. *fn(params)=115, pid.st (REAL params, float math)=20.46, Ladder sub-ladder CALL all run.*
4. ✅ **DONE — casts, MOD, arrays, strings.**
   - ✅ **Casts** — `CAST_F32`/`CAST_I32` (the opcode carries the *source* value-type byte). Operands
     of mixed INT/REAL arithmetic/comparisons are promoted to a common kind, assignments coerce the
     value to the target type, and explicit IEC conversions (`TO_REAL`, `INT_TO_REAL`, `REAL_TO_INT`,
     `TRUNC`, …) lower to one cast. Function return kinds are pre-scanned so a call in a float
     expression types correctly. *cast.st runs: `7*2.5=17.5`, `TO_REAL(7)+0.5=7.5`, `REAL_TO_INT(17.5)=17`.*
   - ✅ **MOD** — `%` lowers to `MATH`+`MATH_MOD_I` with integer operands.
   - ✅ **Arrays** — `LOAD_GA`/`STORE_GA` (global) and `LOAD_LA`/`STORE_LA` (local). The symbol is sized
     `n × elem_bytes` (multi-dim aware); an access pushes the indices, then emits the op with base(u16),
     element type(u8), ndims(u8) and per-dimension `(lo, hi, stride)` i32×3 (row-major strides). Stores
     push the value first, then the indices. Non-zero lower bounds and REAL elements handled.
     *array.st: fill+sum `[0..4]` = 100, `a[2]` = 20; array_real.st: `[1..3] OF REAL` sum = 9.0, `r[3]` = 4.5.*
   - ✅ **Strings** — literals live in a const string pool (`[len][chars]`) appended after the code;
     `PUSH_S` pushes the back-patched code-segment offset (a string pointer). STRING variables hold a
     2-byte `VT_STR` pointer (init/assign store the pointer, not the bytes); comparisons use the VM's
     `str_cmp`. The lexer now accepts IEC single-quoted strings (`'x'`) as well as `"x"`.
     *string.st → hello / world; string_compare.st → `msg='on'` matched=1, `'off'`=0.*

Kept ostc's lexer/parser/AST unchanged; only `codegen.py` (+ `hex_writer` patch_i16/patch_u16) changed.
