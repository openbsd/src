/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This is the cache for files.
 * The hash-table is keyed with (cell, volume, fid).
 */

#include "arla_local.h"
RCSID("$KTH: fcache.c,v 1.311.2.20 2001/12/20 16:36:24 mattiasa Exp $") ;

/*
 * Prototypes
 */

static int get_attr_bulk (FCacheEntry *parent_entry, 
			  FCacheEntry *prefered_entry,
			  VenusFid *prefered_fid,
			  const char *prefered_name,
			  CredCacheEntry *ce);

static int
resolve_mp (FCacheEntry *e, VenusFid *ret_fid, CredCacheEntry **ce);

/*
 * Local data for this module.
 */

/*
 * Hash table for all the vnodes known by the cache manager keyed by
 * (cell, volume, vnode, unique).
 */

static Hashtab *hashtab;

/*
 * List of all hash table entries. This list is sorted in LRU-order.
 * The head is the MRU and the tail the LRU, which is from where we
 * take entries when we need to add new ones.
 */

static List *lrulist;

/*
 * Heap of entries to be invalidated.
 */

static Heap *invalid_heap;

/* low and high-water marks for vnodes and space */

static u_long lowvnodes, highvnodes, current_vnodes, lowbytes, highbytes;

/* current values */

static u_long usedbytes, usedvnodes, needbytes;

/* 
 * This is how far the cleaner will go to clean out entries.
 * The higher this is, the higher is the risk that you will
 * lose any file that you feel is important to disconnected
 * operation. 
 */

Bool fprioritylevel;

static int node_count;		/* XXX */

/*
 * This is set to non-zero when we want to use bulkstatus().  2 means
 * that the nodes should be installed into the kernel.
 */

static int fcache_enable_bulkstatus = 1;
static int fcache_bulkstatus_num = 14; /* XXX should use the [P]MTU */

#define FCHASHSIZE 997

/*
 * The cleaner
 */

#define CLEANER_STACKSIZE (16*1024)
#define CLEANER_SLEEP 10

static PROCESS cleaner_pid;

/*
 * The creator of nodes.
 */

#define CREATE_NODES_STACKSIZE (16*1024)

static PROCESS create_nodes_pid;

/*
 * The invalidator
 */

#define INVALIDATOR_STACKSIZE (16*1024)

static PROCESS invalidator_pid;

/*
 * Smalltalk emulation
 */

u_long
fcache_highbytes(void)
{
    return highbytes;
}

u_long
fcache_usedbytes(void)
{
    return usedbytes;
}

u_long
fcache_lowbytes(void)
{
    return lowbytes;
}

u_long
fcache_highvnodes(void)
{
    return highvnodes;
}

u_long
fcache_usedvnodes(void)
{
    return usedvnodes;
}

u_long
fcache_lowvnodes(void)
{
    return lowvnodes;
}

#define HISTOGRAM_SLOTS 32
#define STATHASHSIZE 997

/* Struct with collected statistics */
struct collect_stat{
    int64_t starttime;
};

struct time_statistics {
    u_int32_t measure_type;
    u_int32_t host;
    u_int32_t partition;
    u_int32_t measure_items; /* normed by get_histgram_slots */
    u_int32_t count[HISTOGRAM_SLOTS];    
    int64_t measure_items_total[HISTOGRAM_SLOTS];
    int64_t elapsed_time[HISTOGRAM_SLOTS];
};

static unsigned
statistics_hash (void *p)
{
    struct time_statistics *stats = (struct time_statistics*)p;

    return stats->measure_type + stats->host +
	stats->partition * 32 * 32 + stats->measure_items * 32;
}

/*
 * Compare two entries. Return 0 if and only if the same.
 */

static int
statistics_cmp (void *a, void *b)
{
    struct time_statistics *f1 = (struct time_statistics*)a;
    struct time_statistics *f2 = (struct time_statistics*)b;

    return f1->measure_type  != f2->measure_type
	|| f1->host          != f2->host
	|| f1->partition     != f2->partition
	|| f1->measure_items != f2->measure_items;
}

static Hashtab *statistics;

static int
get_histogram_slot(u_int32_t value)
{
    int i;

    for (i = HISTOGRAM_SLOTS - 1; i > 0; i--) {
	if (value >> i)
	    return i;
    }
    return 0;
}

static void
add_time_statistics(u_int32_t measure_type, u_int32_t host,
		    u_int32_t partition, u_int32_t measure_items,
		    int64_t elapsed_time)
{
    u_int32_t time_slot;
    struct time_statistics *ts;
    struct time_statistics *ts2;

    ts = malloc(sizeof(*ts));

    time_slot = get_histogram_slot(elapsed_time);
    ts->measure_type = measure_type;
    ts->measure_items = get_histogram_slot(measure_items);
    ts->host = host;
    ts->partition = partition;
    ts2 = hashtabsearch (statistics, (void*)(ts));
    if (ts2) {
	ts2->count[time_slot]++;
	ts2->elapsed_time[time_slot] += elapsed_time;
	ts2->measure_items_total[time_slot] += measure_items;
	free(ts);
    } else {
	memset(ts->count, 0, sizeof(ts->count));
	memset(ts->measure_items_total, 0, sizeof(ts->measure_items_total));
	memset(ts->elapsed_time, 0, sizeof(ts->elapsed_time));
	ts->count[time_slot]++;
	ts->elapsed_time[time_slot] += elapsed_time;
	ts->measure_items_total[time_slot] += measure_items;
	hashtabadd(statistics, ts);
    }

    time_slot = get_histogram_slot(elapsed_time);
}

static void
collectstats_init (void)
{
    statistics = hashtabnew (STATHASHSIZE, statistics_cmp, statistics_hash);

    if (statistics == NULL)
	arla_err(1, ADEBINIT, errno, "collectstats_init: cannot malloc");
}

static void
collectstats_start (struct collect_stat *p)
{
    struct timeval starttime;

    gettimeofday(&starttime, NULL);
    p->starttime = starttime.tv_sec * 1000000LL + starttime.tv_usec;
}

static void
collectstats_stop (struct collect_stat *p,
		   FCacheEntry *entry,
		   ConnCacheEntry *conn,
		   int measure_type, int measure_items)
{
    u_int32_t host = conn->host;
    long partition = -1;
    int volumetype;
    struct nvldbentry vldbentry;
    struct timeval stoptime;
    int64_t elapsed_time;
    int i;

    gettimeofday(&stoptime, NULL);

    volumetype = volcache_volid2bit (entry->volume, entry->fid.fid.Volume);
    vldbentry = entry->volume->entry;

    for (i = 0; i < min(NMAXNSERVERS, vldbentry.nServers); ++i) {
	if (host == htonl(vldbentry.serverNumber[i]) &&
	    vldbentry.serverFlags[i] & volumetype) {
	    partition = vldbentry.serverPartition[i];
	}
    }
    assert(partition != -1);
    elapsed_time = stoptime.tv_sec * 1000000LL + stoptime.tv_usec;
    elapsed_time -= p->starttime;
    add_time_statistics(measure_type, host, partition,
			measure_items, elapsed_time);
}

struct hostpart {
    u_int32_t host;
    u_int32_t part;
};

static unsigned
hostpart_hash (void *p)
{
    struct hostpart *h = (struct hostpart*)p;

    return h->host * 256 + h->part;
}

static int
hostpart_cmp (void *a, void *b)
{
    struct hostpart *h1 = (struct hostpart*)a;
    struct hostpart *h2 = (struct hostpart*)b;

    return h1->host != h2->host ||
	h1->part != h2->part;
}

static Bool
hostpart_addhash (void *ptr, void *arg)
{
    Hashtab *hostparthash = (Hashtab *) arg;
    struct time_statistics *s = (struct time_statistics *) ptr;
    struct hostpart *h;
    
    h = malloc(sizeof(*h));
    h->host = s->host;
    h->part = s->partition;

    hashtabadd(hostparthash, h);
    return FALSE;
}

struct hostpart_collect_args {
    u_int32_t *host;
    u_int32_t *part;
    int *i;
    int max;
};

static Bool
hostpart_collect (void *ptr, void *arg)
{
    struct hostpart_collect_args *collect_args =
	(struct hostpart_collect_args *) arg;
    struct hostpart *h = (struct hostpart *) ptr;

    if (*collect_args->i >= collect_args->max)
	return TRUE;

    collect_args->host[*collect_args->i] = h->host;
    collect_args->part[*collect_args->i] = h->part;
    (*collect_args->i)++;

    return FALSE;
}

int
collectstats_hostpart(u_int32_t *host, u_int32_t *part, int *n)
{
    Hashtab *hostparthash;
    int i;
    struct hostpart_collect_args collect_args;

    hostparthash = hashtabnew (100, hostpart_cmp, hostpart_hash);

    hashtabforeach(statistics, hostpart_addhash, hostparthash);

    i = 0;
    collect_args.host = host;
    collect_args.part = part;
    collect_args.i = &i;
    collect_args.max = *n;
    hashtabforeach(hostparthash, hostpart_collect, &collect_args);
    *n = i;

    hashtabrelease(hostparthash);

    return 0;
}

int
collectstats_getentry(u_int32_t host, u_int32_t part, u_int32_t type,
		      u_int32_t items_slot, u_int32_t *count,
		      int64_t *items_total, int64_t *total_time)
{
    struct time_statistics ts;
    struct time_statistics *ts2;

    ts.measure_type = type;
    ts.measure_items = items_slot;
    ts.host = host;
    ts.partition = part;
    ts2 = hashtabsearch (statistics, (void*)(&ts));
    if (ts2 == NULL) {
	memset(count, 0, 4 * 32);
	memset(items_total, 0, 8 * 32);
	memset(total_time, 0, 8 * 32);
    } else {
	memcpy(count, ts2->count, 4 * 32);
	memcpy(items_total, ts2->measure_items_total, 8 * 32);
	memcpy(total_time, ts2->elapsed_time, 8 * 32);
    }

    return 0;
}

/*
 * Counters
 */

static struct {
    unsigned long fetch_attr;
    unsigned long fetch_attr_cached;
    unsigned long fetch_attr_bulk;
    unsigned long fetch_data;
    unsigned long fetch_data_cached;
    unsigned long store_attr;
    unsigned long store_data;
} fcache_counter;

/*
 * Compare two entries. Return 0 if and only if the same.
 */

static int
fcachecmp (void *a, void *b)
{
     FCacheEntry *f1 = (FCacheEntry*)a;
     FCacheEntry *f2 = (FCacheEntry*)b;

     return f1->fid.Cell != f2->fid.Cell 
	 || f1->fid.fid.Volume != f2->fid.fid.Volume 
	 || f1->fid.fid.Vnode  != f2->fid.fid.Vnode
	 || f1->fid.fid.Unique != f2->fid.fid.Unique;
}

/*
 * Hash the value of an entry.
 */

static unsigned
fcachehash (void *e)
{
     FCacheEntry *f = (FCacheEntry*)e;

     return f->fid.Cell + f->fid.fid.Volume + f->fid.fid.Vnode 
	  + f->fid.fid.Unique;
}

/*
 * Compare expiration times.
 */

static int
expiration_time_cmp (const void *a, const void *b)
{
    const FCacheEntry *f1 = (const FCacheEntry *)a;
    const FCacheEntry *f2 = (const FCacheEntry *)b;

    return f1->callback.ExpirationTime - f2->callback.ExpirationTime;
}

void
recon_hashtabadd(FCacheEntry *entry)
{
    hashtabadd(hashtab,entry);
}
 
void
recon_hashtabdel(FCacheEntry *entry)
{
   hashtabdel(hashtab,entry);
}

/*
 * Globalnames 
 */

char arlasysname[SYSNAMEMAXLEN];

/*
 * return the directory name of the cached file for `entry'
 */

int
fcache_dir_name (FCacheEntry *entry, char *s, size_t len)
{
    return snprintf (s, len, "%02X", entry->index / 0x100);
}

/*
 * return the file name of the cached file for `entry'.
 */

int
fcache_file_name (FCacheEntry *entry, char *s, size_t len)
{
    return snprintf (s, len, "%02X/%02X",
		     entry->index / 0x100, entry->index % 0x100);
}

/*
 * the filename for the extra (converted) directory
 */

static int
real_extra_file_name (FCacheEntry *entry, char *s, size_t len)
{
    int ret;

    ret = fcache_file_name (entry, s, len - 1);
    if (ret < len - 1) {
	s[ret++] = '@';
	s[ret]   = '\0';
    }
    return ret;
}

/*
 * return the file name of the converted directory for `entry'.
 */

int
fcache_extra_file_name (FCacheEntry *entry, char *s, size_t len)
{
    assert (entry->flags.datap &&
	    entry->flags.extradirp &&
	    entry->status.FileType == TYPE_DIR);

    return real_extra_file_name (entry, s, len);
}

static int fhopen_working;

/*
 * open file by handle
 */

static int
fcache_fhopen (fcache_cache_handle *handle, int flags)
{
    if (!handle->valid) {
	errno = EINVAL;
	return -1;
    }

#if defined(HAVE_GETFH) && defined(HAVE_FHOPEN)
    {
	int ret;
	fhandle_t fh;

	memcpy (&fh, &handle->xfs_handle, sizeof(fh));
	ret = fhopen (&fh, flags);
	if (ret >= 0)
	    return ret;
    }
#endif

#ifdef KERBEROS			/* really KAFS */
    {
	struct ViceIoctl vice_ioctl;
	
	vice_ioctl.in      = (caddr_t)&handle->xfs_handle;
	vice_ioctl.in_size = sizeof(handle->xfs_handle);
	
	vice_ioctl.out      = NULL;
	vice_ioctl.out_size = 0;
	
	return k_pioctl (NULL, VIOC_FHOPEN, &vice_ioctl, flags);
    }
#else
    errno = EINVAL;
    return -1;
#endif
}

/*
 * get the handle of `filename'
 */

int
fcache_fhget (char *filename, fcache_cache_handle *handle)
{
    handle->valid = 0;
#if defined(HAVE_GETFH) && defined(HAVE_FHOPEN)
    {
	int ret;
	fhandle_t fh;

	ret = getfh (filename, &fh);
	if (ret == 0) {
	    memcpy (&handle->xfs_handle, &fh, sizeof(fh));
	    handle->valid = 1;
	}

	return ret;
    }
#endif
#ifdef KERBEROS
    {
	struct ViceIoctl vice_ioctl;
	int ret;
	
	if (!fhopen_working)
	    return 0;
	
	vice_ioctl.in      = NULL;
	vice_ioctl.in_size = 0;
	
	vice_ioctl.out      = (caddr_t)&handle->xfs_handle;
	vice_ioctl.out_size = sizeof(handle->xfs_handle);
	
	ret = k_pioctl (filename, VIOC_FHGET, &vice_ioctl, 0);
	if (ret == 0)
	    handle->valid = 1;

	return ret;
    }
#else
    errno = EINVAL;
    return -1;
#endif
}

/*
 * create a new cache vnode, assume the entry is locked or private
 */

int
fcache_create_file (FCacheEntry *entry)
{
    char fname[MAXPATHLEN];
    char extra_fname[MAXPATHLEN];
    int fd;
    int ret;

    fcache_file_name (entry, fname, sizeof(fname));
    fd = open (fname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0) {
	if (errno == ENOENT) {
	    char dname[MAXPATHLEN];

	    fcache_dir_name (entry, dname, sizeof(dname));
	    ret = mkdir (dname, 0777);
	    if (ret < 0)
		arla_err (1, ADEBERROR, errno, "mkdir %s", dname);
	    fd = open (fname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
	    if (fd < 0)
		arla_err (1, ADEBERROR, errno, "open %s", fname);
	} else {
	    arla_err (1, ADEBERROR, errno, "open %s", fname);
	}
    }
    if (close (fd) < 0)
	arla_err (1, ADEBERROR, errno, "close %s", fname);
    fcache_fhget (fname, &entry->handle);
    real_extra_file_name (entry, extra_fname, sizeof(extra_fname));
    unlink (extra_fname);
    return 0;
}

/*
 * return a fd to the cache file of `entry'
 */

int
fcache_open_file (FCacheEntry *entry, int flag)
{
    int ret;
    char fname[MAXPATHLEN];

    if (fhopen_working) {
	ret = fcache_fhopen (&entry->handle, flag);
	if (ret < 0 && errno == EINVAL)
	    fhopen_working = 0;
	else
	    return ret;
    }
    fcache_file_name (entry, fname, sizeof(fname));
    return open (fname, flag | O_BINARY);
}

/*
 * return a fd to the converted directory for `entry'
 */

int
fcache_open_extra_dir (FCacheEntry *entry, int flag, mode_t mode)
{
    char fname[MAXPATHLEN];

    assert (entry->flags.datap && entry->flags.extradirp &&
	    entry->status.FileType == TYPE_DIR);

    fcache_extra_file_name (entry, fname, sizeof(fname));
    return open (fname, flag | O_BINARY, mode);
}

/*
 * Discard the data cached for `entry'.
 */

static void
throw_data (FCacheEntry *entry)
{
    int fd;
    struct stat sb;

    assert (entry->flags.datap && entry->flags.usedp);
    AssertExclLocked(&entry->lock);

    fd = fcache_open_file (entry, O_WRONLY);
    if (fd < 0) {
	arla_warn (ADEBFCACHE, errno, "fcache_open_file");
	goto out;
    }
    if (fstat (fd, &sb) < 0) {
	arla_warn (ADEBFCACHE, errno, "fstat");
	close (fd);
	goto out;
    }
    if (ftruncate (fd, 0) < 0) {
	arla_warn (ADEBFCACHE, errno, "ftruncate");
	close (fd);
	goto out;
    }
    close (fd);
    if (entry->flags.extradirp) {
	char fname[MAXPATHLEN];

	fcache_extra_file_name (entry, fname, sizeof(fname));
	unlink (fname);
    }
    assert(usedbytes >= entry->length);
    /* XXX - things are wrong - continue anyway */
    if (usedbytes < entry->length)
	usedbytes  = entry->length;
    usedbytes -= entry->length;
    entry->length = 0;
    entry->flags.datap = FALSE;
    entry->flags.extradirp = FALSE;

 out:
    cm_check_consistency();
}

/*
 * A probe function for a file server.
 */

static int
fs_probe (struct rx_connection *conn)
{
    u_int32_t sec, usec;

    return RXAFS_GetTime (conn, &sec, &usec);
}

/*
 *
 */

static void
throw_entry (FCacheEntry *entry)
{
    CredCacheEntry *ce;
    ConnCacheEntry *conn;
    AFSCBFids fids;
    AFSCBs cbs;
    int ret;

    assert (entry->flags.usedp);
    AssertExclLocked(&entry->lock);

    hashtabdel (hashtab, entry);

    if (entry->flags.datap)
	throw_data (entry);

    if (entry->invalid_ptr != -1) {
	heap_remove (invalid_heap, entry->invalid_ptr);
	entry->invalid_ptr = -1;
    }

    if (entry->flags.attrp && entry->host) {
	ce = cred_get (entry->fid.Cell, 0, CRED_NONE);
	assert (ce != NULL);
	
	conn = conn_get (entry->fid.Cell, entry->host, afsport,
			 FS_SERVICE_ID, fs_probe, ce);
	cred_free (ce);
	
	if (conn != NULL) {
	    fids.len = cbs.len = 1;
	    fids.val = &entry->fid.fid;
	    cbs.val  = &entry->callback;
	    ret = RXAFS_GiveUpCallBacks (conn->connection, &fids, &cbs);
	    conn_free (conn);
	    if (ret)
		arla_warn (ADEBFCACHE, ret, "RXAFS_GiveUpCallBacks");
	}
    }
    if (entry->volume) {
	volcache_free (entry->volume);
	entry->volume = NULL;
    }
    entry->flags.attrp = FALSE;
    entry->flags.usedp = FALSE;
    --usedvnodes;
    LWP_NoYieldSignal (lrulist);
}

/*
 * Return the next cache node number.
 */

static unsigned
next_cache_index (void)
{
     return node_count++;
}

/*
 * Pre-create cache nodes up to the limit highvnodes.  If you want to
 * create more increase highnodes and signal create_nodes.
 */

static void
create_nodes (char *arg)
{
    FCacheEntry *entries;
    unsigned count = 0;
    struct timeval tv;

    while (1) {
       	unsigned int n, i, j;

	while (highvnodes <= current_vnodes)
	    LWP_WaitProcess (create_nodes);

	n = highvnodes - current_vnodes;

	count = 0;
	
	arla_warnx (ADEBFCACHE,
		    "pre-creating nodes");
	
	entries = calloc (n, sizeof(FCacheEntry));
	if (n != 0 && entries == NULL)
	    arla_errx (1, ADEBERROR, "fcache: calloc failed");
	
	for (i = 0; i < n; ++i) {
	    entries[i].invalid_ptr = -1;
	    entries[i].volume      = NULL;
	    entries[i].refcount    = 0;
	    entries[i].anonaccess  = 0;
	    entries[i].cleanergen  = 0;
	    for (j = 0; j < NACCESS; j++) {
		entries[i].acccache[j].cred = ARLA_NO_AUTH_CRED;
		entries[i].acccache[j].access = 0;
	    }
	    entries[i].length      = 0;
	    Lock_Init(&entries[i].lock);
	    entries[i].index = next_cache_index ();
	    fcache_create_file (&entries[i]);

	    current_vnodes++;

	    ++count;
	    tv.tv_sec = 0;
	    tv.tv_usec = 1000;

	    entries[i].lru_le      = listaddhead (lrulist, &entries[i]);
	    assert (entries[i].lru_le);

	    LWP_NoYieldSignal (lrulist);
	    IOMGR_Select(0, NULL, NULL, NULL, &tv);
	}

	arla_warnx (ADEBFCACHE,
		    "pre-created %u nodes", count);
    }
}

/*
 * This is the almighty cleaner loop
 */

static Bool cleaner_working = FALSE;

static void
cleaner (char *arg)
{
    enum { CL_OPPORTUNISTIC, CL_FORCE, CL_COLLECT } state;
    int cnt = 0, numnodes;
    VenusFid *fids;
    int cleanerrun = 0;
    
    numnodes = 50;
    
    fids = malloc (sizeof(*fids) * numnodes);
    if (fids == NULL)
	arla_err (1, ADEBERROR, errno, "cleaner: malloc");
    
    for (;;) {
	Listitem *item, *prev;
	FCacheEntry *entry;
	
	arla_warnx (ADEBCLEANER,
		    "running cleaner: "
		    "%lu (%lu-(%lu)-%lu) files, "
		    "%lu (%lu-%lu) bytes "
		    "%lu needed bytes",
		    usedvnodes, lowvnodes, current_vnodes, highvnodes,
		    usedbytes, lowbytes, highbytes,
		    needbytes);
	
	cleaner_working = TRUE;

	state = CL_OPPORTUNISTIC;
	cleanerrun++;

	while (usedvnodes > lowvnodes 
	       || usedbytes > lowbytes
	       || needbytes > highbytes - usedbytes)
	{
	    
	    for (item = listtail (lrulist);
		 item &&
		     (usedvnodes > lowvnodes
		      || usedbytes > lowbytes
		      || needbytes > highbytes - usedbytes);
		 item = prev) {
		prev = listprev (lrulist, item);
		entry = (FCacheEntry *)listdata (item);
		
		if (fprioritylevel && entry->priority)
		    continue;
		
		if (entry->flags.usedp
		    && (usedvnodes > lowvnodes 
			|| usedbytes > lowbytes 
			|| (needbytes > highbytes - usedbytes
			    && !entry->flags.attrusedp))
		    && entry->refcount == 0
		    && CheckLock(&entry->lock) == 0) 
		{
		    if (!entry->flags.datausedp
			&& CheckLock(&entry->lock) == 0
			/* && this_is_a_good_node_to_gc(entry,state) */) {
			ObtainWriteLock (&entry->lock);
			listdel (lrulist, item);
			throw_entry (entry);
			entry->lru_le = listaddtail (lrulist, entry);
			assert(entry->lru_le);
			ReleaseWriteLock (&entry->lock);
			break;
		    }

		    if (state == CL_FORCE) {
			if (entry->cleanergen == cleanerrun)
			    continue;
			entry->cleanergen = cleanerrun;
			
			fids[cnt++] = entry->fid;
			
			if (cnt >= numnodes) {
			    xfs_send_message_gc_nodes (kernel_fd, cnt, fids);
			    IOMGR_Poll();
			    cnt = 0;
			}
			break;
		    }
		}
		assert (entry->lru_le == item);
	    }
	    if (item == NULL) {
		switch (state) {
		case CL_OPPORTUNISTIC:
		    state = CL_FORCE;
		    LWP_DispatchProcess(); /* Yield */
		    break;
		case CL_FORCE:
		    state = CL_COLLECT;		    
		    if (cnt > 0) {
			xfs_send_message_gc_nodes (kernel_fd, cnt, fids);
			IOMGR_Poll();
			cnt = 0;
		    }
		    break;
		case CL_COLLECT:
		    if (needbytes > highbytes - usedbytes) {
			int cleaner_again = 0;
			if (!cleaner_again)
			    goto out;
			state = CL_OPPORTUNISTIC;
		    } else {
			goto out;
		    }
		    break;
		default:
		    abort();
		}
	    }
	}
    out:
	
	arla_warnx(ADEBCLEANER,
		   "cleaner done: "
		   "%lu (%lu-(%lu)-%lu) files, "
		   "%lu (%lu-%lu) bytes "
		   "%lu needed bytes",
		   usedvnodes, lowvnodes, current_vnodes, highvnodes,
		   usedbytes, lowbytes, highbytes,
		   needbytes);
	
	cm_check_consistency();
	if (needbytes)
	    LWP_NoYieldSignal (fcache_need_bytes);
	cleaner_working = FALSE;
	IOMGR_Sleep (CLEANER_SLEEP);
    }
}

static void
fcache_wakeup_cleaner (void *wait)
{
    if (cleaner_working == FALSE)
	IOMGR_Cancel (cleaner_pid);
    LWP_WaitProcess (wait);
}

int
fcache_need_bytes (u_long needed)
{
    if (needed + needbytes > highbytes) {
	arla_warnx (ADEBWARN, 
		    "Out of space since there is outstanding requests "
		    "(%lu needed, %lu outstanding, %lu highbytes", 
		    needed, needbytes, highbytes);
	return ENOSPC;
    }

    needbytes += needed;
    fcache_wakeup_cleaner(fcache_need_bytes);
    needbytes -= needed;
    if (needed > highbytes - usedbytes) {
	arla_warnx (ADEBWARN, 
		    "Out of space, couldn't get needed bytes after cleaner "
		    "(%lu bytes missing, %lu used, %lu highbytes)",
		    needed - (highbytes - usedbytes), 
		    usedbytes, highbytes);
	return ENOSPC;
    }
    return 0;
}

Bool
fcache_need_nodes (void)
{
    fcache_wakeup_cleaner (lrulist);
    if (current_vnodes == usedvnodes)
	return FALSE;
    return TRUE;
}


/*
 * Run through the heap of objects to be invalidated and throw them away
 * when they time arrive.
 */

static void
invalidator (char *arg)
{
    for (;;) {
	const void *head;
	struct timeval tv;

	arla_warnx(ADEBCLEANER,
		   "running invalidator");

	while ((head = heap_head (invalid_heap)) == NULL)
	    LWP_WaitProcess (invalid_heap);

	gettimeofday (&tv, NULL);

	while ((head = heap_head (invalid_heap)) != NULL) {
	    FCacheEntry *entry = (FCacheEntry *)head;

	    if (tv.tv_sec < entry->callback.ExpirationTime) {
		unsigned long t = entry->callback.ExpirationTime - tv.tv_sec;

		arla_warnx (ADEBCLEANER,
			    "invalidator: sleeping for %lu second(s)", t);
		IOMGR_Sleep (t);
		break;
	    }

	    ObtainWriteLock (&entry->lock);
	    if (head == heap_head (invalid_heap)) {
		heap_remove_head (invalid_heap);
		entry->invalid_ptr = -1;
		if (entry->flags.kernelp)
		    break_callback (entry);
	    }
	    ReleaseWriteLock (&entry->lock);
	}
    }
}

/*
 * Add `entry' to the list of to invalidate when its time is up.
 */

static void
add_to_invalidate (FCacheEntry *e)
{
    if (e->invalid_ptr != -1)
	heap_remove (invalid_heap, e->invalid_ptr);
    heap_insert (invalid_heap, (const void *)e, &e->invalid_ptr);
    LWP_NoYieldSignal (invalid_heap);
    IOMGR_Cancel(invalidator_pid);
}

/*
 * Remove the entry least-recently used and return it locked.  Sleep until
 * there's an entry.
 */

static FCacheEntry *
unlink_lru_entry (void)
{
     FCacheEntry *entry = NULL;
     Listitem *item;

     if (highvnodes == usedvnodes)
	 fcache_need_nodes();
     
     for (;;) {

	 assert (!listemptyp (lrulist));
	 for (item = listtail (lrulist);
	      item;
	      item = listprev (lrulist, item)) {

	     entry = (FCacheEntry *)listdata (item);
	     if (!entry->flags.usedp
		 && CheckLock(&entry->lock) == 0) {
		 ObtainWriteLock (&entry->lock);
		 listdel (lrulist, entry->lru_le);
		 entry->lru_le = NULL;
		 return entry;
	     }
	 }

	 assert (!listemptyp (lrulist));
	 for (item = listtail (lrulist);
	      item;
	      item = listprev (lrulist, item)) {

	     entry = (FCacheEntry *)listdata (item);
	     if (entry->flags.usedp
		 && !entry->flags.attrusedp
		 && entry->refcount == 0
		 && CheckLock(&entry->lock) == 0) {
		 ObtainWriteLock (&entry->lock);
		 listdel (lrulist, entry->lru_le);
		 entry->lru_le = NULL;
		 throw_entry (entry);
		 return entry;
	     }
	 }

	 arla_warnx (ADEBFCACHE, "unlink_lru_entry: sleeping");
	 fcache_need_nodes();
     }
}

/*
 * Return a usable locked entry.
 */

static FCacheEntry *
find_free_entry (void)
{
    FCacheEntry *entry;

    entry = unlink_lru_entry ();
    if (entry == NULL)
	arla_warnx (ADEBWARN, "All vnode entries in use");
    else {
	AssertExclLocked(&entry->lock);
	++usedvnodes;
    }
    return entry;
}

/*
 *
 */

int
fcache_store_state (void)
{
    Listitem *item;
    int fd;
    unsigned n;
    u_int32_t u1, u2;

    if (lrulist == NULL) {
	arla_warnx (ADEBFCACHE, "store_state: lrulist is NULL");
	return 0;
    }

    fd = open ("fcache.new", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0)
	return errno;
    u1 = FCACHE_MAGIC_COOKIE;
    u2 = FCACHE_VERSION;
    if (write (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| write (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	int save_errno = errno;

	close (fd);
	return save_errno;
    }
    
    n = 0;
    for (item = listtail (lrulist);
	 item;
	 item = listprev (lrulist, item)) {
	FCacheEntry *entry = (FCacheEntry *)listdata (item);

	if (!entry->flags.usedp)
	    continue;
	if (write (fd, entry, sizeof(*entry)) != sizeof(*entry)) {
	    int save_errno = errno;

	    close (fd);
	    return save_errno;
	}
	++n;
    }

    if(close (fd))
	return errno;
    if (rename ("fcache.new", "fcache"))
	return errno;

    arla_warnx (ADEBFCACHE, "wrote %u entries to fcache", n);

    return 0;
}

/*
 *
 */

static void
fcache_recover_state (void)
{
    int fd;
    FCacheEntry tmp;
    unsigned n;
    AFSCallBack broken_callback = {0, 0, CBDROPPED};
    u_int32_t u1, u2;

    fd = open ("fcache", O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return;
    if (read (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| read (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	close (fd);
	return;
    }
    if (u1 != FCACHE_MAGIC_COOKIE) {
	arla_warnx (ADEBFCACHE, "dump file not recognized, ignoring");
	close (fd);
	return;
    }
    if (u2 != FCACHE_VERSION) {
	arla_warnx (ADEBFCACHE, "unknown dump file version number %u", u2);
	close (fd);
	return;
    }

    n = 0;
    while (read (fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
	CredCacheEntry *ce;
	FCacheEntry *e;
	int i;
	VolCacheEntry *vol;
	int res;
	int type;

	ce = cred_get (tmp.fid.Cell, 0, 0);
	assert (ce != NULL);

	res = volcache_getbyid (tmp.fid.fid.Volume, tmp.fid.Cell,
				ce, &vol, &type);
	cred_free (ce);
	if (res)
	    continue;

	e = find_free_entry ();
	assert (e != NULL);

	++n;

	e->fid      = tmp.fid;
	e->host     = 0;
	e->status   = tmp.status;
	e->length   = tmp.length;
	e->callback = broken_callback;
	e->volsync  = tmp.volsync;
	e->refcount = tmp.refcount;

	/* Better not restore the rights. pags don't have to be the same */
	for (i = 0; i < NACCESS; ++i) {
	    e->acccache[i].cred = ARLA_NO_AUTH_CRED;
	    e->acccache[i].access = ANONE;
	}

	e->anonaccess = tmp.anonaccess;
	e->index      = tmp.index;
	e->handle     = tmp.handle; /* XXX */
	node_count = max(node_count, tmp.index + 1);
	e->flags.usedp = TRUE;
	e->flags.attrp = tmp.flags.attrp;
	e->flags.datap = tmp.flags.datap;
	e->flags.attrusedp = FALSE;
	e->flags.datausedp = FALSE;
	e->flags.kernelp   = FALSE;
	e->flags.extradirp = tmp.flags.extradirp;
	e->flags.mountp    = tmp.flags.mountp;
	e->flags.fake_mp   = tmp.flags.fake_mp;
	e->flags.vol_root  = tmp.flags.vol_root;
	e->flags.sentenced = FALSE;
	e->flags.silly 	   = FALSE;
	e->tokens = tmp.tokens;
	e->parent = tmp.parent;
	e->priority = tmp.priority;
	e->hits = 0;
	e->cleanergen = 0;
	e->lru_le = listaddhead (lrulist, e);
	assert(e->lru_le);
	e->volume = vol;
	hashtabadd (hashtab, e);
	if (e->flags.datap)
	    usedbytes += e->length;
	ReleaseWriteLock (&e->lock);
    }
    close (fd);
    arla_warnx (ADEBFCACHE, "recovered %u entries to fcache", n);
    current_vnodes = n;
}

/*
 * Search for `cred' in `ae' and return a pointer in `pos'.  If it
 * already exists return TRUE, else return FALSE and set pos to a
 * random slot.
 */

Bool
findaccess (xfs_pag_t cred, AccessEntry *ae, AccessEntry **pos)
{
     int i;

     for(i = 0; i < NACCESS ; ++i)
	  if(ae[i].cred == cred) {
	      *pos = &ae[i];
	      return TRUE;
	  }

     i = rand() % NACCESS;
     *pos = &ae[i];
     return FALSE;
}

/*
 * Initialize a `fs_server_context'.
 */

static void
init_fs_server_context (fs_server_context *context)
{
    context->num_conns = 0;
}

/*
 * Find the next fileserver for the request in `context'.
 * Returns a ConnCacheEntry or NULL.
 */

ConnCacheEntry *
find_next_fs (fs_server_context *context,
	      ConnCacheEntry *prev_conn,
	      int mark_as_dead)
{
    if (mark_as_dead)
	conn_dead (prev_conn);

    if (context->i < context->num_conns)
	return context->conns[context->i++];
    else
	return NULL;
}

/*
 * Clean up a `fs_server_context'
 */

void
free_fs_server_context (fs_server_context *context)
{
    int i;

    for (i = 0; i < context->num_conns; ++i)
	conn_free (context->conns[i]);
}

/*
 * Find the first file server housing the volume for `e'.
 * The context is saved in `context' and can later be sent to find_next_fs.
 * Returns a ConnCacheEntry or NULL.
 */

ConnCacheEntry *
find_first_fs (FCacheEntry *e,
	       CredCacheEntry *ce,
	       fs_server_context *context)
{
    VolCacheEntry  *ve = e->volume;
    int i;
    int bit;
    int num_clones;
    int cell = e->fid.Cell;
    int ret;

    memset(context, 0, sizeof(*context));

    if (ve == NULL) {
	int type;

	ret = volcache_getbyid (e->fid.fid.Volume, e->fid.Cell,
				ce, &e->volume, &type);
	if (ret)
	    return NULL;
	ve = e->volume;
    }

    ret = volume_make_uptodate (ve, ce);
    if (ret)
	return NULL;

    bit = volcache_volid2bit (ve, e->fid.fid.Volume);

    if (bit == -1) {
	/* the volume entry is inconsistent. */
	volcache_invalidate_ve (ve);
	return NULL;
    }

    num_clones = 0;
    for (i = 0; i < min(NMAXNSERVERS,ve->entry.nServers); ++i) {
	u_long addr = htonl(ve->entry.serverNumber[i]);

	if (ve->entry.serverFlags[i] & bit
	    && addr != 0
	    && (ve->entry.serverFlags[i] & VLSF_DONTUSE) == 0) {
	    ConnCacheEntry *conn;

	    conn = conn_get (cell, addr, afsport,
			     FS_SERVICE_ID, fs_probe, ce);
	    if (conn != NULL) {
		conn->rtt = rx_PeerOf(conn->connection)->rtt
		    + rand() % RTT_FUZZ - RTT_FUZZ / 2;
		context->conns[num_clones] = conn;
		++num_clones;
	    }
	}
    }

    qsort (context->conns, num_clones, sizeof(*context->conns),
	   conn_rtt_cmp);

    context->num_conns = num_clones;
    context->i	       = 0;

    return find_next_fs (context, NULL, FALSE);
}

/*
 * Initialize the file cache in `cachedir', with these values for high
 * and low-water marks.
 */

void
fcache_init (u_long alowvnodes,
	     u_long ahighvnodes,
	     u_long alowbytes,
	     u_long ahighbytes,
	     Bool recover)
{
    /*
     * Initialize all variables.
     */

#ifdef KERBEROS
    fhopen_working = k_hasafs ();
#else
    fhopen_working = 0;
#endif
    collectstats_init ();

    node_count     = 1;		/* XXX */
    lowvnodes      = alowvnodes;
    highvnodes     = ahighvnodes;
    lowbytes       = alowbytes;
    highbytes      = ahighbytes;
    fprioritylevel = FPRIO_DEFAULT;

    hashtab      = hashtabnew (FCHASHSIZE, fcachecmp, fcachehash);
    if (hashtab == NULL)
	arla_errx (1, ADEBERROR, "fcache: hashtabnew failed");

    lrulist      = listnew ();
    if (lrulist == NULL)
	arla_errx (1, ADEBERROR, "fcache: listnew failed");

    invalid_heap = heap_new (ahighvnodes, expiration_time_cmp);
    if (invalid_heap == NULL)
	arla_errx (1, ADEBERROR, "fcache: heap_new failed");

    if (recover)
	fcache_recover_state ();

    if (LWP_CreateProcess (create_nodes, CREATE_NODES_STACKSIZE, 1,
 			   NULL, "fcache-create-nodes",
 			   &create_nodes_pid))
 	arla_errx (1, ADEBERROR,
 		   "fcache: cannot create create-nodes thread");

    if (LWP_CreateProcess (cleaner, CLEANER_STACKSIZE, 1,
			   NULL, "fcache-cleaner", &cleaner_pid))
	arla_errx (1, ADEBERROR,
		   "fcache: cannot create cleaner thread");

    if (LWP_CreateProcess (invalidator, CLEANER_STACKSIZE, 1,
			   NULL, "fcache-invalidator", &invalidator_pid))
	arla_errx (1, ADEBERROR,
		   "fcache: cannot create invalidator thread");
}

/*
 * set new values for those of lowvnodes, highvnodes, lowbytes, highbytes
 * that are not zero.
 * return 0 or an error code
 */

int
fcache_reinit(u_long alowvnodes, 
	      u_long ahighvnodes, 
	      u_long alowbytes,
	      u_long ahighbytes)
{
    arla_warnx (ADEBFCACHE, "fcache_reinit");
    
    if (ahighvnodes != 0) {
	if (ahighvnodes > highvnodes) {
	    highvnodes = ahighvnodes;
	    LWP_NoYieldSignal (create_nodes);
	} else
	    highvnodes = ahighvnodes;
    }

    if (alowvnodes != 0)
	lowvnodes = alowvnodes;

    if (alowbytes != 0)
	lowbytes = alowbytes;

    if (ahighbytes != 0)
	highbytes = ahighbytes;

    return 0;
}

/*
 * Find the entry for `fid' in the hash table.
 * If it's found, move it to the front of `lrulist' as well.
 */

static FCacheEntry *
find_entry_nolock (VenusFid fid)
{
    FCacheEntry key;
    FCacheEntry *e;

    if (hashtab == NULL)
	return NULL;

    key.fid = fid;
    e = (FCacheEntry *)hashtabsearch (hashtab, (void *)&key);
    if (e != NULL) {
	listdel (lrulist, e->lru_le);
	e->lru_le = listaddhead (lrulist, e);
	assert(e->lru_le);
    }
    return e;
}

/*
 * Find the entry and return it locked.
 */

static FCacheEntry *
find_entry (VenusFid fid)
{
    FCacheEntry *e;

    e = find_entry_nolock (fid);
    
    if (e != NULL)
	ObtainWriteLock (&e->lock);
    return e;
}

/*
 * Mark `e' as having `callback' and notify the kernel.
 * This might be overly hash to opened files.
 */

static void
stale (FCacheEntry *e, AFSCallBack callback)
{
    if (callback.CallBackType == CBDROPPED &&
	e->callback.CallBackType == CBDROPPED)
	return;

    if (CheckLock (&e->lock) != 0)
	e->flags.sentenced = TRUE;
    else {
	ObtainWriteLock (&e->lock);
	e->callback = callback;
	e->tokens   = 0;
	if (e->flags.kernelp)
	    break_callback (e);
	ReleaseWriteLock (&e->lock);
    }
}

struct stale_arg {
    VenusFid fid;
    AFSCallBack callback;
};

/*
 * Iterate over all entries till we find an entry that matches in only
 * fid (without cell) and stale it.
 */

static Bool
stale_unknown_cell (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;
    struct stale_arg *sa = (struct stale_arg *)arg;

    if (e->fid.fid.Volume    == sa->fid.fid.Volume
	&& e->fid.fid.Vnode  == sa->fid.fid.Vnode
	&& e->fid.fid.Unique == sa->fid.fid.Unique)
	stale (e, sa->callback);

    return FALSE;
}

/*
 * Call stale on the entry corresponding to `fid', if any.
 */

void
fcache_stale_entry (VenusFid fid, AFSCallBack callback)
{
    FCacheEntry *e;

    if (fid.Cell == -1) {
	struct stale_arg arg;

	arg.fid = fid;
	arg.callback = callback;

	hashtabforeach (hashtab, stale_unknown_cell, &arg);
	return;
    }

    e = find_entry_nolock (fid);
    if (e == NULL) {
	arla_warnx (ADEBFCACHE,
		    "callback for non-existing file (%d, %u, %u, %u)",
		    fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);
	return;
    }
    stale (e, callback);
}

typedef struct {
    xfs_pag_t pag;
    int32_t cell;
} fc_purgecred;

/*
 * If ptr has cred arg, set it invalid
 */

static Bool
purge_cred (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;
    fc_purgecred *cred = (fc_purgecred *) arg;
    AccessEntry *ae = e->acccache;
    int i;

    if (e->fid.Cell == cred->cell ||  cred->cell == -1) {

	for(i = 0; i < NACCESS ; ++i)
	    if(ae[i].cred == cred->pag) {
		ae[i].cred = ARLA_NO_AUTH_CRED;
		ae[i].access = ANONE;
		if (e->flags.kernelp)
		    install_attr (e, FCACHE2XFSNODE_RIGHT);
		break;
	    }
    }
    return FALSE;
}
    

/*
 * Mark as cred as stale in kernel and all fcache-entries,
 * When cell == -1, flush all creds in this pag.
 */

void
fcache_purge_cred (xfs_pag_t pag, int32_t cell)
{
    fc_purgecred cred;

    cred.pag = pag;
    cred.cell = cell;

    hashtabforeach(hashtab, purge_cred, &cred);
}

/*
 * If ptr was retrieved from cell - volume , try to mark stale
 */

static Bool
purge_volume (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;
    VenusFid *fid = (VenusFid *) arg;
    AFSCallBack broken_callback = {0, 0, CBDROPPED};

    if ((e->fid.Cell == fid->Cell || fid->Cell == -1)
	&& e->fid.fid.Volume == fid->fid.Volume) {
	stale (e, broken_callback);
    }
    return FALSE;
}

/*
 * Mark as stale all entries from cell.volume
 */

void
fcache_purge_volume (VenusFid fid)
{
    hashtabforeach(hashtab, purge_volume, &fid);
}

/*
 * If `ptr' was retrieved from `host', mark it as stale.
 */

static Bool
purge_host (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;
    u_long *host = (u_long *)arg;
    AFSCallBack broken_callback = {0, 0, CBDROPPED};

    assert (*host);
    if (e->host == *host)
	stale (e, broken_callback);
    return FALSE;
}

/*
 * Mark as stale all entries from the host `host'.
 */

void
fcache_purge_host (u_long host)
{
    hashtabforeach (hashtab, purge_host, &host);
}

/*
 * Mark `entry' as not being used.
 */

void
fcache_unused (FCacheEntry *entry)
{
    entry->flags.datausedp = entry->flags.attrusedp = FALSE;
    listdel (lrulist, entry->lru_le);
    entry->lru_le = listaddtail (lrulist, entry);
    assert (entry->lru_le);
    /* 
     * we don't signal lrulist here since we never
     * free the node (usedvnode--);
     */
}

/*
 * make up some status that might be valid for a mount-point
 */

static void
fake_mp_status (FCacheEntry *e)
{
    AFSFetchStatus *status = &e->status;

    status->FileType      = TYPE_DIR;
    status->LinkCount     = 100;
    status->UnixModeBits  = 0777;
    status->ClientModTime = 0;
    status->ServerModTime = 0;
    status->Owner         = 0;
    status->Group         = 0;
}

/*
 * Return true if `entry' is a mountpoint
 */

static Bool
mountpointp (FCacheEntry *entry)
{
    if (entry->status.FileType == TYPE_LINK
	&& entry->status.Length != 0
	&& entry->status.UnixModeBits == 0644)
	return TRUE;
    return FALSE;
}

/*
 * Mark `entry' as mountpoint or a fake mountpoint depending on
 * fake_mp is used or not.
 */

void
fcache_mark_as_mountpoint (FCacheEntry *entry)
{
    if (fake_mp) {
	entry->flags.fake_mp = TRUE;
	fake_mp_status (entry);
    } else {
	entry->flags.mountp = TRUE;
    }
}

/*
 * Update all the relevant parts of `entry' after having received new
 * data from the file server.
 */

static void
update_entry (FCacheEntry *entry,
	      AFSFetchStatus *status,
	      AFSCallBack *callback,
	      AFSVolSync *volsync,
	      u_int32_t host,
	      xfs_pag_t cred)
{
    struct timeval tv;
    AccessEntry *ae;
    unsigned long bitmask = 0141777; /* REG, DIR, STICKY, USR, GRP, OTH */

    if (entry->volume && cell_issuid_by_num (entry->volume->cell))
	bitmask |= 0006000; /* SUID, SGID */

    gettimeofday (&tv, NULL);

    entry->status   = *status;
    entry->status.UnixModeBits &= bitmask;
    if (callback) {
	entry->callback = *callback;
	entry->callback.ExpirationTime += tv.tv_sec;
	add_to_invalidate (entry);
    }
    if (volsync) {
	entry->volsync  = *volsync;
	if (entry->volume)
	    volcache_update_volsync (entry->volume, *volsync);
    }
    entry->host     = host;

    entry->anonaccess = status->AnonymousAccess;
    findaccess (cred, entry->acccache, &ae);
    ae->cred   = cred;
    ae->access = status->CallerAccess;
    if (!entry->flags.mountp && mountpointp (entry))
	fcache_mark_as_mountpoint (entry);
}

/*
 * Update entry, common code for do_read_attr and get_attr_bulk
 */

static void
update_attr_entry (FCacheEntry *entry,
		   AFSFetchStatus *status,
		   AFSCallBack *callback,
		   AFSVolSync *volsync,
		   u_int32_t host,
		   xfs_pag_t cred)
{
    if (entry->flags.datap
	&& entry->status.DataVersion != status->DataVersion) {
	throw_data (entry);
	entry->tokens &= ~(XFS_DATA_R|XFS_DATA_W);
    }
    
    update_entry (entry, status, callback, volsync,
		  host, cred);
    
    entry->tokens |= XFS_ATTR_R;
    entry->flags.attrp = TRUE;
}


/*
 * Give up all callbacks.
 */

int
fcache_giveup_all_callbacks (void)
{
    Listitem *item;

    for (item = listtail(lrulist);
	 item != NULL;
	 item = listprev(lrulist, item)) {
	FCacheEntry *entry = (FCacheEntry *)listdata(item);

	if (entry->flags.attrp && 
	    entry->flags.silly == FALSE &&
	    entry->host != 0) {

	    CredCacheEntry *ce;	
	    ConnCacheEntry *conn;
	    AFSCBFids fids;
	    AFSCBs cbs;
	    int ret;

	    ce = cred_get (entry->fid.Cell, 0, CRED_ANY);
	    assert (ce != NULL);

	    conn = conn_get (entry->fid.Cell, entry->host, afsport,
			     FS_SERVICE_ID, fs_probe, ce);
	    cred_free (ce);

	    if (conn != NULL) {
		fids.len = cbs.len = 1;
		fids.val = &entry->fid.fid;
		cbs.val  = &entry->callback;
		
		ret = RXAFS_GiveUpCallBacks (conn->connection, &fids, &cbs);
		conn_free (conn);
		if (ret)
		    arla_warn (ADEBFCACHE, ret, "RXAFS_GiveUpCallBacks");
	    }
	}
    }
    return 0;			/* XXX */
}

/*
 * Obtain new callbacks for all entries in the cache.
 */

int
fcache_reobtain_callbacks (void)
{
    Listitem *item;
    int ret;

    for (item = listtail(lrulist);
	 item != NULL;
	 item = listprev(lrulist, item)) {
	FCacheEntry *entry = (FCacheEntry *)listdata(item);

	if (entry->flags.usedp && 
	    entry->flags.silly == FALSE &&
	    entry->host != 0) {

	    CredCacheEntry *ce;	
	    ConnCacheEntry *conn;
	    AFSFetchStatus status;
	    AFSCallBack callback;
	    AFSVolSync volsync;
	    VolCacheEntry *vol;
	    int type;

	    ce = cred_get (entry->fid.Cell, 0, CRED_ANY);
	    assert (ce != NULL);

	    conn = conn_get (entry->fid.Cell, entry->host, afsport,
			     FS_SERVICE_ID, fs_probe, ce);
	    /*
	     * does this belong here?
	     */

	    ret = volcache_getbyid (entry->fid.fid.Volume,
				    entry->fid.Cell, ce, &vol, &type);
	    if (ret == 0)
		entry->volume = vol;
	    cred_free (ce);

	    if (conn != NULL) {
		ret = RXAFS_FetchStatus (conn->connection,
					 &entry->fid.fid,
					 &status,
					 &callback,
					 &volsync);
		if (ret)
		    arla_warn (ADEBFCACHE, ret, "RXAFS_FetchStatus");
		else
		    update_entry (entry, &status, &callback, &volsync,
				  rx_HostOf(rx_PeerOf (conn->connection)),
				  ce->cred);
		conn_free (conn);
		fcache_counter.fetch_attr++;
	    }
	}
    }
    return 0;			/* XXX */
}

/*
 * Return true iff there's any point in trying the next fs.
 */

static Bool
try_next_fs (int error, const VenusFid *fid)
{
    switch (error) {
    case RXKADUNKNOWNKEY:
    case ARLA_CALL_DEAD :
    case ARLA_INVALID_OPERATION :
    case ARLA_CALL_TIMEOUT :
    case ARLA_EOF :
    case ARLA_PROTOCOL_ERROR :
    case ARLA_USER_ABORT :
    case ARLA_ADDRINUSE :
    case ARLA_MSGSIZE :
    case ARLA_VSALVAGE :
    case ARLA_VNOSERVICE :
    case ARLA_VOFFLINE :
    case ARLA_VBUSY :
    case ARLA_VIO :
	return TRUE;
    case ARLA_VNOVOL :
    case ARLA_VMOVED :
	if (fid && !volcache_reliable (fid->fid.Volume, fid->Cell))
	    volcache_invalidate (fid->fid.Volume, fid->Cell);
	return TRUE;
    case 0 :
	return FALSE;
    default :
	return FALSE;
    }
}

/*
 * Fetch the attributes for the file in `entry' from the file_server,
 * using the credentials in `ce' and returning the connection in
 * `ret_conn'
 *
 * `entry' must be write-locked.
 *
 * If an error code is returned `fs_server_context' is already freed.
 * If everything is ok, `fs_server_context' must be freed by the caller.
 */

static int
do_read_attr (FCacheEntry *entry,
	      CredCacheEntry *ce,
	      ConnCacheEntry **ret_conn,
	      fs_server_context *ret_context)
{
    int ret = ARLA_CALL_DEAD;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    struct collect_stat collectstat;

    AssertExclLocked(&entry->lock);

    *ret_conn = NULL;

    if (connected_mode == DISCONNECTED) {
	if (entry->flags.attrp)
	    return 0;
	else
	    return ENETDOWN;
    }

    for (conn = find_first_fs (entry, ce, ret_context);
	 conn != NULL;
	 conn = find_next_fs (ret_context, conn, host_downp (ret))) {

	collectstats_start(&collectstat);
	ret = RXAFS_FetchStatus (conn->connection,
				 &entry->fid.fid,
				 &status,
				 &callback,
				 &volsync);
	collectstats_stop(&collectstat, entry, conn,
			  STATISTICS_REQTYPE_FETCHSTATUS, 1);
	arla_warnx (ADEBFCACHE, "trying to fetch status: %d", ret);
	if (!try_next_fs (ret, &entry->fid))
	    break;
    }
    if (ret) {
	if (host_downp(ret))
	    ret = ENETDOWN;
	arla_warn (ADEBFCACHE, ret, "fetch-status");
	free_fs_server_context (ret_context);
	return ret;
    }

    fcache_counter.fetch_attr++;

    update_attr_entry (entry, &status, &callback, &volsync,
		       rx_HostOf (rx_PeerOf (conn->connection)),
		       ce->cred);
    
    AssertExclLocked(&entry->lock);

    *ret_conn = conn;
    return 0;
}


/*
 * Read the attributes of `entry' from the file server and store them.
 * `e' must be write-locked.
 */

int
read_attr (FCacheEntry *entry, CredCacheEntry *ce)
{
    int ret;
    ConnCacheEntry *conn;
    fs_server_context context;

    AssertExclLocked(&entry->lock);

    init_fs_server_context (&context);
    ret = do_read_attr (entry, ce, &conn, &context);
    if (ret)
	return ret;
    free_fs_server_context (&context);
    return 0;
}

/*
 * Read the contents of `entry' from the file server and store it.
 */

static int
read_data (FCacheEntry *entry, ConnCacheEntry *conn, CredCacheEntry *ce)
{
    struct rx_call *call;
    int ret = 0;
    u_int32_t sizefs;
    int fd;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    struct collect_stat collectstat;

    arla_warnx (ADEBMISC, "read_data");

    AssertExclLocked(&entry->lock);

    if (connected_mode == DISCONNECTED) {
	ret = ENETDOWN;
	goto out;
    }

    if (usedbytes + entry->status.Length > highbytes) {
	ret = fcache_need_bytes (entry->status.Length);
	if (ret) goto out;
    }

    if (usedbytes + entry->status.Length > highbytes) {
	arla_warnx (ADEBWARN, "Out of space, not enough cache "
		    "(%d file-length %lu usedbytes)",
		    entry->status.Length,  usedbytes);
	ret = ENOSPC;
	goto out;
    }

    call = rx_NewCall (conn->connection);
    if (call == NULL) {
	arla_warnx (ADEBMISC, "rx_NewCall failed");
	ret = ENOMEM;
	goto out;
    }

    collectstats_start(&collectstat);
    ret = StartRXAFS_FetchData (call, &entry->fid.fid, 
				0, entry->status.Length);
    if(ret) {
	arla_warn (ADEBFCACHE, ret, "fetch-data");
	goto out;
    }

    ret = rx_Read (call, &sizefs, sizeof(sizefs));
    if (ret != sizeof(sizefs)) {
	ret = conv_to_arla_errno(rx_Error(call));
	arla_warn (ADEBFCACHE, ret, "Error reading length");
	rx_EndCall(call, 0);
	goto out;
    }
    sizefs = ntohl (sizefs);

    fd = fcache_open_file (entry, O_RDWR);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open cache file %u",
		   (unsigned)entry->index);
	rx_EndCall(call, 0);
	goto out;
    }

    if (ftruncate(fd, sizefs) < 0) {
	close(fd);
	ret = errno;
	rx_EndCall(call, 0);
	goto out;
    }

    ret = copyrx2fd (call, fd, 0, sizefs);
    close (fd);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "copyrx2fd");
	rx_EndCall(call, ret);
	goto out;
    }

    ret = rx_EndCall (call, EndRXAFS_FetchData (call,
						&status,
						&callback,
						&volsync));
    if(ret) {
	arla_warn (ADEBFCACHE, ret, "rx_EndCall");
	goto out;
    }
    collectstats_stop(&collectstat, entry, conn,
		      STATISTICS_REQTYPE_FETCHDATA, sizefs);

    fcache_counter.fetch_data++;
    
    update_entry (entry, &status, &callback, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);
    entry->length = sizefs;
    usedbytes += sizefs;		/* XXX - sync */

    entry->flags.datap = TRUE;
    entry->tokens |= XFS_DATA_R | XFS_DATA_W | XFS_OPEN_NR | XFS_OPEN_NW;

out:
    AssertExclLocked(&entry->lock);

    return ret;
}

/*
 * Write the contents of the cache file back to the file server.
 */

int
write_data (FCacheEntry *entry, AFSStoreStatus *storestatus,
	    CredCacheEntry *ce)
{
     ConnCacheEntry *conn;
     struct rx_call *call;
     int ret = ARLA_CALL_DEAD;
     u_int32_t sizefs;
     int fd;
     struct stat statinfo;
     AFSFetchStatus status;
     AFSVolSync volsync;
     fs_server_context context;

     AssertExclLocked(&entry->lock);

     /* Don't write data to deleted files */
     if (entry->flags.silly)
	 return 0;

     fd = fcache_open_file (entry, O_RDWR);
     if (fd < 0) {
	 ret = errno;
	 arla_warn (ADEBFCACHE, ret, "open cache file %u",
		    (unsigned)entry->index);
	 return ret;
     }

     if (fstat (fd, &statinfo) < 0) {
	 ret = errno;
	 close (fd);
	 arla_warn (ADEBFCACHE, ret, "stat cache file %u",
		    (unsigned)entry->index);
	 return ret;
     }

     sizefs = statinfo.st_size;

     fcache_update_length (entry, sizefs);
     if (connected_mode != CONNECTED) {
	 close (fd);
	 return 0;
     }

     for (conn = find_first_fs (entry, ce, &context);
	  conn != NULL;
	  conn = find_next_fs (&context, conn, host_downp (ret))) {

	 call = rx_NewCall (conn->connection);
	 if (call == NULL) {
	     arla_warnx (ADEBMISC, "rx_NewCall failed");
	     ret = ENOMEM;
	     break;
	 }

	 ret = StartRXAFS_StoreData (call, &entry->fid.fid,
				     storestatus,
				     0,
				     sizefs,
				     sizefs);
	 if (host_downp(ret)) {
	     rx_EndCall(call, ret);
	     continue;
	 } else if (ret) {
	     arla_warn (ADEBFCACHE, ret, "store-data");
	     rx_EndCall(call, 0);
	     break;
	 }

	 ret = copyfd2rx (fd, call, 0, sizefs);
	 if (ret) {
	     rx_EndCall(call, ret);
	     arla_warn (ADEBFCACHE, ret, "copyfd2rx");
	     break;
	 }

	 ret = EndRXAFS_StoreData (call,
				   &status,
				   &volsync);
	 if (ret) {
	     rx_EndCall (call, ret);
	     arla_warnx (ADEBFCACHE, "EndRXAFS_StoreData");
	     break;
	 }

	 ret = rx_EndCall (call, 0);
	 if (ret) {
	     arla_warn (ADEBFCACHE, ret, "rx_EndCall");
	 }
	 break;
     }

     if (conn != NULL) {
	 if (ret == 0) {
	     fcache_counter.store_data++;
	     update_entry (entry, &status, NULL, &volsync,
			   rx_HostOf(rx_PeerOf(conn->connection)),
			   ce->cred);
	 } else {
	     ftruncate (fd, 0);
	     entry->length = 0;
	     /* undo the work of the fcache_update_size just above the loop */
	     usedbytes -= sizefs; 
	     entry->flags.datap = FALSE;
	 }
     }
     if (host_downp(ret))
	 ret = ENETDOWN;
     free_fs_server_context (&context);
     AssertExclLocked(&entry->lock);
     close (fd);
     return ret;
}

/*
 * Truncate the file in `entry' to `size' bytes.
 */

int
truncate_file (FCacheEntry *entry, off_t size, CredCacheEntry *ce)
{
    ConnCacheEntry *conn;
    struct rx_call *call;
    int ret = ARLA_CALL_DEAD;
    AFSStoreStatus storestatus;
    int fd;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fs_server_context context;

    AssertExclLocked(&entry->lock);

    fd = fcache_open_file (entry, O_RDWR);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open fache file %u",
		   (unsigned)entry->index);
	return ret;
    }

    if(ftruncate (fd, size) < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "ftruncate %ld", (long)size);
	close (fd);
	return ret;
    }
    
    close (fd);

    if (!entry->flags.datap)
	entry->length = 0;

    fcache_update_length (entry, size);

    if (connected_mode != CONNECTED)
	return 0;

    ret = ENETDOWN;
    for (conn = find_first_fs (entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	call = rx_NewCall (conn->connection);
	if (call == NULL) {
	    arla_warnx (ADEBMISC, "rx_NewCall failed");
	    ret = ENOMEM;
	    break;
	}

	storestatus.Mask = 0;
	ret = StartRXAFS_StoreData (call,
				    &entry->fid.fid, 
				    &storestatus,
				    size,
				    0,
				    size);
	if (host_downp(ret)) {
	    rx_EndCall(call, ret);
	    continue;
	} else if(ret) {
	    arla_warn (ADEBFCACHE, ret, "store-data");
	    rx_EndCall(call, 0);
	    break;
	}


	ret = EndRXAFS_StoreData (call,
				  &status,
				  &volsync);
	if (ret) {
	    rx_EndCall (call, ret);
	    arla_warnx (ADEBFCACHE, "EndRXAFS_StoreData");
	    break;
	}

	ret = rx_EndCall (call, 0);
	if (ret) {
	    arla_warn (ADEBFCACHE, ret, "rx_EndCall");
	}
	break;
    }

    if (ret == 0) {
	fcache_counter.store_data++;
	update_entry (entry, &status, NULL, &volsync,
		      rx_HostOf(rx_PeerOf(conn->connection)),
		      ce->cred);
    }
    if (host_downp(ret))
	ret = ENETDOWN;
    free_fs_server_context (&context);
    AssertExclLocked(&entry->lock);
    return ret;
}

/*
 * Set the attributes of the file in `entry' to `status'.
 */

int
write_attr (FCacheEntry *entry,
	    const AFSStoreStatus *store_status,
	    CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    AFSFetchStatus status;
    AFSVolSync volsync;

    AssertExclLocked(&entry->lock);

    /* Don't write attributes to deleted files */
    if (entry->flags.silly)
	return 0;

    if (connected_mode == CONNECTED) {
	ConnCacheEntry *conn;
	fs_server_context context;
	u_int32_t host = 0;

	for (conn = find_first_fs (entry, ce, &context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, host_downp (ret))) {

	    host = rx_HostOf (rx_PeerOf (conn->connection));

	    ret = RXAFS_StoreStatus (conn->connection,
				     &entry->fid.fid,
				     store_status,
				     &status,
				     &volsync);
	    if (host_downp(ret)) {
		continue;
	    } else if (ret) {
		arla_warn (ADEBFCACHE, ret, "store-status");
		free_fs_server_context (&context);
		goto out;
	    }
	    break;
	}
	free_fs_server_context (&context);

	if (host_downp(ret)) {
	    ret = ENETDOWN;
	    goto out;
	}

	update_entry (entry, &status, NULL, &volsync, host, ce->cred);
    } else {
	fcache_counter.store_attr++;
	if (store_status->Mask & SS_MODTIME) {
	    entry->status.ClientModTime = store_status->ClientModTime;
	    entry->status.ServerModTime = store_status->ClientModTime;
	}
	if (store_status->Mask & SS_OWNER)
	    entry->status.Owner = store_status->Owner;
	if (store_status->Mask & SS_GROUP)
	    entry->status.Group = store_status->Group;
	if (store_status->Mask & SS_MODEBITS)
	    entry->status.UnixModeBits = store_status->UnixModeBits;
	if (store_status->Mask & SS_SEGSIZE)
	    entry->status.SegSize = store_status->SegSize;
    }

out:
    AssertExclLocked(&entry->lock);

    return ret;
}

/*
 * Create a file.
 */

int
create_file (FCacheEntry *dir_entry,
	     const char *name, AFSStoreStatus *store_attr,
	     VenusFid *child_fid, AFSFetchStatus *fetch_attr,
	     CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    int fd;
    u_int32_t host;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode == CONNECTED) {
	ConnCacheEntry *conn;
	fs_server_context context;

	for (conn = find_first_fs (dir_entry, ce, &context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, host_downp (ret))) {

	    host = rx_HostOf (rx_PeerOf (conn->connection));

	    ret = RXAFS_CreateFile (conn->connection,
				    &dir_entry->fid.fid,
				    name,
				    store_attr,
				    &OutFid,
				    fetch_attr,
				    &status,
				    &callback,
				    &volsync);
	    if (host_downp(ret)) {
		continue;
	    } else if (ret) {
		free_fs_server_context (&context);
		arla_warn (ADEBFCACHE, ret, "CreateFile");
		goto out;
	    }
	    break;
	}
	free_fs_server_context (&context);

	if (host_downp(ret)) {
	    ret = ENETDOWN;
	    goto out;
	}

	update_entry (dir_entry, &status, &callback, &volsync,
		      host, ce->cred);
    } else {
	static int fakefid = 1001;

	OutFid.Volume = dir_entry->fid.fid.Volume;
	OutFid.Vnode  = fakefid;
	OutFid.Unique = fakefid;
	fakefid += 2;

	fetch_attr->InterfaceVersion = 1;
	fetch_attr->FileType         = TYPE_FILE;
	fetch_attr->LinkCount        = 1;
	fetch_attr->Length	     = 0;
	fetch_attr->DataVersion      = 1;
	fetch_attr->Author           = store_attr->Owner;
	fetch_attr->Owner            = store_attr->Owner;
	fetch_attr->CallerAccess     = dir_entry->status.CallerAccess;
	fetch_attr->AnonymousAccess  = dir_entry->status.AnonymousAccess;
	fetch_attr->UnixModeBits     = store_attr->UnixModeBits;
	fetch_attr->ParentVnode      = dir_entry->fid.fid.Vnode;
	fetch_attr->ParentUnique     = dir_entry->fid.fid.Unique;
	fetch_attr->SegSize          = store_attr->SegSize;
	fetch_attr->ClientModTime    = store_attr->ClientModTime;
	fetch_attr->ServerModTime    = store_attr->ClientModTime;
	fetch_attr->Group            = store_attr->Group;
	fetch_attr->SyncCount        = 0;
	fetch_attr->spare1           = 0;
	fetch_attr->spare2           = 0;
	fetch_attr->spare3           = 0;
	fetch_attr->spare4           = 0;

	host = dir_entry->host;
    }

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  host, ce->cred);

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;

    fd = fcache_open_file (child_entry, O_WRONLY);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open cache file %u",
		   (unsigned)child_entry->index);
	fcache_release(child_entry);
	goto out;
    }
    if (ftruncate (fd, 0) < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "ftruncate cache file %u",
		   (unsigned)child_entry->index);
	close (fd);
	fcache_release(child_entry);
	goto out;
    }
    close (fd);
    child_entry->length = 0;

    child_entry->flags.datap = TRUE;
    child_entry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;
	
    fcache_release(child_entry);

out:
    AssertExclLocked(&dir_entry->lock);

    return ret;
}

/*
 * Create a directory.
 */

int
create_directory (FCacheEntry *dir_entry,
		  const char *name, AFSStoreStatus *store_attr,
		  VenusFid *child_fid, AFSFetchStatus *fetch_attr,
		  CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    u_int32_t host;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode == CONNECTED) {
	ConnCacheEntry *conn;
	fs_server_context context;

	for (conn = find_first_fs (dir_entry, ce, &context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, host_downp (ret))) {

	    host = rx_HostOf(rx_PeerOf(conn->connection));

	    ret = RXAFS_MakeDir (conn->connection,
				 &dir_entry->fid.fid,
				 name,
				 store_attr,
				 &OutFid,
				 fetch_attr,
				 &status,
				 &callback,
				 &volsync);

	    if (host_downp(ret)) {
		continue;
	    } else if (ret) {
		free_fs_server_context (&context);
		arla_warn (ADEBFCACHE, ret, "MakeDir");
		goto out;
	    }
	    break;
	}
	free_fs_server_context (&context);

	if (host_downp(ret)) {
	    ret = ENETDOWN;
	    goto out;
	}

	update_entry (dir_entry, &status, &callback, &volsync,
		      host, ce->cred);
    } else {
	static int fakedir = 1000;

	OutFid.Volume = dir_entry->fid.fid.Volume;
	OutFid.Vnode  = fakedir;
	OutFid.Unique = fakedir;
	fakedir += 2;

	fetch_attr->InterfaceVersion = 1;
	fetch_attr->FileType         = TYPE_DIR;
	fetch_attr->LinkCount        = 2;
	fetch_attr->Length           = AFSDIR_PAGESIZE;
	fetch_attr->DataVersion      = 1;
	fetch_attr->Author           = store_attr->Owner;
	fetch_attr->Owner            = store_attr->Owner;
	fetch_attr->CallerAccess     = dir_entry->status.CallerAccess;
	fetch_attr->AnonymousAccess  = dir_entry->status.AnonymousAccess;
	fetch_attr->UnixModeBits     = store_attr->UnixModeBits;
	fetch_attr->ParentVnode      = dir_entry->fid.fid.Vnode;
	fetch_attr->ParentUnique     = dir_entry->fid.fid.Unique;
	fetch_attr->SegSize          = store_attr->SegSize;
	fetch_attr->ClientModTime    = store_attr->ClientModTime;
	fetch_attr->ServerModTime    = store_attr->ClientModTime;
	fetch_attr->Group            = store_attr->Group;
	fetch_attr->SyncCount        = 0;
	fetch_attr->spare1           = 0;
	fetch_attr->spare2           = 0;
	fetch_attr->spare3           = 0;
	fetch_attr->spare4           = 0;

	host = dir_entry->host;
    }

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    if(child_entry->flags.datap)
	throw_data (child_entry);

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  host, ce->cred);

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;

    ret = adir_mkdir (child_entry, child_fid->fid, dir_entry->fid.fid);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "adir_mkdir");
	fcache_release(child_entry);
	goto out;
    }

    child_entry->flags.datap = TRUE;
    child_entry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;
	
    fcache_release(child_entry);

out:
    AssertExclLocked(&dir_entry->lock);
    return ret;
}

/*
 * Create a symbolic link.
 *
 * Note: create_symlink->flags.kernelp is not set on success
 * and that must be done by the caller.
 */

int
create_symlink (FCacheEntry *dir_entry,
		const char *name, AFSStoreStatus *store_attr,
		VenusFid *child_fid, AFSFetchStatus *fetch_attr,
		const char *contents,
		CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    ConnCacheEntry *conn;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSVolSync volsync;
    AFSFetchStatus new_status;
    u_int32_t host;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_Symlink (conn->connection,
			     &dir_entry->fid.fid,
			     name,
			     contents,
			     store_attr,
			     &OutFid,
			     fetch_attr,
			     &new_status,
			     &volsync);
	if (host_downp(ret)) {
	    continue;
	} else if (ret) {
	    arla_warn (ADEBFCACHE, ret, "Symlink");
	    free_fs_server_context (&context);
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &new_status, NULL, &volsync,
		  host, ce->cred);

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  host, ce->cred);

    /* 
     * flags.kernelp is set in cm_symlink since the symlink
     * might be a mountpoint and this entry is never install
     * into the kernel.
     */

    child_entry->flags.attrp = TRUE;
    child_entry->tokens |= XFS_ATTR_R;
	
    fcache_release(child_entry);

out:
    AssertExclLocked(&dir_entry->lock);
    return ret;
}

/*
 * Create a hard link.
 */

int
create_link (FCacheEntry *dir_entry,
	     const char *name,
	     FCacheEntry *existing_entry,
	     CredCacheEntry *ce)
{
    int ret = 0;
    ConnCacheEntry *conn;
    AFSFetchStatus new_status;
    AFSFetchStatus status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_Link (conn->connection,
			  &dir_entry->fid.fid,
			  name,
			  &existing_entry->fid.fid,
			  &new_status,
			  &status,
			  &volsync);
	if (host_downp(ret)) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "Link");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &status, NULL, &volsync,
		  host, ce->cred);

    update_entry (existing_entry, &new_status, NULL, NULL,
		  host, ce->cred);

out:
    AssertExclLocked(&dir_entry->lock);
    return ret;
}

/*
 * Remove a file from a directory.
 */

int
remove_file (FCacheEntry *dir_entry, const char *name, CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_RemoveFile (conn->connection,
				&dir_entry->fid.fid,
				name,
				&status,
				&volsync);
	if (host_downp(ret)) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "RemoveFile");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &status, NULL, &volsync,
		  host, ce->cred);

out:
    AssertExclLocked(&dir_entry->lock);
    return ret;
}

/*
 * Remove a directory from a directory.
 */

int
remove_directory (FCacheEntry *dir_entry,
		  const char *name,
		  CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_RemoveDir (conn->connection,
			       &dir_entry->fid.fid,
			       name,
			       &status,
			       &volsync);
	if (host_downp(ret)) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "RemoveDir");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &status, NULL, &volsync,
		  host, ce->cred);

out:
    AssertExclLocked(&dir_entry->lock);
    return ret;
}

/*
 * Rename a file
 */

int
rename_file (FCacheEntry *old_dir,
	     const char *old_name,
	     FCacheEntry *new_dir,
	     const char *new_name,
	     CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    ConnCacheEntry *conn;
    AFSFetchStatus orig_status, new_status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    AssertExclLocked(&old_dir->lock);
    AssertExclLocked(&new_dir->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    for (conn = find_first_fs (old_dir, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_Rename (conn->connection,
			    &old_dir->fid.fid,
			    old_name,
			    &new_dir->fid.fid,
			    new_name,
			    &orig_status,
			    &new_status,
			    &volsync);
	if (host_downp(ret)) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "Rename");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (old_dir, &orig_status, NULL, &volsync,
		  host, ce->cred);

    update_entry (new_dir, &new_status, NULL, &volsync,
		  host, ce->cred);

out:
    AssertExclLocked(&old_dir->lock);
    AssertExclLocked(&new_dir->lock);
    return ret;
}

/*
 * Return the fid to the root.
 */

int
getroot (VenusFid *res, CredCacheEntry *ce)
{
     VolCacheEntry *ve;
     VenusFid fid;
     const char *root_volume = volcache_get_rootvolume ();
     int type;
     int ret;
     const char *this_cell = cell_getthiscell ();
     int32_t this_cell_id;
     static int busy_wait = 0;

     if (dynroot_enablep()) {
	 res->Cell = dynroot_cellid ();
	 res->fid.Volume = dynroot_volumeid ();
	 res->fid.Vnode = fid.fid.Unique = 1;

	 return 0;
     }

     this_cell_id = cell_name2num (this_cell);
     if (this_cell_id == -1)
	 arla_errx (1, ADEBERROR, "cell %s does not exist", this_cell);

     while (busy_wait)
	 LWP_WaitProcess (getroot);

     busy_wait = 1;
     ret = volcache_getbyname (root_volume, this_cell_id, ce, &ve, &type);
     busy_wait = 0;
     LWP_NoYieldSignal (getroot);
     if (ret) {
	 arla_warn (ADEBWARN, ret,
		    "Cannot find the root volume (%s) in cell %s",
		    root_volume, this_cell);
	 return ret;
     }

     fid.Cell = this_cell_id;
     if (ve->entry.flags & VLF_ROEXISTS) {
	 fid.fid.Volume = ve->entry.volumeId[ROVOL];
     } else if (ve->entry.flags & VLF_RWEXISTS) {
	 arla_warnx(ADEBERROR,
		    "getroot: %s in cell %s is missing a RO clone, not good",
		    root_volume, this_cell);
	 fid.fid.Volume = ve->entry.volumeId[RWVOL];
     } else {
	 arla_errx(1, ADEBERROR,
		   "getroot: %s in cell %s has no RW or RO clone?",
		   root_volume, this_cell);
     }
     fid.fid.Vnode = fid.fid.Unique = 1;

     volcache_free (ve);

     *res = fid;
     return 0;
}

/*
 * Return the type for this volume.
 */

static long
gettype (int32_t volid, const VolCacheEntry *ve)
{
     int i;

     for (i = RWVOL; i <= BACKVOL; ++i)
	  if (ve->entry.volumeId[i] == volid)
	      return i;
     assert (FALSE);
     return -1; /* NOT REACHED */
}

/*
 * Return the entry for `fid' or NULL.
 */

int
fcache_find (FCacheEntry **res, VenusFid fid)
{
    *res = find_entry (fid);
    if (*res != NULL)
	return 0;
    else
	return -1;
}

/*
 * Return the entry for `fid'.  If it's not cached, add it.
 */

int
fcache_get (FCacheEntry **res, VenusFid fid, CredCacheEntry *ce)
{
    FCacheEntry *old;
    FCacheEntry *e;
    int type, i;
    int error;

    *res = NULL;

    old = find_entry (fid);
    if (old) {
	assert (old->flags.usedp);
	*res = old;
	return 0;
    }

    e = find_free_entry ();
    assert (e != NULL);

    old = find_entry (fid);
    if (old) {
	AssertExclLocked(&e->lock);
	ReleaseWriteLock (&e->lock);

	e->lru_le = listaddtail (lrulist, e);
	assert(e->lru_le);

	assert (old->flags.usedp);
	*res = old;
	return 0;
    }

    e->fid     	       = fid;
    e->refcount        = 0;
    e->host	       = 0;
    e->length          = 0;
    memset (&e->status,   0, sizeof(e->status));
    memset (&e->callback, 0, sizeof(e->callback));
    memset (&e->volsync,  0, sizeof(e->volsync));
    for (i = 0; i < NACCESS; i++) {
	e->acccache[i].cred = ARLA_NO_AUTH_CRED;
	e->acccache[i].access = 0;
    }
    e->anonaccess      = 0;
    e->flags.usedp     = TRUE;
    e->flags.datap     = FALSE;
    e->flags.attrp     = FALSE;
    e->flags.attrusedp = FALSE;
    e->flags.datausedp = FALSE;
    e->flags.extradirp = FALSE;
    e->flags.mountp    = FALSE;
    e->flags.fake_mp   = FALSE;
    e->flags.vol_root  = FALSE;
    e->flags.kernelp   = FALSE;
    e->flags.sentenced = FALSE;
    e->flags.silly     = FALSE;
    e->tokens          = 0;
    memset (&e->parent, 0, sizeof(e->parent));
    e->lru_le = listaddhead (lrulist, e);
    assert(e->lru_le);
    e->invalid_ptr     = -1;
    e->volume          = NULL;
    e->priority	       = fprio_get(fid);
    e->hits	       = 0;
    e->cleanergen      = 0;
    
    hashtabadd (hashtab, e);

    if (connected_mode != DISCONNECTED) {
	VolCacheEntry *vol;

	error = volcache_getbyid (fid.fid.Volume, fid.Cell, ce, &vol, &type);
	if (error) {
	    e->volume = NULL;
	    *res = NULL;
	    ReleaseWriteLock (&e->lock);
	    return error;
	}
	e->volume = vol;
    }

    *res = e;
    return 0;
}

/*
 * Release the lock on `e' and mark it as stale if it has been sentenced.
 */

void
fcache_release (FCacheEntry *e)
{
    AssertExclLocked(&e->lock);

    ReleaseWriteLock (&e->lock);

    if (e->flags.sentenced) {
	AFSCallBack broken_callback = {0, 0, CBDROPPED};

	stale (e, broken_callback);
	e->flags.sentenced = FALSE;
    }
}

/*
 *
 */

static Bool
uptodatep (FCacheEntry *e)
{
    struct timeval tv;
    assert (e->flags.usedp);

    if (connected_mode != CONNECTED && 
	connected_mode != FETCH_ONLY)
	return TRUE;

    gettimeofday(&tv, NULL);
    
    if (tv.tv_sec < e->callback.ExpirationTime &&
	e->callback.CallBackType != CBDROPPED &&
	(e->callback.CallBackType != 0
	 || e->volume->volsync.spare1 != e->volsync.spare1))
        return TRUE;
    
    return FALSE;
}

/*
 * The idea is that we start to stat everything after the prefered
 * entry, everything before that is probably not useful to get, the
 * user is probably trying to stat() everything _after_ that node.
 * This might be somewhat bogus, but we dont care (for now).
 */

struct bulkstat {
    int 		len;		   /* used entries in fids and names */
    AFSFid		fids[AFSCBMAX];    /* fids to fetch */
    char		*names[AFSCBMAX];  /* names it install */
    AFSFid		*used;		   /* do we have a prefered node */
    CredCacheEntry	*ce;		   /* cred to use */
};

typedef union {
    struct xfs_message_installnode node;
    struct xfs_message_installattr attr;
} xfs_message_install_node_attr;

static void
bulkstat_help_func (VenusFid *fid, const char *name, void *ptr)
{
    struct bulkstat *bs = (struct bulkstat *) ptr;
    AccessEntry *ae;
    FCacheEntry key;
    FCacheEntry *e;

    /* Is bs full ? */
    if (bs->len > fcache_bulkstatus_num)
	return;

    /* Ignore . and .. */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	return;

    /* 
     * Do we have a prefered node, and is this the one. If we don't know
     * the name of the node (ie bs.names[0] == NULL), fill it in.
     * Set bs->used to NULL it indicate that we should start stat stuff
     * from here, remeber that bs->len == 1 if bs->used is set.
     */
    if (bs->used) { 
	if (memcmp(bs->used, &fid->fid, sizeof(fid->fid)) == 0) {
	    if (bs->names[0] == NULL)
		bs->names[0] = strdup (name);
	    bs->used = NULL; /* stat everything after this */
	}
	return;
    }

    /*
     * Already cached for this pag ?
     */
    key.fid = *fid;
    e = (FCacheEntry *)hashtabsearch (hashtab, (void *)&key);
    if (e 
	&& e->flags.usedp
	&& e->flags.attrp
	&& uptodatep (e)
	&& findaccess (bs->ce->cred, e->acccache, &ae) == TRUE) {
	arla_warnx (ADEBFCACHE, 
		    "bulkstat_help_func: already cached "
		    "(%d.%d.%d.%d) name: %s",
		    fid->Cell, fid->fid.Volume, fid->fid.Vnode, 
		    fid->fid.Unique, name);
	return;
    }

    if (fcache_enable_bulkstatus == 2) {
	/* cache the name for the installnode */
	bs->names[bs->len] = strdup (name);
	if (bs->names[bs->len] == NULL)
	    return;
    } else {
	bs->names[bs->len] = NULL;
    }
    

    bs->fids[bs->len] = fid->fid;
    bs->len++;    
}

/*
 * Do bulkstat for ``parent_entry''. Make sure that ``prefered_entry''
 * is in the list of fids it not NULL, and it ``prefered_name'' is NULL
 * try to find it in the list files in the directory.
 *
 * 			Entry		Success		Failure
 * parent_entry		locked		locked		locked
 * prefered_entry	locked		locked		locked
 *   or if NULL		if set to NULL must not be locked
 * prefered_fid		related fcache-entry must not be locked
 * ce			not NULL
 */

static int
get_attr_bulk (FCacheEntry *parent_entry, 
	       FCacheEntry *prefered_entry,
	       VenusFid *prefered_fid, 
	       const char *prefered_name,
	       CredCacheEntry *ce)
{
    fs_server_context context;
    ConnCacheEntry *conn;
    struct bulkstat bs;
    AFSBulkStats stats;
    AFSVolSync sync;
    AFSCBFids fids;
    fbuf the_fbuf;
    int ret, fd;
    AFSCBs cbs;
    int i;
    int len;
    u_int32_t host;

    arla_warnx (ADEBFCACHE, "get_attr_bulk");

    if (fcache_enable_bulkstatus == 0)
	return -1;

    if (!parent_entry->flags.datap) {
	arla_warnx (ADEBFCACHE, "get_attr_bulk: parent doesn't have data");
	return -1;
    }
    
    fids.val = bs.fids;

    memset (bs.names, 0, sizeof(bs.names));
    memset (bs.fids,  0, sizeof(bs.fids));
    bs.len	= 0;
    bs.ce	= ce;
    bs.used	= NULL;
    
    /*
     * If we have a prefered_entry, and that to the first entry in the
     * array. This is used later. If we find the prefered_entry in the
     * directory-structure its ignored.
     */

    if (prefered_fid) {
	arla_warnx (ADEBFCACHE, "get_attr_bulk: using prefered_entry");
	bs.used			= &prefered_fid->fid;
	fids.val[bs.len]	= prefered_fid->fid;
	if (prefered_name != NULL) {
	    bs.names[bs.len]	= strdup(prefered_name);
	    if (bs.names[bs.len] == NULL)
		return ENOMEM;
	} else {
	    bs.names[bs.len]    = NULL;
	}
	bs.len++;
    }

    ret = fcache_get_fbuf (parent_entry, &fd, &the_fbuf,
			   O_RDONLY, FBUF_READ|FBUF_SHARED);
    if (ret)
	return ret;

    ret = fdir_readdir (&the_fbuf,
			bulkstat_help_func,
			&bs,
			&parent_entry->fid);
    fbuf_end (&the_fbuf);
    close (fd);
    if (ret)
	goto out_names;
    
    fids.len = bs.len;

    /*
     * Don't do BulkStatus when fids.len == 0 since we should never do it.
     * There should at least be the node that we want in the BulkStatus.
     */

    if (fids.len == 0) {
	if (prefered_fid)
	    arla_warnx (ADEBERROR, 
			"get_attr_bulk: "
			"prefered_fid not found in dir");
	/* XXX MAGIC send it back so we don't do it again soon */
	parent_entry->hits -= 64;
	ret = EINVAL;
	goto out_names;
    }

    /*
     * XXX if there is a prefered fid, and and we didn't find the name for it
     * return an error.
     */

    if (prefered_fid && bs.names[0] == NULL) {
	arla_warnx (ADEBFCACHE, 
		    "get_attr_bulk: didn't find prefered_fid's name");
	ret = EINVAL;
	goto out_names;
    }
    
    ret = ARLA_CALL_DEAD;

    for (conn = find_first_fs (parent_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	stats.val = NULL;
	cbs.val   = NULL;
	stats.len = cbs.len = 0;

	ret = RXAFS_BulkStatus (conn->connection, &fids, &stats, &cbs, &sync);
	if (ret) {
	    free (stats.val);
	    free (cbs.val);
	}
	if (!try_next_fs (ret, &parent_entry->fid))
	    break;	
    }

    if (conn != NULL)
	host = rx_HostOf (rx_PeerOf (conn->connection));
    free_fs_server_context (&context);

    if (ret) {
	ret = ENETDOWN;
	goto out_names;
    }

    arla_warnx (ADEBFCACHE,"get_attr_bulk: BulkStatus returned %d",ret);
    
    len = min(fids.len, min(stats.len, cbs.len));

    /*
     * Save results of bulkstatus
     */

    if (ret == 0) {
	FCacheEntry *e;

	fcache_counter.fetch_attr_bulk += len;

	for (i = 0; i < len && ret == 0; i++) {
	    if (prefered_entry && i == 0) {
		e = prefered_entry;
	    } else {
		VenusFid fid;

		fid.Cell = parent_entry->fid.Cell;
		fid.fid = fids.val[i];

		e = find_entry_nolock (fid);
		if (e != NULL && CheckLock(&e->lock) != 0)
		    continue;

		ret = fcache_get (&e, fid, ce);
		if (ret)
		    break;
	    }
	    update_attr_entry (e,
			       &stats.val[i],
			       &cbs.val[i],
			       &sync,
			       host,
			       ce->cred);
	    e->parent		= parent_entry->fid;
	    if (prefered_entry != e) {
		fcache_release(e);
	    }
	}
    }

    /*
     * Insert result into kernel
     */

    if (fcache_enable_bulkstatus == 2 && ret == 0)  {
	xfs_message_install_node_attr msg[AFSCBMAX];
	struct xfs_msg_node *node;
	xfs_handle *parent;
	FCacheEntry *e;
	VenusFid fid;
	int j;

	fid.Cell = parent_entry->fid.Cell;
	for (i = 0 , j = 0; i < len && ret == 0; i++) {

	    fid.fid = fids.val[i];
	    
	    if (prefered_entry && i == 0) {
		e = prefered_entry;
		ret = 0;
	    } else {
		e = find_entry_nolock (fid);
		if (e != NULL && CheckLock(&e->lock) != 0)
		    continue;

		ret = fcache_get (&e, fid, ce);
	    }
	    if (ret == 0) {
		u_int tokens;

		arla_warnx (ADEBFCACHE, "installing %d.%d.%d\n",
			    e->fid.fid.Volume,
			    e->fid.fid.Vnode,
			    e->fid.fid.Unique);
		e->flags.attrusedp 	= TRUE;

		/*
		 * Its its already installed, just update with installattr
		 */

		e->tokens			|= XFS_ATTR_R;
		tokens				= e->tokens;
		if (!e->flags.kernelp || !e->flags.datausedp)
		    tokens			&= ~XFS_DATA_MASK;

		if (e->flags.kernelp) {
		    msg[j].attr.header.opcode	= XFS_MSG_INSTALLATTR;
		    node			= &msg[j].attr.node;
		    parent			= NULL;
		} else {
		    msg[j].node.header.opcode	= XFS_MSG_INSTALLNODE;
		    node			= &msg[j].node.node;
		    parent			= &msg[j].node.parent_handle;
		    e->flags.kernelp		= TRUE;
		    strlcpy (msg[j].node.name, bs.names[i],
			     sizeof(msg[j].node.name));
		}
		node->tokens = tokens;

		/*
		 * Don't install symlink since they might be
		 * mount-points.
		 */

		if (e->status.FileType != TYPE_LINK) {
		    fcacheentry2xfsnode (&e->fid,
					 &e->fid,
					 &stats.val[i],
					 node, 
					 parent_entry->acccache,
					 FCACHE2XFSNODE_ALL);

		    if (parent)
			*parent = *(struct xfs_handle*) &parent_entry->fid;
		    j++;
		}
		if (!(prefered_entry && i == 0)) {
		    fcache_release(e);
		}
	    }
	}

	/*
	 * Install if there is no error and we have something to install
	 */

	if (ret == 0 && j != 0)
	    ret = xfs_send_message_multiple_list (kernel_fd,
						  (struct xfs_message_header *) msg,
						  sizeof (msg[0]),
						  j);
	/* We have what we wanted, ignore errors */
  	if (ret && i > 0 && prefered_entry)
	    ret = 0;
    }

    free (stats.val);
    free (cbs.val);

 out_names:
    for (i = 0 ; i < bs.len && ret == 0; i++) {
	free (bs.names[i]);
    }

    arla_warnx (ADEBFCACHE, "get_attr_bulk: returned %d", ret);

    return ret;
}


/*
 * fetch attributes for the note `entry' with the rights `ce'.  If
 * `parent_entry' is no NULL, its used for doing bulkstatus when guess
 * necessary. If there is a named associated with `entry' it should be
 * filled into `prefered_name' as that will be used for guessing that
 * nodes should be bulkstat:ed.
 *
 * If there is no bulkstatus done, a plain FetchStatus is done.
 */

int
fcache_verify_attr (FCacheEntry *entry, FCacheEntry *parent_entry,
		    const char *prefered_name, CredCacheEntry* ce)
{
    FCacheEntry *parent = parent_entry;
    AccessEntry *ae;
    int error;

    if (dynroot_is_dynrootp (entry))
	return dynroot_get_attr (entry, ce);

    if (entry->flags.usedp
	&& entry->flags.attrp
	&& uptodatep(entry)
	&& findaccess (ce->cred, entry->acccache, &ae) == TRUE)
    {
	arla_warnx (ADEBFCACHE, "fcache_get_attr: have attr");
	fcache_counter.fetch_attr_cached++;
	return 0;
    }

    /* 
     * XXX is this right ?
     * Dont ask fileserver if this file is deleted
     */
    if (entry->flags.silly) {
	entry->tokens |= XFS_ATTR_R;
	entry->flags.attrp = TRUE;
	return 0;
    }

    /*
     * If `entry' is a root-node or the parent is
     * un-initialized, don't bother bulkstatus.
     */
    if (entry->fid.fid.Vnode        != 1
	&& entry->fid.fid.Unique    != 1
	&& !entry->flags.mountp
	&& !entry->flags.fake_mp
	&& entry->parent.Cell       != 0
	&& entry->parent.fid.Volume != 0
	&& entry->parent.fid.Vnode  != 0
	&& entry->parent.fid.Unique != 0)
    {
	if (parent == NULL) {
	    arla_warnx (ADEBFCACHE, "fcache_get_attr: getting parent");
	    error = fcache_get (&parent, entry->parent, ce);
	} else
	    error = 0;

	/*
	 * Check if the entry is used, that means that
	 * there is greater chance that we we'll succeed
	 * when doing bulkstatus.
	 */

	if (error == 0) {
	    int error2 = -1;
	    
	    if (parent->hits++ > fcache_bulkstatus_num &&
		parent->flags.datausedp) {
		
		arla_warnx (ADEBFCACHE,
			    "fcache_get_attr: doing bulk get_attr");
		error2 = get_attr_bulk (parent,
					entry, &entry->fid,
					prefered_name, ce);
		/* magic calculation when we are going to do next bulkstat */
		parent->hits = 0;
	    }
	    if (parent_entry == NULL)
		fcache_release (parent);
	    if (error2 == 0)
		return 0;
	}
    }

    /*
     * We got here because the bulkstatus failed, didn't want to do a
     * bulkstatus or we didn't get a parent for the entry
     */

    arla_warnx (ADEBFCACHE, "fcache_get_attr: doing read_attr");

    return read_attr (entry, ce);
}



/*
 * Make sure that `e' has attributes and that they are up-to-date.
 * `e' must be write-locked.
 */


static int
do_read_data (FCacheEntry *e, CredCacheEntry *ce)
{
    int ret = ARLA_CALL_DEAD;
    fs_server_context context;
    ConnCacheEntry *conn;

    if (connected_mode != CONNECTED)
	return EINVAL;

    for (conn = find_first_fs (e, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {
	ret = read_data (e, conn, ce);
	if (!try_next_fs (ret, &e->fid))
	    break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret))
	ret = ENETDOWN;
    return ret;
}

/*
 * Make sure that `e' has file data and is up-to-date.
 */

int
fcache_verify_data (FCacheEntry *e, CredCacheEntry *ce)
{
    ConnCacheEntry *conn = NULL;
    int ret;
    fs_server_context context;

    assert (e->flags.usedp);
    AssertExclLocked(&e->lock);

    if (dynroot_is_dynrootp (e))
	return dynroot_get_data (e, ce);

    /* Don't get data for deleted files */
    if (e->flags.silly)
	return 0;

    if (e->flags.attrp && uptodatep(e)) {
	if (e->flags.datap) {
	    fcache_counter.fetch_data_cached++;
	    return 0;
	} else
	    return do_read_data (e, ce);
    } else {
	ret = do_read_attr (e, ce, &conn, &context);
	if (ret)
	    return ret;
	if (e->flags.datap) {
	    fcache_counter.fetch_data_cached++;
	    free_fs_server_context (&context);
	    return 0;
	}
    }
    ret = read_data (e, conn, ce);
    free_fs_server_context (&context);
    return ret;
}

/*
 * Fetch `fid' with data, returning the cache entry in `res'.
 * note that `fid' might change.
 */

int
fcache_get_data (FCacheEntry **res, VenusFid *fid, CredCacheEntry **ce)
{
    FCacheEntry *e;
    int ret;

    ret = fcache_get (&e, *fid, *ce);
    if (ret)
	return ret;

    if (e->flags.fake_mp) {
	VenusFid new_fid;
	FCacheEntry *new_root;

	ret = resolve_mp (e, &new_fid, ce);
	if (ret) {
	    fcache_release (e);
	    return ret;
	}
	ret = fcache_get (&new_root, new_fid, *ce);
	if (ret) {
	    fcache_release (e);
	    return ret;
	}
	ret = fcache_verify_attr (new_root, NULL, NULL, *ce);
	if (ret) {
	    fcache_release (e);
	    fcache_release (new_root);
	    return ret;
	}
	e->flags.fake_mp   = FALSE;
	e->flags.mountp    = TRUE;
	e->status.FileType = TYPE_LINK;
	update_fid (*fid, e, new_fid, new_root);
	fcache_release (e);
	*fid = new_fid;
	e  = new_root;
	install_attr (e, FCACHE2XFSNODE_ALL);
    }

    ret = fcache_verify_data (e, *ce);
    if (ret == 0)
	*res = e;
    else
	fcache_release (e);
    return ret;
}

/*
 * Helper function for followmountpoint.
 * Given the contents of a mount-point, figure out the cell and volume name.
 *
 * ``mp'' must be writeable and should not be used afterwards.
 * ``*volname'' is a pointer to somewhere in the mp string.
 * ``cell'' should be set before function is called to default cell.
 */

static int
parse_mountpoint (char *mp, size_t len, int32_t *cell, char **volname)
{
    char *colon;
    
    mp[len - 1] = '\0';
    colon = strchr (mp, ':');
    if (colon != NULL) {
	*colon++ = '\0';
	*cell    = cell_name2num (mp + 1);
	if (*cell == -1)
	    return ENOENT;
	*volname = colon;
    } else {
	*volname = mp + 1;
    }
    return 0;
}

/*
 * Used by followmountpoint to figure out what clone of a volume
 * should be used.
 *
 * Given a `volname', `cell', it uses the given `ce', `mount_symbol'
 * and `parent_type' to return a volume id in `volume'.
 *
 * The rules are:
 *
 * "readonly" -> RO
 * BK + "backup" -> fail
 * "backup" -> BK
 * BK + "" + # -> RO
 * RO + "" + # -> RO
 * * -> RW
 *
 * this_type = "" | "readonly" | "backup"
 * parent_type = RW | RO | BK
 * mount_symbol = "#" | "%"
 */

static int
find_volume (const char *volname, int32_t cell, 
	     CredCacheEntry *ce, char mount_symbol, int parent_type,
	     u_int32_t *volid, VolCacheEntry **ve)
{
    int result_type;
    int this_type;
    int res;

    res = volcache_getbyname (volname, cell, ce, ve, &this_type);
    if (res)
	return res;

    assert (this_type == RWVOL ||
	    this_type == ROVOL ||
	    this_type == BACKVOL);

    if (this_type == ROVOL) {
	if (!((*ve)->entry.flags & VLF_ROEXISTS)) {
	    volcache_free (*ve);
	    return ENOENT;
	}
	result_type = ROVOL;
    } else if (this_type == BACKVOL && parent_type == BACKVOL) {
	volcache_free (*ve);
	return ENOENT;
    } else if (this_type == BACKVOL) {
	if (!((*ve)->entry.flags & VLF_BOEXISTS)) {
	    volcache_free (*ve);
	    return ENOENT;
	}
	result_type = BACKVOL;
    } else if (this_type == RWVOL &&
	       parent_type != RWVOL &&
	       mount_symbol == '#') {
	if ((*ve)->entry.flags & VLF_ROEXISTS)
	    result_type = ROVOL;
	else
	    result_type = RWVOL;
    } else {
	if ((*ve)->entry.flags & VLF_RWEXISTS)
	    result_type = RWVOL;
	else if ((*ve)->entry.flags & VLF_ROEXISTS)
	    result_type = ROVOL;
	else {
	    volcache_free (*ve);
	    return ENOENT;
	}
    }
    *volid = (*ve)->entry.volumeId[result_type];
    return 0;
}

/*
 * Set `fid' to point to the root of the volume pointed to by the
 * mount-point in (buf, len).
 *
 * If succesful, `fid' will be update to the root of the volume, and
 * `ce' will point to a cred in the new cell.
 */

static int
get_root_of_volume (VenusFid *fid, const VenusFid *parent,
		    VolCacheEntry *volume,
		    CredCacheEntry **ce,
		    char *buf, size_t len)
{
    VenusFid oldfid = *fid;
    char *volname;
    int32_t cell;
    u_int32_t volid;
    int res;
    long parent_type;
    char mount_symbol;
    VolCacheEntry *ve;
    FCacheEntry *e;

    cell = fid->Cell;

    res = parse_mountpoint (buf, len, &cell, &volname);
    if (res)
	return res;

    /*
     * If this is a cross-cell mountpoint we need new credentials. 
     */

    if ((*ce)->cell != cell) {
	CredCacheEntry *new_ce;

	new_ce = cred_get(cell, (*ce)->cred, CRED_ANY);
	if (new_ce == NULL)
	    return ENOMEM;
	cred_free (*ce);
	*ce = new_ce;
    }

    parent_type = gettype (fid->fid.Volume, volume);
    mount_symbol = *buf;

    res = find_volume (volname, cell, *ce, mount_symbol,
		       parent_type, &volid, &ve);
    if (res)
	return res;

    /*
     * Create the new fid. The root of a volume always has
     * (Vnode, Unique) = (1,1)
     */

    fid->Cell = cell;
    fid->fid.Volume = volid;
    fid->fid.Vnode = fid->fid.Unique = 1;

    /*
     * Check if we are looking up ourself, if we are, just return.
     */

    if (VenusFid_cmp(fid, parent) == 0) {
	volcache_free (ve);
	return 0;
    }

    res = fcache_get (&e, *fid, *ce);
    if (res) {
	volcache_free (ve);
	return res;
    }

    /*
     * Root nodes are a little bit special.  We keep track of
     * their parent in `parent' so that `..' can be handled
     * properly.
     */

    e->flags.vol_root  = TRUE;
    e->parent          = *parent;
    if (ve->parent == NULL) {
	ve->parent_fid = *parent;
	ve->mp_fid     = oldfid;
    }
    volcache_volref (ve, volume);
    fcache_release (e);
    volcache_free (ve);
    return 0;
}

/*
 * If this entry is a mount point, set the fid data to
 * the root directory of the volume it's pointing at,
 * otherwise just leave it.
 *
 * Mount points are symbol links with the following contents:
 *
 * '#' | '%' [ cell ':' ] volume-name [ '.' ]
 *
 * This function tries to the minimal amount of work.  It always has
 * to fetch the attributes of `fid' and if it's a symbolic link, the
 * contents as well.
 */

int
followmountpoint (VenusFid *fid, const VenusFid *parent, FCacheEntry *parent_e,
		  CredCacheEntry **ce)
{
     FCacheEntry *e;
     int ret;

     /*
      * Get the node for `fid' and verify that it's a symbolic link
      * with the correct bits.  Otherwise, just return the old
      * `fid' without any change.
      */

     ret = fcache_get (&e, *fid, *ce);
     if (ret)
	 return ret;

     ret = fcache_verify_attr (e, parent_e, NULL, *ce);
     if (ret) {
	 fcache_release(e);
	 return ret;
     }

     e->parent = *parent;
     if (e->flags.mountp)
	 ret = resolve_mp (e, fid, ce);
     
     fcache_release(e);
     return ret;
}

/*
 * actually resolve a mount-point
 */

static int
resolve_mp (FCacheEntry *e, VenusFid *ret_fid, CredCacheEntry **ce)
{
    VenusFid fid = e->fid;
    int ret;
    fbuf the_fbuf;
    char *buf;
    int fd;
    u_int32_t length;

    assert (e->flags.fake_mp || e->flags.mountp);
    AssertExclLocked(&e->lock);

    ret = fcache_verify_data (e, *ce);
    if (ret)
	return ret;

    length = e->status.Length;

    fd = fcache_open_file (e, O_RDONLY);
    if (fd < 0)
	return errno;

    ret = fbuf_create (&the_fbuf, fd, length,
		       FBUF_READ|FBUF_WRITE|FBUF_PRIVATE);
    if (ret) {
	close (fd);
	return ret;
    }
    buf = fbuf_buf (&the_fbuf);

    ret = get_root_of_volume (&fid, &e->parent, e->volume, ce, buf, length);

    fbuf_end (&the_fbuf);
    close (fd);
    if (ret) 
	return ret;
    *ret_fid = fid;
    return 0;
}

/*
 *
 */

static Bool
print_entry (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;

    arla_log(ADEBVLOG, "(%d, %u, %u, %u)%s%s%s%s%s%s%s%s%s%s%s length: %ld",
	     e->fid.Cell,
	     e->fid.fid.Volume, e->fid.fid.Vnode, e->fid.fid.Unique,
	     e->flags.usedp?" used":"",
	     e->flags.attrp?" attr":"",
	     e->flags.datap?" data":"",
	     e->flags.attrusedp?" attrused":"",
	     e->flags.datausedp?" dataused":"",
	     e->flags.extradirp?" extradir":"",
	     e->flags.mountp?" mount":"",
	     e->flags.kernelp?" kernel":"",
	     e->flags.sentenced?" sentenced":"",
	     e->flags.silly?" silly":"",
	     e->flags.fake_mp ? " fake mp" : "",
	     e->flags.vol_root ? " vol root" : "",
	     e->status.Length);
    return FALSE;
}


/*
 *
 */

void
fcache_status (void)
{
    arla_log(ADEBVLOG, "%lu (%lu-/%lu)-%lu) files"
	     "%lu (%lu-%lu) bytes\n",
	     usedvnodes, lowvnodes, current_vnodes, highvnodes,
	     usedbytes, lowbytes, highbytes);
    hashtabforeach (hashtab, print_entry, NULL);
}

/*
 *
 */

void
fcache_update_length (FCacheEntry *e, size_t len)
{
    AssertExclLocked(&e->lock);

    assert (len >= e->length || e->length - len <= usedbytes);

    usedbytes = usedbytes - e->length + len;
    e->length = len;
}

/*
 * Request an ACL and put it in opaque
 */

int
getacl(VenusFid fid,
       CredCacheEntry *ce,
       AFSOpaque *opaque)
{
    FCacheEntry *dire;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    int ret;
    fs_server_context context;
  
    opaque->val = NULL;
    opaque->len = 0;

    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return ret;
    }

    ret = ARLA_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {

	ret = RXAFS_FetchACL (conn->connection, &fid.fid,
			      opaque, &status, &volsync);
	if (ret) {
	    free(opaque->val);
	    opaque->val = NULL;
	    opaque->len = 0;
	}

	if (!try_next_fs (ret, &fid))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "FetchACL");

    if (ret == 0)
	update_entry (dire, &status, NULL, &volsync,
		      rx_HostOf(rx_PeerOf(conn->connection)),
		      ce->cred);
    else if (host_downp(ret))
	ret = ENETDOWN;

    free_fs_server_context (&context);
    fcache_release (dire);
    return ret;
}

/*
 * Store the ACL read from opaque
 *
 * If the function return 0, ret_e is set to the dir-entry and must
 * be fcache_released().
 */

int
setacl(VenusFid fid,
       CredCacheEntry *ce,
       AFSOpaque *opaque,
       FCacheEntry **ret_e)
{
    FCacheEntry *dire;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    int ret;
    fs_server_context context;
  
    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    ret = ARLA_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {
	ret = RXAFS_StoreACL (conn->connection, &fid.fid,
			      opaque, &status, &volsync);
	if (!try_next_fs (ret, &fid))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "StoreACL");

    if (ret == 0)
	update_entry (dire, &status, NULL, &volsync,
		      rx_HostOf(rx_PeerOf(conn->connection)),
		      ce->cred);
    else if (host_downp(ret))
	ret = ENETDOWN;

    free_fs_server_context (&context);

    if (ret == 0) {
	*ret_e = dire;
    } else {
	*ret_e = NULL;
	fcache_release (dire);
    }
    return ret;
}

/*
 * Request volume status
 */

int
getvolstat(VenusFid fid, CredCacheEntry *ce,
	   AFSFetchVolumeStatus *volstat,
	   char *volumename,
	   char *offlinemsg,
	   char *motd)
{
    FCacheEntry *dire;
    ConnCacheEntry *conn;
    int ret;
    fs_server_context context;
  
    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    ret = ARLA_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {
	ret = RXAFS_GetVolumeStatus (conn->connection, fid.fid.Volume,
				     volstat, volumename, offlinemsg,
				     motd);
	if (!try_next_fs (ret, &fid))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "GetVolumeStatus");
    free_fs_server_context (&context);
    if (host_downp(ret))
	ret = ENETDOWN;
    if (ret == 0 && volumename[0] == '\0')
	volcache_getname (fid.fid.Volume, fid.Cell,
			  volumename, sizeof(volumename));

    fcache_release (dire);
    return ret;
}

/*
 * Store volume status
 */

int
setvolstat(VenusFid fid, CredCacheEntry *ce,
	   AFSStoreVolumeStatus *volstat,
	   char *volumename,
	   char *offlinemsg,
	   char *motd)
{
    FCacheEntry *dire;
    ConnCacheEntry *conn;
    int ret;
    fs_server_context context;
  
    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    ret = ARLA_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, host_downp (ret))) {
	ret = RXAFS_SetVolumeStatus (conn->connection, fid.fid.Volume,
				     volstat, volumename, offlinemsg,
				     motd);
	if (!try_next_fs (ret, &fid))
	    break;
    }
    if (ret) {
	if (host_downp(ret))
	    ret = ENETDOWN;
	arla_warn (ADEBFCACHE, ret, "SetVolumeStatus");
    }
    free_fs_server_context (&context);

    fcache_release (dire);
    return ret;
}

/*
 * Get `fbuf' from `centry' that is opened with openflags
 * `open_flags' and fbuf flags with `fbuf_flags'
 *
 * Assume that data is valid and `centry' is exclusive locked.
 */

int
fcache_get_fbuf (FCacheEntry *centry, int *fd, fbuf *fbuf,
		 int open_flags, int fbuf_flags)
{
    int ret;
    unsigned len;
    struct stat sb;

    AssertExclLocked(&centry->lock);

    *fd = fcache_open_file (centry, open_flags);
    if (*fd < 0)
	return errno;

    if (fstat (*fd, &sb)) {
	ret = errno;
	close (*fd);
	return ret;
    }

    len = sb.st_size;

    ret = fbuf_create (fbuf, *fd, len, fbuf_flags);
    if (ret) {
	close (*fd);
	return ret;
    }
    return 0;
}

/*
 *
 */

static Bool 
sum_node (List *list, Listitem *li, void *arg)
{
    u_long *a = (u_long *) arg;
    FCacheEntry *e = listdata (li);

    if (e->flags.attrp
	&& e->flags.datap
	&& !dynroot_is_dynrootp (e)) {

	*a += e->length;
    }
    
    return FALSE;
}


u_long
fcache_calculate_usage (void)
{
    u_long size = 0;

    listiter (lrulist, sum_node, &size);

    return size;
}

/*
 *
 */

const VenusFid *
fcache_realfid (const FCacheEntry *entry)
{
    if (entry->flags.vol_root
	|| (entry->fid.fid.Vnode == 1 && entry->fid.fid.Unique == 1))
	return &entry->volume->mp_fid;
    else
	return &entry->fid;
}
