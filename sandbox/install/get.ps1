# ============================================================
# OSOLogic sandbox — one-command installer (Windows, PowerShell)
#
#   irm https://osologic.com/get.ps1 | iex
#
# Uses Docker Desktop (WSL2 backend). Brings up MariaDB + osodb + REST +
# OPC-UA + every web interface, then opens http://localhost:8080.
# (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
# ============================================================
$ErrorActionPreference = 'Stop'

$RepoUrl = if ($env:OSOLOGIC_REPO) { $env:OSOLOGIC_REPO } else { 'https://github.com/OSOlogic/platform.git' }
$Dest    = if ($env:OSOLOGIC_DIR)  { $env:OSOLOGIC_DIR }  else { Join-Path $HOME 'OSOlogic' }
$Port    = if ($env:OSOLOGIC_PORT) { $env:OSOLOGIC_PORT } else { '8080' }

function Info($m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Ok($m)   { Write-Host "  OK $m"  -ForegroundColor Green }
function Warn($m) { Write-Host "  ! $m"   -ForegroundColor Yellow }
function Die($m)  { Write-Host "  x $m"   -ForegroundColor Red; exit 1 }

Info 'OSOLogic sandbox installer (Windows)'

# --- Licence & warranty disclaimer ------------------------------------------
Info 'Licence'
Write-Host '  OSOLogic is free software, licensed under the GNU AGPL-3.0-or-later.'
Write-Host '  It is provided "AS IS", WITHOUT WARRANTY OF ANY KIND, express or implied,'
Write-Host '  to the maximum extent permitted by applicable law. You assume all risk.'
Write-Host '  Full terms: https://github.com/OSOlogic/platform/blob/main/LICENSE'
Read-Host '  Press Enter to accept and continue (Ctrl-C to abort)' | Out-Null

# --- Docker Desktop ---------------------------------------------------------
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
  if (Get-Command winget -ErrorAction SilentlyContinue) {
    $a = Read-Host '  Docker Desktop not found. Install it now via winget? [y/N]'
    if ($a -eq 'y' -or $a -eq 'Y') {
      winget install -e --id Docker.DockerDesktop --accept-source-agreements --accept-package-agreements
      Warn 'Docker Desktop installed — start it once (enable the WSL2 backend), then re-run this.'
      exit 0
    }
  }
  Die 'Install Docker Desktop (WSL2 backend) from https://www.docker.com/products/docker-desktop/ and re-run.'
}
try { docker info *> $null } catch { Die 'Docker Desktop is installed but not running. Start Docker Desktop and re-run.' }
$Compose = 'docker compose'
try { docker compose version *> $null } catch {
  if (Get-Command docker-compose -ErrorAction SilentlyContinue) { $Compose = 'docker-compose' }
  else { Die 'Docker Compose not available. Update Docker Desktop.' }
}
Ok "Docker ready ($Compose)"

# --- Get the platform -------------------------------------------------------
if (Test-Path (Join-Path $Dest '.git')) {
  Info "Updating $Dest"
  git -C $Dest pull --ff-only 2>$null
} else {
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Die 'git not found. Install Git for Windows and re-run.' }
  Info "Cloning into $Dest"
  git clone --depth 1 $RepoUrl $Dest
}
Ok "Platform at $Dest"

# --- Launch -----------------------------------------------------------------
Info 'Building & starting the sandbox (first run pulls images)…'
Push-Location (Join-Path $Dest 'sandbox')
Invoke-Expression "$Compose up -d --build"
Pop-Location

Info 'Waiting for the core to come up…'
$live = $false
for ($i = 0; $i -lt 90; $i++) {
  try { Invoke-RestMethod "http://localhost:$Port/healthz" -TimeoutSec 2 | Out-Null; $live = $true; break } catch { Start-Sleep 2 }
}
if ($live) { Ok 'Sandbox is live.' } else { Warn "Core not answering yet — check: cd $Dest\sandbox; $Compose logs -f" }

Write-Host ''
Write-Host '  OSOLogic sandbox is running' -ForegroundColor Green
Write-Host "    Web UI + REST : http://localhost:$Port"
Write-Host '    OPC-UA        : opc.tcp://localhost:4840/osologic/'
Write-Host '    MariaDB       : localhost:3306  (osodb / osoapp:osoapp)'
Write-Host "    Stop          : cd $Dest\sandbox; $Compose down"
Write-Host ''
Start-Process "http://localhost:$Port"
