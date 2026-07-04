/* ============================================================
   OSOLogic — shared osodb web client
   A tiny REST bridge to osodb (the in-memory hub) and the DB behind it.
   Used by the HMI engines and the osoAdmin scripting UI so every web tool
   talks to the plant the same, data-centric way: read a tag, write a
   set-point, subscribe to live updates.

   Config persists in localStorage. Reads/writes go by tag key (an osodb
   NodeId/address like "2.5", or a tag name) through the osoLogic REST API.

   (C) 2026 Roig Borrell S.L. · Ibercomp S.L. — AGPL-3.0-or-later
   ============================================================ */
(function (root) {
  'use strict';

  const CFG_KEY = 'osodb_client_cfg';
  const DEFAULTS = { baseUrl: '', authType: 'none', token: '', user: '', pass: '', pollMs: 750 };

  const OsoDB = {
    cfg: Object.assign({}, DEFAULTS, JSON.parse(localStorage.getItem(CFG_KEY) || 'null') || {}),
    live: {},          // last known values, keyed by tag key
    _timer: null,
    _keys: () => [],   // function returning the list of keys to poll

    save() { localStorage.setItem(CFG_KEY, JSON.stringify(this.cfg)); },

    _base() { return (this.cfg.baseUrl || '').replace(/\/$/, ''); },
    _headers() {
      const h = { 'Content-Type': 'application/json' };
      if (this.cfg.authType === 'bearer') h.Authorization = 'Bearer ' + this.cfg.token;
      if (this.cfg.authType === 'basic')  h.Authorization = 'Basic ' + btoa(this.cfg.user + ':' + this.cfg.pass);
      return h;
    },

    /** Read one tag's value. */
    async readVar(key) {
      const r = await fetch(this._base() + '/var/' + encodeURIComponent(key), { headers: this._headers() });
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return (await r.json()).value;
    },

    /** Write a set-point to one tag (guarded by the server's permissions). */
    async writeVar(key, value) {
      const r = await fetch(this._base() + '/var/' + encodeURIComponent(key), {
        method: 'PUT', headers: this._headers(), body: JSON.stringify({ value }),
      });
      if (!r.ok) throw new Error('HTTP ' + r.status);
    },

    /** Poll the given keys; broadcasts `osodb:update` (detail = live map). */
    startPolling(keysFn) {
      this.stop();
      if (keysFn) this._keys = keysFn;
      if (!this.cfg.baseUrl) return;
      const poll = async () => {
        const keys = [...new Set(this._keys().filter(Boolean))];
        await Promise.all(keys.map(async k => {
          try { this.live[k] = await this.readVar(k); } catch (_) { /* keep last */ }
        }));
        document.dispatchEvent(new CustomEvent('osodb:update', { detail: this.live }));
      };
      poll();
      this._timer = setInterval(poll, this.cfg.pollMs || 750);
    },

    stop() { if (this._timer) { clearInterval(this._timer); this._timer = null; } },
  };

  root.OsoDB = OsoDB;
})(typeof window !== 'undefined' ? window : globalThis);
