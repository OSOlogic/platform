# Installing OSOLogic

**(C) Roig Borrell S.L. · Ibercomp S.L.** · Part of [OSOLogic](https://github.com/OSOlogic/platform) · AGPL-3.0

There are two ways to install OSOLogic on a board. Both configure the same
stack — database, MQTT broker, runtime, gateways and the web Manager.

> **Where do these scripts live?** A **flashed pre-built image** already has
> `oso-setup` on your `$PATH`, so `sudo oso-setup` just works. From a **fresh
> `git clone`** they live in this `packaging/` directory — run them as
> `sudo packaging/oso-setup` (they locate their own helpers, so any working
> directory is fine) or `cd packaging` first.

## A. Guided install — `oso-setup`  *(recommended)*

A fast, intuitive setup wizard. It renders a full-screen **ncurses UI**
(`dialog`, or `whiptail` as a fallback) and drops to plain-text prompts on a
serial console or dumb terminal — so it works everywhere.

```bash
sudo oso-setup
```

- **Express mode** — smart defaults, the fewest questions, done in minutes.
- **Advanced mode** — review source, ports and every option.
- Autodetects network and storage; can **generate strong passwords** for you
  (saved to `/root/osologic-credentials.txt`, root-only).
- Walk the whole wizard without changing anything:

  ```bash
  sudo oso-setup --dry-run
  ```

Under the hood `oso-setup` collects your choices and runs the advanced
installer unattended — one engine, one source of truth.

## B. Advanced install — `install_OsoLogic.sh`  *(full control)*

The complete installer script: every component, every prompt, fully
scriptable. Built for custom boards, reproducible deployments and building
from source.

```bash
sudo ./install_OsoLogic.sh
```

**Unattended / CI:** pre-seed a config file and skip all prompts:

```bash
sudo ./install_OsoLogic.sh --config /path/to/oso.conf
```

The config is a shell fragment that sets the same variables the wizard
collects, for example:

```sh
SETUP_MODE=2                 # 1 = code already on board, 2 = clone
REPO_URL='https://github.com/OSOlogic/platform.git'
GIT_BRANCH='main'
DB_ROOT_PASS='...'           # MariaDB root
DB_OSO_PASS='...'            # application DB user 'oso'
MQTT_PASS='...'              # MQTT user 'oso'
MQTT_PORT=1883
MGR_PORT=8080
GUI_PORT=8082
NR_PORT=1880
EXT_URL='192.168.1.50'       # IP/host for web access
INSTALL_PMA=n                # phpMyAdmin (osomyadmin coming soon)
```

## Files

| File | Role |
|------|------|
| `oso-setup` | Guided wizard (Path A) |
| `lib/oso-ui.sh` | Terminal UI layer — `dialog` / `whiptail` / plain |
| `install_OsoLogic.sh` | Advanced installer (Path B); also the unattended engine |

## Supported targets

Orange Pi is the reference target today. Raspberry Pi, generic ARM and x86_64
are on the roadmap — see [`../docs/deployment/`](../docs/deployment/).
