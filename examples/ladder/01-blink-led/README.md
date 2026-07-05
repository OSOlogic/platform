# Blink LED — Ladder

The relay-logic Blink LED. Open the [Ladder editor](../../../iec61131/ladder/) and build one rung:

```
   |  led  T1(TON)                    ( led )
   |--|/|----[TON PT:=T#500ms]----------( )----|
```

- **`|/|` NC contact of `led`** — true while the LED is off.
- **`TON` timer, PT = 500 ms** — starts timing while the contact is closed.
- **`( ) coil `led`** driven by the timer's `Q` — flips the LED, which opens the NC contact and
  restarts the cycle → a 500 ms blink.

Bind `led` to your output tag in the variable table (address = the osodb tag id), then **Simulate**
to watch it toggle, **Compilar ST** to see the generated Structured Text, and deploy.

See the one-line equivalent in [Structured Text](../../st/01-blink-led.st).
