/*
 * osoLogic — IEC 61131-3 Structured Text Runtime
 * osoruntime.c — main() entry point and PLC scan cycle for Linux
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Usage / Uso:
 *   osoruntime <program.hex> [options]
 *
 * Options / Opciones:
 *   --scan=N     Scan cycle period in milliseconds (default: 10)
 *                Período del ciclo de scan en milisegundos (defecto: 10)
 *   --ram=N      RAM size in bytes (default: 65536)
 *                Tamaño de RAM en bytes (defecto: 65536)
 *   --debug      Enable debug mode (breakpoints, vm_stop)
 *                Activar modo debug (breakpoints, vm_stop)
 *   --trace      Trace each instruction (stdout)
 *                Trazar cada instrucción (stdout)
 *   --metrics    Print execution metrics on exit
 *                Imprimir métricas de ejecución al salir
 *   --once       Run one scan cycle and exit (useful for testing)
 *                Ejecutar un ciclo de scan y salir (útil para pruebas)
 *   --mode=N     Set vm_mode bitmask directly (overrides --debug/--trace)
 *                Fijar vm_mode directamente (sobrescribe --debug/--trace)
 *
 * Scan cycle / Ciclo de scan (Linux):
 *   Uses timerfd_create(CLOCK_MONOTONIC) for deterministic timing.
 *   With PREEMPT_RT kernel, jitter is typically <50 µs on ARM.
 *   Usa timerfd_create(CLOCK_MONOTONIC) para temporización determinista.
 *   Con kernel PREEMPT_RT, el jitter es típicamente <50 µs en ARM.
 *
 * For bare-metal MCU scan cycle, see hardware_bare.c.
 * Para ciclo de scan en MCU bare-metal, ver hardware_bare.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <signal.h>

#ifdef __linux__
#  include <sys/timerfd.h>
#  include <unistd.h>
#endif

#include "pcodevm.h"

/* ================================================================
 * Internal helpers (duplicated from pcodevm.c to keep this file
 * self-contained; consider moving to a small pcodevm_loader.h)
 * ================================================================ */

#define MAGIC_EXPECTED  0xA55Au
#define HEADER_SIZE     24u
#define STR_MAX         255

typedef struct { uint8_t *data; uint32_t size; } ByteImage;

typedef struct {
    uint16_t magic;
    uint32_t app_id;
    uint16_t version;
    uint32_t code_start;
    uint32_t stack_needed;
    uint32_t global_bytes;
    uint32_t minimum_ram_size;
} ExecHeader;

static void die(const char *msg) { fprintf(stderr, "ERROR: %s\n", msg); exit(1); }

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int parse_hex_byte(const char *s, uint8_t *out) {
    int hi = hex_nibble(s[0]), lo = hex_nibble(s[1]);
    if (hi < 0 || lo < 0) return 0;
    *out = (uint8_t)((hi << 4) | lo);
    return 1;
}

static ByteImage load_hex(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "ERROR: cannot open HEX: %s\n", path); exit(1); }
    uint8_t *img = NULL; uint32_t cap = 0, used = 0, upper = 0;
    int eof = 0; char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]='\0';
        if (!n) continue;
        if (line[0] != ':') die("invalid HEX line");
        uint8_t len=0, type=0, cks=0; uint16_t addr=0;
        parse_hex_byte(line+1, &len);
        uint8_t ah=0, al=0; parse_hex_byte(line+3,&ah); parse_hex_byte(line+5,&al);
        addr = (uint16_t)(((uint16_t)ah<<8)|al);
        parse_hex_byte(line+7, &type);
        uint8_t data[255]; uint8_t sum=(uint8_t)(len+((addr>>8)&0xFF)+(addr&0xFF)+type);
        for (uint32_t i=0;i<len;i++){parse_hex_byte(line+9+i*2,&data[i]);sum=(uint8_t)(sum+data[i]);}
        parse_hex_byte(line+9+len*2,&cks); sum=(uint8_t)(sum+cks);
        if (sum) die("HEX checksum error");
        if (type==0x00){
            uint32_t full=(upper<<16)|addr, need=full+len;
            if (need>cap){uint32_t nc=cap?cap:1024;while(nc<need)nc*=2;img=(uint8_t*)realloc(img,nc);if(!img)die("OOM");if(nc>cap)memset(img+cap,0,nc-cap);cap=nc;}
            if(len)memcpy(img+full,data,len); if(need>used)used=need;
        } else if(type==0x01){eof=1;break;}
        else if(type==0x04&&len==2){upper=((uint32_t)data[0]<<8)|data[1];}
    }
    fclose(f); if(!eof)die("HEX without EOF");
    return (ByteImage){img, used};
}

static ExecHeader parse_header(const ByteImage *img) {
    if (img->size < HEADER_SIZE) die("image too short");
    ExecHeader h;
    h.magic          = (uint16_t)(img->data[0]|(uint16_t)img->data[1]<<8);
    h.app_id         = (uint32_t)img->data[2]|((uint32_t)img->data[3]<<8)|((uint32_t)img->data[4]<<16)|((uint32_t)img->data[5]<<24);
    h.version        = (uint16_t)(img->data[6]|(uint16_t)img->data[7]<<8);
    h.code_start     = (uint32_t)img->data[8]|((uint32_t)img->data[9]<<8)|((uint32_t)img->data[10]<<16)|((uint32_t)img->data[11]<<24);
    h.stack_needed   = (uint32_t)img->data[12]|((uint32_t)img->data[13]<<8)|((uint32_t)img->data[14]<<16)|((uint32_t)img->data[15]<<24);
    h.global_bytes   = (uint32_t)img->data[16]|((uint32_t)img->data[17]<<8)|((uint32_t)img->data[18]<<16)|((uint32_t)img->data[19]<<24);
    h.minimum_ram_size=(uint32_t)img->data[20]|((uint32_t)img->data[21]<<8)|((uint32_t)img->data[22]<<16)|((uint32_t)img->data[23]<<24);
    if (h.magic != MAGIC_EXPECTED) die("invalid magic");
    if (h.code_start < HEADER_SIZE || h.code_start >= img->size) die("invalid code_start");
    return h;
}

/* ================================================================
 * Scan cycle (Linux timerfd)
 * Ciclo de scan (Linux timerfd)
 * ================================================================ */

static volatile int g_running = 1;

static void handle_sigint(int s) { (void)s; g_running = 0; }

/**
 * Run the PLC scan cycle using Linux timerfd for deterministic timing.
 * Each period: read_inputs → run_vm (main()) → write_outputs.
 *
 * Ejecuta el ciclo de scan PLC con timerfd de Linux para temporización
 * determinista. Cada período: leer_entradas → ejecutar_vm → escribir_salidas.
 *
 * @param vm          Initialized VM instance.
 * @param period_ms   Scan period in milliseconds.
 * @param run_once    If nonzero, execute exactly one cycle and return.
 */
static void scan_loop(VM *vm, int period_ms, int run_once) {
#ifdef __linux__
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0) { perror("timerfd_create"); exit(1); }

    long period_ns = (long)period_ms * 1000000L;
    struct itimerspec its = {
        .it_interval = { .tv_sec = 0, .tv_nsec = period_ns },
        .it_value    = { .tv_sec = 0, .tv_nsec = 1 },  /* arm immediately */
    };
    timerfd_settime(tfd, 0, &its, NULL);

    uint64_t overruns_total = 0;
    uint64_t cycles = 0;

    while (g_running) {
        uint64_t exp = 0;
        ssize_t r = read(tfd, &exp, sizeof(exp));
        if (r != (ssize_t)sizeof(exp)) break;

        if (exp > 1) {
            overruns_total += (exp - 1);
            fprintf(stderr, "WARN: scan overrun x%llu (cycle %llu)\n",
                    (unsigned long long)(exp-1), (unsigned long long)cycles);
        }

        /* HAL: read physical inputs into VM globals */
        hardware_read_inputs(vm);

        /* Run one ST scan: call main() */
        vm_reset(vm);
        vm_start(vm);
        extern void run_vm(VM *vm);  /* internal pcodevm.c function */
        run_vm(vm);

        /* HAL: write VM globals to physical outputs */
        hardware_write_outputs(vm);

        cycles++;
        if (run_once) break;
    }

    if (overruns_total)
        fprintf(stderr, "INFO: total scan overruns: %llu in %llu cycles\n",
                (unsigned long long)overruns_total, (unsigned long long)cycles);
    close(tfd);
#else
    /* Fallback for non-Linux: simple blocking loop with nanosleep */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)period_ms * 1000000L };
    uint64_t cycles = 0;
    while (g_running) {
        hardware_read_inputs(vm);
        vm_reset(vm); vm_start(vm);
        extern void run_vm(VM *vm);
        run_vm(vm);
        hardware_write_outputs(vm);
        cycles++;
        if (run_once) break;
        nanosleep(&ts, NULL);
    }
    (void)cycles;
#endif
}

/* ================================================================
 * main()
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("osoLogic PLC Runtime — osologic.org\n");
        printf("Usage: osoruntime <program.hex> [options]\n");
        printf("  --scan=N     Scan period ms (default 10)\n");
        printf("  --ram=N      RAM bytes (default 65536)\n");
        printf("  --debug      Debug mode\n");
        printf("  --trace      Trace instructions\n");
        printf("  --metrics    Print metrics on exit\n");
        printf("  --once       One scan cycle then exit\n");
        printf("  --mode=N     Set vm_mode bitmask\n");
        return 2;
    }

    const char *hex_path = argv[1];
    int  scan_ms  = 10;
    int  ram_size = 65536;
    int  run_once = 0;
    uint8_t vm_mode = VM_MODE_NORMAL;

    for (int i = 2; i < argc; i++) {
        if (!strncmp(argv[i],"--scan=",7))    scan_ms  = atoi(argv[i]+7);
        else if (!strncmp(argv[i],"--ram=",6))ram_size = atoi(argv[i]+6);
        else if (!strcmp(argv[i],"--debug"))   vm_mode |= VM_MODE_DEBUG;
        else if (!strcmp(argv[i],"--trace"))   vm_mode |= VM_MODE_TRACE;
        else if (!strcmp(argv[i],"--metrics")) vm_mode |= VM_MODE_METRICS;
        else if (!strcmp(argv[i],"--once"))    run_once = 1;
        else if (!strncmp(argv[i],"--mode=",7))vm_mode = (uint8_t)atoi(argv[i]+7);
        else { fprintf(stderr,"Unknown option: %s\n", argv[i]); return 2; }
    }

    /* Load program */
    printf("[osoRuntime] loading %s\n", hex_path);
    ByteImage img = load_hex(hex_path);
    ExecHeader hdr = parse_header(&img);

    uint32_t needed = (hdr.minimum_ram_size > (uint32_t)ram_size)
                      ? hdr.minimum_ram_size : (uint32_t)ram_size;
    uint8_t *ram  = (uint8_t *)calloc(1, needed);
    uint8_t *code = (uint8_t *)malloc(img.size - hdr.code_start);
    if (!ram || !code) die("out of memory");
    memcpy(code, img.data + hdr.code_start, img.size - hdr.code_start);

    VM vm = {0};
    vm.ram           = ram;
    vm.ram_size      = needed;
    vm.code          = code;
    vm.code_size     = img.size - hdr.code_start;
    vm.global_bytes  = (int32_t)hdr.global_bytes;
    vm.stack_limit   = (int32_t)needed;
    vm.vm_mode       = vm_mode;

    free(img.data);

    printf("[osoRuntime] program loaded — code=%u bytes, globals=%u bytes, ram=%u bytes\n",
           vm.code_size, hdr.global_bytes, needed);
    printf("[osoRuntime] scan_period=%d ms, mode=0x%02X\n", scan_ms, vm_mode);

    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);

    scan_loop(&vm, scan_ms, run_once);

    if (vm_mode & VM_MODE_METRICS) {
        printf("[osoRuntime] instructions executed: %llu\n",
               (unsigned long long)vm.instr_executed);
    }

    free(ram);
    free(code);
    return 0;
}
