# pgx_routine_tasks

[![PostgreSQL Version](https://img.shields.io/badge/PostgreSQL-12%20|%2013%20|%2014%20|%2015%20|%2016-blue.svg)](https://www.postgresql.org/)
[![License](https://img.shields.io/badge/License-PostgreSQL-blue.svg)](https://opensource.org/licenses/postgresql)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/valeh/pgx_routine_tasks/actions)

A PostgreSQL extension written in C designed to automate, schedule, and simplify daily Database Administration (DBA) routines. 

`pgx_routine_tasks` reduces manual operational overhead, ensures consistent maintenance, and minimizes human error in enterprise database environments. While tools like `pg_cron` can schedule tasks, this extension provides the **intelligent execution logic**—ensuring maintenance only occurs when necessary, respecting cluster state, replication lag, and system load.

## 📑 Table of Contents
- [Features](#-features)
- [Prerequisites](#-prerequisites)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Usage Examples](#-usage-examples)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🚀 Features

* **Intelligent Session Management**: Safely terminate long-running or idle-in-transaction sessions based on configurable thresholds.
* **Automated Bloat Control**: Detect and automatically rebuild heavily bloated indexes without locking using `CONCURRENTLY`.
* **Partition Maintenance**: Automate the creation of future partitions and the dropping/archiving of historical partitions.
* **Smart Vacuum/Analyze**: Targeted maintenance wrappers that trigger based on custom catalog bloat or modification thresholds rather than just standard autovacuum rules.

## 📋 Prerequisites

To compile and install this extension, you will need:
* PostgreSQL 12 or higher.
* PostgreSQL development headers (e.g., `postgresql-server-dev-16` on Debian/Ubuntu or `postgresql16-devel` on RHEL/CentOS).
* `make` and `gcc`.

## 🛠️ Installation

Clone the repository and compile the extension using the standard PostgreSQL build infrastructure (PGXS):

```bash
git clone [https://github.com/yourusername/pgx_routine_tasks.git](https://github.com/yourusername/pgx_routine_tasks.git)
cd pgx_routine_tasks
make
sudo make install
