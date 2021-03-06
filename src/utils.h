#ifndef TIMESCALEDB_UTILS_H
#define TIMESCALEDB_UTILS_H

#include "fmgr.h"
#include "nodes/primnodes.h"

extern Datum pg_timestamp_to_microseconds(PG_FUNCTION_ARGS);
extern Datum pg_microseconds_to_timestamp(PG_FUNCTION_ARGS);
extern Datum pg_timestamp_to_unix_microseconds(PG_FUNCTION_ARGS);
extern Datum pg_unix_microseconds_to_timestamp(PG_FUNCTION_ARGS);

/*
 * Convert a column value into the internal time representation.
 */
extern int64 time_value_to_internal(Datum time_val, Oid type);
extern char *internal_time_to_column_literal_sql(int64 internal_time, Oid type);

#if 0
#define CACHE1_elog(a,b)				elog(a,b)
#define CACHE2_elog(a,b,c)				elog(a,b,c)
#define CACHE3_elog(a,b,c,d)			elog(a,b,c,d)
#define CACHE4_elog(a,b,c,d,e)			elog(a,b,c,d,e)
#define CACHE5_elog(a,b,c,d,e,f)		elog(a,b,c,d,e,f)
#define CACHE6_elog(a,b,c,d,e,f,g)		elog(a,b,c,d,e,f,g)
#else
#define CACHE1_elog(a,b)
#define CACHE2_elog(a,b,c)
#define CACHE3_elog(a,b,c,d)
#define CACHE4_elog(a,b,c,d,e)
#define CACHE5_elog(a,b,c,d,e,f)
#define CACHE6_elog(a,b,c,d,e,f,g)
#endif


extern FmgrInfo *create_fmgr(char *schema, char *function_name, int num_args);
extern RangeVar *makeRangeVarFromRelid(Oid relid);
extern int	int_cmp(const void *a, const void *b);

#endif   /* TIMESCALEDB_UTILS_H */
