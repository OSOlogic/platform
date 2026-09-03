/*
 * Host unit test for osodb-embedded (no RTOS, no hardware).
 *   cc -std=c99 -Wall -Wextra -I../src test_osodb_embedded.c ../src/osodb_embedded.c -o t && ./t
 *
 * (C) 2026 Roig Borrell S.L. and Ibercomp S.L. — AGPL-3.0-or-later
 */
#include "osodb_embedded.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } \
    else         { printf("  ok:   %s\n", msg); } \
} while (0)

int main(void)
{
    osodb_t db;
    osodb_init(&db);

    printf("define + lookup\n");
    int run  = osodb_define(&db, "2.1", OSODB_BOOL, OSODB_RW, 0);
    int spd  = osodb_define(&db, "motor.speed", OSODB_REAL, OSODB_RW, 0);
    int temp = osodb_define(&db, "temp", OSODB_REAL, OSODB_RO, 21.0);
    CHECK(run == 0 && spd == 1 && temp == 2, "three tags defined at 0,1,2");
    CHECK(osodb_count(&db) == 3, "count == 3");
    CHECK(osodb_index(&db, "motor.speed") == 1, "lookup by id");
    CHECK(osodb_index(&db, "nope") == -1, "missing id -> -1");
    CHECK(osodb_define(&db, "2.1", OSODB_BOOL, OSODB_RW, 0) == -1, "duplicate rejected");

    printf("value + set-point (control)\n");
    bool ch = osodb_set_required(&db, spd, 1500.0, 10);
    CHECK(ch, "set-point registered");
    bool has = false;
    double req = osodb_get_required(&db, spd, &has);
    CHECK(has && req == 1500.0, "set-point read back");
    /* a scan loop would apply: value <- required for 'follow' tags */
    osodb_set_value(&db, spd, req, 11);
    CHECK(osodb_get_value(&db, spd) == 1500.0, "value follows set-point");

    printf("change detection\n");
    CHECK(osodb_set_value(&db, temp, 22.5, 20) == true,  "new value -> changed");
    CHECK(osodb_set_value(&db, temp, 22.5, 21) == false, "same value -> not changed");

    printf("federation dirty-tracking\n");
    /* sync everything so far */
    int idx[OSODB_MAX_TAGS];
    uint32_t rev = 0;
    int n = osodb_dirty_since(&db, 0, idx, OSODB_MAX_TAGS, &rev);
    CHECK(n == 3, "all 3 tags dirty from rev 0");
    /* nothing changed since -> zero dirty */
    uint32_t rev2 = rev;
    n = osodb_dirty_since(&db, rev, idx, OSODB_MAX_TAGS, &rev2);
    CHECK(n == 0, "no tags dirty after full sync");
    /* change one -> exactly one dirty, and it's the right one */
    osodb_set_value(&db, run, 1, 30);
    n = osodb_dirty_since(&db, rev, idx, OSODB_MAX_TAGS, &rev2);
    CHECK(n == 1 && idx[0] == run, "one dirty tag after a single change");
    CHECK(rev2 > rev, "revision advanced");

    printf("bounds\n");
    CHECK(osodb_at(&db, -1) == NULL && osodb_at(&db, 99) == NULL, "out-of-range -> NULL");
    CHECK(osodb_set_value(&db, 99, 1, 0) == false, "write out-of-range rejected");

    printf("\n%s (%d failure%s)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED",
           fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
