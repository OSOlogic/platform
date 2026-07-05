/* ============================================================
   OSOLadder — osocompile.js  (PROTOTYPE / tentative)
   Ladder Diagram (osoLadder state model) → Structured Text (IEC 61131-3)

   ST is the common intermediate form: it feeds the osoST toolchain
   (iec61131/st/osoST → p-code → osoruntime), so Ladder gets a real compile
   path without a bespoke backend. This is an early prototype — series/parallel
   contact networks, the four coil types and the standard function blocks are
   handled; complex nested branch re-joins are approximated (flagged in output).

   Works in the browser (window.OsoCompile) and in Node (module.exports),
   so it can be unit-tested headless.

   Copyright (C) 2026 Jose Roig Borrell, Roig Borrell SL, Ibercomp SL
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
'use strict';

(function (root) {

  const CONTACTS = new Set(['NO_CONTACT', 'NC_CONTACT', 'POS_TRANSITION', 'NEG_TRANSITION']);
  const COILS    = new Set(['COIL_NORMAL', 'COIL_NEGATED', 'COIL_SET', 'COIL_RESET']);
  const TIMERS   = new Set(['FB_TON', 'FB_TOF', 'FB_TP']);
  const COUNTERS = new Set(['FB_CTU', 'FB_CTD', 'FB_CTUD']);
  const MATH_OP  = { FB_ADD: '+', FB_SUB: '-', FB_MUL: '*', FB_DIV: '/' };
  const CMP_OP   = { FB_GT: '>', FB_LT: '<', FB_GE: '>=', FB_LE: '<=', FB_EQ: '=', FB_NE: '<>' };

  // A ladder variable reference; fall back to a safe placeholder when unbound.
  function ref(name) { return (name && String(name).trim()) || '_UNBOUND_'; }

  // Boolean expression for a single contact cell.
  function contactExpr(cell, edgeDecls) {
    const v = ref(cell.variableName);
    switch (cell.type) {
      case 'NO_CONTACT': return v;
      case 'NC_CONTACT': return 'NOT ' + v;
      case 'POS_TRANSITION': {
        const inst = 'R_' + sanitize(v);
        edgeDecls.set(inst, 'R_TRIG');
        return `(${inst}(CLK := ${v}), ${inst}.Q)`;   // rising edge
      }
      case 'NEG_TRANSITION': {
        const inst = 'F_' + sanitize(v);
        edgeDecls.set(inst, 'F_TRIG');
        return `(${inst}(CLK := ${v}), ${inst}.Q)`;   // falling edge
      }
      default: return 'FALSE';
    }
  }

  function sanitize(s) { return String(s).replace(/[^A-Za-z0-9_]/g, '_'); }

  // Series AND of a list of boolean expressions.
  function andJoin(list) {
    const xs = list.filter(Boolean);
    if (xs.length === 0) return 'TRUE';
    if (xs.length === 1) return xs[0];
    return xs.map(x => needsParen(x) ? `(${x})` : x).join(' AND ');
  }
  function orJoin(list) {
    const xs = list.filter(Boolean);
    if (xs.length === 0) return 'FALSE';
    if (xs.length === 1) return xs[0];
    return xs.map(x => needsParen(x) ? `(${x})` : x).join(' OR ');
  }
  function needsParen(x) { return /\bAND\b|\bOR\b/.test(x) && !/^\(.*\)$/.test(x); }

  // Compile one rung to ST statements. Series within a row = AND, rows = OR.
  // (Prototype: treats each row as an independent parallel branch feeding the
  // outputs; nested mid-rung re-joins are approximated.)
  function compileRung(rung, edgeDecls, warnings, idx, callDecls) {
    const outLines = [];
    const contactRows = [];
    const outputs = [];   // {cell, row, col}

    for (let r = 0; r < rung.cells.length; r++) {
      const row = rung.cells[r] || [];
      const rowContacts = [];
      for (let c = 0; c < row.length; c++) {
        const cell = row[c];
        if (!cell || !cell.type) continue;
        if (CONTACTS.has(cell.type)) rowContacts.push(contactExpr(cell, edgeDecls));
        else if (COILS.has(cell.type)) outputs.push({ cell, r, c });
        else outputs.push({ cell, r, c });   // function block as an output-side element
      }
      if (rowContacts.length) contactRows.push(andJoin(rowContacts));
    }

    // Parallel branches OR-ed together. No contacts = wired straight to the rail = TRUE
    // (matches the simulator, so what you simulate is what you deploy).
    const cond = contactRows.length ? orJoin(contactRows) : 'TRUE';

    if (outputs.length === 0) {
      warnings.push(`Rung ${idx + 1}${rung.label ? ` ("${rung.label}")` : ''}: no output — skipped.`);
      return outLines;
    }

    for (const o of outputs) {
      const cell = o.cell;
      const v = ref(cell.variableName);
      switch (cell.type) {
        case 'COIL_NORMAL':  outLines.push(`${v} := ${cond};`); break;
        case 'COIL_NEGATED': outLines.push(`${v} := NOT (${cond});`); break;
        case 'COIL_SET':     outLines.push(`IF ${cond} THEN ${v} := TRUE; END_IF;`); break;
        case 'COIL_RESET':   outLines.push(`IF ${cond} THEN ${v} := FALSE; END_IF;`); break;
        default:
          outLines.push(compileFB(cell, cond, edgeDecls, warnings, idx, callDecls));
      }
    }
    return outLines;
  }

  // Function-block call → ST. Timers/counters as instance calls; math/compare inline.
  function compileFB(cell, cond, edgeDecls, warnings, idx, callDecls) {
    const p = cell.params || {};
    const inst = p.instanceName || (cell.type.replace('FB_', '') + '_1');
    // CALL another POU (sub-ladder). In the osoST dialect every POU is a PROCEDURE,
    // so we call it by name — no instance needed.
    if (cell.type === 'FB_CALL') {
      const target = sanitize(p.target || '');
      if (!target) { warnings.push(`Rung ${idx + 1}: CALL with no target — skipped.`); return `(* CALL: no target *)`; }
      return `IF ${cond} THEN ${target}(); END_IF;`;
    }
    // PID controller: emitted as an FB instance call (needs a PID block in the ST library).
    if (cell.type === 'FB_PID') {
      edgeDecls.set(inst, 'PID');
      warnings.push(`Rung ${idx + 1}: PID emitted as FB instance '${inst}' — provide a PID block in the ST library.`);
      const out = p.OUT ? `\n  ${ref(p.OUT)} := ${inst}.OUT;` : '';
      return `${inst}(EN := ${cond}, PV := ${ref(p.PV)}, SP := ${ref(p.SP)}, ` +
             `KP := ${p.Kp || 0}, KI := ${p.Ki || 0}, KD := ${p.Kd || 0});${out}`;
    }
    if (TIMERS.has(cell.type)) {
      const fb = cell.type.replace('FB_', '');           // TON/TOF/TP
      edgeDecls.set(inst, fb);
      const IN = ref(p.IN) === '_UNBOUND_' ? cond : ref(p.IN);
      const out = p.Q ? `  ${ref(p.Q)} := ${inst}.Q;` : '';
      const et  = p.ET ? `  ${ref(p.ET)} := ${inst}.ET;` : '';
      return `${inst}(IN := ${IN}, PT := ${p.PT || 'T#0s'});${out ? '\n' + out : ''}${et ? '\n' + et : ''}`;
    }
    if (COUNTERS.has(cell.type)) {
      const fb = cell.type.replace('FB_', '');           // CTU/CTD/CTUD
      edgeDecls.set(inst, fb);
      const args = [];
      if (fb === 'CTU')  args.push(`CU := ${cond}`, `RESET := ${ref(p.R)}`, `PV := ${p.PV ?? 0}`);
      if (fb === 'CTD')  args.push(`CD := ${cond}`, `LOAD := ${ref(p.LD)}`, `PV := ${p.PV ?? 0}`);
      if (fb === 'CTUD') args.push(`CU := ${cond}`, `CD := ${ref(p.CD)}`, `RESET := ${ref(p.R)}`, `LOAD := ${ref(p.LD)}`, `PV := ${p.PV ?? 0}`);
      const out = p.Q ? `\n  ${ref(p.Q)} := ${inst}.Q;` : '';
      const cv  = p.CV ? `\n  ${ref(p.CV)} := ${inst}.CV;` : '';
      return `${inst}(${args.join(', ')});${out}${cv}`;
    }
    if (cell.type in MATH_OP) {
      const en = `IF ${cond} THEN `;
      return `${en}${ref(p.OUT)} := ${ref(p.IN1)} ${MATH_OP[cell.type]} ${ref(p.IN2)}; END_IF;`;
    }
    if (cell.type === 'FB_MOV') {
      return `IF ${cond} THEN ${ref(p.OUT)} := ${ref(p.IN1)}; END_IF;`;
    }
    if (cell.type in CMP_OP) {
      return `${ref(p.OUT)} := (${ref(p.IN1)} ${CMP_OP[cell.type]} ${ref(p.IN2)});`;
    }
    warnings.push(`Rung ${idx + 1}: unsupported element '${cell.type}' — emitted as comment.`);
    return `(* unsupported: ${cell.type} *)`;
  }

  // Map an osoLadder dataType to an ST type.
  function stType(dt) {
    const t = (dt || 'BOOL').toUpperCase();
    const ok = new Set(['BOOL', 'BYTE', 'WORD', 'DWORD', 'INT', 'DINT', 'UINT', 'UDINT', 'REAL', 'LREAL', 'TIME', 'STRING']);
    return ok.has(t) ? t : 'BOOL';
  }

  // Compile one POU (a named ladder diagram) to a PROCEDURE in the osoST dialect.
  // Shared tags live in a project-level VAR_GLOBAL and are directly visible — no VAR_EXTERNAL.
  function compilePou(name, rungs, warnings) {
    const edgeDecls = new Map();   // local FB instances (timers/counters/edges/PID)
    const callDecls = new Map();   // (unused in the PROCEDURE dialect — procedures called by name)
    const body = [];
    (rungs || []).filter(r => r.enabled !== false).forEach((rung, i) => {
      if (rung.label) body.push(`  (* Rung ${i + 1}: ${rung.label} *)`);
      for (const line of compileRung(rung, edgeDecls, warnings, i, callDecls)) {
        body.push('  ' + line.replace(/\n/g, '\n  '));
      }
    });
    if (!body.length) body.push('  (* empty *)');

    const locals = [];
    for (const [inst, fb] of edgeDecls) locals.push(`  ${inst} : ${fb};`);

    return `PROCEDURE ${sanitize(name)}()\n` +
      (locals.length ? `VAR\n${locals.join('\n')}\nEND_VAR\n` : '') +
      body.join('\n') + `\nEND_PROCEDURE\n`;
  }

  /**
   * Compile an osoLadder `state` object to Structured Text (string).
   * Emits the osoST dialect (PROCEDURE / VAR_GLOBAL) so it compiles with the osoST
   * compiler (Java STLite or the Python ostc) → P-code → osoruntime. Multi-POU: shared
   * tags in one VAR_GLOBAL, each sub-ladder a PROCEDURE, the entry point PROCEDURE main().
   * @returns {{ st:string, warnings:string[] }}
   */
  function compileLadderToST(state) {
    const warnings = [];
    const pous = (state.subroutines && Object.keys(state.subroutines).length)
      ? state.subroutines
      : { main: state.rungs || [] };
    const funcs = Object.keys(pous).filter(n => n !== 'main');

    // Shared tags: one VAR_GLOBAL for the whole project.
    const globals = (state.variables || []).map(v =>
      `  ${sanitize(v.name)} : ${stType(v.dataType)}` +
      (v.initialValue != null && v.initialValue !== '' ? ` := ${v.initialValue}` : '') + ';' +
      (v.address ? `   (* @ ${v.address} *)` : '') +
      (v.comment ? `   (* ${v.comment} *)` : ''));

    const blocks = [];
    // Sub-ladders first, then the entry point.
    for (const fn of funcs) blocks.push(compilePou(fn, pous[fn], warnings));
    blocks.push(compilePou('main', pous['main'] || [], warnings));

    const st =
      `(* Generated by osoLadder → osocompile.js (prototype) *)\n` +
      `(* Source: ${(state.meta && state.meta.name) || 'untitled'} — osoST dialect (IEC 61131-3 subset) *)\n` +
      `(* POUs: main${funcs.length ? ' + ' + funcs.join(', ') : ''} *)\n\n` +
      `VAR_GLOBAL\n${globals.length ? globals.join('\n') : '  (* no tags *)'}\nEND_VAR\n\n` +
      blocks.join('\n');

    return { st, warnings };
  }

  const api = { compileLadderToST };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.OsoCompile = api;

})(typeof window !== 'undefined' ? window : globalThis);
