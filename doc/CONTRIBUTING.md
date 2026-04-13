# Contributing to pgx_routine_tasks

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## Reporting Issues

1. **Search first** — check [existing issues](https://github.com/EltunHajiyev/pgx_routine_tasks/issues) before opening a new one.
2. **Use the template** — include:
   - PostgreSQL version (`SELECT version();`)
   - Extension version
   - Operating system
   - Steps to reproduce
   - Expected vs. actual behavior
   - Relevant log output

## Pull Request Process

### 1. Fork & Branch

```bash
git clone https://github.com/YOUR_USERNAME/pgx_routine_tasks.git
cd pgx_routine_tasks
git checkout -b feature/my-improvement
```

### 2. Make Changes

- Follow the [code style guidelines](DEVELOPMENT.md#code-style)
- Add/update tests in `test/sql/` and `test/expected/`
- Update documentation if adding new features

### 3. Test

```bash
make clean
make
sudo make install
make installcheck
```

### 4. Commit

- Write clear, descriptive commit messages
- Reference issue numbers: `Fix #42: handle NULL partition names`
- Keep commits atomic — one logical change per commit

### 5. Submit PR

- Open a PR against the `main` branch
- Fill in the PR description template
- Ensure CI passes

## Code Review Checklist

Reviewers will check:

- [ ] Code compiles without warnings (`-Wall -Wextra`)
- [ ] All SPI operations are in `PG_TRY/PG_CATCH` blocks
- [ ] NULL arguments are handled properly
- [ ] Memory is allocated in the correct memory context
- [ ] Regression tests pass
- [ ] New functions have `PG_FUNCTION_INFO_V1()` and SQL `CREATE FUNCTION`
- [ ] Documentation is updated
- [ ] No SQL injection vulnerabilities (use `quote_identifier()`, parameterized queries)

## Release Process

1. Update version in `pgx_routine_tasks.control` and SQL migration files
2. Create a migration SQL: `sql/pgx_routine_tasks--OLD--NEW.sql`
3. Update `REGRESS` and `DATA` in `Makefile`
4. Tag the release: `git tag v0.2.0`
5. Push tags: `git push origin --tags`

## Code of Conduct

Be respectful, constructive, and inclusive. We follow the [PostgreSQL Code of Conduct](https://www.postgresql.org/about/policies/coc/).