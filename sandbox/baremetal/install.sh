#!/usr/bin/env bash
# ============================================================
# OSOLogic sandbox — bare-metal installer (Linux, no Docker)
# Debian/Ubuntu. Installs MariaDB + Python deps, loads the schema,
# and runs the reference core (REST + OPC-UA + UI) in the foreground.
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SANDBOX="$(dirname "$HERE")"
REPO="$(dirname "$SANDBOX")"
VENV=/opt/osologic-sandbox/venv

if [ "$(id -u)" -ne 0 ]; then echo "Run with sudo: sudo $0"; exit 1; fi
if ! command -v apt-get >/dev/null; then echo "This installer targets Debian/Ubuntu (apt)."; exit 1; fi

echo "==> Installing MariaDB + Python"
apt-get update -qq
apt-get install -y -qq mariadb-server python3 python3-venv python3-pip

echo "==> Starting MariaDB"
systemctl enable --now mariadb 2>/dev/null || service mysql start || true

echo "==> Loading schema + seed (db/init.sql)"
mysql < "$SANDBOX/db/init.sql"

echo "==> Python venv + deps"
python3 -m venv "$VENV"
"$VENV/bin/pip" install --quiet --upgrade pip
"$VENV/bin/pip" install --quiet -r "$SANDBOX/core/requirements.txt"

echo "==> Starting the OSOLogic core"
echo "    UI + REST  http://localhost:8080"
echo "    OPC-UA     opc.tcp://localhost:4840/osologic/"
echo "    (Ctrl-C to stop; see baremetal/README.md for a systemd service)"
OSO_DB_HOST=127.0.0.1 OSO_DB_USER=osoapp OSO_DB_PASS=osoapp OSO_DB_NAME=osodb \
OSO_UI_DIR="$REPO" \
exec "$VENV/bin/python" "$SANDBOX/core/oso_core.py"
