#!/usr/bin/env bash
# ============================================================
# OSOLogic sandbox — one-command installer (Linux & macOS)
#
#   curl -fsSL https://osologic.com/get.sh | bash
#
# Gets the platform, then `docker compose up` the sandbox: MariaDB + osodb +
# REST + OPC-UA + every web interface. Opens http://localhost:8080.
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
set -euo pipefail

REPO_URL="${OSOLOGIC_REPO:-https://github.com/OSOlogic/platform.git}"
DEST="${OSOLOGIC_DIR:-$HOME/OSOlogic}"
PORT="${OSOLOGIC_PORT:-8080}"

c()   { printf '\033[%sm%s\033[0m\n' "$1" "$2"; }
info() { c '1;36' "==> $1"; }
ok()   { c '1;32' "  ✓ $1"; }
warn() { c '1;33' "  ! $1"; }
die()  { c '1;31' "  ✗ $1"; exit 1; }

case "$(uname -s)" in
  Linux)  OS=linux;  OPEN=xdg-open ;;
  Darwin) OS=mac;    OPEN=open ;;
  *)      OS=other;  OPEN=true ;;
esac

info "OSOLogic sandbox installer ($OS)"

# --- Licence & warranty disclaimer -----------------------------------------
c '1;36' "==> Licence"
echo "  OSOLogic is free software, licensed under the GNU AGPL-3.0-or-later."
echo "  It is provided \"AS IS\", WITHOUT WARRANTY OF ANY KIND, express or implied,"
echo "  to the maximum extent permitted by applicable law. You assume all risk."
echo "  Full terms: https://github.com/OSOlogic/platform/blob/main/LICENSE"
if [ -e /dev/tty ]; then
  read -r -p "  Press Enter to accept and continue, or Ctrl-C to abort… " _ </dev/tty || die "Aborted."
fi

# --- Docker ----------------------------------------------------------------
if ! command -v docker >/dev/null 2>&1; then
  warn "Docker not found."
  if [ "$OS" = linux ]; then
    read -r -p "  Install Docker Engine now (via get.docker.com, needs sudo)? [y/N] " a </dev/tty || a=n
    if [ "${a:-n}" = y ] || [ "${a:-n}" = Y ]; then
      curl -fsSL https://get.docker.com | sh
      sudo usermod -aG docker "$USER" 2>/dev/null || true
      ok "Docker installed (you may need to log out/in for group changes)."
    else
      die "Install Docker, then re-run. See https://docs.docker.com/engine/install/"
    fi
  else
    if command -v brew >/dev/null 2>&1; then
      read -r -p "  Install Docker Desktop now (via Homebrew)? [y/N] " a </dev/tty || a=n
      if [ "${a:-n}" = y ] || [ "${a:-n}" = Y ]; then
        brew install --cask docker
        ok "Docker Desktop installed — open it once (whale in the menu bar), then re-run this."
        exit 0
      fi
    fi
    die "Install Docker Desktop for Mac (https://www.docker.com/products/docker-desktop/) and re-run."
  fi
fi
docker info >/dev/null 2>&1 || die "Docker is installed but not running. Start it (Docker Desktop / 'sudo systemctl start docker') and re-run."
if docker compose version >/dev/null 2>&1; then COMPOSE="docker compose"
elif command -v docker-compose >/dev/null 2>&1; then COMPOSE="docker-compose"
else die "Docker Compose not found. Install Docker Desktop or the compose plugin."; fi
ok "Docker ready ($COMPOSE)"

# --- Get the platform ------------------------------------------------------
if [ -d "$DEST/.git" ]; then
  info "Updating $DEST"
  git -C "$DEST" pull --ff-only || warn "Could not fast-forward; using existing checkout."
else
  command -v git >/dev/null 2>&1 || die "git not found. Install git and re-run."
  info "Cloning into $DEST"
  git clone --depth 1 "$REPO_URL" "$DEST"
fi
ok "Platform at $DEST"

# --- Launch ---------------------------------------------------------------
info "Building & starting the sandbox (first run pulls images — a minute or two)…"
( cd "$DEST/sandbox" && $COMPOSE up -d --build )

info "Waiting for the core to come up…"
for i in $(seq 1 90); do
  if curl -fsS "http://localhost:$PORT/healthz" >/dev/null 2>&1; then ok "Sandbox is live."; break; fi
  sleep 2
  [ "$i" = 90 ] && warn "Core not answering yet — check: cd $DEST/sandbox && $COMPOSE logs -f"
done

echo
c '1;32' "  OSOLogic sandbox is running 🐻"
echo   "    Web UI + REST : http://localhost:$PORT"
echo   "    OPC-UA        : opc.tcp://localhost:4840/osologic/"
echo   "    MariaDB       : localhost:3306  (osodb / osoapp:osoapp)"
echo   "    Stop          : cd $DEST/sandbox && $COMPOSE down"
echo
"$OPEN" "http://localhost:$PORT" >/dev/null 2>&1 || true
