/**
 * @file cycleStats.cpp
 * @author Diego Arcos Sapena
 * @brief Definition of the global CycleStats instances declared in tasks.hpp.
 * @version a-1.0.0
 * @date 2026/03/02
 *
 * @copyright Copyright (c) Roig Borrell S.L. and Ibercomp S.L.
 */

#include "tasks.hpp"

// One stats object per task type. All fields are zero-initialised by the
// atomic<> constructors, and min_us is initialised to LLONG_MAX inside
// the struct definition.
CycleStats g_stats_hw_internal;
ExternalModuleStats g_stats_hw_external;
CycleStats g_stats_database;
