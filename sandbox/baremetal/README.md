# Bare-metal install (Linux, no Docker)

**© 2026 Roig Borrell S.L. · Ibercomp S.L.** · AGPL-3.0-or-later

```bash
sudo sandbox/baremetal/install.sh
```

Installs MariaDB + Python, loads [`../db/init.sql`](../db/init.sql), creates a venv with the core's
deps, and runs [`../core/oso_core.py`](../core/oso_core.py) — serving the UI + REST on `:8080` and
OPC-UA on `:4840`, exactly like the container.

## Run as a service (systemd)

```ini
# /etc/systemd/system/osologic-sandbox.service
[Unit]
Description=OSOLogic sandbox core
After=mariadb.service
Wants=mariadb.service

[Service]
Environment=OSO_DB_HOST=127.0.0.1 OSO_DB_USER=osoapp OSO_DB_PASS=osoapp OSO_DB_NAME=osodb
Environment=OSO_UI_DIR=/opt/osologic-platform
ExecStart=/opt/osologic-sandbox/venv/bin/python /opt/osologic-platform/sandbox/core/oso_core.py
Restart=on-failure
User=root

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable --now osologic-sandbox
```

> Adjust the paths to where you cloned the platform. On Windows, run this under **WSL2** (Ubuntu) —
> the one-click Windows package is on the roadmap.
