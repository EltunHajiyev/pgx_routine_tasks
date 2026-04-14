-- Test: pgx_session_report returns expected columns
SELECT * FROM pgx_session_report();

-- Test: pgx_terminate_idle_sessions with a very high threshold (no sessions killed)
SELECT * FROM pgx_terminate_idle_sessions(99999);

-- Test: pgx_terminate_long_running with a very high threshold (no sessions killed)
SELECT * FROM pgx_terminate_long_running(99999);

-- Test: convenience view
SELECT * FROM pgx_session_overview;