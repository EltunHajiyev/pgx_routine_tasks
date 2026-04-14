# Development Guide — pgx_routine_tasks

## Prerequisites

Install the PostgreSQL development headers and build tools:

```bash
# Debian / Ubuntu
sudo apt-get install postgresql-server-dev-16 build-essential

# RHEL / CentOS / Fedora
sudo dnf install postgresql16-devel gcc make

# macOS (Homebrew)
brew install postgresql@16
```

Ensure `pg_config` is in your `PATH`:

```bash
pg_config --version
# Should print: PostgreSQL 16.x
```

## Building

```bash
git clone https://github.com/EltunHajiyev/pgx_routine_tasks.git
cd pgx_routine_tasks

# Build
make

# Install (requires write access to PG extension dirs)
sudo make install

# Or specify pg_config explicitly
make PG_CONFIG=/usr/lib/postgresql/16/bin/pg_config
sudo make install PG_CONFIG=/usr/lib/postgresql/16/bin/pg_config
```

## Testing

### Regression Tests

```bash
# Start PostgreSQL and ensure the current user has superuser rights
make installcheck
```

This runs the SQL files in `test/sql/` and compares output with `test/expected/`.

If tests fail, inspect the diffs:

```bash
cat regression.diffs
```

### Manual Testing

```sql
CREATE EXTENSION pgx_routine_tasks;

-- Quick smoke tests
SELECT * FROM pgx_session_overview;
SELECT * FROM pgx_bloat_overview;
SELECT * FROM pgx_maintenance_overview;
```

## Code Style

- Follow **PostgreSQL C coding conventions**:
  - Tabs for indentation (tab width = 4)
  - Function names: `lowercase_with_underscores`
  - Braces on their own line for function bodies
  - `ereport()` / `elog()` for all logging
- Every exported function needs `PG_FUNCTION_INFO_V1()`
- Use `PG_TRY / PG_CATCH` for SPI cleanup
- All SQL-callable functions handle NULL arguments gracefully

## Adding a New Feature Module

1. Create `src/new_feature.c` and `src/new_feature.h`
2. Add C functions with `PG_FUNCTION_INFO_V1()` registration
3. Add `src/new_feature.o` to `OBJS` in the `Makefile`
4. Add SQL `CREATE FUNCTION` statements to `sql/pgx_routine_tasks--0.1.0.sql`
5. Add regression tests: `test/sql/new_feature_test.sql` and `test/expected/new_feature_test.out`
6. Add the test name to `REGRESS` in the `Makefile`
7. Include the header in `src/pgx_routine_tasks.c` if `_PG_init` registration is needed

## Debugging Tips

### Enable Debug Logging

```sql
SET pgx_routine_tasks.log_level = 'debug';
```

### View Extension Logs

```bash
tail -f /var/log/postgresql/postgresql-16-main.log
```

### GDB Debugging

```bash
# Find the backend PID
SELECT pg_backend_pid();  -- e.g., 12345

# Attach GDB
sudo gdb -p 12345

# Set a breakpoint
(gdb) break pgx_terminate_idle_sessions
(gdb) continue
```

### Valgrind

```bash
# Run PostgreSQL under valgrind (development only!)
valgrind --leak-check=full postgres -D /path/to/data
```