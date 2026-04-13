/*-------------------------------------------------------------------------
 *
 * utils.h
 *      Shared utility macros, types, and function declarations for
 *      pgx_routine_tasks.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGX_ROUTINE_TASKS_UTILS_H
#define PGX_ROUTINE_TASKS_UTILS_H

#include "postgres.h"
#include "funcapi.h"
#include "executor/spi.h"
#include "utils/builtins.h"

/* ---- Log-level enum (used by the GUC) ---- */
typedef enum PgxLogLevel
{
    PGX_LOG_DEBUG   = 0,
    PGX_LOG_INFO    = 1,
    PGX_LOG_WARNING = 2,
    PGX_LOG_ERROR   = 3
} PgxLogLevel;

/* Extern GUC variables (defined in pgx_routine_tasks.c) */
extern int      pgx_session_idle_timeout;
extern int      pgx_session_max_duration;
extern double   pgx_bloat_threshold_pct;
extern int      pgx_partition_pre_create;
extern double   pgx_vacuum_bloat_threshold;
extern double   pgx_vacuum_mod_threshold;
extern int      pgx_log_level;

/* ---- Logging helpers ---- */
#define pgx_log_debug(fmt, ...)  do { \
    if (pgx_log_level <= PGX_LOG_DEBUG) \
        ereport(DEBUG1, (errmsg("pgx_routine_tasks: " fmt, ##__VA_ARGS__))); \
} while (0)

#define pgx_log_info(fmt, ...)  do { \
    if (pgx_log_level <= PGX_LOG_INFO) \
        ereport(LOG, (errmsg("pgx_routine_tasks: " fmt, ##__VA_ARGS__))); \
} while (0)

#define pgx_log_warning(fmt, ...)  do { \
    if (pgx_log_level <= PGX_LOG_WARNING) \
        ereport(WARNING, (errmsg("pgx_routine_tasks: " fmt, ##__VA_ARGS__))); \
} while (0)

#define pgx_log_error(fmt, ...)  do { \
    ereport(ERROR, (errmsg("pgx_routine_tasks: " fmt, ##__VA_ARGS__))); \
} while (0)

/* ---- SPI helpers ---- */

/*
 * pgx_spi_connect — wrapper around SPI_connect with error handling.
 */
extern void pgx_spi_connect(void);

/*
 * pgx_spi_finish — wrapper around SPI_finish with error handling.
 */
extern void pgx_spi_finish(void);

/*
 * pgx_spi_execute — execute a read-only SPI query and return the result
 *                    count.  The caller must have already called SPI_connect.
 */
extern uint64 pgx_spi_execute(const char *query, bool read_only, int max_rows);

/*
 * pgx_spi_execute_with_args — execute a parameterized SPI query.
 */
extern uint64 pgx_spi_execute_with_args(const char *query,
                                         int nargs,
                                         Oid *argtypes,
                                         Datum *values,
                                         const char *nulls,
                                         bool read_only,
                                         int max_rows);

/*
 * pgx_build_tuplestore — helper to initialise a Tuplestore return context.
 *                         Sets *tupstore and *tupdesc for the caller.
 */
extern void pgx_build_tuplestore(FunctionCallInfo fcinfo,
                                  TupleDesc *tupdesc,
                                  Tuplestorestate **tupstore);

#endif /* PGX_ROUTINE_TASKS_UTILS_H */