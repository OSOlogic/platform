# osoST Runtime — P-code Virtual Machine

The osoST runtime executes IEC 61131-3 ST programs compiled to P-code Intel HEX.
It is written in portable C99 with no dependencies beyond libc.

El runtime osoST ejecuta programas IEC 61131-3 ST compilados a P-code Intel HEX.
Está escrito en C99 portátil sin más dependencias que libc.

## Files / Archivos

| File                | Purpose                                                     |
|---------------------|-------------------------------------------------------------|
| `pcodevm.h`         | VM struct, opcode enum, public API                          |
| `pcodevm.c`         | Stack-machine interpreter (~1500 lines)                     |
| `osoruntime.c`      | `main()`, HEX loader, scan-cycle loop                       |
| `hardware_linux.c`  | HAL for Linux: GPIO (libgpiod), Modbus TCP (libmodbus)      |
| `hardware_bare.c`   | HAL template: STM32 / RP2040 / ESP32                        |
| `hardware_demo.c`   | Demo HAL: Mandelbrot, BACnet stub (no real hardware needed) |
| `Makefile`          | Build targets for Linux and cross-compilation notes         |

## Build / Compilación

```bash
# Demo build (no hardware drivers)
make osoruntime_demo

# Linux GPIO + Modbus build
make gpiod        # needs libgpiod-dev
make modbus       # needs libmodbus-dev

# Run / Ejecutar
./osoruntime program.hex
./osoruntime program.hex --scan=10 --debug --metrics
```

## Scan cycle options / Opciones del ciclo de scan

| Flag         | Default | Description                                          |
|--------------|---------|------------------------------------------------------|
| `--scan=N`   | 0 (free-run) | Cycle period in milliseconds                   |
| `--ram=N`    | from HEX header | RAM allocation override in bytes            |
| `--debug`    | off     | Enable `VM_MODE_DEBUG` (debug() output)              |
| `--trace`    | off     | Enable `VM_MODE_TRACE` (instruction trace)           |
| `--metrics`  | off     | Print instruction count and timing after each cycle  |
| `--once`     | off     | Run exactly one scan cycle then exit                 |
| `--mode=N`   | 0       | Set `vm_mode` directly (bitmask)                     |

## HAL interface / Interfaz HAL

Implement `hardware_linux.c` or `hardware_bare.c` and provide:

```c
void hardware(VM *vm, uint8_t trap_id);
void hardware_read_inputs(VM *vm);   // called before each scan
void hardware_write_outputs(VM *vm); // called after each scan
```

Standard trap IDs / IDs de trap estándar:

| ID  | ST symbol      | Description                        |
|-----|----------------|------------------------------------|
| 10  | `gpio_write`   | Write GPIO output                  |
| 11  | `gpio_read`    | Read GPIO input → I32              |
| 12  | `millis`       | Milliseconds since start → I32     |
| 17  | `mandelbrot`   | Demo: Mandelbrot iteration count   |
| 18  | `cpu`          | Demo: CPU usage placeholder        |
| 20  | `modbus_write_coil` | Write Modbus coil             |
| 21  | `modbus_read_coil`  | Read Modbus coil → I32        |

---

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund \<miguel@ibercomp.com\>  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Jose Roig Borrell, Roig Borrell SL, Ibercomp SL  
Part of **OsoLogic®** — [osologic.org](https://osologic.org)  
SPDX-License-Identifier: AGPL-3.0-or-later
