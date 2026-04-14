/*-------------------------------------------------------------------------
 *
 * bloat_manager.c
 *      Automated Bloat Control for pgx_routine_tasks.
 *
 *      Uses heuristic bloat estimation based on pg_class and
 *      pg_stat_user_indexes to detect bloated indexes and optionally
 *      rebuild them with REINDEX CONCURRENTLY.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "executor/spi.h"
#include "utils/builtins.h"

#include "bloat_manager.h"
#include "utils.h"

PG_FUNCTION_INFO_V1(pgx_detect_bloated_indexes);
PG_FUNCTION_INFO_V1(pgx_rebuild_bloated_indexes);
PG_FUNCTION_INFO_V1(pgx_bloat_report);

/*
 * Bloat estimation query — heuristic based on index size vs.
 * estimated number of live tuples.  Accurate bloat estimation usually
 * requires pgstattuple; this lightweight query gives a useful proxy
 * without the overhead.
 */
static const char *bloat_detect_query =
    "SELECT schemaname, indexrelname, relname, "
    "       pg_relation_size(i.indexrelid) AS index_size, "
    "       CASE WHEN idx_scan = 0 THEN 100.0 "
    "            ELSE ROUND(100.0 * "
    "                 (1.0 - (pg_relation_size(i.indexrelid)::float / "
    "                  NULLIF(pg_relation_size(i.relid), 0))), 2) "
    "       END AS estimated_bloat_pct "
    "FROM pg_stat_user_indexes i "
    "JOIN pg_class c ON c.oid = i.indexrelid "
    "WHERE pg_relation_size(i.indexrelid) > 8192 "  /* skip tiny indexes */
    "ORDER BY estimated_bloat_pct DESC";

/* -----------------------------------------------------------------------
 * pgx_detect_bloated_indexes(threshold_pct float8 DEFAULT NULL)
 *
 * Returns SETOF record: (schema text, index_name text, table_name text,
 *                         index_size_bytes int8, estimated_bloat_pct float8)
 * ----------------------------------------------------------------------- */
Datum
pgx_detect_bloated_indexes(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    double              threshold;
    uint64              proc;
    uint64              i;

    threshold = PG_ARGISNULL(0) ? pgx_bloat_threshold_pct : PG_GETARG_FLOAT8(0);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        proc = pgx_spi_execute(bloat_detect_query, true, 0);

        for (i = 0; i < proc; i++)
        {
            Datum   values[5];
            bool    nulls[5] = {false, false, false, false, false};
            HeapTuple tuple;
            bool    isnull;
            double  bloat_pct;

            values[4] = SPI_getbinval(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 5, &isnull);
            if (isnull)
                continue;

            bloat_pct = DatumGetFloat8(values[4]);
            if (bloat_pct < threshold)
                continue;

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
 * pgx_rebuild_bloated_indexes(threshold_pct float8, concurrent bool)
 *
 * Detects indexes above the bloat threshold and rebuilds them.
 * When concurrent = true (default), uses REINDEX CONCURRENTLY.
 *
 * Returns SETOF record: (schema text, index_name text, table_name text,
 *                         old_size_bytes int8, action text)
 * ----------------------------------------------------------------------- */
Datum
pgx_rebuild_bloated_indexes(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    double              threshold;
    bool                concurrent;
    uint64              proc;
    uint64              i;
    SPITupleTable      *detect_tuptable;
    TupleDesc           detect_tupdesc;

    threshold  = PG_ARGISNULL(0) ? pgx_bloat_threshold_pct : PG_GETARG_FLOAT8(0);
    concurrent = PG_ARGISNULL(1) ? true : PG_GETARG_BOOL(1);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        proc = pgx_spi_execute(bloat_detect_query, true, 0);

        /* Save detection results before we run more SPI queries */
        detect_tuptable = SPI_tuptable;
        detect_tupdesc  = SPI_tuptable->tupdesc;

        for (i = 0; i < proc; i++)
        {
            Datum   values[5];
            bool    nulls[5] = {false, false, false, false, false};
            bool    isnull;
            double  bloat_pct;
            char   *schema;
            char   *idxname;
            char    reindex_cmd[512];
            HeapTuple tuple;

            /* Check bloat percentage */
            Datum bloat_datum = SPI_getbinval(detect_tuptable->vals[i],
                                              detect_tupdesc, 5, &isnull);
            if (isnull)
                continue;

            bloat_pct = DatumGetFloat8(bloat_datum);
            if (bloat_pct < threshold)
                continue;

            /* Extract schema and index name */
            schema  = SPI_getvalue(detect_tuptable->vals[i], detect_tupdesc, 1);
            idxname = SPI_getvalue(detect_tuptable->vals[i], detect_tupdesc, 2);

            pgx_log_info("rebuilding index %s.%s (bloat %.1f%%)",
                          schema, idxname, bloat_pct);

            /* Build REINDEX command */
            if (concurrent)
                snprintf(reindex_cmd, sizeof(reindex_cmd),
                         "REINDEX INDEX CONCURRENTLY %s.%s",
                         quote_identifier(schema),
                         quote_identifier(idxname));
            else
                snprintf(reindex_cmd, sizeof(reindex_cmd),
                         "REINDEX INDEX %s.%s",
                         quote_identifier(schema),
                         quote_identifier(idxname));

            SPI_execute(reindex_cmd, false, 0);

            /* Build output row */
            values[0] = SPI_getbinval(detect_tuptable->vals[i],
                                      detect_tupdesc, 1, &nulls[0]);
            values[1] = SPI_getbinval(detect_tuptable->vals[i],
                                      detect_tupdesc, 2, &nulls[1]);
            values[2] = SPI_getbinval(detect_tuptable->vals[i],
                                      detect_tupdesc, 3, &nulls[2]);
            values[3] = SPI_getbinval(detect_tuptable->vals[i],
                                      detect_tupdesc, 4, &nulls[3]);
            values[4] = CStringGetTextDatum(concurrent ? "REINDEX CONCURRENTLY" : "REINDEX");
            nulls[4]  = false;

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);

            if (schema)  pfree(schema);
            if (idxname) pfree(idxname);
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
 * pgx_bloat_report
 *
 * Summary: total indexes, bloated count, total wasted bytes (estimate).
 * Returns SETOF record: (total_indexes int, bloated_indexes int,
 *                         estimated_wasted_bytes int8)
 * ----------------------------------------------------------------------- */
Datum
pgx_bloat_report(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    uint64              proc;

    const char *query =
        "SELECT count(*)::int AS total_indexes, "
        "       count(*) FILTER (WHERE pg_relation_size(indexrelid) > 8192 "
        "                         AND idx_scan = 0)::int AS bloated_indexes, "
        "       COALESCE(sum(pg_relation_size(indexrelid)) "
        "                FILTER (WHERE idx_scan = 0), 0)::bigint AS estimated_wasted "
        "FROM pg_stat_user_indexes";

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        proc = pgx_spi_execute(query, true, 0);

        if (proc > 0)
        {
            Datum       values[3];
            bool        nulls[3] = {false, false, false};
            HeapTuple   tuple;

            values[0] = SPI_getbinval(SPI_tuptable->vals[0],
                                      SPI_tuptable->tupdesc, 1, &nulls[0]);
            values[1] = SPI_getbinval(SPI_tuptable->vals[0],
                                      SPI_tuptable->tupdesc, 2, &nulls[1]);
            values[2] = SPI_getbinval(SPI_tuptable->vals[0],
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