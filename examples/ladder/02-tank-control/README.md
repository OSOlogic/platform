# Tank control — Ladder

Two rungs with a **set/reset (latch)** on the pump and comparison blocks for the level:

```
   |  level ≤ LOW                                   ( S ) pump   (set: refill)
   |--[ GE  low ≥ level ]-----------------------------( )----|

   |  level ≥ HIGH                                  ( R ) pump   (reset: full)
   |--[ LE  high ≤ level ]----------------------------( )----|
```

The gap between LOW and HIGH is the **hysteresis** band — the pump holds its state between them, so it
doesn't chatter around a single set-point. Bind `level` and `pump` to your tags and simulate.

Equivalent in [Structured Text](../../st/02-tank-control.st) and [Python](../../scripts/python/02-tank-control.py).
