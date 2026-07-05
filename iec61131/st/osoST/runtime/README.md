<div align="center">
  <img src="../../../../logos/osologic_logo.png" width="120" alt="OSOlogic logo">
  <h1>osoST Runtime</h1>
  <p><strong>P-code virtual machine for IEC 61131-3 ST — portable C99, no dependencies</strong></p>
  <p>
    <img src="https://img.shields.io/badge/IEC_61131--3-ST_Runtime-800000?style=flat-square">
    <img src="https://img.shields.io/badge/license-AGPL--3.0-800000?style=flat-square">
    <img src="https://img.shields.io/badge/(C)_A.M._Zúñiga_·_J._Roig_Borrell-Roig_Borrell_S.L._·_Ibercomp_S.L.-111111?style=flat-square">
  </p>
</div>

---

## Files / Archivos

| File                | Purpose                                                     |
|---------------------|-------------------------------------------------------------|
| `pcodevm.h`         | VM struct, opcode enum, public API / Struct VM, opcodes, API pública |
| `pcodevm.c`         | Stack-machine interpreter / Intérprete de máquina de pila   |
| `osoruntime.c`      | `main()`, HEX loader, scan-cycle loop / main(), carga HEX, bucle de scan |
| `hardware_linux.c`  | HAL for Linux: GPIO (libgpiod), Modbus TCP (libmodbus)      |
| `hardware_bare.c`   | HAL template: STM32 / RP2040 / ESP32                        |
| `hardware_demo.c`   | Demo HAL — no real hardware required / sin hardware real    |
| `Makefile`          | Build targets + cross-compilation notes                     |

---

## Build / Compilación

```bash
make osoruntime_demo          # no hardware drivers
make gpiod                    # Linux GPIO (needs libgpiod-dev)
make modbus                   # Linux Modbus TCP (needs libmodbus-dev)

./osoruntime program.hex
./osoruntime program.hex --scan=10 --debug --metrics
```

---

## Scan cycle options / Opciones del ciclo de scan

| Flag         | Default      | Description                                          |
|--------------|--------------|------------------------------------------------------|
| `--scan=N`   | 0 (free-run) | Cycle period in milliseconds / Período en ms         |
| `--ram=N`    | from header  | RAM allocation override in bytes                     |
| `--debug`    | off          | Enable `VM_MODE_DEBUG` (debug() output)              |
| `--trace`    | off          | Enable `VM_MODE_TRACE` (instruction trace)           |
| `--metrics`  | off          | Print instruction count and timing per cycle         |
| `--once`     | off          | Run exactly one scan cycle then exit                 |
| `--mode=N`   | 0            | Set `vm_mode` bitmask directly                       |

---

## HAL trap IDs / IDs de trap HAL

| ID  | ST symbol           | Description                              |
|-----|---------------------|------------------------------------------|
| 10  | `gpio_write`        | Write GPIO output / Escribir salida GPIO |
| 11  | `gpio_read`         | Read GPIO input → I32                    |
| 12  | `millis`            | Milliseconds since start → I32           |
| 17  | `mandelbrot`        | Demo: Mandelbrot iteration count         |
| 18  | `cpu`               | Demo: CPU usage placeholder              |
| 20  | `modbus_write_coil` | Write Modbus TCP coil                    |
| 21  | `modbus_read_coil`  | Read Modbus TCP coil → I32               |
| 30  | `tag_read`          | Read osodb tag by binding index → I32 (ACL-filtered) |
| 31  | `tag_write`         | Write osodb tag by binding index (ACL-filtered)      |

**osodb tag traps (#30/#31)** — the Ladder→ST compiler emits an I/O image sync so a program
reads/writes [osodb](../../../core/osodb) tags (backed by MariaDB) each scan. The binding index
maps to a tag id + ACL mode (`in`/`out`/`inout`) via the manifest in the generated ST header; the
HAL resolves the index and **enforces the ACL** (a `tag_write` to a read-only binding is rejected).
See [`iec61131/ladder`](../../ladder/).

---

## Related / Relacionado

- [`../`](../) — osoST project root
- [`../compiler-python/`](../compiler-python/) — Python compiler (ostc)
- [`../compiler-java/`](../compiler-java/) — Java compiler REST wrapper

---

<div align="center">
  <sub>(C) Angel Miguel Zúñiga Schmemund &lt;miguel@ibercomp.com&gt; · Jose Roig Borrell · Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0</sub>
</div>
