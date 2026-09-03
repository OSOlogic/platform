# MCU & board profiles — configure any board fast

OSOlogic's hard-real-time tier (`firmware/osodb-mcu/`) runs the control loop cycle-exact on a
microcontroller — or on a Linux SBC — and federates its tags with the network hub. You don't
hand-wire pins in C: you **describe the board once** in a small YAML profile and a generator
turns it into the bindings. Porting to new hardware is minutes, not days — and the OSOlogic
assistant can fill the profile in from a datasheet.

## How it fits together

```
boards/<name>.board.yaml ──boardgen.py──▶ generated/board_map.c/.h   (tag↔pin + PID loops)
        ▲ human or LLM                              │ calls only
        │                                           ▼
osodb-embedded · scan · pid · federation ─────────▶ hal.h ──▶ hal_zephyr | hal_freertos | hal_linux | hal_host
```

- **`board.yaml`** — pins, peripherals, the osodb tag each maps to, and the control loops.
- **`boardgen.py`** — generates `board_map.c` (no third-party deps).
- **`hal.h`** — the tiny interface the generated map calls; one implementation per platform.

Adding a target = write a profile + (re)use or write one `hal_*.c`. The control core never
changes, so the same code runs on an MCU (Zephyr/FreeRTOS) and a Linux board (libgpiod).

## Profile format

```yaml
board: myboard                 # id (also the C BOARD_NAME)
soc:   MySoC
rtos:  zephyr                  # zephyr | freertos | linux | host

federation:                    # link to the hub
  transport: uart              # uart | mqtt | can
  baud: 115200                 # or host/port/topic (mqtt), iface/id (can)

io:                            # physical I/O -> osodb tags
  - { tag: machine.run, dir: out, kind: gpio, pin: 25, type: bool }
  - { tag: estop,       dir: in,  kind: gpio, pin: 14, type: bool, active_low: true }
  - { tag: temp,        dir: in,  kind: adc,  channel: 0, type: real, raw_max: 4095, out_min: -40, out_max: 125 }
  - { tag: heater,      dir: out, kind: pwm,  pin: 16, type: real, out_min: 0, out_max: 100 }

control:                       # optional hard-RT loops (pv/cv are io tags; sp is written by the hub)
  - { loop: temp_ctrl, pv: temp, sp: temp.sp, cv: heater, kp: 2.0, ki: 0.8, kd: 0.05, out_min: 0, out_max: 100 }
```

- **gpio** `pin`, optional `active_low`; **adc** `channel` + linear scale (`raw_max`,`out_min`,`out_max`);
  **pwm** `pin` + range (`out_min`,`out_max`) mapped to duty.
- **control** loops run every scan (PID with anti-windup); gains are tunable at runtime — the
  hook for the AI-improved-PID roadmap.

## Included examples (templates)

Like the Arduino IDE's examples, OSOlogic ships ready board profiles you copy and tweak:

| Profile | Target | Notes |
|---------|--------|-------|
| [`TEMPLATE.board.yaml`](../firmware/osodb-mcu/boards/TEMPLATE.board.yaml) | — | Commented blank; start here. |
| `rp2040_demo` | Raspberry Pi Pico (RP2040) | Zephyr; closed-loop PID demo. |
| `esp32_demo` | ESP32 DevKitC | Zephyr; WiFi → MQTT federation. |
| `arduino_giga` | Arduino GIGA R1 (STM32H7) | Zephyr; modern Arduino, analog + PWM. |
| `raspberry_pi` | Raspberry Pi (Linux) | `hal_linux` (libgpiod); digital demo. |
| `orange_pi` | Orange Pi (Linux) | `hal_linux`; set your gpiochip. |

## Build

```sh
cd firmware/osodb-mcu
make gen BOARD=esp32_demo        # profile -> generated/board_map.{c,h}
make host && ./build/osodb-mcu-sim   # try it on the desktop, no hardware
# MCU: west build -b <zephyr_board> zephyr/   (see firmware/osodb-mcu/README.md)
```

## Let the assistant write it

Hand a datasheet or a pinout to the OSOlogic assistant and ask for a board profile — the LLM
emits the `board.yaml`, you review it, run `boardgen`, and build. See the platform AI roadmap
(code generation for ST/Ladder and any language, plus board bring-up).
