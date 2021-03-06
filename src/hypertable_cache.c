#include <postgres.h>
#include <access/relscan.h>
#include <utils/catcache.h>
#include <utils/rel.h>
#include <utils/fmgroids.h>
#include <utils/tqual.h>
#include <utils/acl.h>

#include "hypertable_cache.h"
#include "catalog.h"
#include "cache.h"
#include "metadata_queries.h"
#include "utils.h"
#include "scanner.h"
#include "partitioning.h"

static void *hypertable_cache_create_entry(Cache * cache, CacheQueryCtx * ctx);

typedef struct HypertableCacheQueryCtx
{
	CacheQueryCtx cctx;
	int32		hypertable_id;
}	HypertableCacheQueryCtx;

static void *
hypertable_cache_get_key(CacheQueryCtx * ctx)
{
	return &((HypertableCacheQueryCtx *) ctx)->hypertable_id;
}


static Cache *
hypertable_cache_create()
{
	MemoryContext ctx = AllocSetContextCreate(CacheMemoryContext,
										  HYPERTABLE_CACHE_INVAL_PROXY_TABLE,
											  ALLOCSET_DEFAULT_SIZES);

	Cache	   *cache = MemoryContextAlloc(ctx, sizeof(Cache));

	Cache		tmp = (Cache)
	{
		.hctl =
		{
			.keysize = sizeof(int32),
			.entrysize = sizeof(hypertable_cache_entry),
			.hcxt = ctx,
		},
		.name = HYPERTABLE_CACHE_INVAL_PROXY_TABLE,
		.numelements = 16,
		.flags = HASH_ELEM | HASH_CONTEXT | HASH_BLOBS,
		.get_key = hypertable_cache_get_key,
		.create_entry = hypertable_cache_create_entry,
	};

	*cache = tmp;
	cache_init(cache);

	return cache;
}


static Cache *hypertable_cache_current = NULL;
/* Column numbers for 'hypertable' table in  sql/common/tables.sql */
#define HT_TBL_COL_ID 1
#define HT_TBL_COL_TIME_COL_NAME 10
#define HT_TBL_COL_TIME_TYPE 11
#define HT_TBL_COL_CHUNK_SIZE 13

/* Primary key Index column number */
#define HT_IDX_COL_ID 1

static bool
hypertable_tuple_found(TupleInfo * ti, void *data)
{
	bool		is_null;
	HypertableCacheQueryCtx *hctx = data;
	hypertable_cache_entry *he = hctx->cctx.entry;
	Datum		id_datum = heap_getattr(ti->tuple, HT_TBL_COL_ID, ti->desc, &is_null);
	Datum		time_col_datum = heap_getattr(ti->tuple, HT_TBL_COL_TIME_COL_NAME, ti->desc, &is_null);
	Datum		time_type_datum = heap_getattr(ti->tuple, HT_TBL_COL_TIME_TYPE, ti->desc, &is_null);
	Datum		chunk_size_datum = heap_getattr(ti->tuple, HT_TBL_COL_CHUNK_SIZE, ti->desc, &is_null);
	int32		id = DatumGetInt32(id_datum);

	if (id != hctx->hypertable_id)
	{
		elog(ERROR, "Expected hypertable ID %u, got %u", hctx->hypertable_id, id);
	}

	he->num_epochs = 0;
	he->id = hctx->hypertable_id;
	strncpy(he->time_column_name, DatumGetCString(time_col_datum), NAMEDATALEN);
	he->time_column_type = DatumGetObjectId(time_type_datum);
	he->chunk_size_bytes = DatumGetInt64(chunk_size_datum);

	return true;
}

static void *
hypertable_cache_create_entry(Cache * cache, CacheQueryCtx * ctx)
{
	HypertableCacheQueryCtx *hctx = (HypertableCacheQueryCtx *) ctx;
	Catalog    *catalog = catalog_get();
	ScanKeyData scankey[1];
	ScannerCtx	scanCtx = {
		.table = catalog->tables[HYPERTABLE].id,
		.index = catalog->tables[HYPERTABLE].index_id,
		.scantype = ScannerTypeIndex,
		.nkeys = 1,
		.scankey = scankey,
		.data = ctx,
		.tuple_found = hypertable_tuple_found,
		.lockmode = AccessShareLock,
		.scandirection = ForwardScanDirection,
	};

	/* Perform an index scan on primary key. */
	ScanKeyInit(&scankey[0], HT_IDX_COL_ID, BTEqualStrategyNumber,
				F_INT4EQ, Int32GetDatum(hctx->hypertable_id));

	scanner_scan(&scanCtx);

	return ctx->entry;
}

void
hypertable_cache_invalidate_callback(void)
{
	CACHE1_elog(WARNING, "DESTROY hypertable_cache");
	cache_invalidate(hypertable_cache_current);
	hypertable_cache_current = hypertable_cache_create();
}


/* Get hypertable cache entry. If the entry is not in the cache, add it. */
hypertable_cache_entry *
hypertable_cache_get_entry(Cache * cache, int32 hypertable_id)
{
	HypertableCacheQueryCtx ctx = {
		.hypertable_id = hypertable_id,
	};

	return cache_fetch(cache, &ctx.cctx);
}

/* function to compare epochs */
static int
cmp_epochs(const void *time_pt_pointer, const void *test)
{
	/* note reverse order; assume oldest stuff last */
	int64	   *time_pt = (int64 *) time_pt_pointer;
	epoch_and_partitions_set **entry = (epoch_and_partitions_set **) test;

	if ((*entry)->start_time <= *time_pt && (*entry)->end_time >= *time_pt)
	{
		return 0;
	}

	if (*time_pt < (*entry)->start_time)
	{
		return 1;
	}
	return -1;
}

epoch_and_partitions_set *
hypertable_cache_get_partition_epoch(Cache * cache, hypertable_cache_entry * hce, int64 time_pt, Oid relid)
{
	MemoryContext old;
	epoch_and_partitions_set *epoch,
			  **cache_entry;
	int			j;

	/* fastpath: check latest entry */
	if (hce->num_epochs > 0)
	{
		epoch = hce->epochs[0];

		if (epoch->start_time <= time_pt && epoch->end_time >= time_pt)
		{
			return epoch;
		}
	}

	cache_entry = bsearch(&time_pt, hce->epochs, hce->num_epochs,
						  sizeof(epoch_and_partitions_set *), cmp_epochs);

	if (cache_entry != NULL)
	{
		return (*cache_entry);
	}

	old = cache_switch_to_memory_context(cache);
	epoch = partition_epoch_scan(hce->id, time_pt, relid);

	/* check if full */
	if (hce->num_epochs == MAX_EPOCHS_PER_HYPERTABLE_CACHE_ENTRY)
	{
		/* remove last */
		free_epoch(hce->epochs[MAX_EPOCHS_PER_HYPERTABLE_CACHE_ENTRY - 1]);
		hce->epochs[MAX_EPOCHS_PER_HYPERTABLE_CACHE_ENTRY - 1] = NULL;
		hce->num_epochs--;
	}

	/* ordered insert */
	for (j = hce->num_epochs - 1; j >= 0 && cmp_epochs(&time_pt, hce->epochs + j) < 0; j--)
	{
		hce->epochs[j + 1] = hce->epochs[j];
	}
	hce->epochs[j + 1] = epoch;
	hce->num_epochs++;

	MemoryContextSwitchTo(old);

	return epoch;
}

extern Cache *
hypertable_cache_pin()
{
	return cache_pin(hypertable_cache_current);
}


void
_hypertable_cache_init(void)
{
	CreateCacheMemoryContext();
	hypertable_cache_current = hypertable_cache_create();
}

void
_hypertable_cache_fini(void)
{
	cache_invalidate(hypertable_cache_current);
}
