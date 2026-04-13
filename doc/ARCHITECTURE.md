# Architecture — pgx_routine_tasks

## Overview

`pgx_routine_tasks` is a PostgreSQL extension implemented as a C shared library that is loaded into the PostgreSQL backend process. It uses the **Server Programming Interface (SPI)** to execute SQL queries against system catalogs and statistics views to make intelligent decisions about database maintenance.

## Module Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        PostgreSQL Backend                       │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    pgx_routine_tasks.so                   │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │              pgx_routine_tasks.c                    │  │  │
│  │  │          _PG_init() — GUC Registration              │  │  │
│  │  └──────────────────────┬──────────────────────────────┘  │  │
│  │                         │                                 │  │
│  │  ┌──────────┬───────────┼───────────┬──────────────────┐  │  │
│  │  │          │           │           │                  │  │  │
│  │  ▼          ▼           ▼           ▼                  │  │  │
│  │ session_  bloat_    partition_   vacuum_               │  │  │
│  │ manager   manager   manager     manager               │  │  │
│  │  .c/.h     .c/.h     .c/.h       .c/.h               │  │  │
│  │  │          │           │           │                  │  │  │
│  │  └──────────┴───────────┼───────────┘                  │  │  │
│  │                         │                               │  │  │
│  │                    ┌────▼────┐                          │  │  │
│  │                    │ utils.c │                          │  │  │
│  │                    │  SPI    │                          │  │  │
│  │                    │ helpers │                          │  │  │
│  │                    └────┬────┘                          │  │  │
│  └─────────────────────────┼─────────────────────────────────┘  │
│                            │                                     │
│  ┌─────────────────────────▼─────────────────────────────────┐  │
│  │         PostgreSQL Internals (SPI, GUC, Catalog)          │  │
│  │                                                           │  │
│  │  pg_stat_activity · pg_stat_user_indexes ·                │  │
│  │  pg_stat_user_tables · pg_class · pg_inherits             │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Session Manager Flow

```
pgx_terminate_idle_sessions(threshold)
    │
    ├─ SPI: SELECT pid, usename, duration FROM pg_stat_activity
    │       WHERE state = 'idle in transaction'
    │         AND duration > threshold
    │
    ├─ For each row: SPI: SELECT pg_terminate_backend(pid)
    │
    └─ Return: SETOF (pid, usename, duration, terminated)
```

### Bloat Manager Flow

```
pgx_rebuild_bloated_indexes(threshold, concurrent)
    │
    ├─ SPI: Estimate bloat from pg_stat_user_indexes + pg_class
    │
    ├─ Filter: bloat_pct > threshold
    │
    ├─ For each bloated index:
    │   └─ SPI: REINDEX [CONCURRENTLY] INDEX schema.index_name
    │
    └─ Return: SETOF (schema, index_name, table, old_size, action)
```

### Partition Manager Flow

```
pgx_create_future_partitions(parent, count, period)
    │
    ├─ Calculate date ranges for next 'count' periods
    │
    ├─ For each period:
    │   └─ SPI: CREATE TABLE IF NOT EXISTS ... PARTITION OF parent
    │           FOR VALUES FROM (start) TO (end)
    │
    └─ Return: SETOF (partition_name, range_start, range_end)
```

### Vacuum Manager Flow

```
pgx_smart_vacuum(bloat_pct, mod_pct)
    │
    ├─ SPI: SELECT tables FROM pg_stat_user_tables
    │       WHERE dead_pct > bloat_pct OR mod_pct > threshold
    │
    ├─ For each qualifying table:
    │   └─ SPI: VACUUM VERBOSE schema.table
    │
    └─ Return: SETOF (schema, table, n_dead, dead_pct, action)
```

## GUC Parameter System

All configuration parameters are registered in `_PG_init()` using PostgreSQL's `DefineCustom*Variable()` API:

| GUC Function | Parameter Type | Scope |
|---|---|---|
| `DefineCustomIntVariable` | Integer (seconds, counts) | `PGC_SUSET` |
| `DefineCustomRealVariable` | Float (percentages) | `PGC_SUSET` |
| `DefineCustomEnumVariable` | Enum (log levels) | `PGC_SUSET` |

All GUCs use `PGC_SUSET` context — meaning they can be set by superusers at session level and in `postgresql.conf`.

## Error Handling Strategy

1. **SPI Operations**: All SPI calls are wrapped in `PG_TRY/PG_CATCH` blocks to ensure `SPI_finish()` is always called, even on error.

2. **Input Validation**: NULL arguments fall back to GUC defaults. Invalid arguments raise `ERROR` via `ereport()`.

3. **Logging**: The `pgx_log_*` macros in `utils.h` respect the `pgx_routine_tasks.log_level` GUC. They map to PostgreSQL's `ereport()`:
   - `pgx_log_debug` → `DEBUG1`
   - `pgx_log_info` → `LOG`
   - `pgx_log_warning` → `WARNING`
   - `pgx_log_error` → `ERROR` (always raises)

4. **Memory Management**: All allocations use PostgreSQL memory contexts. Temporary allocations in SRFs use the per-query context via `pgx_build_tuplestore()`.

## Security Model

- All SQL functions are declared `SECURITY DEFINER` — they execute with the privileges of the extension owner (typically a superuser).
- The extension control file requires `superuser = true` for installation.
- Session termination functions exclude the calling backend (`pid <> pg_backend_pid()`).
- DDL operations (REINDEX, VACUUM, partition creation) require appropriate privileges which are inherited from `SECURITY DEFINER`.

## Future: Background Worker

Version 0.2.0 will introduce a PostgreSQL **Background Worker** that:
- Runs as a persistent process registered via `RegisterBackgroundWorker()`
- Wakes up on configurable intervals
- Executes each feature module's maintenance functions automatically
- Logs all actions to an audit table