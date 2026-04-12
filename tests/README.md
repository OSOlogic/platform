# tests/ — OSOlogic® Test Suites

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/BORRELL-AUTOMATION/OSOlogic-OpenSourceOsPLC-CE) — Open Industrial Automation Platform · AGPL-3.0

---

Unit, integration, end-to-end, hardware-in-loop, and real-time benchmark test suites.

Testing an industrial automation platform requires validating correctness at every level — from individual functions up to full system behavior under real hardware and timing constraints. This directory organizes those tests by scope and target.

> Language-specific tests (IEC 61131-3 engine conformance) live in [`iec61131/tests/`](../iec61131/tests/).

## Directory Structure

```
tests/
├── unit/               # Unit tests for individual modules and functions
├── integration/        # Integration tests: multi-module interactions with osodb and osoruntime
├── e2e/                # End-to-end tests: full system scenarios via the API
├── hardware-in-loop/   # HIL tests: real hardware in the test loop
└── rt-benchmarks/      # Real-time performance benchmarks: latency, jitter, throughput
```

### `unit/`
Fine-grained tests for individual functions, classes, and modules. Run entirely in-process without requiring a running OSOlogic system. Fast feedback for development.

### `integration/`
Tests that exercise the interaction between multiple modules — for example, verifying that a gateway correctly writes a received Modbus value into `osodb` and that `osoruntime` reflects the change in the next scan cycle.

### `e2e/`
End-to-end tests that drive the full OSOlogic stack via its public API (REST, WebSocket). Simulate a complete external client: connect, read/write variables, subscribe to alarms, and verify system behavior. Run against a real or containerized OSOlogic instance.

### `hardware-in-loop/`
HIL test suites that require real OSOlogic hardware in the test loop. Validate physical I/O behavior, fieldbus communication, timing, and hardware-specific edge cases that cannot be emulated in software.

### `rt-benchmarks/`
Real-time performance measurement suite. Measures scan cycle latency, scheduling jitter (using `cyclictest`-style methods), `osodb` read/write throughput, and gateway round-trip times. Used to validate PREEMPT_RT tuning and regression-test performance across builds.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
