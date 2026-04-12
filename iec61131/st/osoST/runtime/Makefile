# osoLogic — IEC 61131-3 Structured Text Runtime
# Makefile — Build targets for Linux PLC and bare-metal MCU
#
# Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
#               Ibercomp SL, Roig Borrell SL
#
# Part of the osoLogic open-source PLC project — osologic.org
# SPDX-License-Identifier: AGPL-3.0-or-later

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -I.
LDFLAGS = -lm

# ── Linux PLC runtime (default) ──────────────────────────────────
# Uses hardware_demo.c for testing, swap for hardware_linux.c in production.
# Usa hardware_demo.c para pruebas, sustituir por hardware_linux.c en producción.

all: osoruntime osoruntime_demo

osoruntime: osoruntime.c pcodevm.c hardware_linux.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	@echo "Built: osoruntime (Linux HAL)"

osoruntime_demo: osoruntime.c pcodevm.c hardware_demo.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	@echo "Built: osoruntime_demo (demo HAL)"

# ── With libgpiod support ─────────────────────────────────────────
gpiod: CFLAGS += -DUSE_GPIOD
gpiod: LDFLAGS += -lgpiod
gpiod: osoruntime.c pcodevm.c hardware_linux.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o osoruntime_gpiod
	@echo "Built: osoruntime_gpiod"

# ── With Modbus TCP support ───────────────────────────────────────
modbus: CFLAGS += -DUSE_GPIOD -DUSE_MODBUS
modbus: LDFLAGS += -lgpiod -lmodbus
modbus: osoruntime.c pcodevm.c hardware_linux.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o osoruntime_modbus
	@echo "Built: osoruntime_modbus"

# ── ARM cross-compilation (Raspberry Pi / ARM PLC) ───────────────
# arm:
# 	arm-linux-gnueabihf-gcc $(CFLAGS) osoruntime.c pcodevm.c hardware_linux.c \
# 	  $(LDFLAGS) -o osoruntime_arm

# ── Bare-metal STM32 (example, adapt to your toolchain) ──────────
# stm32:
# 	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -DTARGET_STM32 -I. \
# 	  pcodevm.c hardware_bare.c your_main.c $(LDFLAGS) -o firmware.elf

# ── RP2040 / Raspberry Pi Pico ────────────────────────────────────
# Add to your CMakeLists.txt:
#   target_sources(your_target PRIVATE pcodevm.c hardware_bare.c)
#   target_compile_definitions(your_target PRIVATE TARGET_RP2040)

# ── Quick test with example hex ───────────────────────────────────
test: osoruntime_demo
	./osoruntime_demo ../../STLiteOsoLogic/stt/dist/ejemplo.hex --once --metrics

# ── Clean ─────────────────────────────────────────────────────────
clean:
	rm -f osoruntime osoruntime_demo osoruntime_gpiod osoruntime_modbus *.o

.PHONY: all gpiod modbus test clean
