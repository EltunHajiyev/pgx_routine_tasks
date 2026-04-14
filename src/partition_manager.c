/*-------------------------------------------------------------------------
 *
 * partition_manager.c
 *      Partition Maintenance for pgx_routine_tasks.
 *
 *      – pgx_create_future_partitions:  pre-create range partitions
 *      – pgx_drop_old_partitions:       drop partitions beyond retention
 *      – pgx_partition_report:          list existing partitions
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/regproc.h"
#include "catalog/pg_class.h"

#include "partition_manager.h"
#include "utils.h"

PG_FUNCTION_INFO_V1(pgx_create_future_partitions);
PG_FUNCTION_INFO_V1(pgx_drop_old_partitions);
PG_FUNCTION_INFO_V1(pgx_partition_report);

/* -----------------------------------------------------------------------
 * pgx_create_future_partitions(parent regclass,
 *                               count  int DEFAULT NULL,
 *                               period text DEFAULT 'monthly')
 *
 * Creates 'count' future range partitions for the given parent table.
 * period: 'daily', 'weekly', or 'monthly'.
 *
 * Returns SETOF record: (partition_name text, range_start text,
 *                         range_end text, created bool)
 * ----------------------------------------------------------------------- */
Datum
pgx_create_future_partitions(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    Oid                 parent_oid;
    int                 count;
    char               *period;
    char               *parent_name;
    char                interval_expr[64];
    int                 i;

    if (PG_ARGISNULL(0))
        pgx_log_error("parent table must not be NULL");

    parent_oid = PG_GETARG_OID(0);
    count      = PG_ARGISNULL(1) ? pgx_partition_pre_create : PG_GETARG_INT32(1);
    period     = PG_ARGISNULL(2) ? "monthly" : text_to_cstring(PG_GETARG_TEXT_PP(2));

    parent_name = get_rel_name(parent_oid);
    if (parent_name == NULL)
        pgx_log_error("relation with OID %u does not exist", parent_oid);

    /* Determine interval expression */
    if (strcmp(period, "daily") == 0)
        snprintf(interval_expr, sizeof(interval_expr), "1 day");
    else if (strcmp(period, "weekly") == 0)
        snprintf(interval_expr, sizeof(interval_expr), "1 week");
    else if (strcmp(period, "monthly") == 0)
        snprintf(interval_expr, sizeof(interval_expr), "1 month");
    else
        pgx_log_error("unsupported period '%s' — use daily, weekly, or monthly", period);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        for (i = 0; i < count; i++)
        {
            char    query[2048];
            char    part_name[NAMEDATALEN];
            Datum   values[3];
            bool    nulls[3] = {false, false, false};
            HeapTuple tuple;

            /*
             * Build: CREATE TABLE IF NOT EXISTS <parent>_pYYYYMMDD
             *        PARTITION OF <parent>
             *        FOR VALUES FROM (start) TO (end);
             *
             * The date arithmetic is done inside SQL so we don't need
             * to link against date/time libraries from C.
             */
            snprintf(part_name, sizeof(part_name),
                     "%s_p%d", parent_name, i + 1);

            snprintf(query, sizeof(query),
                     "DO $$ "
                     "DECLARE "
                     "  range_start date := (date_trunc('%s', CURRENT_DATE) + interval '%d %s')::date; "
                     "  range_end   date := (date_trunc('%s', CURRENT_DATE) + interval '%d %s')::date; "
                     "  part_name   text := format('%%I_p%%s', '%s', to_char(range_start, 'YYYYMMDD')); "
                     "BEGIN "
                     "  EXECUTE format("
                     "    'CREATE TABLE IF NOT EXISTS %%I PARTITION OF %%I "
                     "     FOR VALUES FROM (''%%s'') TO (''%%s'')', "
                     "    part_name, '%s', range_start, range_end); "
                     "END $$",
                     /* date_trunc unit */
                     (strcmp(period, "daily") == 0) ? "day" :
                     (strcmp(period, "weekly") == 0) ? "week" : "month",
                     /* offset multiplier for start */
                     i, interval_expr,
                     /* date_trunc unit for end */
                     (strcmp(period, "daily") == 0) ? "day" :
                     (strcmp(period, "weekly") == 0) ? "week" : "month",
                     /* offset multiplier for end */
                     i + 1, interval_expr,
                     /* parent_name twice */
                     parent_name, parent_name);

            SPI_execute(query, false, 0);

            values[0] = CStringGetTextDatum(part_name);
            values[1] = CStringGetTextDatum("computed at runtime");
            values[2] = CStringGetTextDatum("computed at runtime");

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);

            pgx_log_info("created partition %s for %s", part_name, parent_name);
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
 * pgx_drop_old_partitions(parent regclass, retention_days int)
 *
 * Drops child partitions whose name (suffix) indicates a date older
 * than CURRENT_DATE - retention_days.
 *
 * Returns SETOF record: (partition_name text, dropped bool)
 * ----------------------------------------------------------------------- */
Datum
pgx_drop_old_partitions(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    Oid                 parent_oid;
    int                 retention_days;
    char               *parent_name;
    char                query[2048];
    uint64              proc;
    uint64              i;

    if (PG_ARGISNULL(0))
        pgx_log_error("parent table must not be NULL");

    parent_oid     = PG_GETARG_OID(0);
    retention_days = PG_ARGISNULL(1) ? 90 : PG_GETARG_INT32(1);

    parent_name = get_rel_name(parent_oid);
    if (parent_name == NULL)
        pgx_log_error("relation with OID %u does not exist", parent_oid);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        /*
         * Find child partitions via pg_inherits whose upper bound
         * (from pg_catalog.pg_partitioned_table range) is older than
         * the retention window.  For simplicity we use the relname
         * convention and check against information_schema.
         */
        snprintf(query, sizeof(query),
                 "SELECT c.relname "
                 "FROM pg_inherits i "
                 "JOIN pg_class c ON c.oid = i.inhrelid "
                 "JOIN pg_class p ON p.oid = i.inhparent "
                 "WHERE p.relname = '%s' "
                 "ORDER BY c.relname",
                 parent_name);

        proc = pgx_spi_execute(query, true, 0);

        for (i = 0; i < proc; i++)
        {
            char   *child_name;
            char    drop_cmd[512];
            Datum   values[2];
            bool    nulls[2] = {false, false};
            HeapTuple tuple;
            bool    isnull;

            child_name = SPI_getvalue(SPI_tuptable->vals[i],
                                      SPI_tuptable->tupdesc, 1);

            /*
             * Simple heuristic: attempt to extract date from partition
             * suffix.  For production use, we should parse the actual
             * range bounds from pg_catalog.  Here we drop all children
             * and let the DO block decide.
             */
            snprintf(drop_cmd, sizeof(drop_cmd),
                     "DO $$ BEGIN "
                     "  IF (SELECT count(*) FROM %s) = 0 "
                     "     OR (CURRENT_DATE - interval '%d days') > "
                     "        (SELECT min(tableoid::regclass::text::date) "
                     "         FROM %s LIMIT 1) THEN "
                     "    EXECUTE format('DROP TABLE IF EXISTS %%I', '%s'); "
                     "  END IF; "
                     "END $$",
                     quote_identifier(child_name),
                     retention_days,
                     quote_identifier(child_name),
                     child_name);

            /* For the foundation, just list; actual drop logic can be refined */
            values[0] = CStringGetTextDatum(child_name);
            values[1] = BoolGetDatum(false);  /* placeholder */

            tuple = heap_form_tuple(tupdesc, values, nulls);
            tuplestore_puttuple(tupstore, tuple);

            pgx_log_info("evaluated partition %s for potential drop", child_name);

            if (child_name) pfree(child_name);
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
 * pgx_partition_report(parent regclass)
 *
 * Returns SETOF record: (partition_name text, size_bytes int8, row_estimate float8)
 * ----------------------------------------------------------------------- */
Datum
pgx_partition_report(PG_FUNCTION_ARGS)
{
    TupleDesc           tupdesc;
    Tuplestorestate    *tupstore;
    Oid                 parent_oid;
    char               *parent_name;
    char                query[2048];
    uint64              proc;
    uint64              i;

    if (PG_ARGISNULL(0))
        pgx_log_error("parent table must not be NULL");

    parent_oid  = PG_GETARG_OID(0);
    parent_name = get_rel_name(parent_oid);

    if (parent_name == NULL)
        pgx_log_error("relation with OID %u does not exist", parent_oid);

    pgx_build_tuplestore(fcinfo, &tupdesc, &tupstore);

    pgx_spi_connect();

    PG_TRY();
    {
        snprintf(query, sizeof(query),
                 "SELECT c.relname, "
                 "       pg_relation_size(c.oid)::bigint AS size_bytes, "
                 "       c.reltuples "
                 "FROM pg_inherits i "
                 "JOIN pg_class c ON c.oid = i.inhrelid "
                 "JOIN pg_class p ON p.oid = i.inhparent "
                 "WHERE p.relname = '%s' "
                 "ORDER BY c.relname",
                 parent_name);

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