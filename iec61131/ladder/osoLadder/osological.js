/* ============================================================
   OSOLadder — IEC 61131-3 Ladder Logic Editor
   Part of the OSOlogic project

   Copyright (C) 2026 Jose Roig Borrell, Roig Borrell SL, Ibercomp SL

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public
   License along with this program. If not, see
   <https://www.gnu.org/licenses/>.

   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
'use strict';

// ============================================================
// SECTION 1: CONSTANTS & CONFIG
// ============================================================

let CELL_W = 90;
let CELL_H = 64;
const RAIL_W  = 14;
const FB_H    = 130;
const DEF_COLS = 11;
const SVG_NS  = 'http://www.w3.org/2000/svg';

let COLORS = {
  rail:    '#2c6e9e',
  wire:    '#c8d8e8',
  contact: '#4fc3f7',
  coil:    '#81c784',
  fb:      '#ffb74d',
  math:    '#ce93d8',
  cmp:     '#80deea',
  sel:     '#ffd740',
  drop:    'rgba(41,121,255,0.3)',
  bg:      '#1a1d23',
  fbTitle: '#1a3a5c',
};

const THEMES = {
  light: {
    '--bg-primary':'#f0f2f5','--bg-secondary':'#ffffff','--bg-tertiary':'#e8ecf1',
    '--bg-cell':'#f7f9fc','--rail-color':'#1565c0','--wire-color':'#1a237e',
    '--contact-color':'#0277bd','--coil-color':'#2e7d32','--fb-color':'#e65100',
    '--math-color':'#6a1b9a','--cmp-color':'#00695c','--text-primary':'#1a1d23',
    '--text-secondary':'#4a5568','--text-dim':'#9aa5b4','--accent':'#1565c0',
    '--danger':'#c62828','--border':'#cdd3de','--selected':'#f57f17',
    colors:{ rail:'#1565c0', wire:'#1a237e', contact:'#0277bd', coil:'#2e7d32',
             fb:'#e65100', math:'#6a1b9a', cmp:'#00695c', bg:'#f7f9fc', fbTitle:'#bbdefb' }
  },
  dark: {
    '--bg-primary':'#1a1d23','--bg-secondary':'#22262e','--bg-tertiary':'#2a2f3a',
    '--bg-cell':'#1e2330','--rail-color':'#2c6e9e','--wire-color':'#c8d8e8',
    '--contact-color':'#4fc3f7','--coil-color':'#81c784','--fb-color':'#ffb74d',
    '--math-color':'#ce93d8','--cmp-color':'#80deea','--text-primary':'#e8eaf0',
    '--text-secondary':'#8892a0','--text-dim':'#555e6e','--accent':'#2979ff',
    '--danger':'#ef5350','--border':'#2e3444','--selected':'#ffd740',
    colors:{ rail:'#2c6e9e', wire:'#c8d8e8', contact:'#4fc3f7', coil:'#81c784',
             fb:'#ffb74d', math:'#ce93d8', cmp:'#80deea', bg:'#1a1d23', fbTitle:'#1a3a5c' }
  },
  blue: {
    '--bg-primary':'#0d1b2a','--bg-secondary':'#112233','--bg-tertiary':'#15304a',
    '--bg-cell':'#0d1e30','--rail-color':'#0066cc','--wire-color':'#88ccff',
    '--contact-color':'#00bfff','--coil-color':'#00e5a0','--fb-color':'#ffa040',
    '--math-color':'#cc88ff','--cmp-color':'#44ddff','--text-primary':'#ddeeff',
    '--text-secondary':'#6699bb','--text-dim':'#3a5070','--accent':'#0088ff',
    '--danger':'#ff4455','--border':'#1a3a55','--selected':'#ffdd00',
    colors:{ rail:'#0066cc', wire:'#88ccff', contact:'#00bfff', coil:'#00e5a0',
             fb:'#ffa040', math:'#cc88ff', cmp:'#44ddff', bg:'#0d1b2a', fbTitle:'#0a2240' }
  },
  green: {
    '--bg-primary':'#0a1a0a','--bg-secondary':'#111e11','--bg-tertiary':'#162416',
    '--bg-cell':'#0d1a0d','--rail-color':'#1a6e1a','--wire-color':'#88cc88',
    '--contact-color':'#44ee44','--coil-color':'#ffdd00','--fb-color':'#ff9933',
    '--math-color':'#dd88ff','--cmp-color':'#44ffdd','--text-primary':'#ddffdd',
    '--text-secondary':'#669966','--text-dim':'#355035','--accent':'#22cc22',
    '--danger':'#ff4444','--border':'#1e3a1e','--selected':'#ffff44',
    colors:{ rail:'#1a6e1a', wire:'#88cc88', contact:'#44ee44', coil:'#ffdd00',
             fb:'#ff9933', math:'#dd88ff', cmp:'#44ffdd', bg:'#0a1a0a', fbTitle:'#0d250d' }
  },
  hc: {
    '--bg-primary':'#000000','--bg-secondary':'#111111','--bg-tertiary':'#1a1a1a',
    '--bg-cell':'#000000','--rail-color':'#ffffff','--wire-color':'#ffffff',
    '--contact-color':'#ffffff','--coil-color':'#ffff00','--fb-color':'#ff8800',
    '--math-color':'#ff44ff','--cmp-color':'#44ffff','--text-primary':'#ffffff',
    '--text-secondary':'#cccccc','--text-dim':'#888888','--accent':'#00aaff',
    '--danger':'#ff0000','--border':'#444444','--selected':'#ffff00',
    colors:{ rail:'#ffffff', wire:'#ffffff', contact:'#ffffff', coil:'#ffff00',
             fb:'#ff8800', math:'#ff44ff', cmp:'#44ffff', bg:'#000000', fbTitle:'#1a1a1a' }
  },
};

// ============================================================
// SECTION 2: I18N
// ============================================================

const STRINGS = {
  es: {
    appTitle:'OSOLadder', contacts:'Contactos', coils:'Bobinas',
    timers:'Temporizadores', counters:'Contadores', math:'Matemáticas', compare:'Comparación',
    elements:'Elementos', properties:'Propiedades', variables:'Variables',
    addRung:'+ Agregar Escalón', addBranch:'+ Rama', removeBranch:'- Rama',
    newProject:'Nuevo', importBtn:'Importar', exportBtn:'Exportar',
    undo:'Deshacer', redo:'Rehacer', settings:'Ajustes',
    rungComment:'Comentario del escalón...', emptyCell:'Celda vacía. Arrastra un elemento.',
    selectElement:'Selecciona un elemento para editar.',
    variable:'Variable', instanceName:'Nombre instancia', comment:'Comentario',
    inputs:'Entradas', outputs:'Salidas', varName:'Nombre', varType:'Tipo',
    varScope:'Alcance', varAddr:'Dirección', varComment:'Comentario',
    addVar:'+ Nueva', filterVars:'Filtrar...',
    confirmNew:'¿Crear proyecto nuevo? Se perderán los cambios.',
    confirmDeleteRung:'¿Eliminar este escalón?',
    mustHaveOneRung:'Debe haber al menos un escalón.',
    importError:'Error al importar: ', invalidFormat:'Formato inválido',
    settingsTitle:'Ajustes', language:'Idioma', theme:'Tema', cellSize:'Tamaño celda',
    themeLabels:{ light:'Claro', dark:'Oscuro', blue:'Azul', green:'Verde', hc:'Alto contraste' },
    cellSizes:{ s:'Pequeño (72)', m:'Mediano (90)', l:'Grande (110)' },
    close:'Cerrar',
    enableToggle:'Habilitar/Deshabilitar',
    moveUp:'Mover arriba', moveDown:'Mover abajo', deleteRung:'Eliminar escalón',
    ctxDelete:'Eliminar elemento', ctxClear:'Limpiar celda',
    ctxBefore:'Insertar escalón antes', ctxAfter:'Insertar escalón después',
    ctxCopy:'Copiar celda', ctxPaste:'Pegar celda',
    ctxBranchHere:'Rama desde aquí',
    ctxRemoveBranch:'Eliminar esta rama',
    na:'NA', nc:'NC', tp_pos:'TP+', tp_neg:'TP-',
  },
  en: {
    appTitle:'OSOLadder', contacts:'Contacts', coils:'Coils',
    timers:'Timers', counters:'Counters', math:'Math', compare:'Compare',
    elements:'Elements', properties:'Properties', variables:'Variables',
    addRung:'+ Add Rung', addBranch:'+ Branch', removeBranch:'- Branch',
    newProject:'New', importBtn:'Import', exportBtn:'Export',
    undo:'Undo', redo:'Redo', settings:'Settings',
    rungComment:'Rung comment...', emptyCell:'Empty cell. Drag an element here.',
    selectElement:'Select an element to edit its properties.',
    variable:'Variable', instanceName:'Instance name', comment:'Comment',
    inputs:'Inputs', outputs:'Outputs', varName:'Name', varType:'Type',
    varScope:'Scope', varAddr:'Address', varComment:'Comment',
    addVar:'+ New', filterVars:'Filter...',
    confirmNew:'Create new project? Unsaved changes will be lost.',
    confirmDeleteRung:'Delete this rung?',
    mustHaveOneRung:'Must have at least one rung.',
    importError:'Import error: ', invalidFormat:'Invalid format',
    settingsTitle:'Settings', language:'Language', theme:'Theme', cellSize:'Cell size',
    themeLabels:{ light:'Light', dark:'Dark', blue:'Blue', green:'Green', hc:'High contrast' },
    cellSizes:{ s:'Small (72)', m:'Medium (90)', l:'Large (110)' },
    close:'Close',
    enableToggle:'Enable/Disable',
    moveUp:'Move up', moveDown:'Move down', deleteRung:'Delete rung',
    ctxDelete:'Delete element', ctxClear:'Clear cell',
    ctxBefore:'Insert rung before', ctxAfter:'Insert rung after',
    ctxCopy:'Copy cell', ctxPaste:'Paste cell',
    ctxBranchHere:'Branch from here',
    ctxRemoveBranch:'Remove this branch',
    na:'NO', nc:'NC', tp_pos:'P+', tp_neg:'N-',
  },
};

let currentLang  = 'es';
let currentTheme = 'dark';
function t(key) { return STRINGS[currentLang][key] ?? key; }

// ============================================================
// SECTION 3: ELEMENT METADATA
// ============================================================

const ELEMENT_LABELS = {
  NO_CONTACT:'NA',  NC_CONTACT:'NC',
  POS_TRANSITION:'P+', NEG_TRANSITION:'N-',
  COIL_NORMAL:'( )', COIL_NEGATED:'(/)',
  COIL_SET:'(S)', COIL_RESET:'(R)',
  FB_TON:'TON', FB_TOF:'TOF', FB_TP:'TP',
  FB_CTU:'CTU', FB_CTD:'CTD', FB_CTUD:'CTUD',
  FB_ADD:'ADD', FB_SUB:'SUB', FB_MUL:'MUL', FB_DIV:'DIV', FB_MOV:'MOV',
  FB_PID:'PID', FB_CALL:'CALL',
  FB_GT:'GT', FB_LT:'LT', FB_GE:'GE', FB_LE:'LE', FB_EQ:'EQ', FB_NE:'NE',
};

const FB_TYPES      = new Set(['FB_TON','FB_TOF','FB_TP','FB_CTU','FB_CTD','FB_CTUD','FB_ADD','FB_SUB','FB_MUL','FB_DIV','FB_MOV','FB_GT','FB_LT','FB_GE','FB_LE','FB_EQ','FB_NE','FB_PID','FB_CALL']);
const TIMER_TYPES   = new Set(['FB_TON','FB_TOF','FB_TP']);
const COUNTER_TYPES = new Set(['FB_CTU','FB_CTD','FB_CTUD']);
const MATH_TYPES    = new Set(['FB_ADD','FB_SUB','FB_MUL','FB_DIV','FB_MOV']);
const CMP_TYPES     = new Set(['FB_GT','FB_LT','FB_GE','FB_LE','FB_EQ','FB_NE']);

// ============================================================
// SECTION 4: STATE
// ============================================================

let state    = normalizePous(createDefaultProject());
let simCallDepth = 0;
let undoStack = [], redoStack = [];
let selectedCell  = null;  // { rungId, row, col }
let clipboard     = null;
let dragType      = null;
let armedType     = null;  // click-to-place: a palette tool armed by tapping it
let simRunning    = false; // ladder simulation engine
let simPaused     = false; // debugger: auto-scan paused, step manually
let simState      = {};    // runtime variable values during simulation
let simTimer      = null;

function createDefaultProject() {
  return {
    meta: { name:'Proyecto Sin Nombre', version:'1.0.0',
            created: new Date().toISOString(), modified: new Date().toISOString(),
            standard:'IEC61131-3' },
    variables: [],
    subroutines: { main: [] },
    activePou: 'main',
    rungs: [],
  };
}

// POUs (Program Organization Units): the project holds a 'main' program plus named
// functions (sub-ladders). state.rungs always points at the active POU's rungs so the
// whole engine keeps working unchanged. Called after create/undo/redo/import.
function normalizePous(s) {
  if (!s.subroutines) { s.subroutines = { main: s.rungs || [] }; s.activePou = 'main'; }
  if (!s.subroutines.main) s.subroutines.main = s.rungs || [];
  if (!s.activePou || !s.subroutines[s.activePou]) s.activePou = 'main';
  s.rungs = s.subroutines[s.activePou];   // engine edits the active POU in place
  return s;
}
function renderPouBar() {
  const sel = document.getElementById('pou-select'); if (!sel) return;
  const names = Object.keys(state.subroutines);
  sel.innerHTML = names.map(n => `<option value="${escH(n)}"${n===state.activePou?' selected':''}>${n==='main'?'▶ main':'ƒ '+escH(n)}</option>`).join('');
  const del = document.getElementById('btn-pou-del'); if (del) del.style.display = (state.activePou==='main') ? 'none' : '';
}
function switchPou(name) {
  if (!state.subroutines[name]) return;
  state.activePou = name; state.rungs = state.subroutines[name];
  selectedCell = null; showPropsEmpty(); renderAll(); renderPouBar();
}
function newPou() {
  let name = prompt('Nombre de la función (sub-ladder):', 'func1'); if (!name) return;
  name = name.trim().replace(/[^A-Za-z0-9_]/g, '_'); if (!name || name === 'main') return;
  pushUndo();
  if (!state.subroutines[name]) state.subroutines[name] = [createRung(DEF_COLS)];
  switchPou(name);
}
function deletePou() {
  if (state.activePou === 'main') return;
  if (!confirm('¿Eliminar la función "' + state.activePou + '"?')) return;
  pushUndo(); delete state.subroutines[state.activePou]; switchPou('main');
}

let _rc = 0, _vc = 0, _fbc = {};
function nextRungId() { return 'rung_' + (++_rc).toString().padStart(3,'0'); }
function nextVarId()  { return 'v_'    + (++_vc).toString().padStart(4,'0'); }
function nextFbName(type) { _fbc[type] = (_fbc[type]||0)+1; return type+'_'+_fbc[type]; }
function getRung(id) { return state.rungs.find(r => r.id === id) || null; }
function rungIndex(id) { return state.rungs.findIndex(r => r.id === id); }

// ============================================================
// SECTION 5: FACTORIES
// ============================================================

function createRung(cols) {
  const id = nextRungId();
  cols = cols || DEF_COLS;
  // rowStartCols[r] = body column (1-based) where row r's wire starts.
  // Row 0 always starts at 1 (from left rail). Branch rows start at their split column.
  return { id, label:'', enabled:true, rows:1, cols, cells:[new Array(cols).fill(null)], rowStartCols:[1] };
}

function createCell(type) {
  return { type, variableId:null, variableName:'', params: FB_TYPES.has(type) ? defaultFbParams(type) : {}, comment:'' };
}

function defaultFbParams(type) {
  const inst = nextFbName(type);
  if (TIMER_TYPES.has(type))   return { instanceName:inst, IN:'', PT:'T#5s', Q:'', ET:'' };
  if (type==='FB_CTU')         return { instanceName:inst, CU:'', R:'', PV:10, Q:'', CV:'' };
  if (type==='FB_CTD')         return { instanceName:inst, CD:'', LD:'', PV:10, Q:'', CV:'' };
  if (type==='FB_CTUD')        return { instanceName:inst, CU:'', CD:'', R:'', LD:'', PV:10, QU:'', QD:'', CV:'' };
  if (type==='FB_MOV')         return { instanceName:inst, EN:'', IN1:'', ENO:'', OUT:'' };
  if (type==='FB_PID')         return { instanceName:inst, EN:'', PV:'', SP:'', Kp:'1.0', Ki:'0.1', Kd:'0', OUT:'' };
  if (type==='FB_CALL')        return { instanceName:inst, EN:'', target:'', ENO:'' };
  return { instanceName:inst, EN:'', IN1:'', IN2:'', ENO:'', OUT:'' };
}

function createVariable(name, dataType, scope) {
  return { id:nextVarId(), name:name||('VAR_'+(state.variables.length+1)),
           dataType:dataType||'BOOL', scope:scope||'LOCAL',
           initialValue:null, comment:'', address:'' };
}

// Find a variable by name, or auto-declare it. Keeps the variable table in sync
// with what the ladder references, so compile and live monitoring can bind it.
function ensureVar(name, dataType) {
  name = (name || '').trim();
  if (!name) return null;
  let v = state.variables.find(x => x.name === name);
  if (!v) { v = createVariable(name, dataType || 'BOOL'); state.variables.push(v); renderTagTable(); }
  return v;
}

// --- osodb tag browser: pull live tags (incl. hass.* devices) for auto-fill ---
let osodbTags = [];
const OSODB_TYPEMAP = { Boolean:'BOOL', Float:'REAL', Int:'INT', UInt:'UINT', Long:'DINT', String:'STRING' };
async function fetchOsodbTags() {
  let base='';
  try { base = (JSON.parse(localStorage.getItem('osodb_client_cfg')||'{}').baseUrl) || ''; } catch(_) {}
  if (!base) base = location.origin;
  base = base.replace(/\/$/,'');
  for (const path of ['/api/v1/tags','/tags']) {
    try {
      const r = await fetch(base+path); if(!r.ok) continue;
      const j = await r.json(); const arr = Array.isArray(j) ? j : (j.tags||[]);
      osodbTags = arr.map(t => typeof t==='string'
        ? { name:t, dataType:'BOOL' }
        : { name:(t.id||t.key||t.name), dataType:(OSODB_TYPEMAP[t.type||t.data_type]||'BOOL'), units:t.units, access:t.access })
        .filter(x => x.name);
      return osodbTags.length;
    } catch(_) {}
  }
  return 0;
}

// ============================================================
// SECTION 6: UNDO/REDO
// ============================================================

function pushUndo() {
  undoStack.push(JSON.stringify(state));
  if (undoStack.length > 60) undoStack.shift();
  redoStack = [];
  syncUndoBtns();
}
function undo() {
  if (!undoStack.length) return;
  redoStack.push(JSON.stringify(state));
  state = normalizePous(JSON.parse(undoStack.pop()));
  syncUndoBtns(); renderAll(); renderPouBar();
}
function redo() {
  if (!redoStack.length) return;
  undoStack.push(JSON.stringify(state));
  state = normalizePous(JSON.parse(redoStack.pop()));
  syncUndoBtns(); renderAll(); renderPouBar();
}
function syncUndoBtns() {
  document.getElementById('btn-undo').disabled = !undoStack.length;
  document.getElementById('btn-redo').disabled = !redoStack.length;
}

// ============================================================
// SECTION 7: SVG HELPERS
// ============================================================

function svgEl(tag, attrs) {
  const el = document.createElementNS(SVG_NS, tag);
  for (const [k,v] of Object.entries(attrs||{})) el.setAttribute(k,v);
  return el;
}
function svgLine(x1,y1,x2,y2,stroke,sw) {
  return svgEl('line',{x1,y1,x2,y2,stroke:stroke||COLORS.wire,'stroke-width':sw||2,'stroke-linecap':'round'});
}
function svgRect(x,y,w,h,attrs) { return svgEl('rect',{x,y,width:w,height:h,...(attrs||{})}); }
function svgText(txt,x,y,attrs) {
  const el = svgEl('text',{x,y,'text-anchor':'middle','dominant-baseline':'central',...(attrs||{})});
  el.textContent = txt; return el;
}
function svgGroup(attrs) { return svgEl('g',attrs||{}); }
function trunc(s, n) { if(!s) return ''; return s.length>n ? s.slice(0,n-1)+'…' : s; }

// ============================================================
// SECTION 8: SYMBOL DRAWING
// ============================================================

function drawNOContact(g, x0, y0, label) {
  const ym = y0+CELL_H/2, lx=x0+22, rx=x0+CELL_W-22;
  g.appendChild(svgLine(x0,ym,lx,ym,COLORS.contact));
  g.appendChild(svgLine(rx,ym,x0+CELL_W,ym,COLORS.contact));
  g.appendChild(svgLine(lx,ym-13,lx,ym+13,COLORS.contact));
  g.appendChild(svgLine(rx,ym-13,rx,ym+13,COLORS.contact));
  if(label) g.appendChild(svgText(trunc(label,10),x0+CELL_W/2,ym-21,{fill:COLORS.contact,'font-size':'10','font-family':'monospace'}));
}

function drawNCContact(g, x0, y0, label) {
  drawNOContact(g,x0,y0,label);
  const ym=y0+CELL_H/2,lx=x0+22,rx=x0+CELL_W-22;
  g.appendChild(svgLine(lx,ym+11,rx,ym-11,COLORS.contact,2));
}

function drawTransition(g, x0, y0, label, letter) {
  drawNOContact(g,x0,y0,label);
  const ym=y0+CELL_H/2,cx=x0+CELL_W/2;
  g.appendChild(svgText(letter,cx,ym+1,{fill:COLORS.contact,'font-size':'12','font-family':'monospace','font-weight':'bold'}));
}

function drawCoil(g, x0, y0, label, inner) {
  // IEC 61131-3 coil = facing parentheses  ( )   (not a full circle)
  const ym=y0+CELL_H/2, cx=x0+CELL_W/2, gap=9, h=15, bow=8;
  g.appendChild(svgLine(x0,ym,cx-gap,ym,COLORS.coil));
  g.appendChild(svgLine(cx+gap,ym,x0+CELL_W,ym,COLORS.coil));
  g.appendChild(svgEl('path',{d:`M ${cx-gap} ${ym-h} Q ${cx-gap-bow} ${ym} ${cx-gap} ${ym+h}`,
    stroke:COLORS.coil,'stroke-width':2,fill:'none','stroke-linecap':'round'}));
  g.appendChild(svgEl('path',{d:`M ${cx+gap} ${ym-h} Q ${cx+gap+bow} ${ym} ${cx+gap} ${ym+h}`,
    stroke:COLORS.coil,'stroke-width':2,fill:'none','stroke-linecap':'round'}));
  if(inner==='/') g.appendChild(svgLine(cx-7,ym+8,cx+7,ym-8,COLORS.coil,2));
  else if(inner)  g.appendChild(svgText(inner,cx,ym+1,{fill:COLORS.coil,'font-size':'12','font-family':'monospace','font-weight':'bold'}));
  if(label) g.appendChild(svgText(trunc(label,10),cx,ym-22,{fill:COLORS.coil,'font-size':'10','font-family':'monospace'}));
}

function getFbPins(type) {
  if(TIMER_TYPES.has(type)) return {
    left:[{name:'IN',key:'IN'},{name:'PT',key:'PT'}],
    right:[{name:'Q',key:'Q'},{name:'ET',key:'ET'}]};
  if(type==='FB_CTU') return {
    left:[{name:'CU',key:'CU'},{name:'R',key:'R'},{name:'PV',key:'PV'}],
    right:[{name:'Q',key:'Q'},{name:'CV',key:'CV'}]};
  if(type==='FB_CTD') return {
    left:[{name:'CD',key:'CD'},{name:'LD',key:'LD'},{name:'PV',key:'PV'}],
    right:[{name:'Q',key:'Q'},{name:'CV',key:'CV'}]};
  if(type==='FB_CTUD') return {
    left:[{name:'CU',key:'CU'},{name:'CD',key:'CD'},{name:'R',key:'R'},{name:'LD',key:'LD'},{name:'PV',key:'PV'}],
    right:[{name:'QU',key:'QU'},{name:'QD',key:'QD'},{name:'CV',key:'CV'}]};
  if(type==='FB_MOV') return {
    left:[{name:'EN',key:'EN'},{name:'IN',key:'IN1'}],
    right:[{name:'ENO',key:'ENO'},{name:'OUT',key:'OUT'}]};
  if(type==='FB_PID') return {
    left:[{name:'EN',key:'EN'},{name:'PV',key:'PV'},{name:'SP',key:'SP'},{name:'Kp',key:'Kp'},{name:'Ki',key:'Ki'},{name:'Kd',key:'Kd'}],
    right:[{name:'OUT',key:'OUT'}]};
  if(type==='FB_CALL') return {
    left:[{name:'EN',key:'EN'},{name:'ƒ',key:'target'}],
    right:[{name:'ENO',key:'ENO'}]};
  return {
    left:[{name:'EN',key:'EN'},{name:'IN1',key:'IN1'},{name:'IN2',key:'IN2'}],
    right:[{name:'ENO',key:'ENO'},{name:'OUT',key:'OUT'}]};
}

function drawFB(g, x0, y0, type, params) {
  const label = ELEMENT_LABELS[type]||type;
  const bx=x0+2, by=y0+2, bw=CELL_W-4, bh=FB_H-4, titleH=18;
  let color = COLORS.fb;
  if(MATH_TYPES.has(type)) color=COLORS.math;
  if(CMP_TYPES.has(type))  color=COLORS.cmp;

  g.appendChild(svgRect(bx,by,bw,bh,{stroke:color,'stroke-width':2,fill:COLORS.bg,rx:3}));
  g.appendChild(svgRect(bx,by,bw,titleH,{fill:COLORS.fbTitle,rx:3}));
  g.appendChild(svgRect(bx,by+titleH-3,bw,3,{fill:COLORS.fbTitle}));
  g.appendChild(svgText(label,x0+CELL_W/2,by+titleH/2,{fill:color,'font-size':'11','font-family':'monospace','font-weight':'bold'}));
  const inst = params?.instanceName ? trunc(params.instanceName,10) : '';
  if(inst) g.appendChild(svgText(inst,x0+CELL_W/2,by+titleH+10,{fill:COLORS.wire,'font-size':'9','font-family':'monospace'}));

  const pins = getFbPins(type);
  const maxPins = Math.max(pins.left.length, pins.right.length);
  const pinSpace = (bh-titleH-20) / Math.max(maxPins,1);

  pins.left.forEach((pin,i)=>{
    const py = by+titleH+20+i*pinSpace;
    g.appendChild(svgLine(x0,py,bx,py,color,1.5));
    g.appendChild(svgText(pin.name,bx+4,py,{fill:color,'font-size':'9','font-family':'monospace','text-anchor':'start','dominant-baseline':'central'}));
    const val = params?.[pin.key] ? trunc(String(params[pin.key]),8) : '';
    if(val) g.appendChild(svgText(val,bx+44,py-9,{fill:COLORS.wire,'font-size':'8','font-family':'monospace','text-anchor':'middle'}));
  });
  pins.right.forEach((pin,i)=>{
    const py = by+titleH+20+i*pinSpace;
    g.appendChild(svgLine(bx+bw,py,x0+CELL_W,py,color,1.5));
    g.appendChild(svgText(pin.name,bx+bw-4,py,{fill:color,'font-size':'9','font-family':'monospace','text-anchor':'end','dominant-baseline':'central'}));
    const val = params?.[pin.key] ? trunc(String(params[pin.key]),8) : '';
    if(val) g.appendChild(svgText(val,bx+bw-44,py-9,{fill:COLORS.wire,'font-size':'8','font-family':'monospace','text-anchor':'middle'}));
  });
}

// ============================================================
// SECTION 9: RUNG RENDERING
// ============================================================

function rowHeight(rung, r) {
  // Check if any FB in this row
  for(let c=1;c<rung.cols-1;c++){
    const cell=rung.cells[r]?.[c];
    if(cell && FB_TYPES.has(cell.type)) return FB_H;
  }
  return CELL_H;
}

function rungSvgH(rung) {
  let h=0;
  for(let r=0;r<rung.rows;r++) h+=rowHeight(rung,r);
  return h;
}

function rowYOffset(rung, r) {
  let y=0;
  for(let i=0;i<r;i++) y+=rowHeight(rung,i);
  return y;
}

function buildRungSVG(rung) {
  // Ensure rowStartCols exists (backward compat with imported files)
  if(!rung.rowStartCols) rung.rowStartCols = Array.from({length:rung.rows},()=>1);

  const svgH    = rungSvgH(rung);
  const svgW    = rung.cols * CELL_W;
  const rxStart = (rung.cols-1)*CELL_W;  // x of right rail
  // Parallel branches rejoin just before the first coil (so both paths feed it),
  // or at the right rail if the rung has no coil.
  let joinCol = rung.cols-1;
  for(let c=1;c<rung.cols-1;c++){ const cc=rung.cells[0]?.[c]; if(cc && cc.type && cc.type.startsWith('COIL')){ joinCol=c; break; } }
  const joinX = joinCol < rung.cols-1 ? joinCol*CELL_W : rxStart;

  const svg = svgEl('svg',{
    class:'rung-svg',
    viewBox:`0 0 ${svgW} ${svgH}`,
    'data-rung-id':rung.id,
    preserveAspectRatio:'xMinYMid meet',
  });

  svg.appendChild(svgRect(0,0,svgW,svgH,{fill:COLORS.bg}));

  // ── 1. HORIZONTAL WIRES per row ────────────────────────────
  // Row 0: always full wire from left rail to right rail
  // Branch rows: wire starts at their split column
  for(let r=0;r<rung.rows;r++){
    const rh  = rowHeight(rung,r);
    const y0  = rowYOffset(rung,r);
    const ym  = y0 + rh/2;
    const sc  = rung.rowStartCols[r] ?? 1;   // start column index
    const xStart = r === 0 ? RAIL_W : sc * CELL_W;  // row 0 starts at rail edge
    const xEnd   = r === 0 ? rxStart : joinX;       // branch rows rejoin at the coil
    svg.appendChild(svgLine(xStart, ym, xEnd, ym, COLORS.wire, 2));
  }

  // ── 2. VERTICAL BRANCH CONNECTORS ─────────────────────────
  // For each branch row r>0, draw a vertical line at its startCol
  // connecting from the midline of the row ABOVE at that same x
  // down to this row's midline.
  // Also group all rows that share the same startCol to draw a single
  // continuous vertical segment spanning all of them.
  if(rung.rows > 1){
    // Right side: connect all rows at the right rail edge
    const topYm = rowYOffset(rung,0) + rowHeight(rung,0)/2;
    const botYm = rowYOffset(rung,rung.rows-1) + rowHeight(rung,rung.rows-1)/2;
    svg.appendChild(svgLine(joinX, topYm, joinX, botYm, COLORS.wire, 2.5));

    // Left/mid side: per branch row
    for(let r=1;r<rung.rows;r++){
      const sc    = rung.rowStartCols[r] ?? 1;
      const xConn = sc * CELL_W;  // x of vertical connector

      // Find the row above that has a wire reaching this x
      // (i.e., the first row above whose startCol <= sc)
      let topRow = r - 1;
      while(topRow > 0 && (rung.rowStartCols[topRow] ?? 1) > sc) topRow--;

      const topRh = rowHeight(rung, topRow);
      const topYm = rowYOffset(rung, topRow) + topRh/2;
      const botRh = rowHeight(rung, r);
      const botYm = rowYOffset(rung, r) + botRh/2;

      // Vertical line
      svg.appendChild(svgLine(xConn, topYm, xConn, botYm, COLORS.wire, 2.5));

      // Junction dot on the parent row's wire (filled circle = T-junction)
      svg.appendChild(svgEl('circle',{
        cx: xConn, cy: topYm, r: 4,
        fill: COLORS.wire, 'pointer-events':'none'
      }));
    }
  }

  // ── 3. POWER RAILS (drawn after wires, on top) ────────────
  svg.appendChild(svgRect(0,0,RAIL_W,svgH,{fill:COLORS.rail}));
  svg.appendChild(svgRect(rxStart,0,RAIL_W,svgH,{fill:COLORS.rail}));
  if(simRunning && rung._energized) svg.appendChild(svgRect(0,0,RAIL_W,svgH,{fill:'rgba(76,175,80,0.45)'}));
  svg.appendChild(svgLine(RAIL_W,0,RAIL_W,svgH,'rgba(255,255,255,0.15)',1));
  svg.appendChild(svgLine(rxStart,0,rxStart,svgH,'rgba(255,255,255,0.15)',1));

  // ── 4. ELEMENTS ───────────────────────────────────────────
  for(let r=0;r<rung.rows;r++){
    const rh = rowHeight(rung,r);
    const y0 = rowYOffset(rung,r);
    const sc = rung.rowStartCols[r] ?? 1;  // first accessible column for this row

    for(let c=1;c<rung.cols-1;c++){
      const cell = rung.cells[r]?.[c];
      const x0   = c * CELL_W;
      const accessible = (r === 0) || (c >= sc);  // branch rows: only cols >= startCol

      if(accessible){
        if(cell && cell.type){
          // Opaque background erases the wire under the element
          svg.appendChild(svgRect(x0,y0,CELL_W,rh,{fill:COLORS.bg}));
          if(simRunning && cell._energized) svg.appendChild(svgRect(x0+2,y0+2,CELL_W-4,rh-4,{fill:'rgba(76,175,80,0.16)',rx:4,stroke:'rgba(76,175,80,0.5)','stroke-width':1}));
          drawCellSymbol(svg, cell, x0, y0);
        }
        // Hit overlay
        svg.appendChild(svgRect(x0,y0,CELL_W,rh,{
          fill:'transparent', class:'cell-hit',
          'data-rung-id':rung.id, 'data-row':r, 'data-col':c,
        }));
      } else {
        // Inaccessible area (before branch start): grey fill
        svg.appendChild(svgRect(x0,y0,CELL_W,rh,{fill:'rgba(0,0,0,0.25)'}));
      }
    }
  }

  // Subtle column separators
  for(let c=1;c<rung.cols-1;c++){
    svg.appendChild(svgLine(c*CELL_W,0,c*CELL_W,svgH,'rgba(255,255,255,0.04)',1));
  }

  attachSVGEvents(svg, rung);
  return svg;
}

function drawCellSymbol(svg, cell, x0, y0) {
  const g = svgGroup();
  switch(cell.type){
    case 'NO_CONTACT':    drawNOContact(g,x0,y0,cell.variableName); break;
    case 'NC_CONTACT':    drawNCContact(g,x0,y0,cell.variableName); break;
    case 'POS_TRANSITION': drawTransition(g,x0,y0,cell.variableName,'P'); break;
    case 'NEG_TRANSITION': drawTransition(g,x0,y0,cell.variableName,'N'); break;
    case 'COIL_NORMAL':   drawCoil(g,x0,y0,cell.variableName,null); break;
    case 'COIL_NEGATED':  drawCoil(g,x0,y0,cell.variableName,'/'); break;
    case 'COIL_SET':      drawCoil(g,x0,y0,cell.variableName,'S'); break;
    case 'COIL_RESET':    drawCoil(g,x0,y0,cell.variableName,'R'); break;
    default:
      if(FB_TYPES.has(cell.type)) drawFB(g,x0,y0,cell.type,cell.params);
  }
  svg.appendChild(g);
}

function attachSVGEvents(svg, rung) {
  svg.addEventListener('click',      onCellClick);
  svg.addEventListener('contextmenu',onCellRightClick);
  svg.addEventListener('dragover',   onCellDragOver);
  svg.addEventListener('drop',       onCellDrop);
  svg.addEventListener('dragleave',  onCellDragLeave);
}

// Selection / drop overlays
function drawSelOverlay(svg, col, row, rh) {
  clearOverlay(svg,'sel-ov');
  const x0=col*CELL_W, y0=rowOffsetFromSVG(svg,row);
  svg.appendChild(svgRect(x0,y0,CELL_W,rh||CELL_H,{
    fill:'rgba(255,215,64,0.12)',stroke:COLORS.sel,'stroke-width':2,
    'pointer-events':'none', id:'sel-ov-'+svg.dataset.rungId
  }));
}
function drawDropOverlay(svg, col, row, rh) {
  clearOverlay(svg,'drop-ov');
  const x0=col*CELL_W, y0=rowOffsetFromSVG(svg,row);
  svg.appendChild(svgRect(x0,y0,CELL_W,rh||CELL_H,{
    fill:COLORS.drop,stroke:'#2979ff','stroke-width':2,
    'pointer-events':'none', id:'drop-ov-'+svg.dataset.rungId
  }));
}
function clearOverlay(svg, prefix) {
  svg.querySelectorAll(`[id^="${prefix}"]`).forEach(el=>el.remove());
}
function rowOffsetFromSVG(svg, row) {
  const rung = getRung(svg.dataset.rungId);
  return rung ? rowYOffset(rung, row) : row*CELL_H;
}

// ============================================================
// SECTION 10: FULL RENDER
// ============================================================

function renderAll() {
  const list = document.getElementById('rung-list');
  list.innerHTML = '';
  state.rungs.forEach(r => list.appendChild(buildRungContainer(r)));
  renderTagTable();
  document.getElementById('project-name').value = state.meta.name;
}

function rerenderRung(rungId) {
  const rung = getRung(rungId);
  if(!rung) return;
  const wrapper = document.querySelector(`.rung-svg-wrapper[data-rung-id="${rungId}"]`);
  if(!wrapper){ renderAll(); return; }
  wrapper.innerHTML='';
  wrapper.appendChild(buildRungSVG(rung));
  // Re-apply selection overlay
  if(selectedCell?.rungId===rungId){
    const svg=wrapper.querySelector('.rung-svg');
    const rh = rowHeight(rung, selectedCell.row);
    if(svg) drawSelOverlay(svg, selectedCell.col, selectedCell.row, rh);
  }
}

function buildRungContainer(rung) {
  const div = document.createElement('div');
  div.className='rung-container'+(rung.enabled?'':' rung-disabled');
  div.dataset.rungId=rung.id;

  // Header
  const hdr=document.createElement('div');
  hdr.className='rung-header';
  hdr.innerHTML=`
    <span class="rung-num">${rungIndex(rung.id)+1}</span>
    <input class="rung-label" type="text" value="${escH(rung.label)}" placeholder="${t('rungComment')}" data-rung-id="${rung.id}">
    <button class="rung-btn add-branch-btn" data-rung-id="${rung.id}" title="${t('addBranch')}">${t('addBranch')}</button>
    ${rung.rows>1?`<button class="rung-btn rem-branch-btn" data-rung-id="${rung.id}" title="${t('removeBranch')}">${t('removeBranch')}</button>`:''}
    <button class="rung-btn toggle-btn${rung.enabled?' active':''}" data-rung-id="${rung.id}" data-action="toggle" title="${t('enableToggle')}">&#9679;</button>
    <button class="rung-btn" data-rung-id="${rung.id}" data-action="move-up"   title="${t('moveUp')}">&#9650;</button>
    <button class="rung-btn" data-rung-id="${rung.id}" data-action="move-down" title="${t('moveDown')}">&#9660;</button>
    <button class="rung-btn danger" data-rung-id="${rung.id}" data-action="delete" title="${t('deleteRung')}">&#10005;</button>
  `;
  div.appendChild(hdr);

  hdr.querySelector('.rung-label').addEventListener('change', e=>{
    getRung(e.target.dataset.rungId).label=e.target.value;
  });
  hdr.querySelector('.add-branch-btn').addEventListener('click', e=>{
    const rid = e.currentTarget.dataset.rungId;
    // Use selected cell's column as split point (if selection is in this rung)
    const sc = (selectedCell?.rungId === rid)
               ? selectedCell.col
               : 1;
    addBranch(rid, sc);
  });
  hdr.querySelector('.rem-branch-btn')?.addEventListener('click', e=>{
    removeBranch(e.currentTarget.dataset.rungId);
  });
  hdr.querySelectorAll('.rung-btn[data-action]').forEach(btn=>{
    btn.addEventListener('click', onRungHeaderBtn);
  });

  // SVG wrapper
  const wrap=document.createElement('div');
  wrap.className='rung-svg-wrapper';
  wrap.dataset.rungId=rung.id;
  wrap.appendChild(buildRungSVG(rung));
  div.appendChild(wrap);
  return div;
}

function updateRungNumbers() {
  document.querySelectorAll('.rung-num').forEach((el,i)=>{ el.textContent=i+1; });
}

// ============================================================
// SECTION 11: RUNG OPERATIONS
// ============================================================

function addRung(afterId) {
  pushUndo();
  const rung=createRung(DEF_COLS);
  if(afterId){ const idx=rungIndex(afterId); state.rungs.splice(idx+1,0,rung); }
  else state.rungs.push(rung);
  renderAll();
}

function deleteRung(rungId) {
  if(state.rungs.length<=1){ alert(t('mustHaveOneRung')); return; }
  if(!confirm(t('confirmDeleteRung'))) return;
  pushUndo();
  state.rungs=state.rungs.filter(r=>r.id!==rungId);
  if(selectedCell?.rungId===rungId){ selectedCell=null; showPropsEmpty(); }
  renderAll();
}

function moveRung(rungId, dir) {
  const idx=rungIndex(rungId), nIdx=idx+dir;
  if(nIdx<0||nIdx>=state.rungs.length) return;
  pushUndo();
  [state.rungs[idx],state.rungs[nIdx]]=[state.rungs[nIdx],state.rungs[idx]];
  renderAll();
}

function toggleRung(rungId) {
  pushUndo();
  const rung=getRung(rungId);
  rung.enabled=!rung.enabled;
  const container=document.querySelector(`.rung-container[data-rung-id="${rungId}"]`);
  container?.classList.toggle('rung-disabled',!rung.enabled);
  document.querySelector(`.toggle-btn[data-rung-id="${rungId}"]`)?.classList.toggle('active',rung.enabled);
}

// startCol: body column index where this branch splits off (1 = from left rail)
function addBranch(rungId, startCol) {
  pushUndo();
  const rung = getRung(rungId);
  startCol = startCol ?? 1;
  // Clamp to valid body columns
  startCol = Math.max(1, Math.min(startCol, rung.cols - 2));
  rung.rows++;
  rung.cells.push(new Array(rung.cols).fill(null));
  if(!rung.rowStartCols) rung.rowStartCols = Array.from({length:rung.rows-1},()=>1);
  rung.rowStartCols.push(startCol);
  const container = document.querySelector(`.rung-container[data-rung-id="${rungId}"]`);
  if(container) container.replaceWith(buildRungContainer(rung));
}

// rowIdx: which branch row to remove (default = last row)
function removeBranch(rungId, rowIdx) {
  const rung = getRung(rungId);
  if(!rung || rung.rows <= 1) return;
  rowIdx = (rowIdx !== undefined) ? rowIdx : rung.rows - 1;
  if(rowIdx < 1 || rowIdx >= rung.rows) return;  // can't remove row 0
  pushUndo();
  rung.rows--;
  rung.cells.splice(rowIdx, 1);
  if(rung.rowStartCols) rung.rowStartCols.splice(rowIdx, 1);
  if(selectedCell?.rungId === rungId && selectedCell.row >= rung.rows){
    selectedCell = null; showPropsEmpty();
  }
  const container = document.querySelector(`.rung-container[data-rung-id="${rungId}"]`);
  if(container) container.replaceWith(buildRungContainer(rung));
}

function onRungHeaderBtn(e) {
  const rungId=e.currentTarget.dataset.rungId;
  const action=e.currentTarget.dataset.action;
  if(action==='toggle')    toggleRung(rungId);
  if(action==='move-up')   moveRung(rungId,-1);
  if(action==='move-down') moveRung(rungId,1);
  if(action==='delete')    deleteRung(rungId);
}

// ============================================================
// SECTION 12: CELL OPERATIONS
// ============================================================

function placeElement(rungId, row, col, type) {
  if(col<1||col>DEF_COLS-2) return;
  pushUndo();
  const rung=getRung(rungId);
  if(!rung.cells[row]) rung.cells[row]=new Array(rung.cols).fill(null);
  rung.cells[row][col]=createCell(type);
  rerenderRung(rungId);
  selectCellAt(rungId,row,col);
}

function deleteElement(rungId, row, col) {
  pushUndo();
  const rung=getRung(rungId);
  if(rung.cells[row]) rung.cells[row][col]=null;
  if(selectedCell?.rungId===rungId&&selectedCell.row===row&&selectedCell.col===col){
    selectedCell=null; showPropsEmpty();
  }
  rerenderRung(rungId);
}

// ============================================================
// SECTION 13: SELECTION & PROPERTIES
// ============================================================

function selectCellAt(rungId, row, col) {
  if(selectedCell){
    const oldSvg=document.querySelector(`.rung-svg[data-rung-id="${selectedCell.rungId}"]`);
    if(oldSvg) clearOverlay(oldSvg,'sel-ov');
  }
  selectedCell={rungId,row,col};
  const rung=getRung(rungId);
  const svg=document.querySelector(`.rung-svg[data-rung-id="${rungId}"]`);
  if(svg&&rung) drawSelOverlay(svg,col,row,rowHeight(rung,row));
  populateProps(rungId,row,col);
}

function onCellClick(e) {
  const hit=e.target.closest('.cell-hit');
  if(!hit) return;
  const rungId=hit.dataset.rungId, row=+hit.dataset.row, col=+hit.dataset.col;
  // In simulation, tapping a contact forces its variable (virtual push-button).
  if(simRunning){
    const cell=getRung(rungId)?.cells[row]?.[col];
    if(cell && SIM_CONTACTS.has(cell.type)){ simToggleInput(cell); simTick(); }
    return;
  }
  // If a palette tool is armed, tapping a valid cell drops it (stays armed for repeats).
  if(armedType && col>=1 && col<=DEF_COLS-2){
    placeElement(rungId,row,col,armedType);
    return;
  }
  selectCellAt(rungId, row, col);
}

function onCellRightClick(e) {
  e.preventDefault();
  const hit=e.target.closest('.cell-hit');
  if(!hit) return;
  const rungId=hit.dataset.rungId, row=+hit.dataset.row, col=+hit.dataset.col;
  selectCellAt(rungId,row,col);
  showCtxMenu(e.clientX,e.clientY,rungId,row,col);
}

function populateProps(rungId, row, col) {
  const rung=getRung(rungId);
  const cell=rung?.cells[row]?.[col];
  const panel=document.getElementById('props-content');

  if(!cell){ panel.innerHTML=`<div class="props-empty">${t('emptyCell')}</div>`; return; }

  const type=cell.type;
  const isFB=FB_TYPES.has(type);
  const isContact=['NO_CONTACT','NC_CONTACT','POS_TRANSITION','NEG_TRANSITION'].includes(type);
  const isCoil=['COIL_NORMAL','COIL_NEGATED','COIL_SET','COIL_RESET'].includes(type);
  let badge=isFB?(MATH_TYPES.has(type)?'math':CMP_TYPES.has(type)?'cmp':'fb'):isContact?'contact':'coil';

  let html=`<div class="prop-row"><span class="prop-type-badge ${badge}">${ELEMENT_LABELS[type]||type}</span></div>`;

  if(isContact||isCoil){
    html+=`<div class="prop-row"><div class="prop-label">${t('variable')}</div>
      <input class="prop-input" id="prop-varname" type="text" value="${escH(cell.variableName)}" autocomplete="off" placeholder="${t('variable')}..."></div>`;
  }
  if(isFB){
    html+=`<div class="prop-row"><div class="prop-label">${t('instanceName')}</div>
      <input class="prop-input" id="prop-inst" type="text" value="${escH(cell.params.instanceName||'')}"></div>`;
    html+=`<div class="prop-section-title">${t('inputs')}</div>`;
    const pins=getFbPins(type);
    pins.left.forEach(pin=>{
      html+=`<div class="prop-row"><div class="prop-label">${pin.name}</div>
        <input class="prop-input prop-fb-pin" type="text" data-pin="${pin.key}" value="${escH(String(cell.params[pin.key]||''))}" autocomplete="off"></div>`;
    });
    html+=`<div class="prop-section-title">${t('outputs')}</div>`;
    pins.right.forEach(pin=>{
      html+=`<div class="prop-row"><div class="prop-label">${pin.name}</div>
        <input class="prop-input prop-fb-pin" type="text" data-pin="${pin.key}" value="${escH(String(cell.params[pin.key]||''))}" autocomplete="off"></div>`;
    });
  }
  html+=`<div class="prop-section-title">${t('comment')}</div>
    <div class="prop-row"><input class="prop-input" id="prop-comment" type="text" value="${escH(cell.comment||'')}" placeholder="${t('comment')}..."></div>`;

  panel.innerHTML=html;

  panel.querySelector('#prop-varname')?.addEventListener('input',e=>{
    showAC(e.target,rungId,row,col,false);
  });
  panel.querySelector('#prop-varname')?.addEventListener('change',e=>{
    pushUndo();
    const c=getRung(rungId).cells[row][col];
    c.variableName=e.target.value;
    const v=ensureVar(e.target.value, 'BOOL');   // auto-declare if new
    c.variableId=v?v.id:null;
    hideAC(); rerenderRung(rungId);
  });
  panel.querySelector('#prop-varname')?.addEventListener('blur',()=>setTimeout(hideAC,150));

  panel.querySelector('#prop-inst')?.addEventListener('change',e=>{
    pushUndo();
    getRung(rungId).cells[row][col].params.instanceName=e.target.value;
    rerenderRung(rungId);
  });
  panel.querySelectorAll('.prop-fb-pin').forEach(inp=>{
    inp.addEventListener('input',e=>{
      const c=getRung(rungId)?.cells[row]?.[col];
      if(!c) return;
      c.params[e.target.dataset.pin]=e.target.value;
      showAC(e.target,rungId,row,col,true);
    });
    inp.addEventListener('change',()=>{ pushUndo(); rerenderRung(rungId); });
    inp.addEventListener('blur',()=>setTimeout(hideAC,150));
  });
  panel.querySelector('#prop-comment')?.addEventListener('change',e=>{
    const c=getRung(rungId)?.cells[row]?.[col];
    if(c) c.comment=e.target.value;
  });
}

function showPropsEmpty() {
  document.getElementById('props-content').innerHTML=`<div class="props-empty">${t('selectElement')}</div>`;
}

// ============================================================
// SECTION 14: DRAG AND DROP
// ============================================================

function initDnD() {
  document.querySelectorAll('.palette-item').forEach(item=>{
    item.addEventListener('dragstart',e=>{
      dragType=item.dataset.type;
      e.dataTransfer.effectAllowed='copy';
      e.dataTransfer.setData('text/plain',dragType);
    });
    item.addEventListener('dragend',()=>{ dragType=null; });
    // Click-to-place (touch-friendly): tap a tool to arm it, tap a cell to drop it.
    item.addEventListener('click',()=>armTool(item.dataset.type,item));
  });
}

function armTool(type,item){
  const already = armedType===type;
  document.querySelectorAll('.palette-item.armed').forEach(el=>el.classList.remove('armed'));
  armedType = already ? null : type;
  if(armedType && item) item.classList.add('armed');
  document.body.classList.toggle('placing', !!armedType);
}
function disarmTool(){
  if(!armedType) return;
  armedType=null;
  document.querySelectorAll('.palette-item.armed').forEach(el=>el.classList.remove('armed'));
  document.body.classList.remove('placing');
}

function onCellDragOver(e) {
  e.preventDefault();
  const hit=e.target.closest('.cell-hit');
  if(!hit) return;
  const col=+hit.dataset.col;
  if(col<1||col>DEF_COLS-2) return;
  e.dataTransfer.dropEffect='copy';
  const rung=getRung(hit.dataset.rungId);
  const rh=rung?rowHeight(rung,+hit.dataset.row):CELL_H;
  drawDropOverlay(e.currentTarget,col,+hit.dataset.row,rh);
}
function onCellDragLeave(e) {
  if(!e.currentTarget.contains(e.relatedTarget)) clearOverlay(e.currentTarget,'drop-ov');
}
function onCellDrop(e) {
  e.preventDefault();
  clearOverlay(e.currentTarget,'drop-ov');
  const hit=e.target.closest('.cell-hit');
  if(!hit) return;
  const col=+hit.dataset.col;
  if(col<1||col>DEF_COLS-2) return;
  const type=e.dataTransfer.getData('text/plain');
  if(!type||!ELEMENT_LABELS[type]) return;
  placeElement(hit.dataset.rungId,+hit.dataset.row,col,type);
}

// ============================================================
// SECTION 15: AUTOCOMPLETE
// ============================================================

function showAC(input, rungId, row, col, pinMode) {
  const q=input.value.toLowerCase();
  const declared=new Set(state.variables.map(v=>v.name));
  const matches=[];
  for(const v of state.variables) if(v.name.toLowerCase().includes(q)) matches.push({name:v.name,dataType:v.dataType,src:'var'});
  for(const t of osodbTags) if(!declared.has(t.name) && t.name.toLowerCase().includes(q))
    matches.push({name:t.name,dataType:t.dataType,src:t.name.startsWith('hass.')?'device':'osodb'});
  matches.splice(12);
  const ul=document.getElementById('var-autocomplete');
  if(!matches.length){ ul.classList.add('hidden'); return; }
  ul.innerHTML='';
  matches.forEach(v=>{
    const li=document.createElement('li');
    const badge = v.src==='device' ? '🔌 device' : (v.src==='osodb' ? 'osodb' : v.dataType);
    li.innerHTML=`${escH(v.name)} <span class="ac-type">${escH(badge)}</span>`;
    li.addEventListener('mousedown',()=>{
      pushUndo();
      const cell=getRung(rungId)?.cells[row]?.[col];
      if(!cell) return;
      // osodb tag / device → auto-declare a variable wired by address = tag id
      if(v.src!=='var'){ const nv=ensureVar(v.name, v.dataType); if(nv && !nv.address){ nv.address=v.name; renderTagTable(); } }
      const dv=state.variables.find(x=>x.name===v.name);
      if(pinMode){ cell.params[input.dataset.pin]=v.name; }
      else { cell.variableName=v.name; cell.variableId=dv?dv.id:undefined; }
      input.value=v.name;
      hideAC(); rerenderRung(rungId);
    });
    ul.appendChild(li);
  });
  const rect=input.getBoundingClientRect();
  ul.style.left=rect.left+'px'; ul.style.top=(rect.bottom+2)+'px';
  ul.style.minWidth=rect.width+'px';
  ul.classList.remove('hidden');
}
function hideAC(){ document.getElementById('var-autocomplete').classList.add('hidden'); }

// ============================================================
// SECTION 16: CONTEXT MENU
// ============================================================

let ctxTarget=null;
function showCtxMenu(x,y,rungId,row,col) {
  ctxTarget={rungId,row,col};
  const m=document.getElementById('context-menu');
  m.querySelector('[data-action="delete"]').textContent        = t('ctxDelete');
  m.querySelector('[data-action="clear"]').textContent         = t('ctxClear');
  m.querySelector('[data-action="insert-before"]').textContent = t('ctxBefore');
  m.querySelector('[data-action="insert-after"]').textContent  = t('ctxAfter');
  m.querySelector('[data-action="copy"]').textContent          = t('ctxCopy');
  m.querySelector('[data-action="paste"]').textContent         = t('ctxPaste');
  m.querySelector('[data-action="branch-here"]').textContent   = t('ctxBranchHere');
  m.querySelector('[data-action="remove-branch"]').textContent = t('ctxRemoveBranch');
  // "branch here" available on any row
  m.querySelector('[data-action="branch-here"]').style.display   = '';
  // "remove branch" only on branch rows (row > 0)
  m.querySelector('[data-action="remove-branch"]').style.display = (row > 0) ? '' : 'none';
  m.style.left=x+'px'; m.style.top=y+'px';
  m.classList.remove('hidden');
}
function hideCtxMenu(){ document.getElementById('context-menu').classList.add('hidden'); ctxTarget=null; }

function initCtxMenu() {
  document.getElementById('context-menu').querySelectorAll('.ctx-item').forEach(item=>{
    item.addEventListener('click',()=>{
      if(!ctxTarget) return;
      const {rungId,row,col}=ctxTarget;
      const action=item.dataset.action;
      hideCtxMenu();
      if(action==='delete'||action==='clear') deleteElement(rungId,row,col);
      else if(action==='insert-before'){
        pushUndo();
        const idx=rungIndex(rungId);
        state.rungs.splice(idx,0,createRung(DEF_COLS));
        renderAll();
      }
      else if(action==='insert-after') addRung(rungId);
      else if(action==='branch-here'){
        addBranch(rungId, col);
      }
      else if(action==='remove-branch'){
        removeBranch(rungId, row);
      }
      else if(action==='copy'){
        const cell=getRung(rungId)?.cells[row]?.[col];
        clipboard=cell?JSON.parse(JSON.stringify(cell)):null;
      }
      else if(action==='paste'){
        if(!clipboard) return;
        pushUndo();
        const rung=getRung(rungId);
        rung.cells[row][col]=JSON.parse(JSON.stringify(clipboard));
        rerenderRung(rungId);
        selectCellAt(rungId,row,col);
      }
    });
  });
  document.addEventListener('click',e=>{
    if(!document.getElementById('context-menu').contains(e.target)) hideCtxMenu();
  });
}

// ============================================================
// SECTION 17: TAG TABLE
// ============================================================

function renderTagTable() {
  const filter=document.getElementById('var-filter').value.toLowerCase();
  const tbody=document.getElementById('var-table-body');
  tbody.innerHTML='';
  const vars=filter?state.variables.filter(v=>v.name.toLowerCase().includes(filter)||v.comment.toLowerCase().includes(filter)):state.variables;
  vars.forEach(v=>{
    const tr=document.createElement('tr');
    tr.innerHTML=`
      <td><input type="text" value="${escH(v.name)}" data-id="${v.id}" data-field="name"></td>
      <td><select data-id="${v.id}" data-field="dataType">
        ${['BOOL','INT','DINT','LINT','UINT','UDINT','REAL','LREAL','TIME','STRING'].map(dt=>`<option${v.dataType===dt?' selected':''}>${dt}</option>`).join('')}
      </select></td>
      <td><select data-id="${v.id}" data-field="scope">
        ${['LOCAL','GLOBAL','IO_INPUT','IO_OUTPUT'].map(s=>`<option${v.scope===s?' selected':''}>${s}</option>`).join('')}
      </select></td>
      <td><input type="text" value="${escH(v.address||'')}" data-id="${v.id}" data-field="address" placeholder="%IX0.0"></td>
      <td class="var-live" data-vname="${escH(v.name)}">${escH(liveValueFor(v.name))}</td>
      <td><input type="text" value="${escH(v.comment||'')}" data-id="${v.id}" data-field="comment" placeholder="${t('varComment')}..."></td>
      <td><button class="var-del-btn" data-id="${v.id}">&#10005;</button></td>
    `;
    tbody.appendChild(tr);
  });
  tbody.querySelectorAll('input,select').forEach(el=>{
    el.addEventListener('change',e=>{
      const v=state.variables.find(v=>v.id===e.target.dataset.id);
      if(v) v[e.target.dataset.field]=e.target.value;
    });
  });
  tbody.querySelectorAll('.var-del-btn').forEach(btn=>{
    btn.addEventListener('click',e=>{
      pushUndo();
      state.variables=state.variables.filter(v=>v.id!==e.currentTarget.dataset.id);
      renderTagTable();
    });
  });
}

// Live value for a variable name, from the PLC connector (osodb / DB via REST).
function liveValueFor(name) {
  if (simRunning) {
    const v = simState[name];
    return (v === undefined || v === null) ? '—' : String(v);
  }
  try {
    const lv = (typeof PlcConnector !== 'undefined') ? PlcConnector.live() : null;
    const val = lv ? lv[name] : undefined;
    return (val === undefined || val === null) ? '—' : String(val);
  } catch (_) { return '—'; }
}

// Refresh just the live-value cells in place (called on every osodb poll update).
function updateLiveCells() {
  document.querySelectorAll('#var-table-body td.var-live').forEach(td => {
    const val = liveValueFor(td.dataset.vname);
    td.textContent = val;
    td.classList.toggle('live-on', val === 'true' || val === '1');
  });
}

// osoplc.js broadcasts this after each REST poll of osodb / the DB.
document.addEventListener('plc:update', updateLiveCells);

// ============================================================
// SECTION: LADDER SIMULATION (client-side scan engine)
// ============================================================
const SIM_SCAN_MS = 100;
const SIM_CONTACTS = new Set(['NO_CONTACT','NC_CONTACT','POS_TRANSITION','NEG_TRANSITION']);
const SIM_COILS    = new Set(['COIL_NORMAL','COIL_NEGATED','COIL_SET','COIL_RESET']);

function simParseTime(s) {
  if (s == null) return 0;
  const m = String(s).match(/(\d+(?:\.\d+)?)\s*(ms|s|m|h)?/i);
  if (!m) return 0;
  return parseFloat(m[1]) * ({ ms:1, s:1000, m:60000, h:3600000 }[(m[2]||'ms').toLowerCase()] || 1);
}
function simInit() {
  simState = {};
  state.variables.forEach(v => { simState[v.name] = (v.dataType === 'BOOL') ? false : 0; });
  state.rungs.forEach(r => (r.cells||[]).forEach(row => (row||[]).forEach(c => {
    if (c) { delete c._et; delete c._cv; delete c._prev; delete c._prevCU; delete c._prevCD; delete c._pulse; delete c._prevIN; c._energized = false; }
  })));
}
function simContact(cell) {
  const v = !!simState[cell.variableName];
  switch (cell.type) {
    case 'NO_CONTACT': return v;
    case 'NC_CONTACT': return !v;
    case 'POS_TRANSITION': { const p = cell._prev||false; cell._prev = v; return v && !p; }
    case 'NEG_TRANSITION': { const p = cell._prev||false; cell._prev = v; return !v && p; }
  }
  return false;
}
function simFB(cell, cond, dt) {
  const p = cell.params || {};
  const IN = (p.IN != null && p.IN !== '') ? !!simState[p.IN] : cond;
  const PT = simParseTime(p.PT);
  const out = (q, et) => { if (p.Q) simState[p.Q] = q; if (p.ET) simState[p.ET] = Math.round(et||0); cell._q = q; };
  switch (cell.type) {
    case 'FB_TON': if (IN) { cell._et = (cell._et||0)+dt; out(cell._et>=PT, Math.min(cell._et,PT)); } else { cell._et = 0; out(false,0); } break;
    case 'FB_TOF': if (IN) { cell._et = 0; out(true,0); } else { cell._et = (cell._et||0)+dt; out(cell._et<PT, Math.min(cell._et,PT)); } break;
    case 'FB_TP':  if (IN && !cell._prevIN) { cell._et = 0; cell._pulse = true; } cell._prevIN = IN;
                   if (cell._pulse) { cell._et = (cell._et||0)+dt; if (cell._et>=PT) cell._pulse = false; }
                   out(!!cell._pulse, Math.min(cell._et||0,PT)); break;
    case 'FB_CTU': { const CU = (p.CU!=null&&p.CU!=='')?!!simState[p.CU]:cond; if (p.R && simState[p.R]) cell._cv = 0; else if (CU && !cell._prevCU) cell._cv = (cell._cv||0)+1; cell._prevCU = CU; const PV = Number(p.PV)||0; if (p.CV) simState[p.CV] = cell._cv||0; cell._q = (cell._cv||0) >= PV; if (p.Q) simState[p.Q] = cell._q; break; }
    case 'FB_CTD': { const CD = (p.CD!=null&&p.CD!=='')?!!simState[p.CD]:cond; const PV = Number(p.PV)||0; if (p.LD && simState[p.LD]) cell._cv = PV; else if (CD && !cell._prevCD) cell._cv = (cell._cv||0)-1; cell._prevCD = CD; if (p.CV) simState[p.CV] = cell._cv||0; cell._q = (cell._cv||0) <= 0; if (p.Q) simState[p.Q] = cell._q; break; }
    case 'FB_CALL': {
      const en = (p.IN != null && p.IN !== '') ? cond : cond;
      const gate = (p.EN != null && p.EN !== '') ? !!simState[p.EN] : cond;
      cell._q = gate;
      if (gate && p.target && state.subroutines[p.target] && simCallDepth < 8) {
        simCallDepth++; state.subroutines[p.target].forEach(r => simRung(r, dt)); simCallDepth--;
      }
      if (p.ENO) simState[p.ENO] = gate;
      break;
    }
    default: simFBMathCmp(cell, cond); break;
  }
}
function simNum(x) {
  if (x == null || x === '') return 0;
  const n = Number(x);
  return isNaN(n) ? (Number(simState[x]) || 0) : n;
}
function simFBMathCmp(cell, cond) {
  const p = cell.params || {};
  const en = (p.EN != null && p.EN !== '') ? !!simState[p.EN] : cond;
  const a = simNum(p.IN1), b = simNum(p.IN2);
  let res, isBool = false;
  switch (cell.type) {
    case 'FB_ADD': res = a + b; break;
    case 'FB_SUB': res = a - b; break;
    case 'FB_MUL': res = a * b; break;
    case 'FB_DIV': res = b !== 0 ? a / b : 0; break;
    case 'FB_MOV': res = a; break;
    case 'FB_GT': res = a > b;  isBool = true; break;
    case 'FB_LT': res = a < b;  isBool = true; break;
    case 'FB_GE': res = a >= b; isBool = true; break;
    case 'FB_LE': res = a <= b; isBool = true; break;
    case 'FB_EQ': res = a === b; isBool = true; break;
    case 'FB_NE': res = a !== b; isBool = true; break;
    case 'FB_PID': res = simPID(cell, p, en); break;
    default: cell._q = cond; return;
    // (FB_CALL handled in simFB main switch)
  }
  if (en) {
    if (isBool) { if (p.OUT) simState[p.OUT] = res; cell._q = res; }
    else { if (p.OUT) simState[p.OUT] = Math.round(res * 1000) / 1000; cell._q = true; }
    if (p.ENO) simState[p.ENO] = true;
  } else { cell._q = isBool ? false : false; if (p.ENO) simState[p.ENO] = false; }
}
function simPID(cell, p, en) {
  const pv = simNum(p.PV), sp = simNum(p.SP);
  const kp = simNum(p.Kp) || 1, ki = simNum(p.Ki), kd = simNum(p.Kd);
  const dt = SIM_SCAN_MS / 1000;
  if (!en) { cell._pidI = 0; cell._pidPrev = undefined; return 0; }
  const err = sp - pv;
  cell._pidI = (cell._pidI || 0) + err * dt;
  const der = (cell._pidPrev === undefined) ? 0 : (err - cell._pidPrev) / dt;
  cell._pidPrev = err;
  let out = kp * err + ki * cell._pidI + kd * der;
  out = Math.max(0, Math.min(100, out));   // clamp 0..100
  if (p.OUT) simState[p.OUT] = Math.round(out * 100) / 100;
  return out;
}
function simRung(rung, dt) {
  if (rung.enabled === false) { rung._energized = false; return; }
  const rowConds = [];
  for (let r = 0; r < rung.cells.length; r++) {
    const row = rung.cells[r] || [];
    let has = false, acc = true;
    for (let c = 1; c < rung.cols-1; c++) {
      const cell = row[c];
      if (cell && SIM_CONTACTS.has(cell.type)) { has = true; acc = acc && simContact(cell); cell._energized = acc; }
    }
    if (has) rowConds.push(acc);
  }
  const cond = rowConds.length ? rowConds.some(Boolean) : true;
  rung._energized = cond;
  for (let r = 0; r < rung.cells.length; r++) {
    const row = rung.cells[r] || [];
    for (let c = 1; c < rung.cols-1; c++) {
      const cell = row[c]; if (!cell) continue;
      if (SIM_COILS.has(cell.type)) {
        const n = cell.variableName;
        if (n) switch (cell.type) {
          case 'COIL_NORMAL':  simState[n] = cond; break;
          case 'COIL_NEGATED': simState[n] = !cond; break;
          case 'COIL_SET':     if (cond) simState[n] = true; break;
          case 'COIL_RESET':   if (cond) simState[n] = false; break;
        }
        cell._energized = cond;
      } else if (FB_TYPES.has(cell.type)) { simFB(cell, cond, dt); cell._energized = !!cell._q; }
    }
  }
}
function simTick() {
  state.rungs.forEach(r => simRung(r, SIM_SCAN_MS));
  renderAll();
  updateLiveCells();
}
function simToggleInput(cell) {
  // Click a contact in sim mode to force its variable (bit) — a virtual push-button.
  const n = cell.variableName; if (!n) return;
  simState[n] = !simState[n];
}
function toggleSimulation() {
  simRunning = !simRunning;
  simPaused = false;
  document.body.classList.toggle('sim-on', simRunning);
  const btn = document.getElementById('btn-sim');
  if (btn) { btn.textContent = simRunning ? '■ Detener' : '▶ Simular'; btn.classList.toggle('sim-active', simRunning); }
  if (simRunning) { simInit(); simTimer = setInterval(simTick, SIM_SCAN_MS); simTick(); }
  else { clearInterval(simTimer); simTimer = null; renderAll(); updateLiveCells(); }
  updateSimControls();
}
// Debugger controls: pause the auto-scan and step one scan at a time.
function updateSimControls() {
  const p = document.getElementById('btn-sim-pause');
  const s = document.getElementById('btn-sim-step');
  if (p) { p.style.display = simRunning ? '' : 'none'; p.textContent = simPaused ? '▶' : '⏸'; p.title = simPaused ? 'Reanudar' : 'Pausar'; }
  if (s) s.style.display = (simRunning && simPaused) ? '' : 'none';
}
function pauseSimulation() {
  if (!simRunning) return;
  simPaused = !simPaused;
  if (simPaused) { clearInterval(simTimer); simTimer = null; }
  else { simTimer = setInterval(simTick, SIM_SCAN_MS); }
  updateSimControls();
}
function stepSimulation() {
  if (simRunning && simPaused) simTick();   // one scan cycle
}

// ============================================================
// SECTION 18: IMPORT / EXPORT
// ============================================================

function exportJSON() {
  state.meta.modified=new Date().toISOString();
  state.meta.name=document.getElementById('project-name').value||'Proyecto';
  const json=JSON.stringify(state,null,2);
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([json],{type:'application/json'}));
  a.download=(state.meta.name.replace(/\s+/g,'_'))+'.osol';
  a.click(); URL.revokeObjectURL(a.href);
}

function importJSON(file) {
  const reader=new FileReader();
  reader.onload=e=>{
    try{
      const data=JSON.parse(e.target.result);
      if((!data.rungs||!Array.isArray(data.rungs)) && !data.subroutines) throw new Error(t('invalidFormat'));
      pushUndo(); state=normalizePous(data);
      _rc=state.rungs.length; _vc=state.variables.length;
      renderAll(); renderPouBar();
    } catch(err){ alert(t('importError')+err.message); }
  };
  reader.readAsText(file);
}

// ============================================================
// SECTION 19: SETTINGS
// ============================================================

function initSettings() {
  const modal=document.getElementById('settings-modal');
  document.getElementById('btn-settings').addEventListener('click',()=>{
    modal.classList.remove('hidden');
    // Sync current values
    document.getElementById('set-lang').value=currentLang;
    document.getElementById('set-theme').value=currentTheme;
    document.getElementById('set-cellsize').value=CELL_W;
  });
  document.getElementById('set-close').addEventListener('click',()=>modal.classList.add('hidden'));
  modal.addEventListener('click',e=>{ if(e.target===modal) modal.classList.add('hidden'); });

  document.getElementById('set-lang').addEventListener('change',e=>setLang(e.target.value));
  document.getElementById('set-theme').addEventListener('change',e=>applyTheme(e.target.value));
  document.getElementById('set-cellsize').addEventListener('change',e=>{
    CELL_W=parseInt(e.target.value)||90;
    renderAll();
  });
}

function setLang(lang) {
  currentLang=lang;
  // Update static UI text
  document.getElementById('btn-new').textContent=t('newProject');
  document.getElementById('btn-import').textContent=t('importBtn');
  document.getElementById('btn-export').textContent=t('exportBtn');
  document.getElementById('btn-undo').textContent=t('undo');
  document.getElementById('btn-redo').textContent=t('redo');
  document.getElementById('btn-add-rung').textContent=t('addRung');
  document.getElementById('btn-settings').textContent='⚙ '+t('settings');
  document.getElementById('panel-props-title').textContent=t('properties');
  document.getElementById('panel-vars-title').textContent=t('variables');
  document.getElementById('btn-add-var').textContent=t('addVar');
  document.getElementById('var-filter').placeholder=t('filterVars');
  // Rebuild rungs to update all buttons/labels
  renderAll();
}

function applyTheme(name) {
  currentTheme=name;
  const theme=THEMES[name]||THEMES.dark;
  const root=document.documentElement;
  for(const [k,v] of Object.entries(theme)){
    if(k!=='colors') root.style.setProperty(k,v);
  }
  COLORS={...COLORS,...theme.colors};
  renderAll();
}

// ============================================================
// SECTION 20: KEYBOARD
// ============================================================

function initKeyboard() {
  document.addEventListener('keydown',e=>{
    const tag=document.activeElement.tagName;
    const inInput=tag==='INPUT'||tag==='TEXTAREA'||tag==='SELECT';
    if(e.ctrlKey&&e.key==='z'&&!inInput){ e.preventDefault(); undo(); }
    if(e.ctrlKey&&(e.key==='y'||(e.shiftKey&&e.key==='Z'))&&!inInput){ e.preventDefault(); redo(); }
    if(e.ctrlKey&&e.key==='s'){ e.preventDefault(); exportJSON(); }
    if((e.key==='Delete'||e.key==='Backspace')&&!inInput&&selectedCell){
      deleteElement(selectedCell.rungId,selectedCell.row,selectedCell.col);
    }
    if(e.key==='Escape'){ hideCtxMenu(); hideAC(); disarmTool(); }
  });
}

// ============================================================
// SECTION 21: UTILS
// ============================================================

function escH(s){ if(typeof s!=='string') return ''; return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }

// ============================================================
// SECTION 22: INIT
// ============================================================

document.addEventListener('DOMContentLoaded',()=>{
  // Palette group toggles
  document.querySelectorAll('.palette-group-header').forEach(h=>{
    h.addEventListener('click',()=>{
      const body=h.nextElementSibling;
      body.style.display=body.style.display==='none'?'':'none';
    });
  });

  // First rung
  state.rungs.push(createRung(DEF_COLS));
  renderAll();
  syncUndoBtns();

  // Pull osodb tags (incl. hass.* devices) so the variable field auto-fills from the plant
  fetchOsodbTags().then(n => { if(n) console.log('[ladder] '+n+' osodb tags available for auto-fill'); });
  setInterval(fetchOsodbTags, 30000);

  // Toolbar
  document.getElementById('btn-undo').addEventListener('click',undo);
  document.getElementById('btn-redo').addEventListener('click',redo);
  document.getElementById('btn-add-rung').addEventListener('click',()=>addRung());
  document.getElementById('btn-export').addEventListener('click',exportJSON);
  document.getElementById('btn-sim')?.addEventListener('click',toggleSimulation);
  document.getElementById('btn-sim-pause')?.addEventListener('click',pauseSimulation);
  document.getElementById('btn-sim-step')?.addEventListener('click',stepSimulation);
  document.getElementById('pou-select')?.addEventListener('change',e=>switchPou(e.target.value));
  document.getElementById('btn-pou-new')?.addEventListener('click',newPou);
  document.getElementById('btn-pou-del')?.addEventListener('click',deletePou);
  renderPouBar();
  document.getElementById('btn-import').addEventListener('click',()=>document.getElementById('file-input').click());
  document.getElementById('file-input').addEventListener('change',e=>{ if(e.target.files[0]) importJSON(e.target.files[0]); e.target.value=''; });
  document.getElementById('btn-new').addEventListener('click',()=>{
    if(!confirm(t('confirmNew'))) return;
    pushUndo(); state=createDefaultProject(); _rc=0;_vc=0;_fbc={};
    state.rungs.push(createRung(DEF_COLS)); renderAll(); syncUndoBtns();
  });
  document.getElementById('project-name').addEventListener('change',e=>{ state.meta.name=e.target.value; });
  document.getElementById('btn-add-var').addEventListener('click',()=>{ pushUndo(); state.variables.push(createVariable()); renderTagTable(); });
  document.getElementById('var-filter').addEventListener('input',renderTagTable);

  initDnD();
  initCtxMenu();
  initKeyboard();
  initSettings();
});
