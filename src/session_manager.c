/*-------------------------------------------------------------------------
 *
 * session_manager.c
 *      Intelligent Session Management for pgx_routine_tasks.
 *
 *      – pgx_terminate_idle_sessions:  kills idle-in-transaction sessions
 *      – pgx_terminate_long_running:   kills queries exceeding a duration
 *      – pgx_session_report:           overview of current session states
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

#include "session_manager.h"
#include "utils.h"

PG_FUNCTION_INFO_V1(pgx_terminate_idle_sessions);
PG_FUNCTION_INFO_V1(pgx_terminate_long_running);
PG_FUNCTION_INFO_V1(pgx_session_report);

/* -----------------------------------------------------------------------
 * pgx_terminate_idle_sessions
 *
 * Terminates sessions that have been in the 'idle in transaction' state
 * longer than the supplied threshold (interval).  Falls back to the GUC
 * pgx_routine_tasks.session_idle_timeout when the argument is NULL.
 *
 * Returns SETOF record (pid int, usename text, duration interval,
 *                        terminated bool).
 * ----------------------------------------------------------------------- */
Datum
pgx_terminate_idle_sessions(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    int                 threshold_sec;
    char                query[1024];
    uint64              proc;
    uint64              i;

    /* Resolve threshold */
    if (PG_ARGISNULL(0))
        threshold_sec = pgx_session_idle_timeout;
    else
        threshold_sec = PG_GETARG_INT32(0);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        snprintf(query, sizeof(query),
                 "SELECT pid, usename, "
                 "       now() - state_change AS duration, "
                 "       pg_terminate_backend(pid) AS terminated "
                 "FROM pg_stat_activity "
                 "WHERE state = 'idle in transaction' "
                 "  AND now() - state_change > interval '%d seconds' "
                 "  AND pid <> pg_backend_pid()",
                 threshold_sec);

        proc = pgx_spi_execute(query, false, 0);

        pgx_log_info("terminate_idle_sessions: examined %lu sessions (threshold %ds)",
                      (unsigned long) proc, threshold_sec);

        for (i = 0; i < proc; i++)
        {
            Datum       values[4];
            bool        nulls[4] = {false, false, false, false};
            HeapTuple   tuple;

            values[0] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 1, &nulls[0]);
            values[1] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 2, &nulls[1]);
            values[2] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 3, &nulls[2]);
            values[3] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 4, &nulls[3]);

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);
        }
    }
    PG_CATCH();
    {
        pgx_spi_finish();
        PG_RE_THROW();
    }
    PG_END_TRY();

    pgx_spi_finish();

    return (Datum) 0;
}

/* -----------------------------------------------------------------------
 * pgx_terminate_long_running
 *
 * Terminates queries that have been running longer than the supplied
 * threshold.  Falls back to pgx_routine_tasks.session_max_duration.
 * ----------------------------------------------------------------------- */
Datum
pgx_terminate_long_running(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    int                 threshold_sec;
    char                query[1024];
    uint64              proc;
    uint64              i;

    if (PG_ARGISNULL(0))
        threshold_sec = pgx_session_max_duration;
    else
        threshold_sec = PG_GETARG_INT32(0);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        snprintf(query, sizeof(query),
                 "SELECT pid, usename, "
                 "       now() - query_start AS duration, "
                 "       pg_terminate_backend(pid) AS terminated "
                 "FROM pg_stat_activity "
                 "WHERE state = 'active' "
                 "  AND now() - query_start > interval '%d seconds' "
                 "  AND pid <> pg_backend_pid()",
                 threshold_sec);

        proc = pgx_spi_execute(query, false, 0);

        pgx_log_info("terminate_long_running: examined %lu sessions (threshold %ds)",
                      (unsigned long) proc, threshold_sec);

        for (i = 0; i < proc; i++)
        {
            Datum       values[4];
            bool        nulls[4] = {false, false, false, false};
            HeapTuple   tuple;

            values[0] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 1, &nulls[0]);
            values[1] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 2, &nulls[1]);
            values[2] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 3, &nulls[2]);
            values[3] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 4, &nulls[3]);

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);
        }
    }
    PG_CATCH();
    {
        pgx_spi_finish();
        PG_RE_THROW();
    }
    PG_END_TRY();

    pgx_spi_finish();

    return (Datum) 0;
}

/* -----------------------------------------------------------------------
 * pgx_session_report
 *
 * Returns a summary of current sessions grouped by state.
 * Columns: state text, session_count int, max_duration interval
 * ----------------------------------------------------------------------- */
Datum
pgx_session_report(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    uint64              proc;
    uint64              i;

    const char *query =
        "SELECT COALESCE(state, 'unknown') AS state, "
        "       count(*)::int AS session_count, "
        "       max(now() - COALESCE(query_start, backend_start)) AS max_duration "
        "FROM pg_stat_activity "
        "WHERE pid <> pg_backend_pid() "
        "GROUP BY state "
        "ORDER BY session_count DESC";

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        proc = pgx_spi_execute(query, true, 0);

        for (i = 0; i < proc; i++)
        {
            Datum       values[3];
            bool        nulls[3] = {false, false, false};
            HeapTuple   tuple;

            values[0] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 1, &nulls[0]);
            values[1] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 2, &nulls[1]);
            values[2] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 3, &nulls[2]);

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);
        }
    }
    PG_CATCH();
    {
        pgx_spi_finish();
        PG_RE_THROW();
    }
    PG_END_TRY();

    pgx_spi_finish();

    return (Datum) 0;
}