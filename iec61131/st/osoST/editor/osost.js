/**
 * osoLogic — osoST Web Editor
 * editor/osost.js — Monaco editor wiring, project management, compile REST calls
 *
 * Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
 *               Ibercomp SL, Roig Borrell SL
 *
 * Part of the osoLogic open-source PLC project — osologic.org
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

"use strict";

/* ── ST-1: Constants / Constantes ───────────────────────────── */
const LS_PROJECT_KEY = "osost_project";   // localStorage key for project data
const DEFAULT_SOURCE = `(* osoLogic — IEC 61131-3 Structured Text example
   Edit this code and click Compile to produce a HEX file.

   osoLogic — ejemplo IEC 61131-3 Structured Text
   Edite este código y haga clic en Compilar para producir un archivo HEX. *)

VAR_GLOBAL
  counter : DINT := 0;
END_VAR

PROCEDURE main()
VAR
  led : BOOL;
END_VAR
  counter := counter + 1;
  led := (counter MOD 2) = 0;
  debug(counter, led);
END_PROCEDURE
`;

/* ── ST-2: State / Estado ───────────────────────────────────── */
let monacoEditor = null;     // Monaco IEditor instance

/** Project: map of filename → source string */
const project = {
  files:   {},     // { "untitled.st": "source..." }
  current: "",     // active filename
};

let lastHex = null;   // last compiled HEX string (for download)

/* ── ST-3: Monaco initialisation / Inicialización de Monaco ─── */

require(["vs/editor/editor.main"], function () {
  // Register ST language / Registrar lenguaje ST
  _registerSTLanguage();

  monacoEditor = monaco.editor.create(document.getElementById("monaco-editor"), {
    value:            DEFAULT_SOURCE,
    language:         "st",
    theme:            "vs-dark",
    fontSize:         13,
    fontFamily:       "'Cascadia Code', 'Fira Code', Consolas, monospace",
    fontLigatures:    true,
    minimap:          { enabled: false },
    scrollBeyondLastLine: false,
    automaticLayout:  true,
    tabSize:          2,
    insertSpaces:     true,
    wordWrap:         "off",
    rulers:           [80],
    renderWhitespace: "boundary",
    bracketPairColorization: { enabled: true },
  });

  // Init project with default file / Inicializar proyecto con archivo predeterminado
  project.files["untitled.st"] = DEFAULT_SOURCE;
  project.current = "untitled.st";

  // Load persisted project if any / Cargar proyecto guardado si existe
  _loadProject();

  // Wire editor events / Conectar eventos del editor
  monacoEditor.onDidChangeCursorPosition(_updateCursorStatus);
  monacoEditor.onDidChangeModelContent(() => {
    project.files[project.current] = monacoEditor.getValue();
    _updateSizeStatus();
  });

  _updateCursorStatus();
  _updateSizeStatus();

  // Keyboard shortcuts / Atajos de teclado
  monacoEditor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, _saveFile);
  monacoEditor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyB, _compile);
  monacoEditor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyN, _newFile);
  monacoEditor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyO, () =>
    document.getElementById("file-input").click()
  );
});

/* ── ST-4: ST Language definition / Definición del lenguaje ST ── */

function _registerSTLanguage() {
  monaco.languages.register({ id: "st" });

  monaco.languages.setMonarchTokensProvider("st", {
    ignoreCase: true,
    defaultToken: "",
    keywords: [
      "PROGRAM", "END_PROGRAM", "FUNCTION", "END_FUNCTION",
      "FUNCTION_BLOCK", "END_FUNCTION_BLOCK", "PROCEDURE", "END_PROCEDURE",
      "VAR", "VAR_GLOBAL", "VAR_INPUT", "VAR_OUTPUT", "VAR_IN_OUT", "END_VAR",
      "TYPE", "END_TYPE", "ARRAY", "OF", "STRUCT", "END_STRUCT",
      "IF", "THEN", "ELSIF", "ELSE", "END_IF",
      "CASE", "END_CASE",
      "WHILE", "DO", "END_WHILE",
      "REPEAT", "UNTIL", "END_REPEAT",
      "FOR", "TO", "BY", "DOWNTO", "END_FOR",
      "RETURN", "EXIT", "BEGIN", "END",
      "AND", "OR", "NOT", "XOR", "MOD",
      "TRUE", "FALSE",
      "TRAP", "DEBUG",
    ],
    builtinTypes: [
      "BOOL", "SINT", "USINT", "INT", "UINT", "DINT", "UDINT",
      "LONG", "ULONG", "REAL", "LREAL", "FLOAT", "STRING",
      "BYTE", "WORD", "DWORD",
    ],
    operators: [":=", "<>", "<=", ">=", "=", "<", ">", "+", "-", "*", "/", "%", "?"],
    tokenizer: {
      root: [
        [/\(\*/, "comment", "@blockComment"],
        [/\/\/[^\n]*/, "comment"],
        [/"(?:[^"\\]|\\.)*"/, "string"],
        [/\d+\.\d+(?:[eE][+-]?\d+)?/, "number.float"],
        [/\d+/, "number"],
        [/[A-Za-z_][A-Za-z0-9_]*/, {
          cases: {
            "@keywords":     "keyword",
            "@builtinTypes": "type",
            "@default":      "identifier",
          }
        }],
        [/:=|<>|<=|>=|[=<>+\-*/%?]/, "operator"],
        [/[(){}\[\],;:.]/, "delimiter"],
        [/[ \t\r\n]+/, "white"],
      ],
      blockComment: [
        [/\*\)/, "comment", "@pop"],
        [/./, "comment"],
      ],
    },
  });

  monaco.editor.defineTheme("st-dark", {
    base: "vs-dark",
    inherit: true,
    rules: [
      { token: "keyword",    foreground: "569cd6", fontStyle: "bold" },
      { token: "type",       foreground: "4ec9b0" },
      { token: "comment",    foreground: "6a9955", fontStyle: "italic" },
      { token: "string",     foreground: "ce9178" },
      { token: "number",     foreground: "b5cea8" },
      { token: "number.float", foreground: "b5cea8" },
      { token: "operator",   foreground: "d4d4d4" },
      { token: "identifier", foreground: "9cdcfe" },
    ],
    colors: {},
  });
  monaco.editor.setTheme("st-dark");
}

/* ── ST-5: Compile / Compilar ────────────────────────────────── */

async function _compile() {
  const source  = monacoEditor.getValue();
  const server  = document.getElementById("inp-server").value.trim().replace(/\/$/, "");
  const backend = document.getElementById("sel-backend").value;
  const filename = project.current;

  _setStatus("running", "Compiling…");
  _showOutput("console");
  _appendConsole(`[ostc] Compiling ${filename} via ${backend} backend…\n`, "info");

  try {
    const endpoint = (backend === "python")
      ? `${server}/compile/python`
      : `${server}/compile`;

    const resp = await fetch(endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source, filename }),
    });

    const data = await resp.json();

    if (!resp.ok || !data.ok) {
      const msgs = data.errors || [data.error || "Unknown error / Error desconocido"];
      msgs.forEach(e => _appendConsole(`ERROR: ${e}\n`, "error"));
      _setStatus("error", "Compile failed");
      document.querySelector(".statusbar").classList.add("statusbar--error");
      return;
    }

    document.querySelector(".statusbar").classList.remove("statusbar--error");

    if (data.warnings?.length) {
      data.warnings.forEach(w => _appendConsole(`WARN: ${w}\n`, "warn"));
    }

    _appendConsole(
      `[ostc] OK — ${data.size_bytes} bytes, ${data.compile_ms} ms\n`, "ok"
    );

    // Show HEX in output panel / Mostrar HEX en panel de salida
    lastHex = data.hex_raw || atob(data.hex || "");
    document.getElementById("out-hex").textContent = lastHex;
    document.getElementById("btn-download").disabled = false;
    _setStatus("ok", `${data.size_bytes} B`);

  } catch (err) {
    _appendConsole(`ERROR: ${err.message}\n`, "error");
    _setStatus("error", "Network error");
  }
}

/* ── ST-6: Lex / Parse actions / Acciones lex / parse ─────────── */

async function _lexOnly() {
  const source = monacoEditor.getValue();
  const server = document.getElementById("inp-server").value.trim().replace(/\/$/, "");

  _setStatus("running", "Lexing…");
  _showOutput("tokens");

  try {
    const resp = await fetch(`${server}/lex`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source }),
    });
    const data = await resp.json();

    if (!resp.ok || !data.ok) {
      document.getElementById("out-tokens").textContent =
        (data.errors || [data.error]).join("\n");
      _setStatus("error", "Lex failed");
      return;
    }

    // Format token list / Formatear lista de tokens
    const lines = (data.tokens || []).map(t =>
      `${String(t.line).padStart(4)}:${String(t.col).padEnd(4)}  ` +
      `${t.kind.padEnd(20)}  ${JSON.stringify(t.value)}`
    );
    document.getElementById("out-tokens").textContent = lines.join("\n");
    _setStatus("ok", `${lines.length} tokens`);

  } catch (err) {
    _setStatus("error", err.message);
  }
}

async function _parseOnly() {
  const source = monacoEditor.getValue();
  const server = document.getElementById("inp-server").value.trim().replace(/\/$/, "");

  _setStatus("running", "Parsing…");
  _showOutput("ast");

  try {
    const resp = await fetch(`${server}/parse`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source }),
    });
    const data = await resp.json();

    if (!resp.ok || !data.ok) {
      document.getElementById("out-ast").textContent =
        (data.errors || [data.error]).join("\n");
      _setStatus("error", "Parse failed");
      return;
    }

    document.getElementById("out-ast").textContent =
      JSON.stringify(data.ast, null, 2);
    _setStatus("ok", "Parsed");

  } catch (err) {
    _setStatus("error", err.message);
  }
}

/* ── ST-7: File management / Gestión de archivos ─────────────── */

function _newFile() {
  const name = prompt("New file name / Nombre del archivo:", "new_prog.st");
  if (!name) return;
  const fname = name.endsWith(".st") ? name : name + ".st";
  project.files[fname] = `(* ${fname} *)\n\nPROCEDURE main()\nEND_PROCEDURE\n`;
  _switchFile(fname);
  _renderFileTree();
}

function _saveFile() {
  // Save to localStorage / Guardar en localStorage
  project.files[project.current] = monacoEditor.getValue();
  _persistProject();
  _flashStatus("Saved");
}

function _switchFile(fname) {
  if (project.current) {
    project.files[project.current] = monacoEditor.getValue();
  }
  project.current = fname;
  monacoEditor.setValue(project.files[fname] || "");
  document.getElementById("lbl-filename").textContent = fname;
  _renderFileTree();
}

function _renderFileTree() {
  const ul = document.getElementById("file-tree");
  ul.innerHTML = "";
  for (const fname of Object.keys(project.files)) {
    const li = document.createElement("li");
    li.className = "file-tree__item" +
                   (fname === project.current ? " file-tree__item--active" : "");
    li.dataset.file = fname;
    li.innerHTML = `<span class="file-icon">&#128196;</span> ${fname}`;
    li.addEventListener("click", () => _switchFile(fname));
    ul.appendChild(li);
  }
}

/* ── ST-8: Local storage / Almacenamiento local ─────────────── */

function _persistProject() {
  try {
    localStorage.setItem(LS_PROJECT_KEY, JSON.stringify({
      files:   project.files,
      current: project.current,
    }));
  } catch (_) { /* quota exceeded — ignore */ }
}

function _loadProject() {
  try {
    const raw = localStorage.getItem(LS_PROJECT_KEY);
    if (!raw) return;
    const saved = JSON.parse(raw);
    Object.assign(project.files, saved.files || {});
    const cur = saved.current;
    if (cur && project.files[cur]) {
      project.current = cur;
      monacoEditor.setValue(project.files[cur]);
      document.getElementById("lbl-filename").textContent = cur;
    }
    _renderFileTree();
  } catch (_) { /* corrupt data — ignore */ }
}

/* ── ST-9: HEX download / Descarga de HEX ──────────────────── */

function _downloadHex() {
  if (!lastHex) return;
  const blob = new Blob([lastHex], { type: "text/plain" });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement("a");
  a.href     = url;
  a.download = project.current.replace(/\.\w+$/, ".hex");
  a.click();
  URL.revokeObjectURL(url);
}

/* ── ST-10: Output panel helpers / Ayudantes del panel de salida */

function _showOutput(tab) {
  document.querySelectorAll(".out-tab").forEach(b => {
    b.classList.toggle("out-tab--active", b.dataset.out === tab);
  });
  document.querySelectorAll(".out-panel").forEach((p, i) => {
    const tabs = ["console", "tokens", "ast", "hex"];
    p.classList.toggle("out-panel--active", tabs[i] === tab);
  });
}

function _appendConsole(text, cls = "") {
  const el = document.getElementById("out-console");
  if (cls) {
    const span = document.createElement("span");
    span.className = `out-${cls}`;
    span.textContent = text;
    el.appendChild(span);
  } else {
    el.appendChild(document.createTextNode(text));
  }
  el.scrollTop = el.scrollHeight;
}

function _clearOutput() {
  ["out-console", "out-tokens", "out-ast", "out-hex"].forEach(id => {
    document.getElementById(id).textContent = "";
    document.getElementById(id).innerHTML = "";
  });
  document.getElementById("btn-download").disabled = true;
  lastHex = null;
}

/* ── ST-11: Status bar helpers / Ayudantes de barra de estado ── */

function _setStatus(state, text) {
  const dot   = document.getElementById("status-dot");
  const label = document.getElementById("lbl-status");
  dot.className   = `status-dot status-dot--${state}`;
  label.textContent = text;
}

function _flashStatus(msg) {
  document.getElementById("sb-msg").textContent = msg;
  setTimeout(() => { document.getElementById("sb-msg").textContent = ""; }, 2000);
}

function _updateCursorStatus() {
  if (!monacoEditor) return;
  const pos = monacoEditor.getPosition();
  document.getElementById("sb-pos").textContent =
    `Ln ${pos.lineNumber}, Col ${pos.column}`;
}

function _updateSizeStatus() {
  const len = monacoEditor ? monacoEditor.getValue().length : 0;
  document.getElementById("sb-size").textContent = `${len} chars`;
}

/* ── ST-12: Output tab wiring / Conexión de pestañas de salida ── */

document.querySelectorAll(".out-tab").forEach(btn => {
  btn.addEventListener("click", () => _showOutput(btn.dataset.out));
});

/* ── ST-13: Toolbar button wiring / Conexión de botones ─────── */

document.getElementById("btn-new").addEventListener("click",    _newFile);
document.getElementById("btn-save").addEventListener("click",   _saveFile);
document.getElementById("btn-compile").addEventListener("click", _compile);
document.getElementById("btn-lex").addEventListener("click",    _lexOnly);
document.getElementById("btn-parse").addEventListener("click",  _parseOnly);
document.getElementById("btn-download").addEventListener("click", _downloadHex);
document.getElementById("btn-clear-output").addEventListener("click", _clearOutput);

document.getElementById("btn-open").addEventListener("click", () =>
  document.getElementById("file-input").click()
);

document.getElementById("file-input").addEventListener("change", e => {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = ev => {
    project.files[file.name] = ev.target.result;
    _switchFile(file.name);
    _renderFileTree();
  };
  reader.readAsText(file);
  e.target.value = "";   // reset so same file can be re-opened
});

document.getElementById("btn-add-file").addEventListener("click", _newFile);

/* ── ST-14: Backend selector / Selector de backend ───────────── */

document.getElementById("sel-backend").addEventListener("change", e => {
  // Show/hide server field; Python backend can run locally in-browser (future)
  // Mostrar/ocultar campo servidor; backend Python puede ejecutarse localmente (futuro)
  const isJava = e.target.value === "java";
  document.getElementById("lbl-server").style.display = isJava ? "" : "";
  document.getElementById("inp-server").style.display = isJava ? "" : "";
});

/* ── ST-15: Persist on unload / Guardar al cerrar ─────────────── */
window.addEventListener("beforeunload", () => {
  if (monacoEditor) project.files[project.current] = monacoEditor.getValue();
  _persistProject();
});
