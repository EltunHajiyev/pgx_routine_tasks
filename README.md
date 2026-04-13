# pgx_routine_tasks

[![PostgreSQL Version](https://img.shields.io/badge/PostgreSQL-14%20|%2015%20|%2016-blue.svg)](https://www.postgresql.org/)
[![License](https://img.shields.io/badge/License-PostgreSQL-blue.svg)](https://opensource.org/licenses/postgresql)
[![CI](https://github.com/EltunHajiyev/pgx_routine_tasks/actions/workflows/ci.yml/badge.svg)](https://github.com/EltunHajiyev/pgx_routine_tasks/actions)

A PostgreSQL extension written in **C** that automates, schedules, and simplifies daily Database Administration (DBA) routines.

`pgx_routine_tasks` reduces manual operational overhead, ensures consistent maintenance, and minimizes human error in enterprise database environments. While tools like `pg_cron` can schedule tasks, this extension provides **built-in intelligence** — it decides *what* needs maintenance and *executes* the appropriate action, not just *when* to run.

---

## 📑 Table of Contents

- [Features](#-features)
- [Architecture](#-architecture)
- [Prerequisites](#-prerequisites)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Usage Examples](#-usage-examples)
- [API Reference](#-api-reference)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🚀 Features

| Module | Description |
|---|---|
| **Session Manager** | Safely terminate long-running or idle-in-transaction sessions based on configurable thresholds |
| **Bloat Manager** | Detect and automatically rebuild heavily bloated indexes without locking (`REINDEX CONCURRENTLY`) |
| **Partition Manager** | Automate creation of future partitions and dropping/archiving of historical partitions |
| **Vacuum Manager** | Targeted VACUUM/ANALYZE based on custom dead-tuple and modification thresholds |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   pgx_routine_tasks                     │
│                    (_PG_init + GUCs)                    │
├──────────┬──────────┬──────────────┬────────────────────┤
│ Session  │  Bloat   │  Partition   │     Vacuum         │
│ Manager  │  Manager │  Manager     │     Manager        │
├──────────┴──────────┴──────────────┴────────────────────┤
│                   utils (SPI, logging, tuplestore)      │
├─────────────────────────────────────────────────────────┤
│              PostgreSQL SPI / Catalog / GUC              │
└─────────────────────────────────────────────────────────┘
```

See [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) for the full technical design.

---

## 📋 Prerequisites

- PostgreSQL **14** or higher (tested on 14, 15, 16)
- PostgreSQL development headers:
  - Debian/Ubuntu: `postgresql-server-dev-16`
  - RHEL/CentOS: `postgresql16-devel`
- `make` and `gcc`

---

## 🛠️ Installation

```bash
git clone https://github.com/EltunHajiyev/pgx_routine_tasks.git
cd pgx_routine_tasks
make
sudo make install
```

Then in your database:

```sql
CREATE EXTENSION pgx_routine_tasks;
```

To verify:

```sql
SELECT * FROM pgx_session_overview;
SELECT * FROM pgx_bloat_overview;
SELECT * FROM pgx_maintenance_overview;
```

---

## ⚙️ Configuration

All parameters are configured via `postgresql.conf` or `SET`:

| Parameter | Type | Default | Description |
|---|---|---|---|
| `pgx_routine_tasks.session_idle_timeout` | integer (s) | `300` | Max idle-in-transaction time before termination |
| `pgx_routine_tasks.session_max_duration` | integer (s) | `3600` | Max query duration before termination |
| `pgx_routine_tasks.bloat_threshold_pct` | real (%) | `30.0` | Index bloat % to trigger detection/rebuild |
| `pgx_routine_tasks.partition_pre_create_count` | integer | `3` | Number of future partitions to pre-create |
| `pgx_routine_tasks.vacuum_bloat_threshold_pct` | real (%) | `20.0` | Dead-tuple % to trigger smart vacuum |
| `pgx_routine_tasks.vacuum_mod_threshold_pct` | real (%) | `10.0` | Modification % to trigger vacuum/analyze |
| `pgx_routine_tasks.log_level` | enum | `info` | Logging level: `debug`, `info`, `warning`, `error` |

Example:

```sql
-- Set in session
SET pgx_routine_tasks.session_idle_timeout = 600;
SET pgx_routine_tasks.bloat_threshold_pct = 25.0;

-- Or in postgresql.conf
-- pgx_routine_tasks.session_idle_timeout = 600
```

---

## 📖 Usage Examples

### Session Management

```sql
-- View current session states
SELECT * FROM pgx_session_overview;

-- Terminate sessions idle > 5 minutes
SELECT * FROM pgx_terminate_idle_sessions(300);

-- Terminate queries running > 1 hour
SELECT * FROM pgx_terminate_long_running(3600);

-- Full session report
SELECT * FROM pgx_session_report();
```

### Bloat Control

```sql
-- Detect indexes with > 30% bloat
SELECT * FROM pgx_detect_bloated_indexes(30.0);

-- Rebuild bloated indexes concurrently
SELECT * FROM pgx_rebuild_bloated_indexes(30.0, true);

-- Quick bloat summary
SELECT * FROM pgx_bloat_overview;
```

### Partition Maintenance

```sql
-- Create 3 monthly partitions for a table
SELECT * FROM pgx_create_future_partitions('my_table'::regclass, 3, 'monthly');

-- Drop partitions older than 90 days
SELECT * FROM pgx_drop_old_partitions('my_table'::regclass, 90);

-- View partition details
SELECT * FROM pgx_partition_report('my_table'::regclass);
```

### Smart Vacuum/Analyze

```sql
-- Vacuum tables with > 20% dead tuples or > 10% modifications
SELECT * FROM pgx_smart_vacuum(20.0, 10.0);

-- Analyze heavily modified tables
SELECT * FROM pgx_smart_analyze(10.0);

-- Full maintenance status
SELECT * FROM pgx_maintenance_overview;
```

---

## 📚 API Reference

See [doc/API_REFERENCE.md](doc/API_REFERENCE.md) for the complete function reference.

---

## 🗺️ Roadmap

### v0.1.0 (Current) — Foundation
- [x] Session management (terminate idle/long-running)
- [x] Index bloat detection and rebuild
- [x] Partition maintenance (create/drop)
- [x] Smart vacuum/analyze
- [x] GUC-based configuration
- [x] CI pipeline

### v0.2.0 — Enhanced Intelligence
- [ ] Background worker for scheduled execution
- [ ] Configurable cron-like schedules per feature
- [ ] Email/webhook alerting
- [ ] Lock-aware operations (skip if table is locked)

### v0.3.0 — Enterprise Features
- [ ] Multi-database support
- [ ] Audit log table for all actions
- [ ] Dry-run mode for all operations
- [ ] Table-level include/exclude lists

### v1.0.0 — Production Ready
- [ ] Comprehensive pgTAP test suite
- [ ] Performance benchmarks
- [ ] PGXN packaging
- [ ] Full documentation site

---

## 🤝 Contributing

We welcome contributions! See [doc/CONTRIBUTING.md](doc/CONTRIBUTING.md) for guidelines.

---

## 📄 License

This project is licensed under the [PostgreSQL License](LICENSE).

Copyright (c) 2026, Eltun Hajiyev, Gurban Ismayilov, Valeh Agayev, and contributors.
