/*-------------------------------------------------------------------------
 *
 * vacuum_manager.c
 *      Smart Vacuum/Analyze for pgx_routine_tasks.
 *
 *      Identifies tables whose dead tuple ratio or modification ratio
 *      exceeds configurable thresholds and runs targeted VACUUM or
 *      ANALYZE on them.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "executor/spi.h"
#include "utils/builtins.h"

#include "vacuum_manager.h"
#include "utils.h"

PG_FUNCTION_INFO_V1(pgx_smart_vacuum);
PG_FUNCTION_INFO_V1(pgx_smart_analyze);
PG_FUNCTION_INFO_V1(pgx_maintenance_report);

/* -----------------------------------------------------------------------
 * pgx_smart_vacuum(bloat_pct float8 DEFAULT NULL,
 *                   mod_pct   float8 DEFAULT NULL)
 *
 * Finds tables where (n_dead_tup / NULLIF(n_live_tup + n_dead_tup, 0))
 * exceeds bloat_pct OR where modification ratio exceeds mod_pct.
 * Runs VACUUM on each qualifying table.
 *
 * Returns SETOF record: (schema text, table_name text, n_dead_tup int8,
 *                         dead_pct float8, action text)
 * ----------------------------------------------------------------------- */
Datum
pgx_smart_vacuum(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    double              bloat_pct;
    double              mod_pct;
    uint64              proc;
    uint64              i;
    char                query[2048];

    bloat_pct = PG_ARGISNULL(0) ? pgx_vacuum_bloat_threshold : PG_GETARG_FLOAT8(0);
    mod_pct   = PG_ARGISNULL(1) ? pgx_vacuum_mod_threshold   : PG_GETARG_FLOAT8(1);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        snprintf(query, sizeof(query),
                 "SELECT schemaname, relname, n_dead_tup, "
                 "       ROUND(100.0 * n_dead_tup / "
                 "             NULLIF(n_live_tup + n_dead_tup, 0), 2) AS dead_pct, "
                 "       ROUND(100.0 * (n_tup_ins + n_tup_upd + n_tup_del) / "
                 "             NULLIF(n_live_tup, 0), 2) AS mod_pct "
                 "FROM pg_stat_user_tables "
                 "WHERE (100.0 * n_dead_tup / "
                 "       NULLIF(n_live_tup + n_dead_tup, 0)) > %f "
                 "   OR (100.0 * (n_tup_ins + n_tup_upd + n_tup_del) / "
                 "       NULLIF(n_live_tup, 0)) > %f "
                 "ORDER BY n_dead_tup DESC",
                 bloat_pct, mod_pct);

        proc = pgx_spi_execute(query, true, 0);

        for (i = 0; i < proc; i++)
        {
            Datum       values[5];
            bool        nulls[5] = {false, false, false, false, false};
            HeapTuple   tuple;
            char       *schema;
            char       *relname;
            char        vacuum_cmd[512];

            schema  = SPI_getvalue(SPI_tuptable->vals[i],
                                   SPI_tuptable->tupdesc, 1);
            relname = SPI_getvalue(SPI_tuptable->vals[i],
                                   SPI_tuptable->tupdesc, 2);

            pgx_log_info("smart_vacuum: vacuuming %s.%s", schema, relname);

            snprintf(vacuum_cmd, sizeof(vacuum_cmd),
                     "VACUUM VERBOSE %s.%s",
                     quote_identifier(schema),
                     quote_identifier(relname));

            SPI_execute(vacuum_cmd, false, 0);

            values[0] = CStringGetTextDatum(schema);
            values[1] = CStringGetTextDatum(relname);
            values[2] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 3, &nulls[2]);
            values[3] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 4, &nulls[3]);
            values[4] = CStringGetTextDatum("VACUUM");

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);

            if (schema)  pfree(schema);
            if (relname) pfree(relname);
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
 * pgx_smart_analyze(mod_pct float8 DEFAULT NULL)
 *
 * Runs ANALYZE on tables where modification ratio exceeds mod_pct.
 *
 * Returns SETOF record: (schema text, table_name text, mod_pct float8,
 *                         action text)
 * ----------------------------------------------------------------------- */
Datum
pgx_smart_analyze(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    double              mod_pct;
    uint64              proc;
    uint64              i;
    char                query[2048];

    mod_pct = PG_ARGISNULL(0) ? pgx_vacuum_mod_threshold : PG_GETARG_FLOAT8(0);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        snprintf(query, sizeof(query),
                 "SELECT schemaname, relname, "
                 "       ROUND(100.0 * (n_tup_ins + n_tup_upd + n_tup_del) / "
                 "             NULLIF(n_live_tup, 0), 2) AS mod_pct "
                 "FROM pg_stat_user_tables "
                 "WHERE (100.0 * (n_tup_ins + n_tup_upd + n_tup_del) / "
                 "       NULLIF(n_live_tup, 0)) > %f "
                 "ORDER BY mod_pct DESC",
                 mod_pct);

        proc = pgx_spi_execute(query, true, 0);

        for (i = 0; i < proc; i++)
        {
            Datum       values[4];
            bool        nulls[4] = {false, false, false, false};
            HeapTuple   tuple;
            char       *schema;
            char       *relname;
            char        analyze_cmd[512];

            schema  = SPI_getvalue(SPI_tuptable->vals[i],
                                   SPI_tuptable->tupdesc, 1);
            relname = SPI_getvalue(SPI_tuptable->vals[i],
                                   SPI_tuptable->tupdesc, 2);

            pgx_log_info("smart_analyze: analyzing %s.%s", schema, relname);

            snprintf(analyze_cmd, sizeof(analyze_cmd),
                     "ANALYZE VERBOSE %s.%s",
                     quote_identifier(schema),
                     quote_identifier(relname));

            SPI_execute(analyze_cmd, false, 0);

            values[0] = CStringGetTextDatum(schema);
            values[1] = CStringGetTextDatum(relname);
            values[2] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 3, &nulls[2]);
            values[3] = CStringGetTextDatum("ANALYZE");

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);

            if (schema)  pfree(schema);
            if (relname) pfree(relname);
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
 * pgx_maintenance_report
 *
 * Returns SETOF record: (schema text, table_name text, last_vacuum timestamptz,
 *   last_analyze timestamptz, n_dead_tup int8, dead_pct float8, mod_pct float8)
 * ----------------------------------------------------------------------- */
Datum
pgx_maintenance_report(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    uint64              proc;
    uint64              i;

    const char *query =
        "SELECT schemaname, relname, "
        "       last_vacuum, last_analyze, "
        "       n_dead_tup, "
        "       ROUND(100.0 * n_dead_tup / "
        "             NULLIF(n_live_tup + n_dead_tup, 0), 2) AS dead_pct, "
        "       ROUND(100.0 * (n_tup_ins + n_tup_upd + n_tup_del) / "
        "             NULLIF(n_live_tup, 0), 2) AS mod_pct "
        "FROM pg_stat_user_tables "
        "ORDER BY n_dead_tup DESC";

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        proc = pgx_spi_execute(query, true, 0);

        for (i = 0; i < proc; i++)
        {
            Datum       values[7];
            bool        nulls[7] = {false, false, false, false,
                                     false, false, false};
            HeapTuple   tuple;
            int         j;

            for (j = 0; j < 7; j++)
                values[j] = SPI_getbinval(SPI_tuptable->vals[i],
                                          SPI_tuptable->tupdesc,
                                          j + 1, &nulls[j]);

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