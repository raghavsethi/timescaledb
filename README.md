TimescaleDB is an open-source database designed to make SQL scalable for
time-series data. It is engineered up from PostgreSQL, providing automatic
partitioning across time and space (partitioning key), as well as full
SQL support.

TimescaleDB is packaged as a PostgreSQL extension and set of scripts.


## Prerequisites

- The [Postgres client](https://wiki.postgresql.org/wiki/Detailed_installation_guides) (psql)

And either:

- *Option 1* - A standard PostgreSQL installation with development environment (header files), OR

- *Option 2* - [Docker](https://docs.docker.com/engine/installation/) (see separate build and run instructions)

## Installation (from source)

### Option 1: Build and install with local PostgreSQL

```bash
# To build the extension
make

# To install
make install
```

Also, you will need to edit your `postgres.conf` file to include
necessary libraries, and then restart PostgreSQL:
```bash
# Modify postgres.conf to add required libraries. For example,
shared_preload_libraries = 'dblink,timescaledb'

# Then, restart PostgreSQL
```

### Option 2: Build and run in Docker

```bash
# To build a Docker image
make -f docker.mk build-image

# To run a container
make -f docker.mk run
```

You should now have Postgres running locally, accessible with
the following command:

```bash
# access with a superuser named 'postgres'
psql -U postgres -h localhost
```

Next, we'll install our extension and create an initial database.

### Setting up your initial database
You again have two options for setting up your initial database:

1. *Empty Database* - To set up a new, empty database, please follow the instructions below.

2. *Database with pre-loaded sample data* - To help you quickly get started, we have also created some sample datasets. See
[Using our Sample Datasets](docs/UsingSamples.md) for further instructions. (Includes installing our extension.)

#### Setting up an empty database

When creating a new database, it is necessary to install the extension and then run an initialization function.

```bash
# Connect to Postgres, using a superuser named 'postgres'
psql -U postgres -h localhost
```

```sql
-- Install the extension
CREATE database tutorial;
\c tutorial
CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE;

-- Run initialization function
SELECT setup_db();
```

For convenience, this can also be done in one step by running a script from
the command-line:
```bash
DB_NAME=tutorial ./scripts/setup-db.sh
```

#### Accessing your new database
You should now have a brand new time-series database running in Postgres.

```bash
# To access your new database
psql -U postgres -h localhost -d tutorial
```

Next let's load some data.

## Working with time-series data

One of the core ideas of our time-series database are time-series optimized data tables, called **hypertables**.

### Creating a (hyper)table
To create a hypertable, you start with a regular SQL table, and then convert
it into a hypertable via the function
`create_hypertable()`([API definition](docs/API.md)).

The following example creates a hypertable for tracking
temperature and humidity across a collection of devices over time.

```sql
-- We start by creating a regular SQL table
CREATE TABLE conditions (
  time        TIMESTAMP WITH TIME ZONE NOT NULL,
  device_id   TEXT                     NOT NULL,
  temperature DOUBLE PRECISION         NULL,
  humidity    DOUBLE PRECISION         NULL
);
```



Next, transform it into a hypertable using the provided function
`create_hypertable()`:
```sql
SELECT create_hypertable('conditions', 'time');
```

This creates a hypertable that is partitioned by time using the values
in the `time` column. Additionally, you can partition the data on
another dimension (what we call 'space') such as `device_id`. For example,
to partition `device_id` into 2 partitions along:
```sql
SELECT create_hypertable('conditions', 'time', 'device_id', 2);
```

### Inserting and querying
Inserting data into the hypertable is done via normal SQL `INSERT` commands,
e.g. using millisecond timestamps:
```sql
INSERT INTO conditions(time,device_id,temperature,humidity)
VALUES(NOW(), 'office', 70.0, 50.0);
```

Similarly, querying data is done via normal SQL `SELECT` commands.
SQL `UPDATE` and `DELETE` commands also work as expected.

### Indexing data

Data is indexed using normal SQL `CREATE INDEX` commands. For instance,
```sql
CREATE INDEX ON conditions (device_id, time DESC);
```
This can be done before or after converting the table to a hypertable.

**Indexing suggestions:**

Our experience has shown that different types of indexes are most-useful for
time-series data, depending on your data.

For indexing columns with discrete (limited-cardinality) values (e.g., where you are most likely
  to use an "equals" or "not equals" comparator) we suggest using an index like this (using our hypertable `conditions` for the example):
```sql
CREATE INDEX ON conditions (device_id, time DESC);
```
For all other types of columns, i.e., columns with continuous values (e.g., where you are most likely to use a
"less than" or "greater than" comparator) the index should be in the form:
```sql
CREATE INDEX ON conditions (time DESC, temperature);
```
Having a `time DESC` column specification in the index allows for efficient queries by column-value and time. For example, the index defined above would optimize the following query:
```sql
SELECT * FROM conditions WHERE device_id = 'dev_1' ORDER BY time DESC LIMIT 10
```

For sparse data where a column is often NULL, we suggest adding a
`WHERE column IS NOT NULL` clause to the index (unless you are often
searching for missing data). For example,

```sql
CREATE INDEX ON conditions (time DESC, humidity) WHERE humidity IS NOT NULL;
```
this creates a more compact, and thus efficient, index.

### Current limitations
Below are a few current limitations of our database, which we are
actively working to resolve:

- Anyone using TimescaleDB currently needs superuser privileges (this is
getting fixed very soon)
- Custom user-created triggers on hypertables currently will not work (i.e.,
fire)
- `drop_chunks()` (see our [API Reference](docs/API.md)) is currently only
supported for hypertables that are not partitioned by space.

### More APIs
For more information on TimescaleDB's APIs, check out our
[API Reference](docs/API.md).

## Testing
If you want to contribute, please make sure to run the test suite before
submitting a PR.

If you are running locally:
```bash
make installcheck
```

If you are using Docker:
```bash
make -f docker.mk test
```
