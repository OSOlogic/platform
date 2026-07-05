/*
 * osoLogic — IEC 61131-3 Structured Text Runtime
 * pcodevm.h — Shared types, VM struct, and public API
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Include this header in:
 *   - pcodevm.c      (VM implementation)
 *   - hardware_*.c   (HAL implementations — needs VM stack access)
 *   - osoruntime.c   (main loop and scan cycle)
 *
 * Incluir este fichero en:
 *   - pcodevm.c      (implementación de la VM)
 *   - hardware_*.c   (implementaciones HAL — necesitan acceso a la pila VM)
 *   - osoruntime.c   (bucle principal y ciclo de scan)
 */

#ifndef PCODEVM_H
#define PCODEVM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * VM configuration / Configuración de la VM
 * ================================================================ */

/** Maximum number of simultaneous breakpoints.
 *  Número máximo de breakpoints simultáneos. */
#define VM_MAX_BREAKPOINTS 8

/** Execution mode flags (combinable with bitwise OR).
 *  Flags de modo de ejecución (combinables con OR). */
#define VM_MODE_NORMAL  0x00u   /**< Normal execution, no overhead */
#define VM_MODE_DEBUG   0x01u   /**< Enable breakpoints and vm_stop() */
#define VM_MODE_TRACE   0x02u   /**< Print pc/sp/fp each instruction */
#define VM_MODE_METRICS 0x04u   /**< Count executed instructions */

/* ================================================================
 * Value types / Tipos de valor en la pila
 * ================================================================ */

/** VM stack value types used in typed opcodes.
 *  Tipos de valor de la pila usados en opcodes tipados. */
enum VmType {
    VT_S8  = 0,  /**< Signed   8-bit integer  / Entero con signo 8 bits  */
    VT_U8  = 1,  /**< Unsigned 8-bit integer  / Entero sin signo 8 bits  */
    VT_S16 = 2,  /**< Signed  16-bit integer  / Entero con signo 16 bits */
    VT_U16 = 3,  /**< Unsigned 16-bit integer / Entero sin signo 16 bits */
    VT_I32 = 4,  /**< Signed  32-bit integer  / Entero con signo 32 bits */
    VT_F32 = 5,  /**< 32-bit IEEE-754 float   / Flotante 32 bits         */
    VT_STR = 6,  /**< String pointer (16-bit) / Puntero de cadena 16 bits*/
};

/* ================================================================
 * VM state / Estado de la máquina virtual
 * ================================================================
 *
 * Memory layout / Disposición de memoria:
 *
 *   RAM[0 .. global_bytes)         — Global variables / Variables globales
 *   RAM[global_bytes .. sp)        — Call stack frames / Marcos de llamada
 *   RAM[sp .. str_temp_top)        — Free gap / Zona libre
 *   RAM[str_temp_top .. stack_limit) — Temp strings grow ↓ / Cadenas temp ↓
 *
 *   CODE[0 .. code_size)           — Read-only P-code bytecode
 */
typedef struct VM {
    /* Memory pointers / Punteros de memoria */
    uint8_t  *ram;              /**< Runtime memory (globals + stack + temp strings) */
    uint32_t  ram_size;         /**< Total RAM size in bytes */
    uint8_t  *code;             /**< Read-only program P-code */
    uint32_t  code_size;        /**< Code size in bytes */

    /* Registers / Registros */
    int32_t   pc;               /**< Program counter */
    int32_t   sp;               /**< Stack pointer (next free byte) */
    int32_t   fp;               /**< Frame pointer (-1 = no active frame) */

    /* Memory layout / Disposición de memoria */
    int32_t   global_bytes;     /**< Bytes reserved for global variables */
    int32_t   stack_limit;      /**< Upper bound of stack area */
    int32_t   str_temp_limit;   /**< Upper bound of temp string area */
    int32_t   str_temp_top;     /**< Current top of temp string area (grows ↓) */

    /* Execution state / Estado de ejecución */
    uint8_t   running;          /**< 1 = executing, 0 = stopped */
    uint8_t   vm_mode;          /**< Mode flags: VM_MODE_* */
    uint8_t   math_error;       /**< Sticky math error flag (NaN/Inf/div0) */
    uint64_t  instr_executed;   /**< Instruction count (for metrics) */
    int32_t   rng_state;        /**< Park-Miller RNG seed (never 0) */

    /* Debug / Depuración */
    int32_t   breakpoints[VM_MAX_BREAKPOINTS]; /**< Breakpoint PCs (-1 = empty) */
} VM;

/* ================================================================
 * Public VM API / API pública de la VM
 * ================================================================ */

/**
 * Reset VM state and zero RAM. Must be called before vm_start().
 * Reinicia estado de la VM y pone a cero la RAM. Llamar antes de vm_start().
 */
void vm_reset(VM *vm);

/**
 * Start (resume) execution.
 * Inicia (reanuda) la ejecución.
 */
void vm_start(VM *vm);

/**
 * Stop (pause) execution. Useful in debug mode.
 * Detiene (pausa) la ejecución. Útil en modo debug.
 */
void vm_stop(VM *vm);

/**
 * Add a breakpoint at the given PC address.
 * Añade un breakpoint en la dirección PC indicada.
 * @return slot index on success, -1 if no slot, -2 if invalid params.
 */
int vm_set_breakpoint(VM *vm, int32_t pc);

/**
 * Remove a breakpoint at the given PC address.
 * Elimina un breakpoint en la dirección PC indicada.
 * @return 0 on success, -1 if not found.
 */
int vm_reset_breakpoint(VM *vm, int32_t pc);

/* ================================================================
 * Stack access helpers — used by hardware_*.c implementations
 * Helpers de acceso a pila — usados por implementaciones hardware_*.c
 * ================================================================ */

/**
 * Pop an integer of the given VT_* type from the stack.
 * Extrae un entero del tipo VT_* indicado desde la pila.
 */
int32_t popi(VM *vm, int type);

/**
 * Pop a 32-bit float from the stack.
 * Extrae un flotante de 32 bits desde la pila.
 */
float popf(VM *vm);

/**
 * Push an integer of the given VT_* type onto the stack.
 * Inserta un entero del tipo VT_* indicado en la pila.
 */
void pushi(VM *vm, int type, int32_t value);

/**
 * Push a 32-bit float onto the stack.
 * Inserta un flotante de 32 bits en la pila.
 */
void pushf_pub(VM *vm, float value);

/**
 * Read a null-terminated C string from a VM string pointer.
 * Lee una cadena C terminada en nulo desde un puntero STRING de la VM.
 * @param ptr  VM string pointer (16-bit value from stack).
 * @param buf  Destination buffer.
 * @param bs   Buffer size in bytes.
 */
void read_cstr(VM *vm, int32_t ptr, char *buf, size_t bs);

/**
 * Release a temporary string allocated during expression evaluation.
 * Libera una cadena temporal asignada durante la evaluación de una expresión.
 */
void vm_release_temp_string(VM *vm, int32_t ptr);

/* ================================================================
 * HAL interface — implemented in hardware_*.c
 * Interfaz HAL — implementada en hardware_*.c
 * ================================================================ */

/**
 * Hardware trap dispatcher. Called by the VM when a TRAP opcode is executed.
 * Dispatcher de traps de hardware. La VM lo llama al ejecutar opcode TRAP.
 *
 * Each trap ID corresponds to a ST declaration in hardware.cfg:
 *   procedure setBACnetAttribute(name:STRING; value:LONG) trap #1
 *
 * Cada ID de trap corresponde a una declaración ST en hardware.cfg:
 *   procedure cpu() trap #18
 *
 * @param vm      Active VM instance.
 * @param trap_id Trap identifier from bytecode.
 */
void hardware(VM *vm, uint8_t trap_id);

/* Scan loop entry points (osoruntime.c drives these). */
void run_vm(VM *vm);
void hardware_read_inputs(VM *vm);    /* map hardware inputs → VM globals, before main() */
void hardware_write_outputs(VM *vm);  /* map VM globals → hardware outputs, after main() */

#ifdef __cplusplus
}
#endif

#endif /* PCODEVM_H */
