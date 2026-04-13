/*-------------------------------------------------------------------------
 *
 * partition_manager.h
 *      Automated partition creation and cleanup.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGX_PARTITION_MANAGER_H
#define PGX_PARTITION_MANAGER_H

#include "postgres.h"
#include "fmgr.h"

extern Datum pgx_create_future_partitions(PG_FUNCTION_ARGS);
extern Datum pgx_drop_old_partitions(PG_FUNCTION_ARGS);
extern Datum pgx_partition_report(PG_FUNCTION_ARGS);

#endif /* PGX_PARTITION_MANAGER_H */