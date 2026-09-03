# FreeRTOS port (scaffold)

For the smallest targets where Zephyr is too heavy. The port reuses the **same** portable
core (`osodb_embedded`, `pid`, `scan`, `federation`) — only the RTOS glue and the vendor
HAL differ. See [`main_freertos.c`](main_freertos.c).

To bring it up on a target:

1. Add FreeRTOS + your MCU SDK (STM32Cube, pico-sdk, ESP-IDF, …) to your build.
2. Compile the core with `-DOSODB_PORT_HEADER=\"osodb_port_freertos.h\"` and include `../src`
   and `../generated`.
3. Write `hal_<mcu>.c` implementing [`../src/hal.h`](../src/hal.h) against your SDK.
4. Generate the bindings from your board profile: `make gen BOARD=<name>` (repo root of this
   module), which writes `../generated/board_map.c`.
5. Override `federation_emit()` to send over your link (UART/CAN/MQTT) and feed inbound
   `SET <id> <value>` lines to `federation_handle_line()`.

Zephyr is the primary target (native networking, broad board support); use FreeRTOS when
footprint demands it.
