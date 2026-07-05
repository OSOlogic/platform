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

  // Timers/counters have no native FB in the osoST dialect, so they are lowered to
  // primitive ST (millis() trap + per-instance helper globals). Accumulated here and
  // emitted at the top level by compileLadderToST.
  let _helperGlobals = [];   // extra VAR_GLOBAL declarations for FB state
  let _needMillis    = false;
  function _ptMs(s) {
    if (s == null) return 0;
    const m = String(s).match(/(\d+(?:\.\d+)?)\s*(ms|s|m|h)?/i);
    if (!m) return 0;
    return Math.round(parseFloat(m[1]) * ({ ms: 1, s: 1000, m: 60000, h: 3600000 }[(m[2] || 'ms').toLowerCase()] || 1));
  }

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
    // PID controller — lowered to primitive ST (P+I+D on globals). Per-scan integral.
    if (cell.type === 'FB_PID') {
      const iid = sanitize(inst);
      const integ = `__${iid}_i`, prev = `__${iid}_prev`;
      _helperGlobals.push(`  ${integ} : REAL := 0.0;`, `  ${prev} : REAL := 0.0;`);
      const PV = ref(p.PV), SP = ref(p.SP);
      const flt = (x, d) => { const s = String(x ?? d); return /[.eE]/.test(s) ? s : s + '.0'; };
      const kp = flt(p.Kp, '0'), ki = flt(p.Ki, '0'), kd = flt(p.Kd, '0');
      let OUT = ref(p.OUT);
      if (OUT === '_UNBOUND_') { OUT = `__${iid}_out`; _helperGlobals.push(`  ${OUT} : REAL := 0.0;`); }
      const err = `(${SP} - ${PV})`;
      return `IF ${cond} THEN\n  ${integ} := ${integ} + ${err};\n  ${OUT} := ${kp} * ${err} + ${ki} * ${integ} + ${kd} * (${err} - ${prev});\n  ${prev} := ${err};\nELSE\n  ${integ} := 0.0; ${prev} := 0.0;\nEND_IF;`;
    }
    if (TIMERS.has(cell.type)) {
      // Lower TON/TOF/TP to primitive ST using millis() and helper globals.
      _needMillis = true;
      const iid = sanitize(inst);
      const t0 = `__${iid}_t0`, run = `__${iid}_run`;
      _helperGlobals.push(`  ${t0} : DINT := 0;`, `  ${run} : BOOL := FALSE;`);
      const IN = ref(p.IN) === '_UNBOUND_' ? cond : ref(p.IN);
      const pt = _ptMs(p.PT);
      let Q = ref(p.Q);
      if (Q === '_UNBOUND_') { Q = `__${iid}_q`; _helperGlobals.push(`  ${Q} : BOOL := FALSE;`); }
      if (cell.type === 'FB_TON') {
        return `IF ${IN} THEN\n  IF NOT ${run} THEN ${t0} := millis(); ${run} := TRUE; END_IF;\n  IF (millis() - ${t0}) >= ${pt} THEN ${Q} := TRUE; END_IF;\nELSE\n  ${run} := FALSE; ${Q} := FALSE;\nEND_IF;`;
      }
      if (cell.type === 'FB_TOF') {
        return `IF ${IN} THEN\n  ${Q} := TRUE; ${run} := FALSE;\nELSE\n  IF NOT ${run} THEN ${t0} := millis(); ${run} := TRUE; END_IF;\n  IF (millis() - ${t0}) >= ${pt} THEN ${Q} := FALSE; END_IF;\nEND_IF;`;
      }
      // FB_TP — one-shot pulse of PT on the rising edge of IN.
      const ipv = `__${iid}_ip`;
      _helperGlobals.push(`  ${ipv} : BOOL := FALSE;`);
      return `IF ${IN} AND NOT ${ipv} AND NOT ${run} THEN ${t0} := millis(); ${run} := TRUE; END_IF;\n${ipv} := ${IN};\nIF ${run} THEN\n  IF (millis() - ${t0}) < ${pt} THEN ${Q} := TRUE; ELSE ${Q} := FALSE; ${run} := FALSE; END_IF;\nELSE\n  ${Q} := FALSE;\nEND_IF;`;
    }
    if (COUNTERS.has(cell.type)) {
      // Lower CTU/CTD/CTUD to primitive ST with edge detection + a counter global.
      const iid = sanitize(inst);
      const cv = `__${iid}_cv`;
      _helperGlobals.push(`  ${cv} : DINT := 0;`);
      const PV = p.PV ?? 0;
      const CV = ref(p.CV) !== '_UNBOUND_' ? ref(p.CV) : null;
      const Q  = ref(p.Q)  !== '_UNBOUND_' ? ref(p.Q)  : null;
      const lines = [];
      if (cell.type === 'FB_CTU' || cell.type === 'FB_CTUD') {
        const CU = ref(p.CU) === '_UNBOUND_' ? cond : ref(p.CU);
        const R  = ref(p.R)  === '_UNBOUND_' ? 'FALSE' : ref(p.R);
        const pu = `__${iid}_pu`; _helperGlobals.push(`  ${pu} : BOOL := FALSE;`);
        lines.push(`IF ${R} THEN ${cv} := 0;\nELSIF ${CU} AND NOT ${pu} THEN ${cv} := ${cv} + 1; END_IF;\n${pu} := ${CU};`);
      }
      if (cell.type === 'FB_CTD' || cell.type === 'FB_CTUD') {
        const CD = ref(p.CD) === '_UNBOUND_' ? cond : ref(p.CD);
        const LD = ref(p.LD) === '_UNBOUND_' ? 'FALSE' : ref(p.LD);
        const pd = `__${iid}_pd`; _helperGlobals.push(`  ${pd} : BOOL := FALSE;`);
        lines.push(`IF ${LD} THEN ${cv} := ${PV};\nELSIF ${CD} AND NOT ${pd} THEN ${cv} := ${cv} - 1; END_IF;\n${pd} := ${CD};`);
      }
      if (CV) lines.push(`${CV} := ${cv};`);
      if (Q)  lines.push(`${Q} := (${cv} >= ${PV});`);
      return lines.join('\n');
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
  function compilePou(name, rungs, warnings, io) {
    const edgeDecls = new Map();   // local FB instances (timers/counters/edges/PID)
    const callDecls = new Map();   // (unused in the PROCEDURE dialect — procedures called by name)
    const body = [];
    (rungs || []).filter(r => r.enabled !== false).forEach((rung, i) => {
      if (rung.label) body.push(`  (* Rung ${i + 1}: ${rung.label} *)`);
      for (const line of compileRung(rung, edgeDecls, warnings, i, callDecls)) {
        body.push('  ' + line.replace(/\n/g, '\n  '));
      }
    });

    // I/O image sync (main only): read bound osodb tags before, write them after.
    const pre  = (io && io.pre.length)  ? ['  (* --- osodb read (scan input image) --- *)', ...io.pre.map(l => '  ' + l)]  : [];
    const post = (io && io.post.length) ? ['  (* --- osodb write (scan output image) --- *)', ...io.post.map(l => '  ' + l)] : [];
    const allBody = pre.concat(body, post);
    if (!allBody.length) allBody.push('  (* empty *)');

    const locals = [];
    for (const [inst, fb] of edgeDecls) locals.push(`  ${inst} : ${fb};`);

    return `PROCEDURE ${sanitize(name)}()\n` +
      (locals.length ? `VAR\n${locals.join('\n')}\nEND_VAR\n` : '') +
      allBody.join('\n') + `\nEND_PROCEDURE\n`;
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
    _helperGlobals = [];
    _needMillis = false;
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

    // osodb ↔ MariaDB bidirectional binding (I/O image). A tag with an `address`
    // (osodb tag id) is synced each scan, filtered by its ACL mode (in | out | inout):
    //   in    → read osodb into the program (read-only input)
    //   out   → write the program value to osodb (write-only output)
    //   inout → both (default)
    const bound = (state.variables || []).filter(v => v.address);
    const ioPre = [], ioPost = [], manifest = [];
    bound.forEach((v, i) => {
      const mode = String(v.io || 'inout').toLowerCase();
      const nm = sanitize(v.name);
      if (mode === 'in' || mode === 'inout')  ioPre.push(`${nm} := tag_read(${i});`);
      if (mode === 'out' || mode === 'inout') ioPost.push(`tag_write(${i}, ${nm});`);
      manifest.push(`(*   [${i}] ${v.name} <-> osodb '${v.address}' (${mode}) *)`);
    });
    const needTagIO = bound.length > 0;

    const blocks = [];
    // Sub-ladders first, then the entry point (which carries the I/O image sync).
    for (const fn of funcs) blocks.push(compilePou(fn, pous[fn], warnings));
    blocks.push(compilePou('main', pous['main'] || [], warnings, { pre: ioPre, post: ioPost }));

    // Helper globals (timer/counter state) are populated while compiling the blocks above.
    const allGlobals = globals.concat(_helperGlobals);
    const traps =
      (_needMillis ? `TRAP millis() : DINT   TRAP #12;\n` : '') +
      (needTagIO ? `TRAP tag_read(id : DINT) : DINT   TRAP #20;\n` +
                   `TRAP tag_write(id : DINT; value : DINT)   TRAP #21;\n` : '');
    const st =
      `(* Generated by osoLadder → osocompile.js (prototype) *)\n` +
      `(* Source: ${(state.meta && state.meta.name) || 'untitled'} — osoST dialect (IEC 61131-3 subset) *)\n` +
      `(* POUs: main${funcs.length ? ' + ' + funcs.join(', ') : ''} *)\n` +
      (needTagIO ? `(* osodb bindings (index <-> tag, ACL mode): *)\n${manifest.join('\n')}\n` : '') +
      `\n` +
      (traps ? traps + `\n` : '') +
      `VAR_GLOBAL\n${allGlobals.length ? allGlobals.join('\n') : '  (* no tags *)'}\nEND_VAR\n\n` +
      blocks.join('\n');

    return { st, warnings };
  }

  const api = { compileLadderToST };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.OsoCompile = api;

})(typeof window !== 'undefined' ? window : globalThis);
