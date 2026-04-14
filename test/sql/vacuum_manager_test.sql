-- Test: pgx_maintenance_report returns expected columns
SELECT * FROM pgx_maintenance_report();

-- Test: pgx_smart_vacuum with very high thresholds (expect empty or no-op)
SELECT * FROM pgx_smart_vacuum(99.0, 99.0);

-- Test: pgx_smart_analyze with very high threshold (expect empty or no-op)
SELECT * FROM pgx_smart_analyze(99.0);

-- Test: convenience view
SELECT * FROM pgx_maintenance_overview;