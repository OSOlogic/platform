# OSOlogic MCU tier — host build (simulator + unit tests). No RTOS/hardware needed.
# The MCU builds (Zephyr/FreeRTOS) live under their own dirs; this Makefile is the
# dependency-free way to build and verify the portable core on a desktop.
#
# (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2
INC      = -Isrc -Igenerated
BUILD    = build

CORE = src/osodb_embedded.c src/pid.c src/federation.c src/scan.c generated/board_map.c

.PHONY: all host test gen clean
all: host test

# regenerate board_map from the board profile (BOARD overridable)
BOARD ?= rp2040_demo
gen:
	python3 tools/boardgen.py boards/$(BOARD).board.yaml -o generated/

$(BUILD):
	mkdir -p $(BUILD)

host: gen $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(CORE) host/hal_host.c host/main_host.c -o $(BUILD)/osodb-mcu-sim
	@echo "built $(BUILD)/osodb-mcu-sim"

test: $(BUILD)
	$(CC) $(CFLAGS) $(INC) src/osodb_embedded.c tests/test_osodb_embedded.c -o $(BUILD)/osodb-test
	./$(BUILD)/osodb-test

clean:
	rm -rf $(BUILD)
