/* ============================================================
   OSOLadder — osoplc.js
   Módulo de conexión con osoLogic PLC
   Protocolos: DB (MySQL/MariaDB/PgSQL) · Redis · REST · MQTT

   Copyright (C) 2026 Jose Roig Borrell, Roig Borrell SL, Ibercomp SL
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
'use strict';

// ============================================================
// SECTION PLC-1: CONFIGURACIÓN Y ESTADO
// ============================================================

const PLC_CFG_DEFAULT = {
  proto: 'none',
  db: {
    type: 'mysql', host: '', port: 3306,
    dbname: '', table: 'plc_vars', user: '', pass: '',
  },
  redis: { host: '', port: 6379, pass: '', prefix: 'plc:' },
  rest:  {
    baseUrl: '', authType: 'none',
    token: '', user: '', pass: '', pollMs: 500,
  },
  mqtt: {
    brokerUrl: 'ws://localhost:9001',
    clientId:  'osoLadder-' + Math.random().toString(36).slice(2, 6),
    user: '', pass: '', topicPrefix: 'plc/', qos: 0,
  },
};

let plcCfg     = JSON.parse(localStorage.getItem('osol_plc_cfg') || 'null')
                 || JSON.parse(JSON.stringify(PLC_CFG_DEFAULT));
let plcStatus  = 'disconnected';  // disconnected | connecting | connected | error
let plcErrMsg  = '';

function savePlcCfg() {
  localStorage.setItem('osol_plc_cfg', JSON.stringify(plcCfg));
}

// ============================================================
// SECTION PLC-2: CONECTOR
// ============================================================

const PlcConnector = {

  _ws:        null,
  _pollTimer: null,

  // ── Conectar ─────────────────────────────────────────────
  async connect() {
    if (plcStatus === 'connected' || plcStatus === 'connecting') return;
    PlcConnector.disconnect();
    plcStatus = 'connecting';
    plcErrMsg = '';
    updatePlcStatusUI();

    try {
      switch (plcCfg.proto) {
        case 'rest':  await this._connectRest();   break;
        case 'mqtt':  await this._connectMqtt();   break;
        case 'db':
        case 'redis': await this._connectBridge(); break;
        case 'none':
        default:
          plcStatus = 'disconnected';
          updatePlcStatusUI();
          return;
      }
      plcStatus = 'connected';
    } catch (e) {
      plcStatus = 'error';
      plcErrMsg = e.message || String(e);
      console.warn('[PlcConnector]', plcErrMsg);
    }
    updatePlcStatusUI();
  },

  // ── Desconectar ───────────────────────────────────────────
  disconnect() {
    if (this._pollTimer) { clearInterval(this._pollTimer); this._pollTimer = null; }
    if (this._ws) {
      this._ws.onclose = null;
      try { this._ws.close(); } catch (_) {}
      this._ws = null;
    }
    plcStatus = 'disconnected';
    plcErrMsg = '';
    updatePlcStatusUI();
  },

  // ── API pública ───────────────────────────────────────────

  /** Lee el valor de una variable del PLC. Devuelve Promise<value>. */
  async readVar(name) {
    this._assertConnected();
    switch (plcCfg.proto) {
      case 'rest': return this._restRead(name);
      case 'mqtt': return this._mqttRead(name);
      default:     throw new Error('readVar via bridge not implemented');
    }
  },

  /** Escribe un valor en una variable del PLC. Devuelve Promise<void>. */
  async writeVar(name, value) {
    this._assertConnected();
    switch (plcCfg.proto) {
      case 'rest': return this._restWrite(name, value);
      case 'mqtt': return this._mqttWrite(name, value);
      default:     throw new Error('writeVar via bridge not implemented');
    }
  },

  _assertConnected() {
    if (plcStatus !== 'connected') throw new Error('PLC no conectado');
  },

  // ── REST ──────────────────────────────────────────────────

  async _connectRest() {
    const url  = plcCfg.rest.baseUrl.replace(/\/$/, '') + '/status';
    const resp = await fetch(url, {
      headers: this._restHeaders(),
      signal:  AbortSignal.timeout(4000),
    });
    if (!resp.ok) throw new Error(`HTTP ${resp.status} ${resp.statusText}`);
    if (plcCfg.rest.pollMs > 0)
      this._pollTimer = setInterval(() => this._restPoll(), plcCfg.rest.pollMs);
  },

  _restHeaders() {
    const h = { 'Content-Type': 'application/json' };
    if (plcCfg.rest.authType === 'bearer')
      h['Authorization'] = `Bearer ${plcCfg.rest.token}`;
    if (plcCfg.rest.authType === 'basic')
      h['Authorization'] = 'Basic ' + btoa(`${plcCfg.rest.user}:${plcCfg.rest.pass}`);
    return h;
  },

  async _restRead(name) {
    const url  = plcCfg.rest.baseUrl.replace(/\/$/, '') + `/var/${encodeURIComponent(name)}`;
    const resp = await fetch(url, { headers: this._restHeaders() });
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    return (await resp.json()).value;
  },

  async _restWrite(name, value) {
    const url  = plcCfg.rest.baseUrl.replace(/\/$/, '') + `/var/${encodeURIComponent(name)}`;
    const resp = await fetch(url, {
      method: 'PUT', headers: this._restHeaders(),
      body: JSON.stringify({ value }),
    });
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
  },

  _restPoll() {
    // TODO: actualizar valores de variables activas en el editor
  },

  // ── MQTT (WebSocket) ──────────────────────────────────────

  async _connectMqtt() {
    return new Promise((resolve, reject) => {
      let done = false;
      const finish = (err) => { if (done) return; done = true; err ? reject(err) : resolve(); };
      const ws = new WebSocket(plcCfg.mqtt.brokerUrl);
      ws.binaryType = 'arraybuffer';
      ws.onopen    = () => { this._ws = ws; finish(); };
      ws.onerror   = () => finish(new Error('WebSocket error — revisa la URL del broker'));
      ws.onclose   = () => {
        if (plcStatus === 'connected') {
          plcStatus = 'disconnected';
          plcErrMsg = 'Conexión perdida';
          updatePlcStatusUI();
        }
      };
      ws.onmessage = (ev) => this._mqttOnMessage(ev);
      setTimeout(() => finish(new Error('Timeout de conexión')), 6000);
    });
  },

  _mqttOnMessage(_ev) {
    // TODO: parsear MQTT sobre WebSocket y actualizar variables
  },

  async _mqttRead(_name)         { return null; /* TODO: subscribe + await response */ },
  async _mqttWrite(name, value)  {
    if (!this._ws || this._ws.readyState !== WebSocket.OPEN)
      throw new Error('WebSocket no está abierto');
    // Envío JSON simple — el bridge lo traducirá a MQTT PUBLISH
    this._ws.send(JSON.stringify({
      topic: plcCfg.mqtt.topicPrefix + name,
      value,
      qos: plcCfg.mqtt.qos,
    }));
  },

  // ── Bridge (DB / Redis) ───────────────────────────────────

  async _connectBridge() {
    // Acceso directo a DB/Redis no es posible desde el navegador.
    // La configuración se persiste para que el bridge backend la lea.
    throw new Error(
      'Este protocolo requiere un bridge backend. ' +
      'La configuración ha sido guardada; conéctate a través del servidor osoLogic.'
    );
  },
};

// ============================================================
// SECTION PLC-3: UI — ACTUALIZAR ESTADO
// ============================================================

const PLC_STATUS_LABELS = {
  disconnected: ['Desconectado', 'Disconnected'],
  connecting:   ['Conectando…',  'Connecting…'],
  connected:    ['Conectado',    'Connected'],
  error:        ['Error',        'Error'],
};

function plcLabel(key) {
  const lang  = (typeof currentLang !== 'undefined') ? currentLang : 'es';
  const idx   = (lang === 'en') ? 1 : 0;
  return (PLC_STATUS_LABELS[key] || [key, key])[idx];
}

function updatePlcStatusUI() {
  // Dot en toolbar
  const dot = document.getElementById('plc-status-dot');
  const lbl = document.getElementById('plc-status-label');
  if (dot) dot.className = `plc-dot plc-dot--${plcStatus}`;
  if (lbl) { lbl.textContent = plcLabel(plcStatus); lbl.title = plcErrMsg; }

  // Dot en la pestaña
  const tabDot = document.getElementById('set-tab-plc-dot');
  if (tabDot) tabDot.className = `plc-dot plc-dot--${plcStatus}`;

  // Span de estado dentro del modal
  const si = document.getElementById('set-plc-status');
  if (si) {
    si.textContent = plcLabel(plcStatus) + (plcErrMsg ? ` — ${plcErrMsg}` : '');
    si.className = 'set-plc-status';
    if (plcStatus === 'connected')  si.classList.add('plc-ok');
    if (plcStatus === 'error')      si.classList.add('plc-err');
    if (plcStatus === 'connecting') si.classList.add('plc-info');
  }
}

// ============================================================
// SECTION PLC-4: UI — MOSTRAR/OCULTAR CAMPOS
// ============================================================

function showPlcFields(proto) {
  ['db', 'redis', 'rest', 'mqtt'].forEach(p => {
    const el = document.getElementById('plc-fields-' + p);
    if (el) el.style.display = (p === proto) ? '' : 'none';
  });
  if (proto === 'rest') syncRestAuthFields();
}

function syncRestAuthFields() {
  const auth = document.getElementById('set-plc-rest-auth')?.value || 'none';
  document.querySelectorAll('.plc-rest-bearer')
    .forEach(el => el.style.display = auth === 'bearer' ? '' : 'none');
  document.querySelectorAll('.plc-rest-basic')
    .forEach(el => el.style.display = auth === 'basic'  ? '' : 'none');
}

// ============================================================
// SECTION PLC-5: SYNC CONFIG ↔ FORMULARIO
// ============================================================

function syncPlcFormFromCfg() {
  _sv('set-plc-proto', plcCfg.proto);
  showPlcFields(plcCfg.proto);

  _sv('set-plc-db-type',  plcCfg.db.type);
  _sv('set-plc-db-host',  plcCfg.db.host);
  _sv('set-plc-db-port',  plcCfg.db.port);
  _sv('set-plc-db-name',  plcCfg.db.dbname);
  _sv('set-plc-db-table', plcCfg.db.table);
  _sv('set-plc-db-user',  plcCfg.db.user);
  _sv('set-plc-db-pass',  plcCfg.db.pass);

  _sv('set-plc-redis-host',   plcCfg.redis.host);
  _sv('set-plc-redis-port',   plcCfg.redis.port);
  _sv('set-plc-redis-pass',   plcCfg.redis.pass);
  _sv('set-plc-redis-prefix', plcCfg.redis.prefix);

  _sv('set-plc-rest-url',   plcCfg.rest.baseUrl);
  _sv('set-plc-rest-auth',  plcCfg.rest.authType);
  _sv('set-plc-rest-token', plcCfg.rest.token);
  _sv('set-plc-rest-user',  plcCfg.rest.user);
  _sv('set-plc-rest-pass',  plcCfg.rest.pass);
  _sv('set-plc-rest-poll',  plcCfg.rest.pollMs);
  syncRestAuthFields();

  _sv('set-plc-mqtt-url',      plcCfg.mqtt.brokerUrl);
  _sv('set-plc-mqtt-clientid', plcCfg.mqtt.clientId);
  _sv('set-plc-mqtt-user',     plcCfg.mqtt.user);
  _sv('set-plc-mqtt-pass',     plcCfg.mqtt.pass);
  _sv('set-plc-mqtt-prefix',   plcCfg.mqtt.topicPrefix);
  _sv('set-plc-mqtt-qos',      plcCfg.mqtt.qos);

  updatePlcStatusUI();
}

function syncPlcCfgFromForm() {
  plcCfg.proto = _gv('set-plc-proto');

  plcCfg.db.type   = _gv('set-plc-db-type');
  plcCfg.db.host   = _gv('set-plc-db-host');
  plcCfg.db.port   = parseInt(_gv('set-plc-db-port'))  || 3306;
  plcCfg.db.dbname = _gv('set-plc-db-name');
  plcCfg.db.table  = _gv('set-plc-db-table');
  plcCfg.db.user   = _gv('set-plc-db-user');
  plcCfg.db.pass   = _gv('set-plc-db-pass');

  plcCfg.redis.host   = _gv('set-plc-redis-host');
  plcCfg.redis.port   = parseInt(_gv('set-plc-redis-port')) || 6379;
  plcCfg.redis.pass   = _gv('set-plc-redis-pass');
  plcCfg.redis.prefix = _gv('set-plc-redis-prefix');

  plcCfg.rest.baseUrl  = _gv('set-plc-rest-url');
  plcCfg.rest.authType = _gv('set-plc-rest-auth');
  plcCfg.rest.token    = _gv('set-plc-rest-token');
  plcCfg.rest.user     = _gv('set-plc-rest-user');
  plcCfg.rest.pass     = _gv('set-plc-rest-pass');
  plcCfg.rest.pollMs   = parseInt(_gv('set-plc-rest-poll')) || 500;

  plcCfg.mqtt.brokerUrl   = _gv('set-plc-mqtt-url');
  plcCfg.mqtt.clientId    = _gv('set-plc-mqtt-clientid');
  plcCfg.mqtt.user        = _gv('set-plc-mqtt-user');
  plcCfg.mqtt.pass        = _gv('set-plc-mqtt-pass');
  plcCfg.mqtt.topicPrefix = _gv('set-plc-mqtt-prefix');
  plcCfg.mqtt.qos         = parseInt(_gv('set-plc-mqtt-qos')) || 0;

  savePlcCfg();
}

function _sv(id, val) {
  const el = document.getElementById(id);
  if (el && val !== undefined && val !== null) el.value = val;
}
function _gv(id) { return document.getElementById(id)?.value ?? ''; }

// ============================================================
// SECTION PLC-6: UI — PESTAÑAS DEL MODAL
// ============================================================

function initModalTabs() {
  document.querySelectorAll('.set-tab').forEach(btn => {
    btn.addEventListener('click', () => {
      // Activar tab
      document.querySelectorAll('.set-tab').forEach(b => b.classList.remove('set-tab--active'));
      btn.classList.add('set-tab--active');
      // Mostrar panel
      const target = btn.dataset.tab;
      document.querySelectorAll('.set-tab-panel').forEach(p => {
        p.classList.toggle('set-tab-panel--hidden', p.id !== 'set-tab-' + target);
      });
    });
  });
}

// ============================================================
// SECTION PLC-7: INIT
// ============================================================

document.addEventListener('DOMContentLoaded', () => {
  initModalTabs();

  // Al abrir el modal, sincronizar formulario con cfg guardada
  document.getElementById('btn-settings').addEventListener('click', () => {
    syncPlcFormFromCfg();
  }, true);  // capture: antes del handler principal

  // Cambio de protocolo
  document.getElementById('set-plc-proto').addEventListener('change', e => {
    showPlcFields(e.target.value);
    syncPlcCfgFromForm();
  });

  // Cambio de auth REST
  document.getElementById('set-plc-rest-auth').addEventListener('change', () => {
    syncRestAuthFields();
    syncPlcCfgFromForm();
  });

  // Auto-guardar al cambiar cualquier campo del panel PLC
  document.getElementById('set-tab-plc').querySelectorAll('input, select').forEach(el => {
    el.addEventListener('change', syncPlcCfgFromForm);
  });

  // Botones conectar / desconectar
  document.getElementById('set-plc-connect').addEventListener('click', () => {
    syncPlcCfgFromForm();
    PlcConnector.connect();
  });
  document.getElementById('set-plc-disconnect').addEventListener('click', () => {
    PlcConnector.disconnect();
  });

  // Estado inicial
  showPlcFields(plcCfg.proto);
  updatePlcStatusUI();
});
