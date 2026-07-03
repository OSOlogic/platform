# ci/ — OSOlogic® CI/CD Infrastructure

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · AGPL-3.0

---

Continuous integration and delivery workflows, Docker build environments, and automation scripts for building, testing, and releasing OSOlogic.

## Directory Structure

```
ci/
├── workflows/      # CI/CD workflow definitions (GitHub Actions)
├── docker/         # Docker images for build and test environments
└── scripts/        # Build, test, and release automation scripts
```

### `workflows/`
GitHub Actions workflow files (`.yml`). Covers: building OSOlogic for all supported targets (x86_64, arm64, armv7), running the unit and integration test suites, building OS images, and publishing releases and packages.

### `docker/`
Dockerfile definitions for reproducible build and test environments:
- Cross-compilation toolchain containers for ARM targets
- Test environment containers with `osodb` and `osoruntime` pre-installed
- Hardware-in-loop test runner environment

### `scripts/`
Shell scripts used by the CI workflows and developers locally:
- `build.sh` — builds the OSOlogic stack for a given target
- `test.sh` — runs the appropriate test suite for a given module
- `package.sh` — builds `.deb`, `.rpm`, and `.ipk` packages
- `image.sh` — builds bootable OS images

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
