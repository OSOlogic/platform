# osodb-mcu — the OSOlogic hard-real-time MCU tier (Tier 0)

The microcontroller face of OSOlogic. A tiny, allocation-free **`osodb-embedded`** tag
store runs the control loop **cycle-exact** on an MCU (Zephyr or FreeRTOS) and **federates**
its tags with the network hub — the Linux/x86 "big siblings" running the Community Edition.
This is the **hard-real-time** tier of the platform's [real-time tiering](../../docs/):
MCU (hard-RT, here) · Linux PREEMPT_RT (firm-RT, the CE) · validated deterministic (Enterprise).

> **Prototype.** The portable core, the board-profile generator and the desktop simulator
> are built and verified here; the Zephyr/FreeRTOS board targets are scaffolds to flesh out.

## Adapt to any hardware, fast — the board profile

You don't hand-wire pins in C. You describe the board once in **`boards/<name>.board.yaml`** —
pins, peripherals (GPIO/ADC/PWM, and CAN/UART/…), the tag each maps to, and the control
loops — and **`tools/boardgen.py`** turns it into `board_map.c` (the tag↔pin bindings + PID
loops) that calls only [`hal.h`](src/hal.h). Porting to a new MCU/SoC — STM32, RP2040, ESP32,
nRF, **any Rockchip M-core**, … — means writing one profile and implementing `hal.h` once.
The control core never changes. You can even have the **LLM generate the board.yaml from a
datasheet/pinout**.

```
board.yaml ──boardgen──▶ board_map.c/.h   (generated bindings + control loops)
   ▲ human or LLM                 │ calls only
   │                              ▼
osodb_embedded · scan · pid · federation ──▶ hal.h ──▶ hal_zephyr / hal_freertos / hal_host(mock)
```

## Layout

| Path | What |
|------|------|
| `src/osodb_embedded.{h,c}` | Fixed-size, allocation-free tag store (current/required, quality, per-tag change seq). Portable C99; RTOS-agnostic locking via a port header. |
| `src/pid.{h,c}` | PID with anti-windup + output clamp — the control primitive (tunable gains = seed for AI-improved PID). |
| `src/scan.{h,c}` | The hard-RT control step: read inputs → apply set-points → control loops → write outputs → publish. |
| `src/federation.{h,c}` | Sync tags with the hub. Portable line-protocol (`PUB`/`SET`) over the console; MQTT/CAN backends per target. |
| `src/hal.h` | The small hardware-abstraction interface `board_map` calls. |
| `src/osodb_port_{zephyr,freertos}.h` | Locking bindings for each RTOS. |
| `boards/*.board.yaml` | Board profiles (the thing you hand "the bear"). |
| `tools/boardgen.py` | Profile → `board_map.c/.h`. |
| `generated/` | Example generated `board_map` (checked in for reference; regenerate with `make gen`). |
| `host/` | Desktop simulator (`hal_host.c` mock + toy plant, `main_host.c`). |
| `tests/` | Host unit test for `osodb_embedded`. |
| `zephyr/`, `freertos/` | RTOS build scaffolds. |

## Build & verify on a desktop (no hardware)

```sh
make test     # unit-test the embedded tag store
make host     # build the simulator (regenerates board_map from the profile)
./build/osodb-mcu-sim
```

The simulator runs the **same** code that ships to the MCU. It seeds the board's tags, takes
a set-point from the hub (`SET temp.sp 75`), and shows the PID drive a toy thermal plant to
set-point while emitting the `PUB` traffic the node would send upstream:

```
closed loop (PID temp -> heater), scan 100 ms:
    t=  0.1s  temp= 20.00  sp=75  heater=100.00%  run=1
    t= 15.1s  temp= 75.00  sp=75  heater= 36.66%  run=1
result: temp settled at 75.04 °C (target 75) — CONVERGED
```

Regenerate for another board: `make gen BOARD=<name>` (add `boards/<name>.board.yaml`).

## RTOS targets

- **Zephyr** (primary) — Apache-2.0, native networking to federate, huge board support
  (STM32/RP2040/ESP32/nRF and Rockchip M-cores via AMP/OpenAMP). See [`zephyr/`](zephyr/).
- **FreeRTOS** (port) — for the smallest targets. See [`freertos/`](freertos/).

## Roadmap

- Board profiles for real targets (RP2040, ESP32, STM32; RK3576 Cortex-M0 via AMP).
- Federation over MQTT/CAN-FD; time-sync; store-and-forward on reconnect.
- Control beyond PID: feed-forward, signal filtering, and the **AI** hooks (adaptive PID,
  anomaly/predictive-maintenance scoring on the tag stream) — see the platform AI roadmap.
