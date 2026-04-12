/* ============================================================
   OSOLadder — osoplc.js
   Módulo de conexión con osoLogic PLC
   Protocolos soportados: DB (MySQL/MariaDB/PgSQL), Redis,
                          REST API/JSON, MQTT (WebSocket)

   Copyright (C) 2026 Jose Roig Borrell, Roig Borrell SL, Ibercomp SL
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
'use strict';

// ============================================================
// SECTION PLC-1: INTERNACIONALIZACIÓN
// ============================================================

const PLC_STRINGS = {
  es: {
    plcSection:           'Conexión osoLogic PLC',
    plcProto:             'Protocolo',
    plcNone:              'Sin conexión',
    plcDB:                'Base de datos (MySQL / MariaDB / PgSQL)',
    plcRedis:             'Redis',
    plcRest:              'REST API / JSON',
    plcMqtt:              'MQTT (WebSocket)',
    plcHost:              'Host',
    plcPort:              'Puerto',
    plcUser:              'Usuario',
    plcPass:              'Contraseña',
    plcDbName:            'Base de datos',
    plcDbType:            'Motor',
    plcTable:             'Tabla de variables',
    plcPrefix:            'Prefijo de clave',
    plcBaseUrl:           'URL base',
    plcAuthType:          'Autenticación',
    plcAuthNone:          'Ninguna',
    plcAuthBearer:        'Bearer token',
    plcAuthBasic:         'Basic (usuario/contraseña)',
    plcToken:             'Token',
    plcPollMs:            'Polling (ms)',
    plcBrokerUrl:         'Broker WebSocket',
    plcClientId:          'Client ID',
    plcTopicPrefix:       'Prefijo de topic',
    plcQos:               'QoS',
    plcConnect:           'Conectar',
    plcDisconnect:        'Desconectar',
    plcStatusDisconnected:'Desconectado',
    plcStatusConnecting:  'Conectando…',
    plcStatusConnected:   'Conectado',
    plcStatusError:       'Error de conexión',
    plcBridgeNote:        'Este protocolo requiere un bridge backend — no es accesible directamente desde el navegador.',
    plcMqttNote:          'El broker MQTT debe soportar WebSocket (puerto típico 9001).',
  },
  en: {
    plcSection:           'osoLogic PLC Connection',
    plcProto:             'Protocol',
    plcNone:              'No connection',
    plcDB:                'Database (MySQL / MariaDB / PgSQL)',
    plcRedis:             'Redis',
    plcRest:              'REST API / JSON',
    plcMqtt:              'MQTT (WebSocket)',
    plcHost:              'Host',
    plcPort:              'Port',
    plcUser:              'User',
    plcPass:              'Password',
    plcDbName:            'Database',
    plcDbType:            'Engine',
    plcTable:             'Variable table',
    plcPrefix:            'Key prefix',
    plcBaseUrl:           'Base URL',
    plcAuthType:          'Authentication',
    plcAuthNone:          'None',
    plcAuthBearer:        'Bearer token',
    plcAuthBasic:         'Basic (user/password)',
    plcToken:             'Token',
    plcPollMs:            'Polling (ms)',
    plcBrokerUrl:         'WebSocket broker',
    plcClientId:          'Client ID',
    plcTopicPrefix:       'Topic prefix',
    plcQos:               'QoS',
    plcConnect:           'Connect',
    plcDisconnect:        'Disconnect',
    plcStatusDisconnected:'Disconnected',
    plcStatusConnecting:  'Connecting…',
    plcStatusConnected:   'Connected',
    plcStatusError:       'Connection error',
    plcBridgeNote:        'This protocol requires a backend bridge — not directly accessible from the browser.',
    plcMqttNote:          'The MQTT broker must support WebSocket (typical port 9001).',
  },
};

function tp(key) {
  const lang = (typeof currentLang !== 'undefined') ? currentLang : 'es';
  return PLC_STRINGS[lang]?.[key] ?? key;
}

// ============================================================
// SECTION PLC-2: CONFIGURACIÓN Y ESTADO
// ============================================================

const PLC_CFG_DEFAULT = {
  proto: 'none',
  db: {
    type:   'mysql',      // mysql | pgsql
    host:   '',
    port:   3306,
    dbname: '',
    table:  'plc_vars',
    user:   '',
    pass:   '',
  },
  redis: {
    host:   '',
    port:   6379,
    pass:   '',
    prefix: 'plc:',
  },
  rest: {
    baseUrl:  '',
    authType: 'none',     // none | bearer | basic
    token:    '',
    user:     '',
    pass:     '',
    pollMs:   500,
  },
  mqtt: {
    brokerUrl:   'ws://localhost:9001',
    clientId:    'osoLadder-' + Math.random().toString(36).slice(2, 6),
    user:        '',
    pass:        '',
    topicPrefix: 'plc/',
    qos:         0,
  },
};

let plcCfg    = JSON.parse(localStorage.getItem('osol_plc_cfg') || 'null')
                || JSON.parse(JSON.stringify(PLC_CFG_DEFAULT));
let plcStatus = 'disconnected';   // disconnected | connecting | connected | error
let plcErrorMsg = '';

function savePlcCfg() {
  localStorage.setItem('osol_plc_cfg', JSON.stringify(plcCfg));
}

// ============================================================
// SECTION PLC-3: CONECTOR
// ============================================================

const PlcConnector = {

  _ws:        null,
  _pollTimer: null,

  // ── Conectar ─────────────────────────────────────────────
  async connect() {
    if (plcStatus === 'connected' || plcStatus === 'connecting') return;
    PlcConnector.disconnect();
    plcStatus   = 'connecting';
    plcErrorMsg = '';
    updatePlcStatusUI();

    try {
      switch (plcCfg.proto) {
        case 'rest':  await this._connectRest();  break;
        case 'mqtt':  await this._connectMqtt();  break;
        case 'db':
        case 'redis':
          // Requieren bridge backend; registramos config pero no hay socket real
          await this._connectBridge();
          break;
        case 'none':
        default:
          plcStatus = 'disconnected';
          updatePlcStatusUI();
          return;
      }
      plcStatus = 'connected';
    } catch (e) {
      plcStatus   = 'error';
      plcErrorMsg = e.message || String(e);
      console.warn('[PlcConnector]', plcErrorMsg);
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
    plcStatus   = 'disconnected';
    plcErrorMsg = '';
    updatePlcStatusUI();
  },

  // ── API pública ───────────────────────────────────────────

  /** Lee el valor de una variable del PLC. Devuelve Promise<value>. */
  async readVar(name) {
    this._assertConnected();
    switch (plcCfg.proto) {
      case 'rest':  return this._restRead(name);
      case 'mqtt':  return this._mqttRead(name);
      default:      throw new Error('readVar via bridge not implemented');
    }
  },

  /** Escribe un valor en una variable del PLC. Devuelve Promise<void>. */
  async writeVar(name, value) {
    this._assertConnected();
    switch (plcCfg.proto) {
      case 'rest':  return this._restWrite(name, value);
      case 'mqtt':  return this._mqttWrite(name, value);
      default:      throw new Error('writeVar via bridge not implemented');
    }
  },

  _assertConnected() {
    if (plcStatus !== 'connected') throw new Error('PLC not connected');
  },

  // ── REST ──────────────────────────────────────────────────

  async _connectRest() {
    const url  = plcCfg.rest.baseUrl.replace(/\/$/, '') + '/status';
    const resp = await fetch(url, {
      headers: this._restHeaders(),
      signal:  AbortSignal.timeout(4000),
    });
    if (!resp.ok) throw new Error(`HTTP ${resp.status} ${resp.statusText}`);

    if (plcCfg.rest.pollMs > 0) {
      this._pollTimer = setInterval(() => this._restPoll(), plcCfg.rest.pollMs);
    }
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
    const j = await resp.json();
    return j.value;
  },

  async _restWrite(name, value) {
    const url = plcCfg.rest.baseUrl.replace(/\/$/, '') + `/var/${encodeURIComponent(name)}`;
    const resp = await fetch(url, {
      method:  'PUT',
      headers: this._restHeaders(),
      body:    JSON.stringify({ value }),
    });
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
  },

  _restPoll() {
    // TODO: leer variables activas y actualizar el estado del editor en tiempo real
  },

  // ── MQTT (WebSocket) ──────────────────────────────────────

  async _connectMqtt() {
    const cfg = plcCfg.mqtt;
    return new Promise((resolve, reject) => {
      let settled = false;
      const done = (err) => {
        if (settled) return; settled = true;
        err ? reject(err) : resolve();
      };

      const ws = new WebSocket(cfg.brokerUrl);
      ws.binaryType = 'arraybuffer';

      ws.onopen  = () => { this._ws = ws; done(); };
      ws.onerror = ()  => done(new Error('WebSocket error — check broker URL'));
      ws.onclose = ()  => {
        if (plcStatus === 'connected') {
          plcStatus   = 'disconnected';
          plcErrorMsg = 'Connection lost';
          updatePlcStatusUI();
        }
      };
      ws.onmessage = (ev) => this._mqttOnMessage(ev);

      setTimeout(() => done(new Error('Connection timeout')), 6000);
    });
  },

  _mqttOnMessage(_ev) {
    // TODO: parsear mensajes MQTT/WebSocket y actualizar variables
  },

  async _mqttRead(_name) {
    // TODO: subscribe al topic y esperar respuesta
    return null;
  },

  async _mqttWrite(name, value) {
    if (!this._ws || this._ws.readyState !== WebSocket.OPEN)
      throw new Error('WebSocket not open');
    // TODO: empaquetar mensaje MQTT PUBLISH sobre el WebSocket
    // Por ahora enviamos JSON plano (requiere bridge que lo entienda)
    this._ws.send(JSON.stringify({
      topic: plcCfg.mqtt.topicPrefix + name,
      value,
      qos: plcCfg.mqtt.qos,
    }));
  },

  // ── Bridge (DB / Redis) ───────────────────────────────────

  async _connectBridge() {
    // El acceso directo a DB/Redis desde el navegador no es posible.
    // Registramos la configuración para que el bridge backend la consuma.
    // En el futuro: conectar al bridge vía REST o WebSocket.
    // Por ahora lanzamos un error explicativo en lugar de conectar.
    throw new Error(tp('plcBridgeNote'));
  },
};

// ============================================================
// SECTION PLC-4: UI — INDICADOR DE ESTADO EN TOOLBAR
// ============================================================

function updatePlcStatusUI() {
  const dot = document.getElementById('plc-status-dot');
  const lbl = document.getElementById('plc-status-label');
  if (!dot || !lbl) return;

  const labels = {
    disconnected: tp('plcStatusDisconnected'),
    connecting:   tp('plcStatusConnecting'),
    connected:    tp('plcStatusConnected'),
    error:        tp('plcStatusError'),
  };

  dot.className = `plc-dot plc-dot--${plcStatus}`;
  lbl.textContent = labels[plcStatus] ?? plcStatus;
  lbl.title = plcErrorMsg || '';

  // Actualizar también el span dentro del modal si está abierto
  const si = document.getElementById('set-plc-status');
  if (si) {
    si.textContent = labels[plcStatus] ?? plcStatus;
    si.className   = 'set-plc-status';
    if (plcStatus === 'connected')   si.classList.add('plc-ok');
    if (plcStatus === 'error')       si.classList.add('plc-err');
    if (plcStatus === 'connecting')  si.classList.add('plc-info');
    si.title = plcErrorMsg || '';
  }
}

// ============================================================
// SECTION PLC-5: UI — FORMULARIO EN SETTINGS
// ============================================================

function buildPlcSettingsHTML() {
  return `
<div class="set-section">${tp('plcSection')}</div>

<div class="set-row">
  <label>${tp('plcProto')}</label>
  <select id="set-plc-proto">
    <option value="none">${tp('plcNone')}</option>
    <option value="db">${tp('plcDB')}</option>
    <option value="redis">${tp('plcRedis')}</option>
    <option value="rest">${tp('plcRest')}</option>
    <option value="mqtt">${tp('plcMqtt')}</option>
  </select>
</div>

<!-- ── Campos DB ─────────────────────────────────────── -->
<div class="plc-fields" id="plc-fields-db">
  <div class="plc-note plc-note--bridge">${tp('plcBridgeNote')}</div>
  <div class="set-row">
    <label>${tp('plcDbType')}</label>
    <select id="set-plc-db-type">
      <option value="mysql">MySQL / MariaDB</option>
      <option value="pgsql">PostgreSQL</option>
    </select>
  </div>
  <div class="set-row"><label>${tp('plcHost')}</label>
    <input id="set-plc-db-host" type="text" placeholder="localhost" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcPort')}</label>
    <input id="set-plc-db-port" type="number" placeholder="3306" min="1" max="65535"></div>
  <div class="set-row"><label>${tp('plcDbName')}</label>
    <input id="set-plc-db-name" type="text" placeholder="osologic" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcTable')}</label>
    <input id="set-plc-db-table" type="text" placeholder="plc_vars" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcUser')}</label>
    <input id="set-plc-db-user" type="text" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcPass')}</label>
    <input id="set-plc-db-pass" type="password" autocomplete="new-password"></div>
</div>

<!-- ── Campos Redis ───────────────────────────────────── -->
<div class="plc-fields" id="plc-fields-redis">
  <div class="plc-note plc-note--bridge">${tp('plcBridgeNote')}</div>
  <div class="set-row"><label>${tp('plcHost')}</label>
    <input id="set-plc-redis-host" type="text" placeholder="localhost" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcPort')}</label>
    <input id="set-plc-redis-port" type="number" placeholder="6379" min="1" max="65535"></div>
  <div class="set-row"><label>${tp('plcPass')}</label>
    <input id="set-plc-redis-pass" type="password" autocomplete="new-password"></div>
  <div class="set-row"><label>${tp('plcPrefix')}</label>
    <input id="set-plc-redis-prefix" type="text" placeholder="plc:"></div>
</div>

<!-- ── Campos REST ────────────────────────────────────── -->
<div class="plc-fields" id="plc-fields-rest">
  <div class="set-row"><label>${tp('plcBaseUrl')}</label>
    <input id="set-plc-rest-url" type="text" placeholder="http://192.168.1.10:8080" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcAuthType')}</label>
    <select id="set-plc-rest-auth">
      <option value="none">${tp('plcAuthNone')}</option>
      <option value="bearer">${tp('plcAuthBearer')}</option>
      <option value="basic">${tp('plcAuthBasic')}</option>
    </select>
  </div>
  <div class="set-row plc-rest-bearer"><label>${tp('plcToken')}</label>
    <input id="set-plc-rest-token" type="password" autocomplete="new-password"></div>
  <div class="set-row plc-rest-basic"><label>${tp('plcUser')}</label>
    <input id="set-plc-rest-user" type="text" autocomplete="off"></div>
  <div class="set-row plc-rest-basic"><label>${tp('plcPass')}</label>
    <input id="set-plc-rest-pass" type="password" autocomplete="new-password"></div>
  <div class="set-row"><label>${tp('plcPollMs')}</label>
    <input id="set-plc-rest-poll" type="number" min="100" max="60000" placeholder="500"></div>
</div>

<!-- ── Campos MQTT ────────────────────────────────────── -->
<div class="plc-fields" id="plc-fields-mqtt">
  <div class="plc-note plc-note--info">${tp('plcMqttNote')}</div>
  <div class="set-row"><label>${tp('plcBrokerUrl')}</label>
    <input id="set-plc-mqtt-url" type="text" placeholder="ws://192.168.1.10:9001" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcClientId')}</label>
    <input id="set-plc-mqtt-clientid" type="text" placeholder="osoLadder" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcUser')}</label>
    <input id="set-plc-mqtt-user" type="text" autocomplete="off"></div>
  <div class="set-row"><label>${tp('plcPass')}</label>
    <input id="set-plc-mqtt-pass" type="password" autocomplete="new-password"></div>
  <div class="set-row"><label>${tp('plcTopicPrefix')}</label>
    <input id="set-plc-mqtt-prefix" type="text" placeholder="plc/"></div>
  <div class="set-row"><label>${tp('plcQos')}</label>
    <select id="set-plc-mqtt-qos">
      <option value="0">0 — At most once</option>
      <option value="1">1 — At least once</option>
      <option value="2">2 — Exactly once</option>
    </select>
  </div>
</div>

<!-- ── Acciones ───────────────────────────────────────── -->
<div class="set-row set-plc-actions">
  <button class="tb-btn plc-btn-connect"    id="set-plc-connect">${tp('plcConnect')}</button>
  <button class="tb-btn plc-btn-disconnect" id="set-plc-disconnect">${tp('plcDisconnect')}</button>
  <span class="set-plc-status" id="set-plc-status">${tp('plcStatusDisconnected')}</span>
</div>
`;
}

// ── Mostrar/ocultar secciones según protocolo ─────────────────

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

// ── Sync config ↔ formulario ──────────────────────────────────

function syncPlcFormFromCfg() {
  _v('set-plc-proto', plcCfg.proto);
  showPlcFields(plcCfg.proto);

  const db = plcCfg.db;
  _v('set-plc-db-type',  db.type);
  _v('set-plc-db-host',  db.host);
  _v('set-plc-db-port',  db.port);
  _v('set-plc-db-name',  db.dbname);
  _v('set-plc-db-table', db.table);
  _v('set-plc-db-user',  db.user);
  _v('set-plc-db-pass',  db.pass);

  const red = plcCfg.redis;
  _v('set-plc-redis-host',   red.host);
  _v('set-plc-redis-port',   red.port);
  _v('set-plc-redis-pass',   red.pass);
  _v('set-plc-redis-prefix', red.prefix);

  const rest = plcCfg.rest;
  _v('set-plc-rest-url',   rest.baseUrl);
  _v('set-plc-rest-auth',  rest.authType);
  _v('set-plc-rest-token', rest.token);
  _v('set-plc-rest-user',  rest.user);
  _v('set-plc-rest-pass',  rest.pass);
  _v('set-plc-rest-poll',  rest.pollMs);
  syncRestAuthFields();

  const mqtt = plcCfg.mqtt;
  _v('set-plc-mqtt-url',      mqtt.brokerUrl);
  _v('set-plc-mqtt-clientid', mqtt.clientId);
  _v('set-plc-mqtt-user',     mqtt.user);
  _v('set-plc-mqtt-pass',     mqtt.pass);
  _v('set-plc-mqtt-prefix',   mqtt.topicPrefix);
  _v('set-plc-mqtt-qos',      mqtt.qos);

  updatePlcStatusUI();
}

function syncPlcCfgFromForm() {
  plcCfg.proto = _g('set-plc-proto');

  plcCfg.db.type   = _g('set-plc-db-type');
  plcCfg.db.host   = _g('set-plc-db-host');
  plcCfg.db.port   = parseInt(_g('set-plc-db-port'))   || 3306;
  plcCfg.db.dbname = _g('set-plc-db-name');
  plcCfg.db.table  = _g('set-plc-db-table');
  plcCfg.db.user   = _g('set-plc-db-user');
  plcCfg.db.pass   = _g('set-plc-db-pass');

  plcCfg.redis.host   = _g('set-plc-redis-host');
  plcCfg.redis.port   = parseInt(_g('set-plc-redis-port'))  || 6379;
  plcCfg.redis.pass   = _g('set-plc-redis-pass');
  plcCfg.redis.prefix = _g('set-plc-redis-prefix');

  plcCfg.rest.baseUrl  = _g('set-plc-rest-url');
  plcCfg.rest.authType = _g('set-plc-rest-auth');
  plcCfg.rest.token    = _g('set-plc-rest-token');
  plcCfg.rest.user     = _g('set-plc-rest-user');
  plcCfg.rest.pass     = _g('set-plc-rest-pass');
  plcCfg.rest.pollMs   = parseInt(_g('set-plc-rest-poll')) || 500;

  plcCfg.mqtt.brokerUrl   = _g('set-plc-mqtt-url');
  plcCfg.mqtt.clientId    = _g('set-plc-mqtt-clientid');
  plcCfg.mqtt.user        = _g('set-plc-mqtt-user');
  plcCfg.mqtt.pass        = _g('set-plc-mqtt-pass');
  plcCfg.mqtt.topicPrefix = _g('set-plc-mqtt-prefix');
  plcCfg.mqtt.qos         = parseInt(_g('set-plc-mqtt-qos')) || 0;

  savePlcCfg();
}

function _v(id, val) {
  const el = document.getElementById(id);
  if (el && val !== undefined && val !== null) el.value = val;
}
function _g(id) { return document.getElementById(id)?.value ?? ''; }

// ============================================================
// SECTION PLC-6: INIT
// ============================================================

function initPlcSettings() {
  const body = document.querySelector('#settings-modal .modal-body');
  if (!body) return;

  const block = document.createElement('div');
  block.id = 'plc-settings-block';
  block.innerHTML = buildPlcSettingsHTML();
  body.appendChild(block);

  // Protocolo cambia → mostrar campos y guardar
  document.getElementById('set-plc-proto').addEventListener('change', e => {
    showPlcFields(e.target.value);
    syncPlcCfgFromForm();
  });

  // Auth REST cambia → mostrar sub-campos
  document.getElementById('set-plc-rest-auth')?.addEventListener('change', () => {
    syncRestAuthFields();
    syncPlcCfgFromForm();
  });

  // Cualquier input/select del bloque → guardar
  block.querySelectorAll('input, select').forEach(el => {
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

  // Sincronizar formulario al abrir el modal (capture antes del handler principal)
  document.getElementById('btn-settings').addEventListener('click', () => {
    syncPlcFormFromCfg();
  }, true);
}

document.addEventListener('DOMContentLoaded', () => {
  initPlcSettings();
  updatePlcStatusUI();
});
