-- Test: pgx_bloat_report returns expected columns
SELECT * FROM pgx_bloat_report();

-- Test: pgx_detect_bloated_indexes with 100% threshold (expect empty set)
SELECT * FROM pgx_detect_bloated_indexes(100.0);

-- Test: convenience view
SELECT * FROM pgx_bloat_overview;