# Install the OSOLogic sandbox — 1 command / 1 double-click

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · AGPL-3.0-or-later

Brings up a full OSOLogic on **x86_64** (MariaDB + osodb + REST + OPC-UA + every web UI) with Docker,
then opens **http://localhost:8080**. Needs Docker (the installer checks/guides you).

## One line

| OS | Command |
|----|---------|
| 🐧 **Linux** | `curl -fsSL https://osologic.com/get.sh \| bash` |
| 🍎 **macOS** | `curl -fsSL https://osologic.com/get.sh \| bash` |
| 🪟 **Windows** | `irm https://osologic.com/get.ps1 \| iex`  *(PowerShell, Docker Desktop + WSL2)* |

## One double-click

Download and double-click:

- **Linux** — [`osologic-sandbox.desktop`](osologic-sandbox.desktop)
- **macOS** — [`osologic-sandbox.command`](osologic-sandbox.command)  *(first time: right-click → Open)*
- **Windows** — [`osologic-sandbox.bat`](osologic-sandbox.bat)

## From a checkout (offline / dev)

```bash
git clone https://github.com/OSOlogic/platform && cd platform/sandbox
docker compose up --build       # → http://localhost:8080
# or:  bash install/get.sh
```

Stop it with `docker compose down` (add `-v` to wipe the DB volume).

## Requirements

- **Linux** — Docker Engine (the script can install it via `get.docker.com`).
- **macOS / Windows** — Docker Desktop (Windows uses the WSL2 backend). Docker Desktop runs it in a
  lightweight VM — a headless VM works too.

Config via env: `OSOLOGIC_DIR` (checkout path), `OSOLOGIC_PORT` (default 8080), `OSOLOGIC_REPO`.
