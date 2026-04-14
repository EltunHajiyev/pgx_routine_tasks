/*-------------------------------------------------------------------------
 *
 * bloat_manager.h
 *      Automated index bloat detection and rebuild.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGX_BLOAT_MANAGER_H
#define PGX_BLOAT_MANAGER_H

#include "postgres.h"
#include "fmgr.h"

extern Datum pgx_detect_bloated_indexes(PG_FUNCTION_ARGS);
extern Datum pgx_rebuild_bloated_indexes(PG_FUNCTION_ARGS);
extern Datum pgx_bloat_report(PG_FUNCTION_ARGS);

#endif /* PGX_BLOAT_MANAGER_H */