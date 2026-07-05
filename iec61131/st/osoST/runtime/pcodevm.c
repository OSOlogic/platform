/*
 * osoLogic — IEC 61131-3 Structured Text Runtime
 * pcodevm.c — P-Code Virtual Machine interpreter
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Architecture / Arquitectura:
 *   ST source (.st) --[STLite compiler]--> Intel HEX (.hex)
 *                   --[pcodevm]----------> execution on any C99 platform
 *
 *   The VM is intentionally minimal so it compiles unchanged on:
 *     - Linux ARM64 / x86-64 (PLC with OS)
 *     - Bare-metal MCU (STM32, RP2040, ESP32) via cross-compiler
 *     - WASM (browser sandbox, future)
 *
 * File layout / Estructura de ficheros:
 *   pcodevm.h         — Shared types, VM struct, public API
 *   pcodevm.c         — VM implementation (this file)
 *   hardware.h        — HAL interface (hardware() dispatcher declaration)
 *   hardware_demo.c   — Demo HAL: BACnet stubs, cpu() introspection
 *   hardware_linux.c  — Linux HAL: libgpiod, timerfd, sysfs
 *   hardware_bare.c   — Bare-metal HAL template
 *   osoruntime.c      — main() with POSIX scan cycle for Linux PLCs
 */

//pcodevm.c

#include <math.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "pcodevm.h"

/**
 * @file pcodevm.c
 * @brief Intérprete de P-Code para STLite.
 *
 * Flujo general:
 * 1) Carga un fichero Intel HEX.
 * 2) Valida y parsea la cabecera ejecutable.
 * 3) Inicializa RAM/estado de VM.
 * 4) Ejecuta el bucle fetch-decode-execute.
 * 5) Opcionalmente recoge métricas de rendimiento.
 */

#define MAGIC_EXPECTED 0xA55A
#define HEADER_SIZE 24u
#define DEFAULT_RAM_SIZE 2048u
#define FRAME_HEADER_BYTES 6u
#define STR_PTR_RAM_FLAG 0x8000
#define STR_PTR_PAYLOAD_MASK 0x7FFF
#define STR_MAX 255
#define STR_WORK_CAP 1024

/** @brief Tabla de opcodes soportados por la VM. */
enum {
    PUSH_I = 1, PUSH_F = 2, PUSH_S = 3,
    LOAD_G = 10, STORE_G = 11, LOAD_L = 12, STORE_L = 13,
    ADD = 20, SUB = 21, MUL = 22, DIV = 23, NEG = 24,
    LT = 30, LE = 31, GT = 32, GE = 33, EQ = 34, NE = 35,
    AND = 40, OR = 41, NOT = 42, XOR = 43,
    JMP = 50, JMPF = 51, JMP8 = 52, JMPF8 = 53,
    CALL = 60, RET = 61, HALT = 62, LINK = 63, UNLINK = 64, LEAVE = 65, CALL8 = 66, CALL16 = 67,
    DEBUG = 70, STRING = 71, LOG = 72, MATH = 73,
    TRAP = 80,
    MOV_GI8 = 90, MOV_GI32 = 91, MOV_GG = 92, INC_GI8 = 93, ADD_GG_G = 94, SUB_GG_G = 95,
    NEWARR_G = 96, NEWARR_L = 97,
    LOAD_GA = 98, STORE_GA = 99, LOAD_LA = 100, STORE_LA = 101, LOAD_CA = 102,
    CAST_S8 = 103, CAST_U8 = 104, CAST_S16 = 105, CAST_U16 = 106, CAST_I32 = 107, CAST_F32 = 108,
    SHL = 109, SHR = 110, BITAND = 111, BITOR = 112, BITXOR = 113, BITNOT = 114, ROL = 115, ROR = 116,
    ADD_S8 = 120, ADD_U8 = 121, ADD_S16 = 122, ADD_U16 = 123, ADD_I32 = 124, ADD_F32 = 125,
    SUB_S8 = 126, SUB_U8 = 127, SUB_S16 = 128, SUB_U16 = 129, SUB_I32 = 130, SUB_F32 = 131,
    MUL_S8 = 132, MUL_U8 = 133, MUL_S16 = 134, MUL_U16 = 135, MUL_I32 = 136, MUL_F32 = 137,
    DIV_S8 = 138, DIV_U8 = 139, DIV_S16 = 140, DIV_U16 = 141, DIV_I32 = 142, DIV_F32 = 143,
    NEG_S8 = 144, NEG_U8 = 145, NEG_S16 = 146, NEG_U16 = 147, NEG_I32 = 148, NEG_F32 = 149,
    LT_S8 = 150, LT_U8 = 151, LT_S16 = 152, LT_U16 = 153, LT_I32 = 154, LT_F32 = 155,
    LE_S8 = 156, LE_U8 = 157, LE_S16 = 158, LE_U16 = 159, LE_I32 = 160, LE_F32 = 161,
    GT_S8 = 162, GT_U8 = 163, GT_S16 = 164, GT_U16 = 165, GT_I32 = 166, GT_F32 = 167,
    GE_S8 = 168, GE_U8 = 169, GE_S16 = 170, GE_U16 = 171, GE_I32 = 172, GE_F32 = 173,
    EQ_S8 = 174, EQ_U8 = 175, EQ_S16 = 176, EQ_U16 = 177, EQ_I32 = 178, EQ_F32 = 179,
    NE_S8 = 180, NE_U8 = 181, NE_S16 = 182, NE_U16 = 183, NE_I32 = 184, NE_F32 = 185,
    AND_B = 186, OR_B = 187, XOR_B = 188, NOT_B = 189,
    MOV_LI8 = 190, MOV_LI32 = 191, MOV_LL = 192, INC_LI8 = 193,
    ADD_LL_L = 194, SUB_LL_L = 195, MUL_LL_L = 196, ADD_LI_L = 197
};

enum {
    STR_LEN = 1, STR_CONCAT = 2, STR_LEFT = 3, STR_RIGHT = 4, STR_MID = 5,
    STR_INSERT = 6, STR_DELETE = 7, STR_REPLACE = 8, STR_FIND = 9, STR_CHARAT = 10,
    STR_EQ = 11, STR_NE = 12, STR_LT = 13, STR_LE = 14, STR_GT = 15, STR_GE = 16,
    STR_ASSIGN = 17, STR_APPEND_ASSIGN = 18
};

enum {
    MATH_ABS_F = 1, MATH_MIN_F = 2, MATH_MAX_F = 3, MATH_LIMIT_F = 4, MATH_CLAMP_F = 5,
    MATH_ROUND = 6, MATH_FLOOR = 7, MATH_CEIL = 8, MATH_SQRT = 9, MATH_EXP = 10,
    MATH_LOG = 11, MATH_LN = 12, MATH_POW = 13, MATH_MOD_F = 14, MATH_SIGN_F = 15,
    MATH_TRUNC = 16, MATH_SIN = 17, MATH_COS = 18, MATH_TAN = 19, MATH_ASIN = 20,
    MATH_ACOS = 21, MATH_ATAN = 22, MATH_ATAN2 = 23,
    MATH_ABS_I = 101, MATH_MIN_I = 102, MATH_MAX_I = 103, MATH_LIMIT_I = 104,
    MATH_CLAMP_I = 105, MATH_MOD_I = 106, MATH_SIGN_I = 107, MATH_ERROR = 108,
    MATH_RANDOM_I = 109, MATH_RANDOM_F = 110
};

/** @brief Imagen lineal en memoria obtenida desde el HEX. */
typedef struct {
    uint8_t *data;
    uint32_t size;
} ByteImage;

/** @brief Cabecera binaria del programa STLite embebido en la imagen. */
typedef struct {
    uint16_t magic;
    uint32_t app_id;
    uint16_t version;
    uint32_t code_start;
    uint32_t stack_needed;
    uint32_t global_bytes;
    uint32_t minimum_ram_size;
} ExecHeader;

/** @brief Temporizador de alta resolución (Windows) o clock estándar (resto). */
typedef struct {
#ifdef _WIN32
    LARGE_INTEGER start;
    LARGE_INTEGER freq;
#else
    clock_t start;
#endif
} VmTimer;

/** @brief Aborta la ejecución con mensaje de error y código 1. */
static void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

/** @brief Configura salida UTF-8 en consola Windows para textos con acentos. */
static void setup_console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

/** @brief Arranca el temporizador. */
static void timer_start(VmTimer *t) {
#ifdef _WIN32
    QueryPerformanceFrequency(&t->freq);
    QueryPerformanceCounter(&t->start);
#else
    t->start = clock();
#endif
}

/**
 * @brief Calcula segundos transcurridos desde @ref timer_start.
 * @return Tiempo en segundos.
 */
static double timer_elapsed_seconds(const VmTimer *t) {
#ifdef _WIN32
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double) (now.QuadPart - t->start.QuadPart) / (double) t->freq.QuadPart;
#else
    clock_t now = clock();
    return (double) (now - t->start) / (double) CLOCKS_PER_SEC;
#endif
}

/** @brief Lee un entero little-endian de 16 bits. */
static uint16_t rd16le(const uint8_t *p) {
    return (uint16_t) ((uint16_t) p[0] | ((uint16_t) p[1] << 8));
}

/** @brief Lee un entero little-endian de 32 bits. */
static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

/**
 * @brief Convierte un dígito hexadecimal ASCII a valor numérico.
 * @return 0..15 si es válido, -1 si no lo es.
 */
static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/**
 * @brief Convierte dos caracteres hex ASCII a un byte.
 * @param s Cadena con dos caracteres hex.
 * @param out Byte de salida.
 * @return 1 si parsea bien, 0 si hay caracteres no hexadecimales.
 */
static int parse_hex_byte(const char *s, uint8_t *out) {
    int hi = hex_nibble(s[0]);
    int lo = hex_nibble(s[1]);
    if (hi < 0 || lo < 0) return 0;
    *out = (uint8_t) ((hi << 4) | lo);
    return 1;
}

/**
 * @brief Asegura capacidad mínima para un buffer dinámico.
 * @param buf Buffer a redimensionar.
 * @param cap Capacidad actual/resultado.
 * @param need Capacidad mínima requerida.
 * @return 1 en éxito, 0 en error de memoria o desbordamiento de tamaño.
 */
static int ensure_cap(uint8_t **buf, uint32_t *cap, uint32_t need) {
    if (need <= *cap) return 1;
    uint32_t ncap = (*cap == 0) ? 1024u : *cap;
    while (ncap < need) {
        if (ncap > 0x7FFFFFFFu) return 0;
        ncap *= 2u;
    }
    uint8_t *nb = (uint8_t *) realloc(*buf, ncap);
    if (!nb) return 0;
    if (ncap > *cap) memset(nb + *cap, 0, ncap - *cap);
    *buf = nb;
    *cap = ncap;
    return 1;
}

/**
 * @brief Carga y materializa un Intel HEX en memoria lineal.
 * @param path Ruta del fichero HEX.
 * @return Imagen cargada con tamaño efectivo máximo escrito.
 */
static ByteImage load_intel_hex(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: no se pudo abrir HEX: %s\n", path);
        exit(1);
    }

    uint8_t *image = NULL;
    uint32_t cap = 0, used = 0;
    uint32_t upper = 0;
    int eof_seen = 0;
    char line[4096];

    while (fgets(line, sizeof(line), f) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;
        if (line[0] != ':') die("linea HEX invalida");
        if (n < 11) die("linea HEX corta");

        uint8_t len = 0, type = 0, cks = 0;
        uint16_t addr = 0;
        if (!parse_hex_byte(line + 1, &len)) die("len HEX invalido");
        {
            uint8_t ah = 0, al = 0;
            if (!parse_hex_byte(line + 3, &ah) || !parse_hex_byte(line + 5, &al)) die("addr HEX invalido");
            addr = (uint16_t) (((uint16_t) ah << 8) | al);
        }
        if (!parse_hex_byte(line + 7, &type)) die("tipo HEX invalido");
        if (n < (size_t) (11 + len * 2)) die("linea HEX truncada");

        uint8_t data[255];
        uint8_t sum = 0;
        sum = (uint8_t) (sum + len + ((addr >> 8) & 0xFF) + (addr & 0xFF) + type);
        for (uint32_t i = 0; i < len; i++) {
            if (!parse_hex_byte(line + 9 + i * 2u, &data[i])) die("dato HEX invalido");
            sum = (uint8_t) (sum + data[i]);
        }
        if (!parse_hex_byte(line + 9 + len * 2u, &cks)) die("checksum HEX invalido");
        sum = (uint8_t) (sum + cks);
        if (sum != 0) die("checksum HEX incorrecto");

        if (type == 0x00) {
            uint32_t full = (upper << 16) | addr;
            uint32_t need = full + len;
            if (!ensure_cap(&image, &cap, need)) die("sin memoria leyendo HEX");
            if (len > 0) memcpy(image + full, data, len);
            if (need > used) used = need;
        } else if (type == 0x01) {
            eof_seen = 1;
            break;
        } else if (type == 0x04) {
            if (len != 2) die("registro 04 invalido");
            upper = ((uint32_t) data[0] << 8) | data[1];
        }
    }
    fclose(f);
    if (!eof_seen) die("HEX sin EOF");

    ByteImage out;
    out.data = image;
    out.size = used;
    return out;
}

/**
 * @brief Parsea y valida cabecera ejecutable de STLite.
 * @param img Imagen lineal ya cargada.
 * @return Cabecera decodificada.
 */
static ExecHeader parse_header(const ByteImage *img) {
    if (img->size < HEADER_SIZE) die("imagen demasiado corta");
    ExecHeader h;
    h.magic = rd16le(img->data + 0);
    h.app_id = rd32le(img->data + 2);
    h.version = rd16le(img->data + 6);
    h.code_start = rd32le(img->data + 8);
    h.stack_needed = rd32le(img->data + 12);
    h.global_bytes = rd32le(img->data + 16);
    h.minimum_ram_size = rd32le(img->data + 20);
    if (h.magic != MAGIC_EXPECTED) die("magic incorrecto");
    if (h.code_start < HEADER_SIZE || h.code_start >= img->size) die("code_start invalido");
    return h;
}

/** @brief Inicializa la tabla de breakpoints a estado vacío (-1). */
static void vm_clear_breakpoints(VM *vm) {
    if (!vm) return;
    for (int i = 0; i < VM_MAX_BREAKPOINTS; i++) vm->breakpoints[i] = -1;
}

/** @brief Pone la VM en estado de ejecución. */
void vm_start(VM *vm) {
    if (!vm) return;
    vm->running = 1;
}

/** @brief Pone la VM en pausa (no ejecuta nuevas instrucciones). */
void vm_stop(VM *vm) {
    if (!vm) return;
    vm->running = 0;
}

/**
 * @brief Reinicia estado de VM y RAM.
 *
 * Mantiene code/ram asignados, pero resetea registros, stack, contadores y breakpoints.
 */
void vm_reset(VM *vm) {
    if (!vm) return;
    if (vm->ram && vm->ram_size > 0) memset(vm->ram, 0, vm->ram_size);
    if (vm->stack_limit <= vm->global_bytes) {
        vm->stack_limit = (int32_t) vm->ram_size;
    }
    if (vm->stack_limit < vm->global_bytes || (uint32_t) vm->stack_limit > vm->ram_size) {
        die("Layout de stack invalido");
    }
    if (vm->stack_limit > (STR_PTR_PAYLOAD_MASK + 1)) {
        die("RAM STRING excede limite de puntero (32767)");
    }
    vm->str_temp_limit = vm->stack_limit;
    vm->str_temp_top = vm->str_temp_limit;
    vm->pc = 0;
    vm->sp = vm->global_bytes;
    vm->fp = -1;
    vm->instr_executed = 0;
    vm->math_error = 0;
    vm->rng_state = 1;
    vm_clear_breakpoints(vm);
    vm->running = 0;
}

/**
 * @brief Añade un breakpoint por PC.
 * @param vm Instancia VM.
 * @param pc Dirección de código donde parar.
 * @return Índice del slot usado, -1 si no hay hueco, -2 si parámetros inválidos.
 */
int vm_set_breakpoint(VM *vm, int32_t pc) {
    if (!vm) return -2;
    if (pc < 0 || (uint32_t) pc >= vm->code_size) return -2;
    for (int i = 0; i < VM_MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i] == pc) return i;
    }
    for (int i = 0; i < VM_MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i] < 0) {
            vm->breakpoints[i] = pc;
            return i;
        }
    }
    return -1;
}

/**
 * @brief Elimina breakpoint por PC.
 * @return 0 si se elimina, -1 si no existe.
 */
int vm_reset_breakpoint(VM *vm, int32_t pc) {
    if (!vm) return -1;
    for (int i = 0; i < VM_MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i] == pc) {
            vm->breakpoints[i] = -1;
            return 0;
        }
    }
    return -1;
}

/** @brief Comprueba si existe breakpoint activo en la PC dada. */
static int vm_has_breakpoint_at(const VM *vm, int32_t pc) {
    if (!vm) return 0;
    for (int i = 0; i < VM_MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i] == pc) return 1;
    }
    return 0;
}

/** @brief Verifica rango de acceso en RAM y aborta si es inválido. */
static void chk_ram(VM *vm, int32_t a, int32_t s, const char *op) {
    if (a < 0 || s < 0 || (uint32_t) (a + s) > vm->ram_size) {
        fprintf(stderr, "ERROR: %s fuera de RAM: %d size=%d\n", op, a, s);
        exit(1);
    }
}

/** @brief Verifica rango de acceso en CODE y aborta si es inválido. */
static void chk_code(VM *vm, int32_t a, int32_t s, const char *op) {
    if (a < 0 || s < 0 || (uint32_t) (a + s) > vm->code_size) {
        fprintf(stderr, "ERROR: %s fuera de CODE: %d size=%d\n", op, a, s);
        exit(1);
    }
}

/** @brief Lecturas tipadas sobre RAM/CODE en little-endian. */
static uint16_t rd16_ram(VM *vm, int32_t a) { chk_ram(vm, a, 2, "rd16"); return (uint16_t) (vm->ram[a] | (vm->ram[a + 1] << 8)); }
static int32_t rd32_ram(VM *vm, int32_t a) { chk_ram(vm, a, 4, "rd32"); return (int32_t) ((uint32_t) vm->ram[a] | ((uint32_t) vm->ram[a + 1] << 8) | ((uint32_t) vm->ram[a + 2] << 16) | ((uint32_t) vm->ram[a + 3] << 24)); }
static uint16_t rd16_code(VM *vm, int32_t a) { chk_code(vm, a, 2, "rd16Code"); return (uint16_t) (vm->code[a] | (vm->code[a + 1] << 8)); }
static int32_t rd32_code(VM *vm, int32_t a) { chk_code(vm, a, 4, "rd32Code"); return (int32_t) ((uint32_t) vm->code[a] | ((uint32_t) vm->code[a + 1] << 8) | ((uint32_t) vm->code[a + 2] << 16) | ((uint32_t) vm->code[a + 3] << 24)); }

/** @brief Escrituras little-endian en RAM. */
static void wr16_ram(VM *vm, int32_t a, uint16_t v) {
    chk_ram(vm, a, 2, "wr16");
    vm->ram[a] = (uint8_t) (v & 0xFF);
    vm->ram[a + 1] = (uint8_t) ((v >> 8) & 0xFF);
}

static void wr32_ram(VM *vm, int32_t a, int32_t v) {
    chk_ram(vm, a, 4, "wr32");
    vm->ram[a] = (uint8_t) (v & 0xFF);
    vm->ram[a + 1] = (uint8_t) ((v >> 8) & 0xFF);
    vm->ram[a + 2] = (uint8_t) ((v >> 16) & 0xFF);
    vm->ram[a + 3] = (uint8_t) ((v >> 24) & 0xFF);
}

/** @brief Lectura secuencial de stream de bytecode con avance de PC. */
static uint8_t cu8(VM *vm) { chk_code(vm, vm->pc, 1, "u8"); return vm->code[vm->pc++]; }
static uint16_t cu16(VM *vm) { uint16_t v = rd16_code(vm, vm->pc); vm->pc += 2; return v; }
static int8_t ci8(VM *vm) { return (int8_t) cu8(vm); }
static int16_t ci16(VM *vm) { return (int16_t) cu16(vm); }
static int32_t ci32(VM *vm) { int32_t v = rd32_code(vm, vm->pc); vm->pc += 4; return v; }

/** @brief Utilidades de borrado/copia segura sobre RAM. */
static void zero_ram(VM *vm, int32_t a, int32_t n) { if (n > 0) { chk_ram(vm, a, n, "zeroRam"); memset(vm->ram + a, 0, (size_t) n); } }
static void copy_ram(VM *vm, int32_t s, int32_t d, int32_t n) { if (n > 0 && s != d) { chk_ram(vm, s, n, "copyRam(s)"); chk_ram(vm, d, n, "copyRam(d)"); memmove(vm->ram + d, vm->ram + s, (size_t) n); } }

static void chk_stack_push(VM *vm, int32_t bytes, const char *op) {
    if (bytes < 0 || vm->sp < vm->global_bytes || vm->sp + bytes > vm->str_temp_top) {
        fprintf(stderr, "ERROR: %s overflow stack: sp=%d bytes=%d tempTop=%d\n", op, vm->sp, bytes, vm->str_temp_top);
        exit(1);
    }
    chk_ram(vm, vm->sp, bytes, op);
}

static void chk_stack_pop(VM *vm, int32_t bytes, const char *op) {
    int32_t next = vm->sp - bytes;
    if (bytes < 0 || next < vm->global_bytes) {
        fprintf(stderr, "ERROR: %s underflow stack: sp=%d bytes=%d\n", op, vm->sp, bytes);
        exit(1);
    }
    chk_ram(vm, next, bytes, op);
}

/** @brief Primitivas de pila en anchos 1/2/4 bytes. */
static void push1(VM *vm, int32_t v) { chk_stack_push(vm, 1, "push1"); vm->ram[vm->sp++] = (uint8_t) (v & 0xFF); }
static void push2(VM *vm, int32_t v) { chk_stack_push(vm, 2, "push2"); wr16_ram(vm, vm->sp, (uint16_t) (v & 0xFFFF)); vm->sp += 2; }
static void push4(VM *vm, int32_t v) { chk_stack_push(vm, 4, "push4"); wr32_ram(vm, vm->sp, v); vm->sp += 4; }

static int32_t pop1(VM *vm) { chk_stack_pop(vm, 1, "pop1"); vm->sp -= 1; int32_t v = vm->ram[vm->sp] & 0xFF; vm->ram[vm->sp] = 0; return v; }
static int32_t pop2(VM *vm) { chk_stack_pop(vm, 2, "pop2"); vm->sp -= 2; int32_t v = rd16_ram(vm, vm->sp); zero_ram(vm, vm->sp, 2); return v; }
static int32_t pop4(VM *vm) { chk_stack_pop(vm, 4, "pop4"); vm->sp -= 4; int32_t v = rd32_ram(vm, vm->sp); zero_ram(vm, vm->sp, 4); return v; }

static int32_t popn(VM *vm, int n) { if (n == 1) return pop1(vm); if (n == 2) return pop2(vm); if (n == 4) return pop4(vm); die("popN invalido"); return 0; }
static void pushn(VM *vm, int32_t v, int n) { if (n == 1) push1(vm, v); else if (n == 2) push2(vm, v); else if (n == 4) push4(vm, v); else die("pushN invalido"); }

/** @brief Tamaño en bytes por tipo VM. */
static int tbytes(int t) { if (t == VT_S8 || t == VT_U8) return 1; if (t == VT_S16 || t == VT_U16 || t == VT_STR) return 2; return 4; }

/**
 * @brief Extrae entero de la pila con semántica del tipo indicado.
 * @param t Tipo VT_* esperado en pila.
 */
int32_t popi(VM *vm, int t) {
    if (t == VT_S8) return (int8_t) pop1(vm);
    if (t == VT_U8) return pop1(vm) & 0xFF;
    if (t == VT_S16) return (int16_t) pop2(vm);
    if (t == VT_U16 || t == VT_STR) return pop2(vm) & 0xFFFF;
    return pop4(vm);
}

/** @brief Extrae float de 32 bits desde pila. */
float popf(VM *vm) {
    union { uint32_t u; float f; } c;
    c.u = (uint32_t) pop4(vm);
    return c.f;
}

/** @brief Inserta entero en pila ajustando anchura/signo según tipo VT_*. */
void pushi(VM *vm, int t, int32_t v) {
    if (t == VT_S8) push1(vm, (int8_t) v);
    else if (t == VT_U8) push1(vm, v & 0xFF);
    else if (t == VT_S16) push2(vm, (int16_t) v);
    else if (t == VT_U16 || t == VT_STR) push2(vm, v & 0xFFFF);
    else push4(vm, v);
}

/** @brief Inserta float de 32 bits en pila. */
static void pushf(VM *vm, float f) {
    union { uint32_t u; float f; } c;
    c.f = f;
    push4(vm, (int32_t) c.u);
}

/** @brief Representación booleana de VM: 0xFF=true, 0=false. */
static void push_bool(VM *vm, int b) { push1(vm, b ? 0xFF : 0); }
/** @brief Activa bandera sticky de error matemático. */
static void set_math_error(VM *vm) { if (vm) vm->math_error = 1; }
/** @brief Inserta F32 y marca error si el resultado no es finito (NaN/Inf). */
static void push_mathf(VM *vm, float f) {
    if (!isfinite(f)) set_math_error(vm);
    pushf(vm, f);
}

/** @brief PRNG Park-Miller (31-bit) para RANDOM(). */
static int32_t rng_next_u31(VM *vm) {
    if (!vm) return 1;
    if (vm->rng_state <= 0 || vm->rng_state >= 2147483647) {
        vm->rng_state = 1;
    }
    int32_t hi = vm->rng_state / 127773;
    int32_t lo = vm->rng_state % 127773;
    int32_t test = 16807 * lo - 2836 * hi;
    if (test <= 0) {
        test += 2147483647;
    }
    vm->rng_state = test;
    return vm->rng_state;
}

void vm_release_temp_string(VM *vm, int32_t ptr);

/** @brief Helpers de casting, verdad lógica y mapeo de opcodes tipados. */
static int32_t pop_int_cast(VM *vm, int src) { return (src == VT_F32) ? (int32_t) popf(vm) : popi(vm, src); }
static int pop_truthy(VM *vm, int t) {
    if (t == VT_F32) return popf(vm) != 0.0f;
    if (t == VT_STR) {
        int32_t p = pop2(vm) & 0xFFFF;
        vm_release_temp_string(vm, p);
        return p != 0;
    }
    return popi(vm, t) != 0;
}
static int vm_type_from_typed_idx(int idx) {
    switch (idx) {
        case 0: return VT_S8;
        case 1: return VT_U8;
        case 2: return VT_S16;
        case 3: return VT_U16;
        case 4: return VT_I32;
        case 5: return VT_F32;
        default: die("indice tipo tipado invalido"); return VT_I32;
    }
}

/** @brief Helpers de comparación entera y bitwise dependientes del ancho. */
static uint32_t umask(int t) { return (t == VT_U8) ? 0xFFu : 0xFFFFu; }
static int lt_i(int t, int32_t a, int32_t b) { return (t == VT_U8 || t == VT_U16) ? (((uint32_t) a & umask(t)) < ((uint32_t) b & umask(t))) : (a < b); }
static int le_i(int t, int32_t a, int32_t b) { return (t == VT_U8 || t == VT_U16) ? (((uint32_t) a & umask(t)) <= ((uint32_t) b & umask(t))) : (a <= b); }
static int gt_i(int t, int32_t a, int32_t b) { return (t == VT_U8 || t == VT_U16) ? (((uint32_t) a & umask(t)) > ((uint32_t) b & umask(t))) : (a > b); }
static int ge_i(int t, int32_t a, int32_t b) { return (t == VT_U8 || t == VT_U16) ? (((uint32_t) a & umask(t)) >= ((uint32_t) b & umask(t))) : (a >= b); }

static int bitw(int t) { if (t == VT_S8 || t == VT_U8) return 8; if (t == VT_S16 || t == VT_U16) return 16; return 32; }
static uint32_t bitm(int t) { if (t == VT_S8 || t == VT_U8) return 0xFFu; if (t == VT_S16 || t == VT_U16) return 0xFFFFu; return 0xFFFFFFFFu; }
static int snorm(int n, int w) { int r = (w <= 0) ? 0 : (n % w); return (r < 0) ? (r + w) : r; }
static int32_t shl_bits(int t, int32_t v, int n) { int w = bitw(t), s = snorm(n, w); uint32_t m = bitm(t), x = ((uint32_t) v) & m; return (int32_t) ((x << s) & m); }
static int32_t shr_bits(int t, int32_t v, int n) { int w = bitw(t), s = snorm(n, w); uint32_t m = bitm(t), x = ((uint32_t) v) & m; return (s == 0) ? (int32_t) (x & m) : (int32_t) ((x >> s) & m); }
static int32_t rol_bits(int t, int32_t v, int n) { int w = bitw(t), s = snorm(n, w); uint32_t m = bitm(t), x = ((uint32_t) v) & m; if (s == 0) return (int32_t) (x & m); if (w == 32) return (int32_t) ((x << s) | (x >> (32 - s))); return (int32_t) (((x << s) | (x >> (w - s))) & m); }
static int32_t ror_bits(int t, int32_t v, int n) { int w = bitw(t), s = snorm(n, w); uint32_t m = bitm(t), x = ((uint32_t) v) & m; if (s == 0) return (int32_t) (x & m); if (w == 32) return (int32_t) ((x >> s) | (x << (32 - s))); return (int32_t) (((x >> s) | (x << (w - s))) & m); }

/**
 * @brief Traduce desplazamiento local (relativo a FP) a dirección RAM.
 * @note Valida que quede dentro del frame activo.
 */
static int32_t local_addr(VM *vm, int32_t off, const char *op) {
    if (vm->fp < 0) { fprintf(stderr, "ERROR: %s sin frame\n", op); exit(1); }
    int32_t fs = rd16_ram(vm, vm->fp + 4) & 0xFFFF;
    int32_t s = vm->fp + (int32_t) FRAME_HEADER_BYTES;
    int32_t e = s + fs;
    int32_t a = vm->fp + off;
    if (a < s || a >= e) { fprintf(stderr, "ERROR: %s fuera de frame\n", op); exit(1); }
    return a;
}

/**
 * @brief Calcula offset lineal de array multidimensional.
 * @note Consume índices de la pila y valida límites por dimensión.
 */
static int32_t pop_array_offset(VM *vm, int nd, int32_t dp, const char *op) {
    int32_t off = 0;
    for (int d = nd - 1; d >= 0; d--) {
        int32_t p = dp + d * 12;
        int32_t lo = rd32_code(vm, p), hi = rd32_code(vm, p + 4), st = rd32_code(vm, p + 8);
        int32_t idx = pop4(vm);
        if (idx < lo || idx > hi) { fprintf(stderr, "ERROR: Indice fuera de rango %s\n", op); exit(1); }
        off += (idx - lo) * st;
    }
    return off;
}

/** @brief Lee valor tipado desde RAM y lo apila. */
static void push_ram(VM *vm, int32_t a, int t, const char *op) {
    int s = tbytes(t);
    chk_ram(vm, a, s, op);
    if (s == 1) push1(vm, vm->ram[a] & 0xFF);
    else if (s == 2) push2(vm, rd16_ram(vm, a));
    else push4(vm, rd32_ram(vm, a));
}

/** @brief Lee valor tipado desde CODE (constante) y lo apila. */
static void push_code(VM *vm, int32_t a, int t, const char *op) {
    int s = tbytes(t);
    chk_code(vm, a, s, op);
    if (s == 1) push1(vm, vm->code[a] & 0xFF);
    else if (s == 2) push2(vm, rd16_code(vm, a));
    else push4(vm, rd32_code(vm, a));
}

/** @brief Desapila y guarda valor tipado en RAM. */
static void store_ram(VM *vm, int32_t a, int t, const char *op) {
    int s = tbytes(t);
    chk_ram(vm, a, s, op);
    if (s == 1) vm->ram[a] = (uint8_t) pop1(vm);
    else if (s == 2) wr16_ram(vm, a, (uint16_t) pop2(vm));
    else wr32_ram(vm, a, pop4(vm));
}

static int str_clamp_max(int max_len) {
    if (max_len < 0) return 0;
    if (max_len > STR_MAX) return STR_MAX;
    return max_len;
}

static int str_ptr_payload(int32_t ptr) { return (ptr & 0xFFFF) & STR_PTR_PAYLOAD_MASK; }

static int str_ptr_is_temp(VM *vm, int32_t ptr) {
    int p = ptr & 0xFFFF;
    if ((p & STR_PTR_RAM_FLAG) == 0) return 0;
    int payload = str_ptr_payload(p);
    // Los temporales STRING activos viven en [str_temp_top, str_temp_limit),
    // pero ademas deben estar en la zona libre superior (>= sp) para no
    // confundir punteros de STRING en globals/locales.
    return payload >= vm->str_temp_top && payload < vm->str_temp_limit && payload >= vm->sp;
}

static int str_ptr_is_ram(VM *vm, int32_t ptr) {
    int p = ptr & 0xFFFF;
    if ((p & STR_PTR_RAM_FLAG) == 0) return 0;
    int payload = str_ptr_payload(p);
    return payload >= 0 && (uint32_t) payload < vm->ram_size && !str_ptr_is_temp(vm, p);
}

static int alloc_temp_ptr(VM *vm, int max_len) {
    int cap = str_clamp_max(max_len);
    int bytes = cap + 2;
    if (vm->str_temp_top < vm->sp || vm->str_temp_top > vm->str_temp_limit) {
        die("Temp STRING stack corrupto");
    }
    if (vm->str_temp_top - bytes < vm->sp) {
        die("Temp STRING overflow");
    }
    int32_t addr = vm->str_temp_top - bytes;
    if (addr < 0 || addr > STR_PTR_PAYLOAD_MASK) {
        die("Temp STRING fuera de rango de puntero");
    }
    vm->str_temp_top = addr;
    zero_ram(vm, addr, bytes);
    vm->ram[addr] = (uint8_t) cap;
    vm->ram[addr + 1] = 0;
    return STR_PTR_RAM_FLAG | (addr & STR_PTR_PAYLOAD_MASK);
}

void vm_release_temp_string(VM *vm, int32_t ptr) {
    int p = ptr & 0xFFFF;
    if (!str_ptr_is_temp(vm, p)) return;
    int addr = str_ptr_payload(p);
    chk_ram(vm, addr, 1, "vm_release_temp_string");
    int cap = vm->ram[addr] & 0xFF;
    int bytes = cap + 2;
    if (addr != vm->str_temp_top) {
        fprintf(stderr,
                "ERROR: Temp STRING no LIFO ptr=%d addr=%d top=%d sp=%d fp=%d pc=%d cap=%d bytes=%d\n",
                p, addr, vm->str_temp_top, vm->sp, vm->fp, vm->pc, cap, bytes);
        exit(1);
    }
    zero_ram(vm, addr, bytes);
    vm->str_temp_top += bytes;
}

static void release_temp_pair(VM *vm, int32_t p1, int32_t p2) {
    int32_t a = p1 & 0xFFFF;
    int32_t b = p2 & 0xFFFF;
    if (a == b) {
        vm_release_temp_string(vm, a);
        return;
    }
    int a_temp = str_ptr_is_temp(vm, a);
    int b_temp = str_ptr_is_temp(vm, b);
    if (a_temp && b_temp) {
        int a_addr = str_ptr_payload(a);
        int b_addr = str_ptr_payload(b);
        if (a_addr == vm->str_temp_top) {
            vm_release_temp_string(vm, a);
            vm_release_temp_string(vm, b);
            return;
        }
        if (b_addr == vm->str_temp_top) {
            vm_release_temp_string(vm, b);
            vm_release_temp_string(vm, a);
            return;
        }
    }
    vm_release_temp_string(vm, a);
    vm_release_temp_string(vm, b);
}

static int str_max(VM *vm, int32_t ptr) {
    int p = ptr & 0xFFFF;
    if (p == 0) return 0;
    if (str_ptr_is_temp(vm, p) || str_ptr_is_ram(vm, p)) {
        int a = str_ptr_payload(p);
        chk_ram(vm, a, 1, "str_max(ram)");
        return vm->ram[a] & 0xFF;
    }
    if ((uint32_t) p < vm->code_size) {
        return vm->code[p] & 0xFF;
    }
    return 0;
}

static int str_byte(VM *vm, int32_t ptr, int idx) {
    if (idx < 0) return 0;
    int p = ptr & 0xFFFF;
    if (p == 0) return 0;
    if (str_ptr_is_temp(vm, p) || str_ptr_is_ram(vm, p)) {
        int a = str_ptr_payload(p);
        int m = str_max(vm, p);
        if (idx >= m) return 0;
        int pos = a + 1 + idx;
        chk_ram(vm, pos, 1, "str_byte(ram)");
        return vm->ram[pos] & 0xFF;
    }
    int m = str_max(vm, p);
    if (idx >= m) return 0;
    int pos = p + 1 + idx;
    if ((uint32_t) pos >= vm->code_size) return 0;
    return vm->code[pos] & 0xFF;
}

static int str_len(VM *vm, int32_t ptr) {
    int m = str_max(vm, ptr);
    for (int i = 0; i < m; i++) {
        if (str_byte(vm, ptr, i) == 0) return i;
    }
    return m;
}

static int str_to_buf(VM *vm, int32_t ptr, uint8_t *out, int cap) {
    int len = str_len(vm, ptr);
    if (cap < 0) cap = 0;
    int n = (len < cap) ? len : cap;
    for (int i = 0; i < n; i++) out[i] = (uint8_t) str_byte(vm, ptr, i);
    return n;
}

/**
 * @brief Lee cadena textual desde puntero STRING VM (CODE/RAM/temp).
 */
void read_cstr(VM *vm, int32_t ptr, char *buf, size_t bs) {
    if (bs == 0) return;
    int p = ptr & 0xFFFF;
    if (p != 0 && !str_ptr_is_ram(vm, p) && !str_ptr_is_temp(vm, p) && (uint32_t) p >= vm->code_size) {
        snprintf(buf, bs, "<str@%d>", p);
        return;
    }
    int len = str_len(vm, p);
    size_t n = (size_t) len;
    if (n + 1 > bs) n = bs - 1;
    for (size_t i = 0; i < n; i++) buf[i] = (char) (str_byte(vm, p, (int) i) & 0xFF);
    buf[n] = '\0';
}

static void str_write_byte(VM *vm, int32_t ptr, int idx, uint8_t v) {
    int p = ptr & 0xFFFF;
    if (str_ptr_is_temp(vm, p) || str_ptr_is_ram(vm, p)) {
        int a = str_ptr_payload(p);
        int m = str_max(vm, p);
        if (idx < 0 || idx > m) return;
        int pos = a + 1 + idx;
        chk_ram(vm, pos, 1, "str_write_byte(ram)");
        vm->ram[pos] = v;
        return;
    }
    die("Destino STRING no escribible");
}

static void str_write_bytes(VM *vm, int32_t dst_ptr, const uint8_t *src, int src_len) {
    int m = str_max(vm, dst_ptr);
    int n = (src_len < m) ? src_len : m;
    if (n < 0) n = 0;
    for (int i = 0; i < n; i++) str_write_byte(vm, dst_ptr, i, src ? src[i] : 0);
    str_write_byte(vm, dst_ptr, n, 0);
}

static int str_cmp_ptr(VM *vm, int32_t a_ptr, int32_t b_ptr) {
    int al = str_len(vm, a_ptr);
    int bl = str_len(vm, b_ptr);
    int n = (al < bl) ? al : bl;
    for (int i = 0; i < n; i++) {
        int aa = str_byte(vm, a_ptr, i) & 0xFF;
        int bb = str_byte(vm, b_ptr, i) & 0xFF;
        if (aa != bb) return aa - bb;
    }
    return al - bl;
}

static int str_find_buf(const uint8_t *hay, int hlen, const uint8_t *needle, int nlen) {
    if (nlen == 0) return 1;
    if (nlen > hlen) return 0;
    int lim = hlen - nlen;
    for (int i = 0; i <= lim; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (hay[i + j] != needle[j]) {
                ok = 0;
                break;
            }
        }
        if (ok) return i + 1;
    }
    return 0;
}

/** @brief Convierte un valor en RAM a texto para soporte de STRING. */
static void value_to_text_at(VM *vm, int32_t a, int t, char *out, size_t out_sz) {
    if (out_sz == 0) return;
    int s = tbytes(t);
    chk_ram(vm, a, s, "readAt");
    if (t == VT_S8) snprintf(out, out_sz, "%d", (int8_t) vm->ram[a]);
    else if (t == VT_U8) snprintf(out, out_sz, "%u", (unsigned) (vm->ram[a] & 0xFF));
    else if (t == VT_S16) snprintf(out, out_sz, "%d", (int16_t) rd16_ram(vm, a));
    else if (t == VT_U16) snprintf(out, out_sz, "%u", (unsigned) (rd16_ram(vm, a) & 0xFFFF));
    else if (t == VT_F32) {
        union { uint32_t u; float f; } c;
        c.u = (uint32_t) rd32_ram(vm, a);
        snprintf(out, out_sz, "%g", (double) c.f);
    } else if (t == VT_STR) read_cstr(vm, rd16_ram(vm, a) & 0xFFFF, out, out_sz);
    else snprintf(out, out_sz, "%d", rd32_ram(vm, a));
}

/** @brief Codifica/decodifica frame pointer previo con centinela 0xFFFF para -1. */
static int32_t enc_fp(int32_t v) { return (v < 0) ? 0xFFFF : (v & 0xFFFF); }
static int32_t dec_fp(int32_t v) { return (v == 0xFFFF) ? -1 : (v & 0xFFFF); }

/**
 * @brief Ejecuta una comparación genérica (LT/LE/GT/GE/EQ/NE).
 * @param t Tipo de operandos.
 * @param mode Código de comparación: 0..5.
 */
static void cmp_op(VM *vm, int t, int mode) {
    if (t == VT_F32) {
        float b = popf(vm), a = popf(vm);
        int r = 0;
        if (mode == 0) r = a < b; else if (mode == 1) r = a <= b; else if (mode == 2) r = a > b;
        else if (mode == 3) r = a >= b; else if (mode == 4) r = (a == b); else r = (a != b);
        push_bool(vm, r);
        return;
    }
    if (t == VT_STR) {
        int32_t b = pop2(vm) & 0xFFFF, a = pop2(vm) & 0xFFFF;
        int c = str_cmp_ptr(vm, a, b);
        release_temp_pair(vm, b, a);
        int r = 0;
        if (mode == 0) r = (c < 0);
        else if (mode == 1) r = (c <= 0);
        else if (mode == 2) r = (c > 0);
        else if (mode == 3) r = (c >= 0);
        else if (mode == 4) r = (c == 0);
        else r = (c != 0);
        push_bool(vm, r);
        return;
    }
    int32_t b = popi(vm, t), a = popi(vm, t);
    int r = 0;
    if (mode == 0) r = lt_i(t, a, b); else if (mode == 1) r = le_i(t, a, b); else if (mode == 2) r = gt_i(t, a, b);
    else if (mode == 3) r = ge_i(t, a, b); else if (mode == 4) r = (a == b); else r = (a != b);
    push_bool(vm, r);
}

static void exec_math(VM *vm, int subop) {
    switch (subop) {
        case MATH_ABS_F: { push_mathf(vm, fabsf(popf(vm))); break; }
        case MATH_MIN_F: { float b = popf(vm), a = popf(vm); push_mathf(vm, fminf(a, b)); break; }
        case MATH_MAX_F: { float b = popf(vm), a = popf(vm); push_mathf(vm, fmaxf(a, b)); break; }
        case MATH_LIMIT_F:
        case MATH_CLAMP_F: {
            float hi = popf(vm), lo = popf(vm), x = popf(vm);
            push_mathf(vm, fmaxf(lo, fminf(x, hi)));
            break;
        }
        case MATH_ROUND: { push_mathf(vm, roundf(popf(vm))); break; }
        case MATH_FLOOR: { push_mathf(vm, floorf(popf(vm))); break; }
        case MATH_CEIL: { push_mathf(vm, ceilf(popf(vm))); break; }
        case MATH_SQRT: { push_mathf(vm, sqrtf(popf(vm))); break; }
        case MATH_EXP: { push_mathf(vm, expf(popf(vm))); break; }
        case MATH_LOG: { push_mathf(vm, log10f(popf(vm))); break; }
        case MATH_LN: { push_mathf(vm, logf(popf(vm))); break; }
        case MATH_POW: { float b = popf(vm), a = popf(vm); push_mathf(vm, powf(a, b)); break; }
        case MATH_MOD_F: { float b = popf(vm), a = popf(vm); push_mathf(vm, fmodf(a, b)); break; }
        case MATH_SIGN_F: {
            float x = popf(vm);
            push_mathf(vm, (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f));
            break;
        }
        case MATH_TRUNC: {
            float x = popf(vm);
            push_mathf(vm, (x >= 0.0f) ? floorf(x) : ceilf(x));
            break;
        }
        case MATH_SIN: { push_mathf(vm, sinf(popf(vm))); break; }
        case MATH_COS: { push_mathf(vm, cosf(popf(vm))); break; }
        case MATH_TAN: { push_mathf(vm, tanf(popf(vm))); break; }
        case MATH_ASIN: { push_mathf(vm, asinf(popf(vm))); break; }
        case MATH_ACOS: { push_mathf(vm, acosf(popf(vm))); break; }
        case MATH_ATAN: { push_mathf(vm, atanf(popf(vm))); break; }
        case MATH_ATAN2: { float b = popf(vm), a = popf(vm); push_mathf(vm, atan2f(a, b)); break; }

        case MATH_ABS_I: { pushi(vm, VT_I32, abs(popi(vm, VT_I32))); break; }
        case MATH_MIN_I: { int32_t b = popi(vm, VT_I32), a = popi(vm, VT_I32); pushi(vm, VT_I32, (a < b) ? a : b); break; }
        case MATH_MAX_I: { int32_t b = popi(vm, VT_I32), a = popi(vm, VT_I32); pushi(vm, VT_I32, (a > b) ? a : b); break; }
        case MATH_LIMIT_I:
        case MATH_CLAMP_I: {
            int32_t hi = popi(vm, VT_I32), lo = popi(vm, VT_I32), x = popi(vm, VT_I32);
            if (x < lo) x = lo;
            if (x > hi) x = hi;
            pushi(vm, VT_I32, x);
            break;
        }
        case MATH_MOD_I: {
            int32_t b = popi(vm, VT_I32), a = popi(vm, VT_I32);
            if (b == 0) {
                set_math_error(vm);
                pushi(vm, VT_I32, 0);
            } else {
                pushi(vm, VT_I32, a % b);
            }
            break;
        }
        case MATH_SIGN_I: {
            int32_t x = popi(vm, VT_I32);
            pushi(vm, VT_I32, (x > 0) ? 1 : ((x < 0) ? -1 : 0));
            break;
        }
        case MATH_RANDOM_I: {
            int32_t max_exclusive = popi(vm, VT_I32);
            if (max_exclusive <= 0) {
                pushi(vm, VT_I32, 0);
            } else {
                pushi(vm, VT_I32, (rng_next_u31(vm) - 1) % max_exclusive);
            }
            break;
        }
        case MATH_RANDOM_F: {
            float r = (float) (rng_next_u31(vm) - 1) / 2147483647.0f;
            pushf(vm, r);
            break;
        }
        case MATH_ERROR: {
            push_bool(vm, vm->math_error ? 1 : 0);
            vm->math_error = 0;
            break;
        }
        default:
            fprintf(stderr, "ERROR: MATH subop desconocido %d\n", subop);
            exit(1);
    }
}

static void exec_string(VM *vm, int subop) {
    uint8_t a[STR_WORK_CAP], b[STR_WORK_CAP], r[STR_WORK_CAP];
    int al, bl, rl;
    switch (subop) {
        case STR_LEN: {
            int32_t s = popi(vm, VT_STR) & 0xFFFF;
            int len = str_len(vm, s);
            vm_release_temp_string(vm, s);
            pushi(vm, VT_I32, len);
            return;
        }
        case STR_CONCAT: {
            int32_t bptr = popi(vm, VT_STR) & 0xFFFF;
            int32_t aptr = popi(vm, VT_STR) & 0xFFFF;
            int out_max = str_clamp_max(str_max(vm, aptr) + str_max(vm, bptr));
            al = str_to_buf(vm, aptr, a, STR_WORK_CAP);
            bl = str_to_buf(vm, bptr, b, STR_WORK_CAP);
            rl = 0;
            for (int i = 0; i < al && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            for (int i = 0; i < bl && rl < STR_WORK_CAP; i++) r[rl++] = b[i];
            release_temp_pair(vm, bptr, aptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, r, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_LEFT: {
            int32_t n = popi(vm, VT_I32);
            int32_t sptr = popi(vm, VT_STR) & 0xFFFF;
            int out_max = str_clamp_max((n < 0) ? 0 : ((n < str_max(vm, sptr)) ? n : str_max(vm, sptr)));
            al = str_to_buf(vm, sptr, a, STR_WORK_CAP);
            rl = n;
            if (rl < 0) rl = 0;
            if (rl > al) rl = al;
            vm_release_temp_string(vm, sptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, a, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_RIGHT: {
            int32_t n = popi(vm, VT_I32);
            int32_t sptr = popi(vm, VT_STR) & 0xFFFF;
            int out_max = str_clamp_max((n < 0) ? 0 : ((n < str_max(vm, sptr)) ? n : str_max(vm, sptr)));
            al = str_to_buf(vm, sptr, a, STR_WORK_CAP);
            rl = n;
            if (rl < 0) rl = 0;
            if (rl > al) rl = al;
            int from = al - rl;
            vm_release_temp_string(vm, sptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, a + from, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_MID: {
            int32_t len = popi(vm, VT_I32);
            int32_t pos = popi(vm, VT_I32);
            int32_t sptr = popi(vm, VT_STR) & 0xFFFF;
            int src_max = str_max(vm, sptr);
            int out_max = str_clamp_max((len < 0) ? 0 : ((len < src_max) ? len : src_max));
            al = str_to_buf(vm, sptr, a, STR_WORK_CAP);
            int start = pos - 1;
            if (start < 0) start = 0;
            if (start > al) start = al;
            rl = len;
            if (rl < 0) rl = 0;
            if (rl > al - start) rl = al - start;
            vm_release_temp_string(vm, sptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, a + start, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_INSERT: {
            int32_t pos = popi(vm, VT_I32);
            int32_t insptr = popi(vm, VT_STR) & 0xFFFF;
            int32_t baseptr = popi(vm, VT_STR) & 0xFFFF;
            int out_max = str_clamp_max(str_max(vm, baseptr) + str_max(vm, insptr));
            al = str_to_buf(vm, baseptr, a, STR_WORK_CAP);
            bl = str_to_buf(vm, insptr, b, STR_WORK_CAP);
            int at = pos - 1;
            if (at < 0) at = 0;
            if (at > al) at = al;
            rl = 0;
            for (int i = 0; i < at && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            for (int i = 0; i < bl && rl < STR_WORK_CAP; i++) r[rl++] = b[i];
            for (int i = at; i < al && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            release_temp_pair(vm, insptr, baseptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, r, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_DELETE: {
            int32_t len = popi(vm, VT_I32);
            int32_t pos = popi(vm, VT_I32);
            int32_t sptr = popi(vm, VT_STR) & 0xFFFF;
            int out_max = str_clamp_max(str_max(vm, sptr));
            al = str_to_buf(vm, sptr, a, STR_WORK_CAP);
            int at = pos - 1;
            if (at < 0) at = 0;
            if (at > al) at = al;
            int del = len;
            if (del < 0) del = 0;
            if (del > al - at) del = al - at;
            rl = 0;
            for (int i = 0; i < at && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            for (int i = at + del; i < al && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            vm_release_temp_string(vm, sptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, r, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_REPLACE: {
            int32_t repptr = popi(vm, VT_STR) & 0xFFFF;
            int32_t len = popi(vm, VT_I32);
            int32_t pos = popi(vm, VT_I32);
            int32_t sptr = popi(vm, VT_STR) & 0xFFFF;
            int out_max = str_clamp_max(str_max(vm, sptr) + str_max(vm, repptr));
            al = str_to_buf(vm, sptr, a, STR_WORK_CAP);
            bl = str_to_buf(vm, repptr, b, STR_WORK_CAP);
            int at = pos - 1;
            if (at < 0) at = 0;
            if (at > al) at = al;
            int del = len;
            if (del < 0) del = 0;
            if (del > al - at) del = al - at;
            rl = 0;
            for (int i = 0; i < at && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            for (int i = 0; i < bl && rl < STR_WORK_CAP; i++) r[rl++] = b[i];
            for (int i = at + del; i < al && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            release_temp_pair(vm, repptr, sptr);
            int32_t t = alloc_temp_ptr(vm, out_max);
            str_write_bytes(vm, t, r, rl);
            pushi(vm, VT_STR, t);
            return;
        }
        case STR_FIND: {
            int32_t needle = popi(vm, VT_STR) & 0xFFFF;
            int32_t hay = popi(vm, VT_STR) & 0xFFFF;
            al = str_to_buf(vm, hay, a, STR_WORK_CAP);
            bl = str_to_buf(vm, needle, b, STR_WORK_CAP);
            int pos = str_find_buf(a, al, b, bl);
            release_temp_pair(vm, needle, hay);
            pushi(vm, VT_I32, pos);
            return;
        }
        case STR_CHARAT: {
            int32_t pos = popi(vm, VT_I32);
            int32_t sptr = popi(vm, VT_STR) & 0xFFFF;
            al = str_len(vm, sptr);
            int idx = pos - 1;
            int v = (idx >= 0 && idx < al) ? (str_byte(vm, sptr, idx) & 0xFF) : 0;
            vm_release_temp_string(vm, sptr);
            pushi(vm, VT_U8, v);
            return;
        }
        case STR_EQ:
        case STR_NE:
        case STR_LT:
        case STR_LE:
        case STR_GT:
        case STR_GE: {
            int32_t bptr = popi(vm, VT_STR) & 0xFFFF;
            int32_t aptr = popi(vm, VT_STR) & 0xFFFF;
            int c = str_cmp_ptr(vm, aptr, bptr);
            int rbool = 0;
            if (subop == STR_EQ) rbool = (c == 0);
            else if (subop == STR_NE) rbool = (c != 0);
            else if (subop == STR_LT) rbool = (c < 0);
            else if (subop == STR_LE) rbool = (c <= 0);
            else if (subop == STR_GT) rbool = (c > 0);
            else rbool = (c >= 0);
            release_temp_pair(vm, bptr, aptr);
            push_bool(vm, rbool);
            return;
        }
        case STR_ASSIGN: {
            int32_t dst = popi(vm, VT_STR) & 0xFFFF;
            int32_t src = popi(vm, VT_STR) & 0xFFFF;
            al = str_to_buf(vm, src, a, STR_WORK_CAP);
            str_write_bytes(vm, dst, a, al);
            if (dst == src) {
                vm_release_temp_string(vm, src);
            } else {
                release_temp_pair(vm, dst, src);
            }
            return;
        }
        case STR_APPEND_ASSIGN: {
            int32_t rhs = popi(vm, VT_STR) & 0xFFFF;
            int32_t dst = popi(vm, VT_STR) & 0xFFFF;
            al = str_to_buf(vm, dst, a, STR_WORK_CAP);
            bl = str_to_buf(vm, rhs, b, STR_WORK_CAP);
            rl = 0;
            for (int i = 0; i < al && rl < STR_WORK_CAP; i++) r[rl++] = a[i];
            for (int i = 0; i < bl && rl < STR_WORK_CAP; i++) r[rl++] = b[i];
            str_write_bytes(vm, dst, r, rl);
            if (dst == rhs) {
                vm_release_temp_string(vm, rhs);
            } else {
                release_temp_pair(vm, rhs, dst);
            }
            return;
        }
        default:
            fprintf(stderr, "ERROR: STRING subop desconocido %d\n", subop);
            exit(1);
    }
}

/**
 * @brief Bucle principal fetch-decode-execute de la VM.
 *
 * Política por modo:
 * - `VM_MODE_DEBUG`: permite pausa (`vm_stop`) y breakpoints.
 * - `VM_MODE_TRACE`: traza estado por instrucción.
 * - `VM_MODE_METRICS`: incrementa contador de instrucciones.
 * - `0`: ejecución normal sin instrumentación explícita.
 */
void run_vm(VM *vm) {   /* external: osoruntime.c drives the scan loop */
    while (1) {
        switch (vm->vm_mode) {
            case VM_MODE_DEBUG:
            if (!vm->running) return; // stop => no ejecutar
            if (vm_has_breakpoint_at(vm, vm->pc)) {
                vm->running = 0;
                if (vm->vm_mode & VM_MODE_TRACE) {
                    printf("[DEBUG] breakpoint en pc=%d\n", vm->pc);
                }
                return;
            }
            break;            
            
            case VM_MODE_TRACE:
                printf("[TRACE] pc=%d sp=%d fp=%d\n", vm->pc, vm->sp, vm->fp);
                break;
            case VM_MODE_METRICS:
                vm->instr_executed++;
                break;

            default:
                break;
        }

        int op = cu8(vm);
        switch (op) {
            case PUSH_I: {
                int t = cu8(vm);
                if (t == VT_S8) push1(vm, (int8_t) ci8(vm));
                else if (t == VT_U8) push1(vm, cu8(vm));
                else if (t == VT_S16) push2(vm, (int16_t) ci16(vm));
                else if (t == VT_U16 || t == VT_STR) push2(vm, cu16(vm));
                else if (t == VT_I32) push4(vm, ci32(vm));
                else die("PUSH_I tipo invalido");
                break;
            }
            case PUSH_F: push4(vm, ci32(vm)); break;
            case PUSH_S: push2(vm, cu16(vm)); break;

            case LOAD_G: { int a = cu16(vm), t = cu8(vm); push_ram(vm, a, t, "LOAD_G"); break; }
            case STORE_G: { int a = cu16(vm), t = cu8(vm); store_ram(vm, a, t, "STORE_G"); break; }
            case LOAD_L: { int o = cu16(vm), t = cu8(vm); push_ram(vm, local_addr(vm, o, "LOAD_L"), t, "LOAD_L"); break; }
            case STORE_L: { int o = cu16(vm), t = cu8(vm); store_ram(vm, local_addr(vm, o, "STORE_L"), t, "STORE_L"); break; }

            case LOAD_GA: { int b = cu16(vm), t = cu8(vm), nd = cu8(vm), dp = vm->pc; int off = pop_array_offset(vm, nd, dp, "LOAD_GA"); vm->pc += nd * 12; push_ram(vm, b + off * tbytes(t), t, "LOAD_GA"); break; }
            case STORE_GA: { int b = cu16(vm), t = cu8(vm), nd = cu8(vm), dp = vm->pc; int off = pop_array_offset(vm, nd, dp, "STORE_GA"); vm->pc += nd * 12; store_ram(vm, b + off * tbytes(t), t, "STORE_GA"); break; }
            case LOAD_LA: { int o = cu16(vm), t = cu8(vm), nd = cu8(vm), dp = vm->pc; int off = pop_array_offset(vm, nd, dp, "LOAD_LA"); vm->pc += nd * 12; int b = local_addr(vm, o, "LOAD_LA(base)"); push_ram(vm, b + off * tbytes(t), t, "LOAD_LA"); break; }
            case STORE_LA: { int o = cu16(vm), t = cu8(vm), nd = cu8(vm), dp = vm->pc; int off = pop_array_offset(vm, nd, dp, "STORE_LA"); vm->pc += nd * 12; int b = local_addr(vm, o, "STORE_LA(base)"); store_ram(vm, b + off * tbytes(t), t, "STORE_LA"); break; }
            case LOAD_CA: { int a = cu16(vm), t = cu8(vm), nd = cu8(vm), dp = vm->pc; int off = pop_array_offset(vm, nd, dp, "LOAD_CA"); vm->pc += nd * 12; push_code(vm, a + off * tbytes(t), t, "LOAD_CA"); break; }

            case NEWARR_G: { cu16(vm); cu8(vm); int nd = cu8(vm); vm->pc += nd * 8; break; }
            case NEWARR_L: { cu16(vm); cu8(vm); int nd = cu8(vm); vm->pc += nd * 8; break; }

            case MOV_GI8: { int a = cu16(vm), i = ci8(vm); wr32_ram(vm, a, i); break; }
            case MOV_GI32: { int a = cu16(vm), i = ci32(vm); wr32_ram(vm, a, i); break; }
            case MOV_GG: { int s = cu16(vm), d = cu16(vm); copy_ram(vm, s, d, 4); break; }
            case INC_GI8: { int a = cu16(vm), d = ci8(vm); wr32_ram(vm, a, rd32_ram(vm, a) + d); break; }
            case ADD_GG_G: { int a = cu16(vm), b = cu16(vm), d = cu16(vm); wr32_ram(vm, d, rd32_ram(vm, a) + rd32_ram(vm, b)); break; }
            case SUB_GG_G: { int a = cu16(vm), b = cu16(vm), d = cu16(vm); wr32_ram(vm, d, rd32_ram(vm, a) - rd32_ram(vm, b)); break; }
            case MOV_LI8: { int o = cu16(vm), i = ci8(vm); int a = local_addr(vm, o, "MOV_LI8"); wr32_ram(vm, a, i); break; }
            case MOV_LI32: { int o = cu16(vm), i = ci32(vm); int a = local_addr(vm, o, "MOV_LI32"); wr32_ram(vm, a, i); break; }
            case MOV_LL: { int so = cu16(vm), dof = cu16(vm); int s = local_addr(vm, so, "MOV_LL(src)"); int d = local_addr(vm, dof, "MOV_LL(dst)"); copy_ram(vm, s, d, 4); break; }
            case INC_LI8: { int o = cu16(vm), d = ci8(vm); int a = local_addr(vm, o, "INC_LI8"); wr32_ram(vm, a, rd32_ram(vm, a) + d); break; }
            case ADD_LL_L: { int ao = cu16(vm), bo = cu16(vm), dof = cu16(vm); int a = local_addr(vm, ao, "ADD_LL_L(a)"); int b = local_addr(vm, bo, "ADD_LL_L(b)"); int d = local_addr(vm, dof, "ADD_LL_L(d)"); wr32_ram(vm, d, rd32_ram(vm, a) + rd32_ram(vm, b)); break; }
            case SUB_LL_L: { int ao = cu16(vm), bo = cu16(vm), dof = cu16(vm); int a = local_addr(vm, ao, "SUB_LL_L(a)"); int b = local_addr(vm, bo, "SUB_LL_L(b)"); int d = local_addr(vm, dof, "SUB_LL_L(d)"); wr32_ram(vm, d, rd32_ram(vm, a) - rd32_ram(vm, b)); break; }
            case MUL_LL_L: { int ao = cu16(vm), bo = cu16(vm), dof = cu16(vm); int a = local_addr(vm, ao, "MUL_LL_L(a)"); int b = local_addr(vm, bo, "MUL_LL_L(b)"); int d = local_addr(vm, dof, "MUL_LL_L(d)"); wr32_ram(vm, d, rd32_ram(vm, a) * rd32_ram(vm, b)); break; }
            case ADD_LI_L: { int so = cu16(vm), imm = ci32(vm), dof = cu16(vm); int s = local_addr(vm, so, "ADD_LI_L(src)"); int d = local_addr(vm, dof, "ADD_LI_L(dst)"); wr32_ram(vm, d, rd32_ram(vm, s) + imm); break; }

            case ADD: { int t = cu8(vm); if (t == VT_F32) pushf(vm, popf(vm) + popf(vm)); else { int32_t rb = popi(vm, t), ra = popi(vm, t); pushi(vm, t, ra + rb); } break; }
            case SUB: { int t = cu8(vm); if (t == VT_F32) { float b = popf(vm), a = popf(vm); pushf(vm, a - b); } else { int32_t b = popi(vm, t), a = popi(vm, t); pushi(vm, t, a - b); } break; }
            case MUL: { int t = cu8(vm); if (t == VT_F32) pushf(vm, popf(vm) * popf(vm)); else { int32_t rb = popi(vm, t), ra = popi(vm, t); pushi(vm, t, ra * rb); } break; }
            case DIV: {
                int t = cu8(vm);
                if (t == VT_F32) {
                    float b = popf(vm), a = popf(vm);
                    push_mathf(vm, a / b);
                } else {
                    int32_t b = popi(vm, t), a = popi(vm, t);
                    if (b == 0) {
                        set_math_error(vm);
                        pushi(vm, t, 0);
                    } else {
                        pushi(vm, t, a / b);
                    }
                }
                break;
            }
            case NEG: { int t = cu8(vm); if (t == VT_F32) pushf(vm, -popf(vm)); else pushi(vm, t, -popi(vm, t)); break; }

            case LT: cmp_op(vm, cu8(vm), 0); break;
            case LE: cmp_op(vm, cu8(vm), 1); break;
            case GT: cmp_op(vm, cu8(vm), 2); break;
            case GE: cmp_op(vm, cu8(vm), 3); break;
            case EQ: cmp_op(vm, cu8(vm), 4); break;
            case NE: cmp_op(vm, cu8(vm), 5); break;
            case AND: { int t = cu8(vm); int b = pop_truthy(vm, t), a = pop_truthy(vm, t); push_bool(vm, a && b); break; }
            case OR: { int t = cu8(vm); int b = pop_truthy(vm, t), a = pop_truthy(vm, t); push_bool(vm, a || b); break; }
            case NOT: { int t = cu8(vm); push_bool(vm, !pop_truthy(vm, t)); break; }
            case XOR: { int t = cu8(vm); int b = pop_truthy(vm, t), a = pop_truthy(vm, t); push_bool(vm, (!!a) ^ (!!b)); break; }

            case BITAND: { int t = cu8(vm); int32_t b = popi(vm, t), a = popi(vm, t); pushi(vm, t, a & b); break; }
            case BITOR: { int t = cu8(vm); int32_t b = popi(vm, t), a = popi(vm, t); pushi(vm, t, a | b); break; }
            case BITXOR: { int t = cu8(vm); int32_t b = popi(vm, t), a = popi(vm, t); pushi(vm, t, a ^ b); break; }
            case BITNOT: { int t = cu8(vm); int32_t a = popi(vm, t); pushi(vm, t, ~a); break; }
            case SHL: { int t = cu8(vm); int32_t n = popi(vm, VT_I32), a = popi(vm, t); pushi(vm, t, shl_bits(t, a, n)); break; }
            case SHR: { int t = cu8(vm); int32_t n = popi(vm, VT_I32), a = popi(vm, t); pushi(vm, t, shr_bits(t, a, n)); break; }
            case ROL: { int t = cu8(vm); int32_t n = popi(vm, VT_I32), a = popi(vm, t); pushi(vm, t, rol_bits(t, a, n)); break; }
            case ROR: { int t = cu8(vm); int32_t n = popi(vm, VT_I32), a = popi(vm, t); pushi(vm, t, ror_bits(t, a, n)); break; }

            case CAST_S8: { int s = cu8(vm); pushi(vm, VT_S8, pop_int_cast(vm, s)); break; }
            case CAST_U8: { int s = cu8(vm); pushi(vm, VT_U8, pop_int_cast(vm, s)); break; }
            case CAST_S16: { int s = cu8(vm); pushi(vm, VT_S16, pop_int_cast(vm, s)); break; }
            case CAST_U16: { int s = cu8(vm); pushi(vm, VT_U16, pop_int_cast(vm, s)); break; }
            case CAST_I32: { int s = cu8(vm); pushi(vm, VT_I32, pop_int_cast(vm, s)); break; }
            case CAST_F32: { int s = cu8(vm); float f = (s == VT_F32) ? popf(vm) : (float) pop_int_cast(vm, s); pushf(vm, f); break; }

            case ADD_S8: case ADD_U8: case ADD_S16: case ADD_U16: case ADD_I32: case ADD_F32: {
                int t = vm_type_from_typed_idx(op - ADD_S8);
                if (t == VT_F32) pushf(vm, popf(vm) + popf(vm));
                else { int32_t rb = popi(vm, t), ra = popi(vm, t); pushi(vm, t, ra + rb); }
                break;
            }
            case SUB_S8: case SUB_U8: case SUB_S16: case SUB_U16: case SUB_I32: case SUB_F32: {
                int t = vm_type_from_typed_idx(op - SUB_S8);
                if (t == VT_F32) { float b = popf(vm), a = popf(vm); pushf(vm, a - b); }
                else { int32_t b = popi(vm, t), a = popi(vm, t); pushi(vm, t, a - b); }
                break;
            }
            case MUL_S8: case MUL_U8: case MUL_S16: case MUL_U16: case MUL_I32: case MUL_F32: {
                int t = vm_type_from_typed_idx(op - MUL_S8);
                if (t == VT_F32) pushf(vm, popf(vm) * popf(vm));
                else { int32_t rb = popi(vm, t), ra = popi(vm, t); pushi(vm, t, ra * rb); }
                break;
            }
            case DIV_S8: case DIV_U8: case DIV_S16: case DIV_U16: case DIV_I32: case DIV_F32: {
                int t = vm_type_from_typed_idx(op - DIV_S8);
                if (t == VT_F32) {
                    float b = popf(vm), a = popf(vm);
                    push_mathf(vm, a / b);
                } else {
                    int32_t b = popi(vm, t), a = popi(vm, t);
                    if (b == 0) {
                        set_math_error(vm);
                        pushi(vm, t, 0);
                    } else {
                        pushi(vm, t, a / b);
                    }
                }
                break;
            }
            case NEG_S8: case NEG_U8: case NEG_S16: case NEG_U16: case NEG_I32: case NEG_F32: {
                int t = vm_type_from_typed_idx(op - NEG_S8);
                if (t == VT_F32) pushf(vm, -popf(vm)); else pushi(vm, t, -popi(vm, t));
                break;
            }
            case LT_S8: case LT_U8: case LT_S16: case LT_U16: case LT_I32: case LT_F32:
                cmp_op(vm, vm_type_from_typed_idx(op - LT_S8), 0);
                break;
            case LE_S8: case LE_U8: case LE_S16: case LE_U16: case LE_I32: case LE_F32:
                cmp_op(vm, vm_type_from_typed_idx(op - LE_S8), 1);
                break;
            case GT_S8: case GT_U8: case GT_S16: case GT_U16: case GT_I32: case GT_F32:
                cmp_op(vm, vm_type_from_typed_idx(op - GT_S8), 2);
                break;
            case GE_S8: case GE_U8: case GE_S16: case GE_U16: case GE_I32: case GE_F32:
                cmp_op(vm, vm_type_from_typed_idx(op - GE_S8), 3);
                break;
            case EQ_S8: case EQ_U8: case EQ_S16: case EQ_U16: case EQ_I32: case EQ_F32:
                cmp_op(vm, vm_type_from_typed_idx(op - EQ_S8), 4);
                break;
            case NE_S8: case NE_U8: case NE_S16: case NE_U16: case NE_I32: case NE_F32:
                cmp_op(vm, vm_type_from_typed_idx(op - NE_S8), 5);
                break;
            case AND_B: { int b = pop_truthy(vm, VT_S8), a = pop_truthy(vm, VT_S8); push_bool(vm, a && b); break; }
            case OR_B: { int b = pop_truthy(vm, VT_S8), a = pop_truthy(vm, VT_S8); push_bool(vm, a || b); break; }
            case XOR_B: { int b = pop_truthy(vm, VT_S8), a = pop_truthy(vm, VT_S8); push_bool(vm, (!!a) ^ (!!b)); break; }
            case NOT_B: { push_bool(vm, !pop_truthy(vm, VT_S8)); break; }

            case JMP: vm->pc += ci16(vm); break;
            case JMP8: vm->pc += ci8(vm); break;
            case JMPF: { int16_t off = ci16(vm); if (!pop_truthy(vm, VT_S8)) vm->pc += off; break; }
            case JMPF8: { int8_t off = ci8(vm); if (!pop_truthy(vm, VT_S8)) vm->pc += off; break; }

            case CALL: { int a = cu16(vm); push2(vm, vm->pc); vm->pc = a; break; }
            case CALL8: { int off = ci8(vm); push2(vm, vm->pc); vm->pc += off; break; }
            case CALL16: { int off = ci16(vm); push2(vm, vm->pc); vm->pc += off; break; }
            case LINK: {
                int frame = cu16(vm), pbytes = cu16(vm);
                int fb = vm->sp - (pbytes + 2);
                if (fb < vm->global_bytes) die("LINK pisa globals");
                int ret = rd16_ram(vm, fb + pbytes);
                int new_sp = fb + (int) FRAME_HEADER_BYTES + frame;
                if (new_sp > vm->str_temp_top || new_sp > vm->stack_limit) die("Stack overflow LINK");
                copy_ram(vm, fb, fb + (int) FRAME_HEADER_BYTES, pbytes);
                wr16_ram(vm, fb, (uint16_t) enc_fp(vm->fp));
                wr16_ram(vm, fb + 2, (uint16_t) ret);
                wr16_ram(vm, fb + 4, (uint16_t) frame);
                int ls = fb + (int) FRAME_HEADER_BYTES + pbytes;
                int le = fb + (int) FRAME_HEADER_BYTES + frame;
                if (le < ls || le > vm->str_temp_top || le > vm->stack_limit) die("Frame invalido LINK");
                zero_ram(vm, ls, le - ls);
                vm->fp = fb;
                vm->sp = new_sp;
                break;
            }
            case UNLINK: {
                if (vm->fp < 0) die("UNLINK sin frame");
                int prev = dec_fp(rd16_ram(vm, vm->fp));
                int ret = rd16_ram(vm, vm->fp + 2);
                vm->sp = vm->fp;
                vm->fp = prev;
                push2(vm, ret);
                break;
            }
            case RET: vm->pc = pop2(vm) & 0xFFFF; break;
            case LEAVE: {
                int rb = cu8(vm);
                int raw = (rb > 0) ? popn(vm, rb) : 0;
                if (vm->fp < 0) die("LEAVE sin frame");
                int ret = rd16_ram(vm, vm->fp + 2);
                int prev = dec_fp(rd16_ram(vm, vm->fp));
                vm->sp = vm->fp;
                vm->fp = prev;
                if (rb > 0) pushn(vm, raw, rb);
                vm->pc = ret;
                break;
            }

            case DEBUG: {
                int n = cu8(vm), tp = vm->pc;
                vm->pc += n;
                int total = 0;
                for (int i = 0; i < n; i++) total += tbytes(vm->code[tp + i] & 0xFF);
                int base = vm->sp - total;
                if (base < vm->global_bytes) die("DEBUG underflow");
                int *offs = NULL;
                if (n > 0) {
                    offs = (int *) malloc((size_t) n * sizeof(int));
                    if (!offs) die("sin memoria DEBUG");
                }
                int off = 0;
                for (int i = 0; i < n; i++) {
                    int t = vm->code[tp + i] & 0xFF;
                    offs[i] = off;
                    char tmp[256];
                    value_to_text_at(vm, base + off, t, tmp, sizeof(tmp));
                    printf("%s ", tmp);
                    off += tbytes(t);
                }
                printf("\n");
                for (int i = n - 1; i >= 0; i--) {
                    int t = vm->code[tp + i] & 0xFF;
                    if (t == VT_STR) {
                        int32_t ptr = rd16_ram(vm, base + offs[i]) & 0xFFFF;
                        vm_release_temp_string(vm, ptr);
                    }
                }
                free(offs);
                zero_ram(vm, base, total);
                vm->sp = base;
                break;
            }
            case STRING: {
                int subop = cu8(vm);
                exec_string(vm, subop);
                break;
            }
            case TRAP: {
                uint8_t trap_id = cu8(vm);
                hardware(vm, trap_id);
                break;
            }
            case MATH: {
                int subop = cu8(vm);
                exec_math(vm, subop);
                break;
            }
            case LOG: { float x = popf(vm); push_mathf(vm, logf(x)); break; }
            case HALT:
                printf("\n[PCODEVM HALT]\n");
                return;
            default:
                fprintf(stderr, "ERROR: opcode desconocido %d pc=%d\n", op, vm->pc - 1);
                exit(1);
        }
    }
}

/**
 * @brief Punto de entrada de la aplicación `pcodevm`.
 *
 * Responsabilidades:
 * 1) Parsear argumentos y modo de ejecución.
 * 2) Cargar y validar imagen de programa (Intel HEX + header STLite).
 * 3) Reservar memoria de CODE/RAM e inicializar `VM`.
 * 4) Ejecutar VM y mostrar métricas opcionales.
 */
