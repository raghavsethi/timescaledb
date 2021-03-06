#ifndef TIMESCALEDB_METADATA_QUERIES_H
#define TIMESCALEDB_METADATA_QUERIES_H

#include "timescaledb.h"
#include "utils/lsyscache.h"
#include "catalog/namespace.h"
#include "executor/spi.h"
#include "fmgr.h"

#define OPEN_START_TIME -1
#define OPEN_END_TIME PG_INT64_MAX

typedef struct epoch_and_partitions_set epoch_and_partitions_set;

typedef struct chunk_row
{
	int32		id;
	int32		partition_id;
	int64		start_time;
	int64		end_time;
}	chunk_row;

typedef struct crn_row
{
	NameData	schema_name;
	NameData	table_name;
	NameData	database_name;
}	crn_row;


typedef struct crn_set
{
	int32		chunk_id;
	List	   *tables;
}	crn_set;

/* utility func */
extern SPIPlanPtr prepare_plan(const char *src, int nargs, Oid *argtypes);


/* db access func */
extern epoch_and_partitions_set *fetch_epoch_and_partitions_set(epoch_and_partitions_set * entry,
							   int32 hypertable_id, int64 time_pt, Oid relid);

extern void free_epoch(epoch_and_partitions_set * epoch);

extern crn_set *fetch_crn_set(crn_set * entry, int32 chunk_id);

chunk_row *
			chunk_row_insert_new(int32 partition_id, int64 timepoint, bool lock);

bool		chunk_row_timepoint_is_member(const chunk_row * row, const int64 time_pt);

extern crn_row *crn_set_get_crn_row_for_db(crn_set * set, char *dbname);

#endif   /* TIMESCALEDB_METADATA_QUERIES_H */
