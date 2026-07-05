/* ============================================================
   osodb_tags.h — osodb tag I/O bridge for the osoST runtime HAL.

   Implements the tag_read(#30) / tag_write(#31) traps against osodb
   (the tag hub, backed by MariaDB), with per-binding ACL (in/out/inout).
   Shared by the HAL back-ends (hardware_linux.c, hardware_demo.c…).

   Bindings — loaded from OSODB_BINDINGS (default "osodb_bindings.cfg"),
   one per line, matching the manifest the Ladder→ST compiler emits:
       <index> <tag_id> <mode>          e.g.  0 hass.switch.start in
   mode ∈ { in, out, inout }.

   Backend — with -DUSE_OSODB (needs libcurl) it talks to the osodb REST
   API at OSODB_URL (default http://127.0.0.1:8080): GET/PUT /var/<tag>.
   Without it, a reference in-memory store is used that STILL enforces the ACL,
   so logic can be exercised on the demo build.

   Note: osodb (core/osodb) is the in-memory cache and owns the write-through/
   write-back sync to MariaDB (the source of truth) via its MariaDB adapter — so
   this bridge only needs to reach osodb, not MariaDB directly. The native path is
   core/osodb/bindings/c (the C client); REST is the interim until that lands.
   The ACL here is the PLC program's per-tag I/O contract (in/out/inout).

   Copyright (C) 2026 Roig Borrell S.L. · Ibercomp S.L.
   SPDX-License-Identifier: AGPL-3.0-or-later
   ============================================================ */
#ifndef OSODB_TAGS_H
#define OSODB_TAGS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { char tag[64]; char mode; /* 'i'=in 'o'=out 'b'=inout */ } osodb_binding_t;
static osodb_binding_t osodb_bindings[256];
static int osodb_nbindings = 0;

static void osodb_load_bindings(void) {
    static int loaded = 0;
    if (loaded) return;
    loaded = 1;
    const char *path = getenv("OSODB_BINDINGS");
    if (!path || !*path) path = "osodb_bindings.cfg";
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[HAL] osodb: no bindings file '%s'\n", path); return; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        int idx; char tag[64], mode[16];
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        if (sscanf(line, "%d %63s %15s", &idx, tag, mode) == 3 && idx >= 0 && idx < 256) {
            strncpy(osodb_bindings[idx].tag, tag, sizeof osodb_bindings[idx].tag - 1);
            osodb_bindings[idx].tag[sizeof osodb_bindings[idx].tag - 1] = '\0';
            osodb_bindings[idx].mode = strcmp(mode, "in") == 0 ? 'i'
                                     : strcmp(mode, "out") == 0 ? 'o' : 'b';
            if (idx + 1 > osodb_nbindings) osodb_nbindings = idx + 1;
        }
    }
    fclose(f);
    fprintf(stderr, "[HAL] osodb: loaded %d binding(s)\n", osodb_nbindings);
}

#if defined(USE_OSODB_NATIVE)
/* Native, in-process path: link the C binding over the C++ osodb hub (core/osodb).
   osodb owns the MariaDB sync; ACL is also enforced by osodb's own Access. */
#include "../../../../core/osodb/bindings/c/osodb_c.h"
static int32_t osodb_backend_get(const char *tag) { int64_t v = 0; osodb_c_read(tag, &v); return (int32_t)v; }
static void    osodb_backend_set(const char *tag, int32_t v) { osodb_c_write(tag, (int64_t)v); }
#elif defined(USE_OSODB)
#include <curl/curl.h>
struct osodb_buf { char *p; size_t n; };
static size_t osodb__wr(void *d, size_t s, size_t n, void *u) {
    struct osodb_buf *b = (struct osodb_buf *)u; size_t add = s * n;
    char *np = (char *)realloc(b->p, b->n + add + 1); if (!np) return 0;
    b->p = np; memcpy(b->p + b->n, d, add); b->n += add; b->p[b->n] = '\0'; return add;
}
static const char *osodb_base(void) { const char *u = getenv("OSODB_URL"); return (u && *u) ? u : "http://127.0.0.1:8080"; }
static int32_t osodb_backend_get(const char *tag) {
    CURL *c = curl_easy_init(); if (!c) return 0;
    char url[320]; snprintf(url, sizeof url, "%s/var/%s", osodb_base(), tag);
    struct osodb_buf b = { NULL, 0 }; double val = 0;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, osodb__wr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 3L);
    if (curl_easy_perform(c) == CURLE_OK && b.p) {
        char *k = strstr(b.p, "\"value\"");
        if (k) sscanf(k, "\"value\"%*[: ]%lf", &val);
    }
    free(b.p); curl_easy_cleanup(c);
    return (int32_t)val;
}
static void osodb_backend_set(const char *tag, int32_t v) {
    CURL *c = curl_easy_init(); if (!c) return;
    char url[320], body[64];
    snprintf(url, sizeof url, "%s/var/%s", osodb_base(), tag);
    snprintf(body, sizeof body, "{\"value\": %d}", (int)v);
    struct curl_slist *h = curl_slist_append(NULL, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 3L);
    curl_easy_perform(c);
    curl_slist_free_all(h); curl_easy_cleanup(c);
}
#else
/* Reference in-memory store (no network). ACL is still enforced above it. */
static struct { char tag[64]; int32_t val; } osodb_store[256];
static int osodb_nstore = 0;
static int32_t osodb_backend_get(const char *tag) {
    for (int i = 0; i < osodb_nstore; i++) if (!strcmp(osodb_store[i].tag, tag)) return osodb_store[i].val;
    return 0;
}
static void osodb_backend_set(const char *tag, int32_t v) {
    for (int i = 0; i < osodb_nstore; i++) if (!strcmp(osodb_store[i].tag, tag)) { osodb_store[i].val = v; return; }
    if (osodb_nstore < 256) { strncpy(osodb_store[osodb_nstore].tag, tag, 63); osodb_store[osodb_nstore].tag[63] = '\0'; osodb_store[osodb_nstore].val = v; osodb_nstore++; }
}
#endif /* USE_OSODB */

/* ACL-enforced tag I/O by binding index (used by trap #30/#31). */
static int32_t osodb_tag_read(int32_t id) {
    osodb_load_bindings();
    if (id < 0 || id >= osodb_nbindings || osodb_bindings[id].tag[0] == '\0') {
        fprintf(stderr, "[HAL] tag_read(%d): no binding\n", (int)id);
        return 0;
    }
    if (osodb_bindings[id].mode == 'o') {   /* out-only → reads denied */
        fprintf(stderr, "[HAL] ACL: tag_read(%d '%s') denied — write-only binding\n", (int)id, osodb_bindings[id].tag);
        return 0;
    }
    return osodb_backend_get(osodb_bindings[id].tag);
}
static void osodb_tag_write(int32_t id, int32_t v) {
    osodb_load_bindings();
    if (id < 0 || id >= osodb_nbindings || osodb_bindings[id].tag[0] == '\0') {
        fprintf(stderr, "[HAL] tag_write(%d): no binding\n", (int)id);
        return;
    }
    if (osodb_bindings[id].mode == 'i') {   /* in-only → writes denied */
        fprintf(stderr, "[HAL] ACL: tag_write(%d '%s') denied — read-only binding\n", (int)id, osodb_bindings[id].tag);
        return;
    }
    osodb_backend_set(osodb_bindings[id].tag, v);
}

#endif /* OSODB_TAGS_H */
