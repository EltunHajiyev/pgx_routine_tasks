# API Reference — pgx_routine_tasks v0.1.0

## Session Management

### `pgx_terminate_idle_sessions(threshold_seconds integer DEFAULT NULL)`

Terminates sessions that have been in the `idle in transaction` state longer than the specified threshold.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `threshold_seconds` | `integer` | GUC value | Seconds of idle time before termination |

**Returns:** `SETOF pgx_terminated_session`

| Column | Type | Description |
|---|---|---|
| `pid` | `integer` | Process ID of the terminated session |
| `usename` | `text` | Username of the session owner |
| `duration` | `interval` | How long the session was idle |
| `terminated` | `boolean` | Whether termination succeeded |

**Example:**
```sql
SELECT * FROM pgx_terminate_idle_sessions(300);
```

---

### `pgx_terminate_long_running(threshold_seconds integer DEFAULT NULL)`

Terminates active queries running longer than the specified threshold.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `threshold_seconds` | `integer` | GUC value | Max query runtime in seconds |

**Returns:** `SETOF pgx_terminated_session` (same as above)

**Example:**
```sql
SELECT * FROM pgx_terminate_long_running(3600);
```

---

### `pgx_session_report()`

Returns a summary of all current sessions grouped by state.

**Returns:** `SETOF pgx_session_state`

| Column | Type | Description |
|---|---|---|
| `state` | `text` | Session state (active, idle, idle in transaction, etc.) |
| `session_count` | `integer` | Number of sessions in this state |
| `max_duration` | `interval` | Longest session duration in this state |

**Example:**
```sql
SELECT * FROM pgx_session_report();
-- Or use the convenience view:
SELECT * FROM pgx_session_overview;
```

---

## Bloat Control

### `pgx_detect_bloated_indexes(threshold_pct double precision DEFAULT NULL)`

Detects indexes with estimated bloat above the specified percentage.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `threshold_pct` | `double precision` | GUC value | Minimum bloat % to report |

**Returns:** `SETOF pgx_bloated_index`

| Column | Type | Description |
|---|---|---|
| `schema_name` | `text` | Schema containing the index |
| `index_name` | `text` | Name of the bloated index |
| `table_name` | `text` | Table the index belongs to |
| `index_size_bytes` | `bigint` | Current index size in bytes |
| `estimated_bloat_pct` | `double precision` | Estimated bloat percentage |

**Example:**
```sql
SELECT * FROM pgx_detect_bloated_indexes(25.0);
```

---

### `pgx_rebuild_bloated_indexes(threshold_pct double precision DEFAULT NULL, concurrent boolean DEFAULT true)`

Detects and rebuilds bloated indexes. Uses `REINDEX CONCURRENTLY` by default.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `threshold_pct` | `double precision` | GUC value | Minimum bloat % to rebuild |
| `concurrent` | `boolean` | `true` | Use CONCURRENTLY (non-blocking) |

**Returns:** `SETOF pgx_rebuilt_index`

| Column | Type | Description |
|---|---|---|
| `schema_name` | `text` | Schema containing the index |
| `index_name` | `text` | Name of the rebuilt index |
| `table_name` | `text` | Table the index belongs to |
| `old_size_bytes` | `bigint` | Index size before rebuild |
| `action` | `text` | Action taken (REINDEX or REINDEX CONCURRENTLY) |

**Example:**
```sql
SELECT * FROM pgx_rebuild_bloated_indexes(30.0, true);
```

---

### `pgx_bloat_report()`

Returns a summary of index bloat across all user indexes.

**Returns:** `SETOF pgx_bloat_summary`

| Column | Type | Description |
|---|---|---|
| `total_indexes` | `integer` | Total user indexes |
| `bloated_indexes` | `integer` | Indexes with zero scans (possibly unused) |
| `estimated_wasted_bytes` | `bigint` | Total bytes in unused indexes |

**Example:**
```sql
SELECT * FROM pgx_bloat_report();
-- Or: SELECT * FROM pgx_bloat_overview;
```

---

## Partition Maintenance

### `pgx_create_future_partitions(parent_table regclass, part_count integer DEFAULT NULL, period text DEFAULT 'monthly')`

Pre-creates future range partitions for a partitioned table.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `parent_table` | `regclass` | *(required)* | The parent partitioned table |
| `part_count` | `integer` | GUC value | Number of future partitions to create |
| `period` | `text` | `'monthly'` | Partition interval: `daily`, `weekly`, or `monthly` |

**Returns:** `SETOF pgx_partition_info`

| Column | Type | Description |
|---|---|---|
| `partition_name` | `text` | Name of the created partition |
| `range_start` | `text` | Start of the partition range |
| `range_end` | `text` | End of the partition range |

**Example:**
```sql
SELECT * FROM pgx_create_future_partitions('events'::regclass, 6, 'daily');
```

---

### `pgx_drop_old_partitions(parent_table regclass, retention_days integer DEFAULT 90)`

Identifies and drops partitions older than the retention period.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `parent_table` | `regclass` | *(required)* | The parent partitioned table |
| `retention_days` | `integer` | `90` | Drop partitions older than this many days |

**Returns:** `SETOF pgx_partition_drop_result`

| Column | Type | Description |
|---|---|---|
| `partition_name` | `text` | Name of the partition |
| `dropped` | `boolean` | Whether it was actually dropped |

**Example:**
```sql
SELECT * FROM pgx_drop_old_partitions('events'::regclass, 30);
```

---

### `pgx_partition_report(parent_table regclass)`

Lists all child partitions of a parent table with size and row estimates.

**Returns:** `SETOF pgx_partition_detail`

| Column | Type | Description |
|---|---|---|
| `partition_name` | `text` | Partition name |
| `size_bytes` | `bigint` | Partition size in bytes |
| `row_estimate` | `real` | Estimated row count |

**Example:**
```sql
SELECT * FROM pgx_partition_report('events'::regclass);
```

---

## Smart Vacuum/Analyze

### `pgx_smart_vacuum(bloat_pct double precision DEFAULT NULL, mod_pct double precision DEFAULT NULL)`

Runs targeted `VACUUM` on tables exceeding dead-tuple or modification thresholds.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `bloat_pct` | `double precision` | GUC value | Dead-tuple percentage threshold |
| `mod_pct` | `double precision` | GUC value | Modification percentage threshold |

**Returns:** `SETOF pgx_vacuum_result`

| Column | Type | Description |
|---|---|---|
| `schema_name` | `text` | Schema of the vacuumed table |
| `table_name` | `text` | Table name |
| `n_dead_tup` | `bigint` | Dead tuple count |
| `dead_pct` | `double precision` | Dead tuple percentage |
| `action` | `text` | Action performed (`VACUUM`) |

**Example:**
```sql
SELECT * FROM pgx_smart_vacuum(15.0, 5.0);
```

---

### `pgx_smart_analyze(mod_pct double precision DEFAULT NULL)`

Runs targeted `ANALYZE` on tables exceeding the modification threshold.

**Parameters:**
| Name | Type | Default | Description |
|---|---|---|---|
| `mod_pct` | `double precision` | GUC value | Modification percentage threshold |

**Returns:** `SETOF pgx_analyze_result`

| Column | Type | Description |
|---|---|---|
| `schema_name` | `text` | Schema |
| `table_name` | `text` | Table name |
| `mod_pct` | `double precision` | Modification percentage |
| `action` | `text` | Action performed (`ANALYZE`) |

**Example:**
```sql
SELECT * FROM pgx_smart_analyze(10.0);
```

---

### `pgx_maintenance_report()`

Returns a complete maintenance overview of all user tables.

**Returns:** `SETOF pgx_maintenance_info`

| Column | Type | Description |
|---|---|---|
| `schema_name` | `text` | Schema |
| `table_name` | `text` | Table name |
| `last_vacuum` | `timestamptz` | Last vacuum timestamp |
| `last_analyze` | `timestamptz` | Last analyze timestamp |
| `n_dead_tup` | `bigint` | Dead tuple count |
| `dead_pct` | `double precision` | Dead tuple percentage |
| `mod_pct` | `double precision` | Modification percentage |

**Example:**
```sql
SELECT * FROM pgx_maintenance_report();
-- Or: SELECT * FROM pgx_maintenance_overview;
```

---

## Convenience Views

| View | Source Function | Description |
|---|---|---|
| `pgx_session_overview` | `pgx_session_report()` | Current session states |
| `pgx_bloat_overview` | `pgx_bloat_report()` | Index bloat summary |
| `pgx_maintenance_overview` | `pgx_maintenance_report()` | Table maintenance status |