/*-------------------------------------------------------------------------
 *
 * pgx_routine_tasks.c
 *      Main entry point for the pgx_routine_tasks extension.
 *      Registers GUC parameters and initializes all submodules.
 *
 * Copyright (c) 2026, Valeh Agayev and contributors
 * Licensed under the PostgreSQL License.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "utils/guc.h"

#include "session_manager.h"
#include "bloat_manager.h"
#include "partition_manager.h"
#include "vacuum_manager.h"
#include "utils.h"

PG_MODULE_MAGIC;

/* -------- GUC variables -------- */

/* Session manager */
int     pgx_session_idle_timeout    = 300;    /* seconds */
int     pgx_session_max_duration    = 3600;   /* seconds */

/* Bloat manager */
double  pgx_bloat_threshold_pct     = 30.0;

/* Partition manager */
int     pgx_partition_pre_create    = 3;

/* Vacuum manager */
double  pgx_vacuum_bloat_threshold  = 20.0;
double  pgx_vacuum_mod_threshold    = 10.0;

/* Logging */
int     pgx_log_level               = PGX_LOG_INFO;

/* Enum definition for log level GUC */
static const struct config_enum_entry pgx_log_level_options[] = {
    {"debug",   PGX_LOG_DEBUG,   false},
    {"info",    PGX_LOG_INFO,    false},
    {"warning", PGX_LOG_WARNING, false},
    {"error",   PGX_LOG_ERROR,   false},
    {NULL, 0, false}
};

/*
 * _PG_init
 *      Called when the shared library is loaded.  Registers all custom
 *      GUC (Grand Unified Configuration) parameters used by the extension.
 */
void
_PG_init(void)
{
    /* ---- Session manager GUCs ---- */

    DefineCustomIntVariable(
        "pgx_routine_tasks.session_idle_timeout",
        "Maximum seconds a session may remain idle-in-transaction before termination.",
        NULL,
        &pgx_session_idle_timeout,
        300,        /* default */
        1,          /* min     */
        86400,      /* max     */
        PGC_SUSET,
        GUC_UNIT_S,
        NULL, NULL, NULL);

    DefineCustomIntVariable(
        "pgx_routine_tasks.session_max_duration",
        "Maximum seconds a single query may run before termination.",
        NULL,
        &pgx_session_max_duration,
        3600,
        1,
        604800,
        PGC_SUSET,
        GUC_UNIT_S,
        NULL, NULL, NULL);

    /* ---- Bloat manager GUCs ---- */

    DefineCustomRealVariable(
        "pgx_routine_tasks.bloat_threshold_pct",
        "Index bloat percentage above which an index is considered bloated.",
        NULL,
        &pgx_bloat_threshold_pct,
        30.0,
        0.0,
        100.0,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    /* ---- Partition manager GUCs ---- */

    DefineCustomIntVariable(
        "pgx_routine_tasks.partition_pre_create_count",
        "Number of future partitions to pre-create.",
        NULL,
        &pgx_partition_pre_create,
        3,
        1,
        365,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    /* ---- Vacuum manager GUCs ---- */

    DefineCustomRealVariable(
        "pgx_routine_tasks.vacuum_bloat_threshold_pct",
        "Table bloat percentage above which VACUUM is triggered.",
        NULL,
        &pgx_vacuum_bloat_threshold,
        20.0,
        0.0,
        100.0,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    DefineCustomRealVariable(
        "pgx_routine_tasks.vacuum_mod_threshold_pct",
        "Modification percentage threshold for triggering VACUUM or ANALYZE.",
        NULL,
        &pgx_vacuum_mod_threshold,
        10.0,
        0.0,
        100.0,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    /* ---- Logging GUC ---- */

    DefineCustomEnumVariable(
        "pgx_routine_tasks.log_level",
        "Logging verbosity for pgx_routine_tasks operations.",
        NULL,
        &pgx_log_level,
        PGX_LOG_INFO,
        pgx_log_level_options,
        PGC_SUSET,
        0,
        NULL, NULL, NULL);

    elog(LOG, "pgx_routine_tasks: extension loaded");
}