/*-------------------------------------------------------------------------
 *
 * utils.c
 *      Shared utility functions for pgx_routine_tasks:
 *        – SPI connect/execute wrappers with error handling
 *        – Tuplestore builder for SRFs
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/spi.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"

#include "utils.h"

/* -----------------------------------------------------------
 * SPI helpers
 * ----------------------------------------------------------- */

void
pgx_spi_connect(void)
{
    if (SPI_connect() != SPI_OK_CONNECT)
        pgx_log_error("failed to connect to SPI");
}

void
pgx_spi_finish(void)
{
    if (SPI_finish() != SPI_OK_FINISH)
        pgx_log_warning("SPI_finish returned unexpected status");
}

uint64
pgx_spi_execute(const char *query, bool read_only, int max_rows)
{
    int ret;

    ret = SPI_execute(query, read_only, max_rows);
    if (ret < 0)
        pgx_log_error("SPI_execute failed (code %d) for query: %s", ret, query);

    return SPI_processed;
}

uint64
pgx_spi_execute_with_args(const char *query,
                            int nargs,
                            Oid *argtypes,
                            Datum *values,
                            const char *nulls,
                            bool read_only,
                            int max_rows)
{
    int ret;

    ret = SPI_execute_with_args(query, nargs, argtypes, values, nulls,
                                read_only, max_rows);
    if (ret < 0)
        pgx_log_error("SPI_execute_with_args failed (code %d) for query: %s",
                       ret, query);

    return SPI_processed;
}

/* -----------------------------------------------------------
 * Tuplestore SRF builder
 * ----------------------------------------------------------- */

void
pgx_build_tuplestore(FunctionCallInfo fcinfo,
                      TupleDesc *tupdesc,
                      Tuplestorestate **tupstore)
{
    ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
    MemoryContext   per_query_ctx;
    MemoryContext   old_ctx;

    /* Verify the caller expects a tuplestore */
    if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("set-valued function called in context that cannot accept a set")));

    if (!(rsinfo->allowedModes & SFRM_Materialize))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("materialize mode required, but it is not allowed in this context")));

    per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
    old_ctx = MemoryContextSwitchTo(per_query_ctx);

    *tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);
    *tupstore = tuplestore_begin_heap(true, false, work_mem);

    rsinfo->returnMode = SFRM_Materialize;
    rsinfo->setResult  = *tupstore;
    rsinfo->setDesc    = *tupdesc;

    MemoryContextSwitchTo(old_ctx);
}