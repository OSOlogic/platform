# ostc → pcodevm retarget spec

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · AGPL-3.0-or-later

The pure-Python compiler `ostc` currently emits its **own** bytecode scheme, which the C runtime VM
(`runtime/pcodevm.c`) does **not** execute (`opcode desconocido …`). Only the Java **STLite** compiler
targets this VM. To make the no-Java backend *deployable* (not just a syntax check), `ostc`'s codegen
must emit the VM's opcodes and encoding **byte-for-byte**. This is that spec — the VM is authoritative.

Verified already: the runtime **container/header** format is fine (the loader reads ostc HEX:
`code=… globals=…`), and the demo runtime builds + runs. The gap is purely the **instruction encoding**.

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

## Milestones (verify each by running on `osoruntime_demo`)

1. **Single `main`, globals only** — counter.st: PUSH_I(t), LOAD_G/STORE_G(addr,t), typed ADD, typed
   GE/LT, JMP/JMPF relative, HALT. → must print and loop without `opcode desconocido`.
2. **Traps** — blink/tag programs: TRAP #12/#30/#31 (the osodb path). Needs #1 + TRAP.
3. **Procedures with params/locals** — LINK/UNLINK/LEAVE + LOAD_L/STORE_L. pid.st (functions).
4. **Casts / MOD / arrays** — CAST_*, MATH subops, *_GA/*_LA.

Do it milestone by milestone, re-running the example after each so a wrong byte is caught immediately.
Keep ostc's lexer/parser/AST as-is; only `codegen.py` (+ `hex_writer` jump patching) change.
