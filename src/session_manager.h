/*-------------------------------------------------------------------------
 *
 * session_manager.h
 *      Intelligent session management: detect and terminate idle or
 *      long-running sessions.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGX_SESSION_MANAGER_H
#define PGX_SESSION_MANAGER_H

#include "postgres.h"
#include "fmgr.h"

/* SQL-callable functions */
extern Datum pgx_terminate_idle_sessions(PG_FUNCTION_ARGS);
extern Datum pgx_terminate_long_running(PG_FUNCTION_ARGS);
extern Datum pgx_session_report(PG_FUNCTION_ARGS);

#endif /* PGX_SESSION_MANAGER_H */