#!/usr/bin/env node
/* ============================================================
   OSOLadder — osocompile-cli.js  (PROTOTYPE)
   Compile an osoLadder project (.osol / JSON) to Structured Text.

     node osocompile-cli.js program.osol            # writes program.st
     node osocompile-cli.js program.osol -o out.st  # explicit output
     node osocompile-cli.js program.osol -           # ST to stdout

   The emitted ST feeds the osoST toolchain (../../st/osoST) → p-code → runtime.

   Copyright (C) 2026 Jose Roig Borrell, Roig Borrell SL, Ibercomp SL
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
'use strict';

const fs = require('fs');
const path = require('path');
const { compileLadderToST } = require('./osocompile.js');

function main(argv) {
  const args = argv.slice(2);
  if (!args.length || args[0] === '-h' || args[0] === '--help') {
    console.error('usage: osocompile-cli.js <project.osol> [-o output.st | -]');
    process.exit(args.length ? 0 : 2);
  }
  const input = args[0];
  let output = args.includes('-o') ? args[args.indexOf('-o') + 1] : null;
  const toStdout = args.includes('-') || output === '-';

  let state;
  try {
    state = JSON.parse(fs.readFileSync(input, 'utf8'));
  } catch (e) {
    console.error(`error: cannot read/parse ${input}: ${e.message}`);
    process.exit(1);
  }

  const { st, warnings } = compileLadderToST(state);
  for (const w of warnings) console.error('warning: ' + w);

  if (toStdout) {
    process.stdout.write(st);
  } else {
    if (!output) output = input.replace(/\.osol$/i, '') + '.st';
    fs.writeFileSync(output, st);
    console.error(`wrote ${output} (${st.split('\n').length} lines${warnings.length ? `, ${warnings.length} warning(s)` : ''})`);
  }
}

main(process.argv);
