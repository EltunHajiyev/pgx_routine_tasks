/*-------------------------------------------------------------------------
 *
 * vacuum_manager.h
 *      Smart VACUUM/ANALYZE based on custom thresholds.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGX_VACUUM_MANAGER_H
#define PGX_VACUUM_MANAGER_H

#include "postgres.h"
#include "fmgr.h"

extern Datum pgx_smart_vacuum(PG_FUNCTION_ARGS);
extern Datum pgx_smart_analyze(PG_FUNCTION_ARGS);
extern Datum pgx_maintenance_report(PG_FUNCTION_ARGS);

#endif /* PGX_VACUUM_MANAGER_H */