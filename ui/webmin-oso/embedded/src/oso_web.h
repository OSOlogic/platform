/*
 * OSOlogic — webmin-oso embedded web server
 * oso_web.h — Thin wrapper interface for mongoose.ws
 *
 * Copyright (C) 2026 Jose Roig Borrell <rrroig@gmail.com>
 *               Roig Borrell S.L. / Ibercomp S.L.
 *
 * Part of the OSOlogic project — https://osologic.com
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once
/**
 * oso_web.h — OSOlogic embedded web server interface
 *
 * Thin wrapper around mongoose.ws that:
 *  - Serves ui/webmin-oso/embedded/html/index.html from flash
 *  - Implements the [CORE] REST endpoints defined in api/openapi/osologic-admin-api.yaml
 *  - Implements the WebSocket protocol defined in api/websocket/protocol.md
 *
 * Usage (from your main loop / RTOS task):
 *
 *   oso_web_init("0.0.0.0", 80);   // call once at boot
 *   for (;;) {
 *     oso_web_poll(10);             // call every 10 ms (or from a task)
 *   }
 *
 * The implementation (oso_web.c) depends on:
 *   - lib/mongoose/mongoose.h   (mongoose.ws — add as git submodule)
 *   - osodb public API          (core/osodb/include/osodb.h)
 *   - osoruntime public API     (core/osoruntime/include/osoruntime.h)
 *
 * Compile-time knobs (define before including or in your CMakeLists):
 *   OSO_WEB_MAX_SUBS    Max simultaneous WS tag subscriptions (default: 32)
 *   OSO_WEB_FRAME_MAX   Max WS frame size in bytes           (default: 512)
 *   OSO_WEB_BATCH_MS    WS batch flush interval in ms        (default: 50)
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Lifecycle ─────────────────────────────────────────── */

/**
 * Initialise the web server and bind to addr:port.
 * Must be called once after network initialisation.
 * @param addr  Bind address, e.g. "0.0.0.0"
 * @param port  TCP port, typically 80
 * @return 0 on success, negative on error
 */
int oso_web_init(const char *addr, uint16_t port);

/**
 * Drive the mongoose event loop. Call repeatedly from main loop or RTOS task.
 * @param timeout_ms  Max time to block waiting for events (0 = non-blocking)
 */
void oso_web_poll(uint32_t timeout_ms);

/**
 * Broadcast a tag update to all WebSocket clients subscribed to that tag.
 * Call this from your scan cycle whenever a tag value changes.
 * Safe to call from interrupt context if OSO_WEB_THREAD_SAFE is defined.
 *
 * @param tag_name  Null-terminated tag name, e.g. "PLC.Q0.0"
 * @param value_json  JSON representation of the value, e.g. "true", "42", "\"hello\""
 * @param ts_us  Timestamp in microseconds since boot
 */
void oso_web_broadcast_tag(const char *tag_name,
                           const char *value_json,
                           uint64_t    ts_us);

/**
 * Broadcast a runtime state change event.
 * Call whenever the scan cycle starts, stops, or faults.
 *
 * @param state  One of: "running", "stopped", "error", "halted"
 * @param cycle_count  Current scan cycle count
 */
void oso_web_broadcast_runtime(const char *state, uint32_t cycle_count);

/* ── Optional callbacks (implement in your app) ─────────── */

/**
 * Called when a client requests a tag write via REST PUT /tags/{id} or WS write.
 * Implement this to forward the write to osodb.
 *
 * @param tag_name  Tag name
 * @param value_json  New value as JSON string
 * @return 0 if write accepted, -1 if rejected (read-only, not found, etc.)
 */
int oso_web_on_tag_write(const char *tag_name, const char *value_json);

/**
 * Called when a client requests runtime start/stop/reset.
 * Implement this to call osoruntime_start() / osoruntime_stop() / osoruntime_reset().
 *
 * @param cmd  "start", "stop", or "reset"
 * @return 0 if accepted, -1 if rejected
 */
int oso_web_on_runtime_cmd(const char *cmd);

#ifdef __cplusplus
}
#endif
