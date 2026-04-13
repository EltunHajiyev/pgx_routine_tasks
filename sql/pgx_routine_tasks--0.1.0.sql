/* pgx_routine_tasks -- 0.1.0 */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgx_routine_tasks" to load this file. \quit

-- ============================================================
-- COMPOSITE TYPES for return values
-- ============================================================

CREATE TYPE pgx_terminated_session AS (
    pid             integer,
    usename         text,
    duration        interval,
    terminated      boolean
);

CREATE TYPE pgx_session_state AS (
    state           text,
    session_count   integer,
    max_duration    interval
);

CREATE TYPE pgx_bloated_index AS (
    schema_name         text,
    index_name          text,
    table_name          text,
    index_size_bytes    bigint,
    estimated_bloat_pct double precision
);

CREATE TYPE pgx_rebuilt_index AS (
    schema_name         text,
    index_name          text,
    table_name          text,
    old_size_bytes      bigint,
    action              text
);

CREATE TYPE pgx_bloat_summary AS (
    total_indexes       integer,
    bloated_indexes     integer,
    estimated_wasted_bytes bigint
);

CREATE TYPE pgx_partition_info AS (
    partition_name  text,
    range_start     text,
    range_end       text
);

CREATE TYPE pgx_partition_drop_result AS (
    partition_name  text,
    dropped         boolean
);

CREATE TYPE pgx_partition_detail AS (
    partition_name  text,
    size_bytes      bigint,
    row_estimate    real
);

CREATE TYPE pgx_vacuum_result AS (
    schema_name text,
    table_name  text,
    n_dead_tup  bigint,
    dead_pct    double precision,
    action      text
);

CREATE TYPE pgx_analyze_result AS (
    schema_name text,
    table_name  text,
    mod_pct     double precision,
    action      text
);

CREATE TYPE pgx_maintenance_info AS (
    schema_name     text,
    table_name      text,
    last_vacuum     timestamptz,
    last_analyze    timestamptz,
    n_dead_tup      bigint,
    dead_pct        double precision,
    mod_pct         double precision
);


-- ============================================================
-- FEATURE 1: Session Management
-- ============================================================

CREATE FUNCTION pgx_terminate_idle_sessions(
    threshold_seconds integer DEFAULT NULL
)
RETURNS SETOF pgx_terminated_session
AS 'MODULE_PATHNAME', 'pgx_terminate_idle_sessions'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_terminate_idle_sessions(integer) IS
'Terminates sessions idle-in-transaction longer than threshold (seconds). '
'Defaults to pgx_routine_tasks.session_idle_timeout GUC.';


CREATE FUNCTION pgx_terminate_long_running(
    threshold_seconds integer DEFAULT NULL
)
RETURNS SETOF pgx_terminated_session
AS 'MODULE_PATHNAME', 'pgx_terminate_long_running'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_terminate_long_running(integer) IS
'Terminates queries running longer than threshold (seconds). '
'Defaults to pgx_routine_tasks.session_max_duration GUC.';


CREATE FUNCTION pgx_session_report()
RETURNS SETOF pgx_session_state
AS 'MODULE_PATHNAME', 'pgx_session_report'
LANGUAGE C STABLE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_session_report() IS
'Returns a summary of current sessions grouped by state.';


-- ============================================================
-- FEATURE 2: Bloat Control
-- ============================================================

CREATE FUNCTION pgx_detect_bloated_indexes(
    threshold_pct double precision DEFAULT NULL
)
RETURNS SETOF pgx_bloated_index
AS 'MODULE_PATHNAME', 'pgx_detect_bloated_indexes'
LANGUAGE C STABLE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_detect_bloated_indexes(double precision) IS
'Detects indexes with estimated bloat above threshold_pct. '
'Defaults to pgx_routine_tasks.bloat_threshold_pct GUC.';


CREATE FUNCTION pgx_rebuild_bloated_indexes(
    threshold_pct double precision DEFAULT NULL,
    concurrent    boolean DEFAULT true
)
RETURNS SETOF pgx_rebuilt_index
AS 'MODULE_PATHNAME', 'pgx_rebuild_bloated_indexes'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_rebuild_bloated_indexes(double precision, boolean) IS
'Detects and rebuilds bloated indexes. Uses REINDEX CONCURRENTLY when concurrent=true.';


CREATE FUNCTION pgx_bloat_report()
RETURNS SETOF pgx_bloat_summary
AS 'MODULE_PATHNAME', 'pgx_bloat_report'
LANGUAGE C STABLE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_bloat_report() IS
'Returns a summary of index bloat across all user indexes.';


-- ============================================================
-- FEATURE 3: Partition Maintenance
-- ============================================================

CREATE FUNCTION pgx_create_future_partitions(
    parent_table regclass,
    part_count   integer DEFAULT NULL,
    period       text DEFAULT 'monthly'
)
RETURNS SETOF pgx_partition_info
AS 'MODULE_PATHNAME', 'pgx_create_future_partitions'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_create_future_partitions(regclass, integer, text) IS
'Pre-creates future range partitions. period: daily, weekly, monthly. '
'Defaults to pgx_routine_tasks.partition_pre_create_count GUC.';


CREATE FUNCTION pgx_drop_old_partitions(
    parent_table    regclass,
    retention_days  integer DEFAULT 90
)
RETURNS SETOF pgx_partition_drop_result
AS 'MODULE_PATHNAME', 'pgx_drop_old_partitions'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_drop_old_partitions(regclass, integer) IS
'Drops partitions older than retention_days for the specified parent table.';


CREATE FUNCTION pgx_partition_report(
    parent_table regclass
)
RETURNS SETOF pgx_partition_detail
AS 'MODULE_PATHNAME', 'pgx_partition_report'
LANGUAGE C STABLE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_partition_report(regclass) IS
'Lists all partitions of a parent table with size and row estimates.';


-- ============================================================
-- FEATURE 4: Smart Vacuum/Analyze
-- ============================================================

CREATE FUNCTION pgx_smart_vacuum(
    bloat_pct double precision DEFAULT NULL,
    mod_pct   double precision DEFAULT NULL
)
RETURNS SETOF pgx_vacuum_result
AS 'MODULE_PATHNAME', 'pgx_smart_vacuum'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_smart_vacuum(double precision, double precision) IS
'Runs targeted VACUUM on tables exceeding dead-tuple or modification thresholds.';


CREATE FUNCTION pgx_smart_analyze(
    mod_pct double precision DEFAULT NULL
)
RETURNS SETOF pgx_analyze_result
AS 'MODULE_PATHNAME', 'pgx_smart_analyze'
LANGUAGE C VOLATILE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_smart_analyze(double precision) IS
'Runs targeted ANALYZE on tables exceeding modification threshold.';


CREATE FUNCTION pgx_maintenance_report()
RETURNS SETOF pgx_maintenance_info
AS 'MODULE_PATHNAME', 'pgx_maintenance_report'
LANGUAGE C STABLE STRICT SECURITY DEFINER;

COMMENT ON FUNCTION pgx_maintenance_report() IS
'Returns a maintenance overview of all user tables: last vacuum/analyze, dead tuples, modification ratio.';


-- ============================================================
-- CONVENIENCE VIEWS
-- ============================================================

CREATE VIEW pgx_session_overview AS
    SELECT * FROM pgx_session_report();

COMMENT ON VIEW pgx_session_overview IS
'Convenience view: current session states summary.';


CREATE VIEW pgx_bloat_overview AS
    SELECT * FROM pgx_bloat_report();

COMMENT ON VIEW pgx_bloat_overview IS
'Convenience view: index bloat summary.';


CREATE VIEW pgx_maintenance_overview AS
    SELECT * FROM pgx_maintenance_report();

COMMENT ON VIEW pgx_maintenance_overview IS
'Convenience view: table maintenance status.';