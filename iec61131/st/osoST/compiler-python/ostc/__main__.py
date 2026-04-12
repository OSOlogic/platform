"""
osoLogic — osoST Python Compiler
ostc/__main__.py — Entry point for `python -m ostc`

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later
"""

import sys
from .cli import main

sys.exit(main())
