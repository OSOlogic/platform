/*
 * Federation — sync the MCU's osodb-embedded tags with the network hub
 * (the Linux/x86 "big siblings"). Transport-agnostic: the default backend is a
 * tiny text line-protocol over the console/UART that works on ANY target
 * (including native_sim), and an MQTT backend is compiled in when CONFIG_MQTT
 * is available (ESP32/WiFi, or any IP-capable board).
 *
 *   out (MCU -> hub):  PUB <id> <value> <seq>
 *   in  (hub -> MCU):  SET <id> <value>      -> becomes a set-point (osodb_set_required)
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#ifndef OSODB_FEDERATION_H
#define OSODB_FEDERATION_H
#include "osodb_embedded.h"

/* Publish every tag that changed since the last call. Returns tags published. */
int  federation_publish_dirty(osodb_t *db);

/* Feed one inbound line ("SET <id> <value>"). Returns 0 on applied, <0 on parse error. */
int  federation_handle_line(osodb_t *db, const char *line, uint32_t now_ms);

#endif /* OSODB_FEDERATION_H */
