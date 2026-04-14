-- Setup: create a test partitioned table
CREATE TABLE test_partitioned (
    id      serial,
    ts      date NOT NULL,
    data    text
) PARTITION BY RANGE (ts);

-- Test: pgx_partition_report on empty partitioned table
SELECT * FROM pgx_partition_report('test_partitioned'::regclass);

-- Test: create future partitions
SELECT * FROM pgx_create_future_partitions('test_partitioned'::regclass, 2, 'monthly');

-- Test: report should now show partitions
SELECT * FROM pgx_partition_report('test_partitioned'::regclass);

-- Test: drop old partitions (nothing old yet)
SELECT * FROM pgx_drop_old_partitions('test_partitioned'::regclass, 1);

-- Cleanup
DROP TABLE test_partitioned CASCADE;