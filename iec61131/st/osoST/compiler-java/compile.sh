#!/usr/bin/env bash
# osoLogic — STLite CLI wrapper
# Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
JAR="$DIR/STLite.jar"

if [ ! -f "$JAR" ]; then
  echo "ERROR: STLite.jar not found at $JAR" >&2
  echo "Copy STLite.jar to $(dirname "$JAR")/" >&2
  exit 1
fi

if [ $# -lt 1 ]; then
  echo "Usage: compile.sh <file.st|file.prj> [--run]"
  exit 2
fi

SOURCE="$1"
shift

if [[ "$*" == *"--run"* ]]; then
  exec java -jar "$JAR" -run "$SOURCE"
else
  exec java -jar "$JAR" -compile "$SOURCE"
fi
