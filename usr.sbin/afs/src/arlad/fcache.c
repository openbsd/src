/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
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
RCSID("$arla: fcache.c,v 1.417 2003/04/08 00:38:09 mattiasa Exp $") ;

#ifdef __CYGWIN32__
#include <windows.h>
#endif

/*
 * Prototypes
 */

static int get_attr_bulk (FCacheEntry *parent_entry, 
			  FCacheEntry *prefered_entry,
			  VenusFid *prefered_fid,
			  const char *prefered_name,
			  CredCacheEntry *ce);

static int
resolve_mp (FCacheEntry **e, VenusFid *ret_fid, CredCacheEntry **ce);

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

static u_long highvnodes, lowvnodes, current_vnodes;
static int64_t highbytes, lowbytes;

/* current values */

static int64_t usedbytes, needbytes;
static u_long usedvnodes;

/* Map with recovered nodes */

static u_long maxrecovered;

static char *recovered_map;

static void
set_recovered(u_long index)
{
    char *p;
    u_long oldmax;

    if (index >= maxrecovered) {
	oldmax = maxrecovered;
	maxrecovered = (index + 16) * 2;
	p = realloc(recovered_map, maxrecovered);
	if (p == NULL) {
	    u_long m = maxrecovered;
	    free(recovered_map);
	    recovered_map = NULL; 
	    maxrecovered = 0;
	    arla_errx(1, ADEBERROR, "fcache: realloc %lu recovered_map failed",
		      m);
	}
	recovered_map = p;
	memset(recovered_map + oldmax, 0, maxrecovered - oldmax);
    }
    recovered_map[index] = 1;
}

#define IS_RECOVERED(index) (recovered_map[(index)])

/* 
 * This is how far the cleaner will go to clean out entries.
 * The higher this is, the higher is the risk that you will
 * lose any file that you feel is important to disconnected
 * operation. 
 */

Bool fprioritylevel = FPRIO_DEFAULT;

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

int64_t
fcache_highbytes(void)
{
    return highbytes;
}

int64_t
fcache_usedbytes(void)
{
    return usedbytes;
}

int64_t
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

    return VenusFid_cmp(&f1->fid, &f2->fid);
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

char **sysnamelist = NULL;
int sysnamenum = 0;

/*
 *
 */

static void
fcache_poller_unref(FCacheEntry *e)
{
    AssertExclLocked(&e->lock);

    if (e->poll) {
	poller_remove(e->poll);
	e->poll = NULL;
    }
}

static void
fcache_poller_reref(FCacheEntry *e, ConnCacheEntry *conn)
{
    PollerEntry *pe = e->poll;
    AssertExclLocked(&e->lock);

    e->poll = poller_add_conn(conn);
    if (pe)
	poller_remove(pe);
}


/*
 *
 */

const char *
fcache_getdefsysname (void)
{
    if (sysnamenum == 0)
	return "fool-dont-remove-all-sysnames";
    return sysnamelist[0];
}

/*
 *
 */

int
fcache_setdefsysname (const char *sysname)
{
    if (sysnamenum == 0)
	return fcache_addsysname (sysname);
    free (sysnamelist[0]);
    sysnamelist[0] = estrdup (sysname);
    return 0;
}

/*
 *
 */

int
fcache_addsysname (const char *sysname)
{
    sysnamenum += 1;
    sysnamelist = erealloc (sysnamelist, 
			    sysnamenum * sizeof(char *));
    sysnamelist[sysnamenum - 1] = estrdup(sysname);
    return 0;
}

/*
 *
 */

int
fcache_removesysname (const char *sysname)
{
    int i;
    for (i = 0; i < sysnamenum; i++)
	if (strcmp (sysnamelist[i], sysname) == 0)
	    break;
    if (i == sysnamenum)
	return 1;
    free (sysnamelist[i]);
    for (;i < sysnamenum; i++)
	sysnamelist[i] = sysnamelist[i + 1];
    sysnamenum--;
    sysnamelist = erealloc (sysnamelist, 
			    sysnamenum * sizeof(char *));
    return 0;
}

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
 * return kernel version of path to the cache file for `entry'.
 */

int
fcache_conv_file_name (FCacheEntry *entry, char *s, size_t len)
{
#ifdef __CYGWIN32__
    char buf[1024];
    GetCurrentDirectory(1024, buf);

    return snprintf (s, len, "%s\\%02X\\%02X",
		     buf, entry->index / 0x100, entry->index % 0x100);
#else
    return snprintf (s, len, "%02X/%02X",
		     entry->index / 0x100, entry->index % 0x100);
#endif
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
    assert (entry->flags.extradirp &&
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

#ifdef __CYGWIN32__
    return -1;
#endif

#if defined(HAVE_GETFH) && defined(HAVE_FHOPEN)
    {
	int ret;
	fhandle_t fh;

	memcpy (&fh, &handle->nnpfs_handle, sizeof(fh));
	ret = fhopen (&fh, flags);
	if (ret >= 0)
	    return ret;
    }
#endif

#ifdef KERBEROS			/* really KAFS */
    {
	struct ViceIoctl vice_ioctl;
	
	vice_ioctl.in      = (caddr_t)&handle->nnpfs_handle;
	vice_ioctl.in_size = sizeof(handle->nnpfs_handle);
	
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

#ifdef __CYGWIN32__
    {
	int ret, a, b;
	char buf[1024]; /* XXX */

	ret = sscanf(filename, "%02X/%02X", &a, &b);
	if (ret != 2)
	    return EINVAL;

	GetCurrentDirectory(1024, buf);
	
	ret = snprintf ((char *)handle, CACHEHANDLESIZE,
	    "%s\\%02X\\%02X", buf, a, b);

	if (ret > 0 && ret < CACHEHANDLESIZE)
	    handle->valid = 1;

	return ret;
    }
#endif

#if defined(HAVE_GETFH) && defined(HAVE_FHOPEN)
    {
	int ret;
	fhandle_t fh;

	ret = getfh (filename, &fh);
	if (ret == 0) {
	    memcpy (&handle->nnpfs_handle, &fh, sizeof(fh));
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
	
	vice_ioctl.out      = (caddr_t)&handle->nnpfs_handle;
	vice_ioctl.out_size = sizeof(handle->nnpfs_handle);
	
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

static int
fcache_create_file (FCacheEntry *entry, int create)
{
    char fname[MAXPATHLEN];
    char extra_fname[MAXPATHLEN];
    int fd;
    int ret;
    int flags;

    flags = O_RDWR | O_BINARY;

    if (create)
	flags |= O_CREAT | O_TRUNC;

    fcache_file_name (entry, fname, sizeof(fname));
    fd = open (fname, flags, 0666);
    if (fd < 0) {
	if (errno == ENOENT && create) {
	    char dname[MAXPATHLEN];

	    fcache_dir_name (entry, dname, sizeof(dname));
	    ret = mkdir (dname, 0777);
	    if (ret < 0)
		arla_err (1, ADEBERROR, errno, "mkdir %s", dname);
	    fd = open (fname, flags, 0666);
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
	if (ret < 0 && (errno == EINVAL || errno == EPERM))
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

    assert (entry->flags.extradirp &&
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

    assert (entry->flags.usedp);
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
    if (usedbytes < entry->length) {
	arla_warnx(ADEBCONN, "usedbytes %d < entry->length %d", usedbytes, 
	    entry->length);
	exit(-1);
    }
    /* XXX - things are wrong - continue anyway */
    if (usedbytes < entry->length)
	usedbytes  = entry->length;
    usedbytes -= entry->length;
    entry->length = 0;
    entry->wanted_length = 0;
    entry->fetched_length = 0;
    entry->flags.extradirp = FALSE;

 out:
    cm_check_consistency();
}

/*
 * A probe function for a file server.
 */

int
fs_probe (struct rx_connection *conn)
{
    uint32_t sec, usec;

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
    assert (!entry->flags.kernelp);

    AssertExclLocked(&entry->lock);
    assert(LockWaiters(&entry->lock) == 0);

    hashtabdel (hashtab, entry);

    /* 
     * Throw data when there is data, length is a good test since the
     * node most not be used (in kernel) when we get here.
     */
    if (entry->length)
	throw_data (entry);

    if (entry->invalid_ptr != -1) {
	heap_remove (invalid_heap, entry->invalid_ptr);
	entry->invalid_ptr = -1;
    }

    fcache_poller_unref(entry);

    if (entry->flags.attrp && entry->host) {
	ce = cred_get (entry->fid.Cell, 0, CRED_NONE);
	if (ce == NULL) {
		arla_warnx (ADEBMISC, "cred_get failed");
		exit(-1);
	}

	conn = conn_get (entry->fid.Cell, entry->host, afsport,
			 FS_SERVICE_ID, fs_probe, ce);
	cred_free (ce);
	
	if (conn_isalivep(conn)) {
	    fids.len = cbs.len = 1;
	    fids.val = &entry->fid.fid;
	    cbs.val  = &entry->callback;
	    if (conn_isalivep (conn))
		ret = RXAFS_GiveUpCallBacks(conn->connection, &fids, &cbs);
	    else
		ret = ENETDOWN;
	    conn_free (conn);
	    if (ret)
		arla_warn (ADEBFCACHE, ret, "RXAFS_GiveUpCallBacks");
	}
    }
    if (entry->volume) {
	volcache_free (entry->volume);
	entry->volume = NULL;
    }
    assert_not_flag(entry,kernelp);
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
    do {
	node_count++;
    } while ((node_count < maxrecovered)
	     && IS_RECOVERED(node_count));
    
    return node_count;
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
	    entries[i].poll = NULL;
	    for (j = 0; j < NACCESS; j++) {
		entries[i].acccache[j].cred = ARLA_NO_AUTH_CRED;
		entries[i].acccache[j].access = 0;
	    }
	    entries[i].length      = 0;
	    Lock_Init(&entries[i].lock);
	    entries[i].index = next_cache_index ();
	    fcache_create_file (&entries[i], 1);

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
    
    numnodes = NNPFS_GC_NODES_MAX_HANDLE;
    
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
		    (long)usedbytes, (long)lowbytes, (long)highbytes,
		    (long)needbytes);
	
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
		
		if (entry->cleanergen == cleanerrun)
		    continue;
		entry->cleanergen = cleanerrun;

		if (entry->flags.usedp
		    && (usedvnodes > lowvnodes 
			|| usedbytes > lowbytes 
			|| needbytes > highbytes - usedbytes)
		    && entry->refcount == 0
		    && CheckLock(&entry->lock) == 0) 
		{
		    if (!entry->flags.datausedp
			&& !entry->flags.kernelp
			/* && this_is_a_good_node_to_gc(entry,state) */) {

			ObtainWriteLock (&entry->lock);
			listdel (lrulist, item);
			throw_entry (entry);
			entry->lru_le = listaddtail (lrulist, entry);
			if(!entry->lru_le)
				exit(-1);
			ReleaseWriteLock (&entry->lock);
			break;
		    }

		    if (state == CL_FORCE && entry->flags.kernelp) {
			
			fids[cnt++] = entry->fid;
			
			if (cnt >= numnodes) {
			    nnpfs_send_message_gc_nodes (kernel_fd, cnt, fids);
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
			nnpfs_send_message_gc_nodes (kernel_fd, cnt, fids);
			IOMGR_Poll();
			cnt = 0;
		    }
		    break;
		case CL_COLLECT:
		    goto out;
		    break;
		default:
		    errx(-1, "fcache.c: uknown state %d\n", state);
		    /* NOTREACHED */
		}
		cleanerrun++;
	    }
	}
    out:
	
	arla_warnx(ADEBCLEANER,
		   "cleaner done: "
		   "%lu (%lu-(%lu)-%lu) files, "
		   "%ld (%ld-%ld) bytes "
		   "%ld needed bytes",
		   usedvnodes, lowvnodes, current_vnodes, highvnodes,
		   (long)usedbytes, (long)lowbytes, (long)highbytes,
		   (long)needbytes);
	
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
		    "(%ld needed, %ld outstanding, %ld highbytes)", 
		    (long)needed, (long)needbytes, (long)highbytes);
	return ENOSPC;
    }

    needbytes += needed;
    fcache_wakeup_cleaner(fcache_need_bytes);
    needbytes -= needed;
    if (needed > highbytes - usedbytes) {
	arla_warnx (ADEBWARN, 
		    "Out of space, couldn't get needed bytes after cleaner "
		    "(%lu bytes missing, %lu used, %lu highbytes)",
		    (long)(needed - (highbytes - usedbytes)), 
		    (long)usedbytes, (long)highbytes);
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

	arla_warnx(ADEBINVALIDATOR,
		   "running invalidator");

	while ((head = heap_head (invalid_heap)) == NULL)
	    LWP_WaitProcess (invalid_heap);

	gettimeofday (&tv, NULL);

	while ((head = heap_head (invalid_heap)) != NULL) {
	    FCacheEntry *entry = (FCacheEntry *)head;

	    if (tv.tv_sec < entry->callback.ExpirationTime) {
		unsigned long t = entry->callback.ExpirationTime - tv.tv_sec;

		arla_warnx (ADEBINVALIDATOR,
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
		fcache_poller_unref(entry);
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

    if (current_vnodes == usedvnodes)
	fcache_need_nodes();
     
    for (;;) {

	assert (!listemptyp (lrulist));
	for (item = listtail (lrulist);
	     item;
	     item = listprev (lrulist, item)) {

	    entry = (FCacheEntry *)listdata (item);
	    if (!entry->flags.usedp
		&& CheckLock(&entry->lock) == 0) {
		assert_not_flag(entry,kernelp);
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
		&& !entry->flags.kernelp
		&& entry->refcount == 0
		&& CheckLock(&entry->lock) == 0) {
		assert_not_flag(entry,kernelp);
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

struct fstore_context {
    Listitem *item;
    unsigned n;
};

static int
fcache_store_entry (struct fcache_store *st, void *ptr)
{
    struct fstore_context *c;
    FCacheEntry *entry;

    c = (struct fstore_context *)ptr;
    if (c->item == NULL)		/* check if done ? */
	return STORE_DONE;

    entry = (FCacheEntry *)listdata (c->item);
    c->item = listprev (lrulist, c->item);

    if (!entry->flags.usedp)
	return STORE_SKIP;
    
    strlcpy(st->cell, cell_num2name(entry->fid.Cell), sizeof(st->cell));
    st->fid		= entry->fid.fid;
    st->refcount	= entry->refcount;
    st->length		= entry->length;
    st->fetched_length	= entry->fetched_length;
    st->volsync		= entry->volsync;
    st->status		= entry->status;
    st->anonaccess	= entry->anonaccess;
    st->index		= entry->index;
    st->flags.attrp	= entry->flags.attrp;
    st->flags.datap	= entry->length ? TRUE : FALSE;
    st->flags.extradirp = entry->flags.extradirp;
    st->flags.mountp    = entry->flags.mountp;
    st->flags.fake_mp   = entry->flags.fake_mp;
    st->flags.vol_root  = entry->flags.vol_root;
    strlcpy(st->parentcell, cell_num2name(entry->parent.Cell), 
	    sizeof(st->parentcell));
    st->parent		= entry->parent.fid;
    st->priority	= entry->priority;
    
    c->n++;
    return STORE_NEXT;
}

/*
 *
 */

int
fcache_store_state (void)
{
    struct fstore_context c;
    int ret;

    if (lrulist == NULL) {
	arla_warnx (ADEBFCACHE, "store_state: lrulist is NULL\n");
	return 0;
    }

    c.item = listtail(lrulist);
    c.n = 0;

    ret = state_store_fcache("fcache", fcache_store_entry, &c);
    if (ret)
	arla_warn(ADEBWARN, ret, "failed to write fcache state");
    else
	arla_warnx (ADEBFCACHE, "wrote %u entries to fcache", c.n);

    return 0;
}

/*
 *
 */

static int
fcache_recover_entry (struct fcache_store *st, void *ptr)
{
    AFSCallBack broken_callback = {0, 0, CBDROPPED};
    unsigned *n = (unsigned *)ptr;

    CredCacheEntry *ce;
    FCacheEntry *e;
    int i;
    VolCacheEntry *vol;
    int res;
    int32_t cellid;

    cellid = cell_name2num(st->cell);
    assert (cellid != -1);
    
    ce = cred_get (cellid, 0, 0);
    if (ce == NULL) {
	    arla_warnx (ADEBMISC, "cred_get failed");
	    exit(-1);
    }
    
    res = volcache_getbyid (st->fid.Volume, cellid, ce, &vol, NULL);
    cred_free (ce);
    if (res)
	return 0;
    if (!vol)
	    exit(-1);
    
    e = calloc(1, sizeof(FCacheEntry));
    e->invalid_ptr = -1;
    Lock_Init(&e->lock);
    ObtainWriteLock(&e->lock);
    

    e->fid.Cell = cellid;
    e->fid.fid  = st->fid;
    e->host     = 0;
    e->status   = st->status;
    e->length   = st->length;
    e->fetched_length = st->fetched_length;
    e->callback = broken_callback;
    e->volsync  = st->volsync;
    e->refcount = st->refcount;
    
    /* Better not restore the rights. pags don't have to be the same */
    for (i = 0; i < NACCESS; ++i) {
	e->acccache[i].cred = ARLA_NO_AUTH_CRED;
	e->acccache[i].access = ANONE;
    }
    
    e->anonaccess = st->anonaccess;
    e->index      = st->index;
    fcache_create_file(e, 0);
    set_recovered(e->index);
    e->flags.usedp = TRUE;
    e->flags.attrp = st->flags.attrp;
    /* st->flags.datap */
    e->flags.attrusedp = FALSE;
    e->flags.datausedp = FALSE;
    e->flags.kernelp   = FALSE;
    e->flags.extradirp = st->flags.extradirp;
    e->flags.mountp    = st->flags.mountp;
    e->flags.fake_mp   = st->flags.fake_mp;
    e->flags.vol_root  = st->flags.vol_root;
    e->flags.sentenced = FALSE;
    e->flags.silly 	   = FALSE;
    e->tokens	       = 0;
    e->parent.Cell = cell_name2num(st->parentcell);
    if (e->parent.Cell == -1)
	    exit(-1);
    e->parent.fid = st->parent;
    e->priority = st->priority;
    e->hits = 0;
    e->cleanergen = 0;
    e->lru_le = listaddhead (lrulist, e);
    if (!e->lru_le)
	    exit(-1);
    e->volume = vol;
    hashtabadd (hashtab, e);
    if (e->length)
	usedbytes += e->length;
    ReleaseWriteLock (&e->lock);
    
    (*n)++;

    return 0;
}

/*
 *
 */

static void
fcache_recover_state (void)
{
    unsigned n;

    n = 0;
    state_recover_fcache("fcache", fcache_recover_entry, &n);

    arla_warnx (ADEBFCACHE, "recovered %u entries to fcache", n);
    current_vnodes = n;
}

/*
 * Search for `cred' in `ae' and return a pointer in `pos'.  If it
 * already exists return TRUE, else return FALSE and set pos to a
 * random slot.
 */

Bool
findaccess (nnpfs_pag_t cred, AccessEntry *ae, AccessEntry **pos)
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
 *
 */


static int
fs_rtt_cmp (const void *v1, const void *v2)
{
    struct fs_server_entry *e1 = (struct fs_server_entry *)v1;
    struct fs_server_entry *e2 = (struct fs_server_entry *)v2;
    
    return conn_rtt_cmp(&e1->conn, &e2->conn);
}

/*
 * Initialize a `fs_server_context'.
 */

static void
init_fs_server_context (fs_server_context *context)
{
    context->num_conns = 0;
}

static long
find_partition (fs_server_context *context)
{
    int i = context->conns[context->i - 1].ve_ent;

    if (i < 0 || i >= context->ve->entry.nServers)
	return 0;
    return context->ve->entry.serverPartition[i];
}

/*
 * Find the next fileserver for the request in `context'.
 * Returns a ConnCacheEntry or NULL.
 */

ConnCacheEntry *
find_next_fs (fs_server_context *context,
	      ConnCacheEntry *prev_conn,
	      int error)
{
    if (error) {
	if (host_downp(error))
	    conn_dead (prev_conn);
	if (volume_downp(error))
	    volcache_mark_down (context->ve, 
				context->conns[context->i - 1].ve_ent,
				error);
    } else if (prev_conn) {
	if(prev_conn != context->conns[context->i - 1].conn)
		exit(-1);
	volcache_reliable_el(context->ve, context->conns[context->i - 1].ve_ent);
    }

    if (context->i < context->num_conns)
	return context->conns[context->i++].conn;
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
	conn_free (context->conns[i].conn);

    if (context->ve)
	volcache_process_marks(context->ve);
}

/*
 * Find the the file servers housing the volume for `e' and store it
 * in the `context'.
 */

int
init_fs_context (FCacheEntry *e,
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
	ret = volcache_getbyid (e->fid.fid.Volume, e->fid.Cell,
				ce, &e->volume, NULL);
	if (ret)
	    return ret;
	ve = e->volume;
    }

    ret = volume_make_uptodate (ve, ce);
    if (ret)
	return ret;

    bit = volcache_volid2bit (ve, e->fid.fid.Volume);

    if (bit == -1) {
	/* the volume entry is inconsistent. */
	volcache_invalidate_ve (ve);
	return ENOENT;
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
	    if (!conn_isalivep (conn))
		conn->rtt = INT_MAX/2 ;
	    else if (!volcache_reliablep_el(ve, i))
		conn->rtt = INT_MAX/4;
	    else
		conn->rtt = rx_PeerOf(conn->connection)->srtt
		    + rand() % RTT_FUZZ - RTT_FUZZ / 2;
	    context->conns[num_clones].conn = conn;
	    context->conns[num_clones].ve_ent = i;
	    ++num_clones;
	}
    }

    if (num_clones == 0)
	return ENOENT;
    
    context->ve = ve;

    qsort (context->conns, num_clones, sizeof(*context->conns),
	   fs_rtt_cmp);

    context->num_conns = num_clones;
    context->i	       = 0;

    return 0;
}

/*
 * Find the first file server housing the volume for `e'.
 */

ConnCacheEntry *
find_first_fs (fs_server_context *context)
{
    return find_next_fs (context, NULL, 0);
}

/*
 * Initialize the file cache in `cachedir', with these values for high
 * and low-water marks.
 */

void
fcache_init (u_long alowvnodes,
	     u_long ahighvnodes,
	     int64_t alowbytes,
	     int64_t ahighbytes,
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

    node_count     = 0;
    lowvnodes      = alowvnodes;
    highvnodes     = ahighvnodes;
    lowbytes       = alowbytes;
    highbytes      = ahighbytes;

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
	      int64_t alowbytes,
	      int64_t ahighbytes)
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
	if(!e->lru_le)
		return NULL;
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
	fcache_poller_unref(e);
	e->callback = callback;

	if (e->flags.kernelp)
	    break_callback (e);
	else
	    e->tokens = 0;
	if (e->status.FileType == TYPE_DIR && e->length)
	    throw_data(e);
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
    nnpfs_pag_t pag;
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
		    install_attr (e, FCACHE2NNPFSNODE_NO_LENGTH);
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
fcache_purge_cred (nnpfs_pag_t pag, int32_t cell)
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
 * If `ptr' is a mountpoint, mark it as stale.
 */

static Bool
invalidate_mp (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;
    AFSCallBack broken_callback = {0, 0, CBDROPPED};

    if (e->flags.mountp)
	stale (e, broken_callback);
    return FALSE;


}

/*
 * Invalidate all mountpoints to force the to be reread.
 */

void
fcache_invalidate_mp (void)
{
    hashtabforeach (hashtab, invalidate_mp, NULL);
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
	      ConnCacheEntry *conn,
	      nnpfs_pag_t cred)
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

    if (conn) {
	fcache_poller_reref(entry, conn);
	entry->host     = rx_HostOf(rx_PeerOf(conn->connection));
    } else {
	fcache_poller_unref(entry);
	entry->host = 0;
    }

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
		   ConnCacheEntry *conn,
		   nnpfs_pag_t cred)
{
    if (entry->fetched_length
	&& entry->status.DataVersion != status->DataVersion
	&& !entry->flags.datausedp) 
    {
	throw_data (entry);
	entry->tokens &= ~(NNPFS_DATA_R|NNPFS_DATA_W);
    }
    
    update_entry (entry, status, callback, volsync,
		  conn, cred);
    
    entry->tokens |= NNPFS_ATTR_R;
    entry->flags.attrp = TRUE;
}


/*
 * Give up all callbacks.
 */

static int
giveup_all_callbacks (uint32_t cell, uint32_t host, void *arg)
{
    CredCacheEntry *ce;	
    ConnCacheEntry *conn;
    Listitem *item;
    int ret;

    ce = cred_get (cell, 0, CRED_ANY);
    assert (ce != NULL);
    
    conn = conn_get (cell, host, afsport, FS_SERVICE_ID, fs_probe, ce);
    cred_free (ce);

    if (!conn_isalivep (conn))
	goto out;

    ret = RXAFS_GiveUpAllCallBacks(conn->connection);
    if (ret == 0) {
	for (item = listtail(lrulist);
	     item != NULL;
	     item = listprev(lrulist, item)) {
	    FCacheEntry *entry = (FCacheEntry *)listdata(item);
	    
	    if (entry->host == host)
		entry->flags.attrp = FALSE;
	}
    } else if (ret != RXGEN_OPCODE) {
	struct in_addr in_addr;

	in_addr.s_addr = rx_HostOf(rx_PeerOf(conn->connection));
	arla_warn (ADEBWARN, ret, "GiveUpAllCallBacks %s",
		   inet_ntoa (in_addr));
    }

 out:
    conn_free (conn);

    return 0;
}

int
fcache_giveup_all_callbacks (void)
{
    Listitem *item;

    poller_foreach(giveup_all_callbacks, NULL);

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
		if (ret) {
		    struct in_addr in_addr;

		    in_addr.s_addr = rx_HostOf(rx_PeerOf(conn->connection));
		    arla_warn (ADEBFCACHE, ret, "RXAFS_GiveUpCallBacks %s",
			       inet_ntoa (in_addr));
		} else
		    entry->flags.attrp = FALSE;
	    }
	}
    }
    return 0;			/* XXX */
}

/*
 * Obtain new callbacks for all entries in the cache.
 */

int
fcache_reobtain_callbacks (struct nnpfs_cred *cred)
{
    Listitem *item;
    int ret;

    for (item = listtail(lrulist);
	 item != NULL;
	 item = listprev(lrulist, item)) {
	FCacheEntry *entry = (FCacheEntry *)listdata(item);

	ObtainWriteLock (&entry->lock);
	if (entry->flags.usedp && 
	    entry->flags.silly == FALSE &&
	    entry->host != 0) {

	    CredCacheEntry *ce;	
	    ConnCacheEntry *conn;
	    AFSFetchStatus status;
	    AFSCallBack callback;
	    AFSVolSync volsync;
	    VolCacheEntry *vol;

	    ce = cred_get (entry->fid.Cell, cred->pag, CRED_ANY);
	    assert (ce != NULL);

	    conn = conn_get (entry->fid.Cell, entry->host, afsport,
			     FS_SERVICE_ID, fs_probe, ce);
	    if (!conn_isalivep(conn))
		goto out;
	    /*
	     * does this belong here?
	     */

	    ret = volcache_getbyid (entry->fid.fid.Volume,
				    entry->fid.Cell, ce, &vol, NULL);
	    if (ret == 0)
		entry->volume = vol;

	    ret = RXAFS_FetchStatus (conn->connection,
				     &entry->fid.fid,
				     &status,
				     &callback,
				     &volsync);
	    if (ret)
		arla_warn (ADEBFCACHE, ret, "RXAFS_FetchStatus");
	    else {
		update_attr_entry (entry, &status, &callback, &volsync,
				   conn, ce->cred);
		if (entry->flags.kernelp)
		    break_callback (entry);
	    }
	    fcache_counter.fetch_attr++;
	out:
	    if (conn)
		conn_free (conn);
	    cred_free (ce);
	}
	ReleaseWriteLock (&entry->lock);
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
#ifdef KERBEROS
    case RXKADUNKNOWNKEY:
#endif
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
	if (fid && !volcache_reliablep (fid->fid.Volume, fid->Cell))
	    volcache_invalidate (fid->fid.Volume, fid->Cell);
	return TRUE;
    case 0 :
	return FALSE;
    default :
	return FALSE;
    }
}

/*
 * If the whole file is fetched as we last saw it, lets write down
 * the whole file to the fileserver. If the file is shrinking,
 * make sure we don't cache non-existing bytes.
 */

static size_t
new_fetched_length(FCacheEntry *entry, size_t cache_file_size)
{
    size_t have_len;

    AssertExclLocked(&entry->lock);

    if (entry->fetched_length == entry->status.Length)
	have_len = cache_file_size;
    else {
	have_len = entry->fetched_length;
	/* have file shrinked ? */
	if (have_len > cache_file_size)
	    have_len = cache_file_size;
    }

    return have_len;
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
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    struct collect_stat collectstat;
    int ret;

    AssertExclLocked(&entry->lock);

    *ret_conn = NULL;

    ret = init_fs_context(entry, ce, ret_context);
    if (ret)
	return ret;

    for (conn = find_first_fs (ret_context);
	 conn != NULL;
	 conn = find_next_fs (ret_context, conn, ret)) {

	collectstats_start(&collectstat);
	ret = RXAFS_FetchStatus (conn->connection,
				 &entry->fid.fid,
				 &status,
				 &callback,
				 &volsync);
	collectstats_stop(&collectstat, entry, conn,
			  find_partition(ret_context),
			  STATISTICS_REQTYPE_FETCHSTATUS, 1);
	arla_warnx (ADEBFCACHE, "trying to fetch status: %d", ret);
	if (!try_next_fs (ret, &entry->fid))
	    break;
    }
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fetch-status");
	if (host_downp(ret))
	    ret = ENETDOWN;
	free_fs_server_context (ret_context);
	return ret;
    }

    fcache_counter.fetch_attr++;

    update_attr_entry (entry, &status, &callback, &volsync,
		       conn, ce->cred);
    
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
read_data (FCacheEntry *entry, ConnCacheEntry *conn, CredCacheEntry *ce,
	   long partition)
{
    struct rx_call *call;
    int ret = 0;
    uint32_t wanted_length, sizefs, nbytes = 0;
    int32_t sizediff;
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

    /* are we already done ? */
    if (entry->wanted_length <= entry->fetched_length) {
	ret = 0;
	goto out;
    }

    /* figure out how much more then we need we want to fetch */
    wanted_length = stats_fetch_round(conn, partition, entry->wanted_length);
    if (wanted_length > entry->status.Length)
	wanted_length = entry->status.Length;

    /* we need more space ? */
    if (wanted_length > entry->length)
	nbytes = wanted_length - entry->length;

    if (usedbytes + nbytes > highbytes) {
	ret = fcache_need_bytes (nbytes);
	if (ret)
	    goto out;
    }

    if (usedbytes + nbytes > highbytes) {
	arla_warnx (ADEBWARN, "Out of space, not enough cache "
		    "(file-length: %d need bytes: %ld usedbytes: %ld)",
		    entry->status.Length, (long)nbytes, (long)usedbytes);
	ret = ENOSPC;
	goto out;
    }

    /* now go talk to the world */
    call = rx_NewCall (conn->connection);
    if (call == NULL) {
	arla_warnx (ADEBMISC, "rx_NewCall failed");
	ret = ENOMEM;
	goto out;
    }

    collectstats_start(&collectstat);
    ret = StartRXAFS_FetchData (call, &entry->fid.fid, entry->fetched_length,
				wanted_length - entry->fetched_length);
    if(ret) {
	arla_warn (ADEBFCACHE, ret, "fetch-data");
	rx_EndCall(call,ret);
	goto out;
    }

    ret = rx_Read (call, &sizefs, sizeof(sizefs));
    if (ret != sizeof(sizefs)) {
	ret = conv_to_arla_errno(rx_GetCallError(call));
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

    if (ftruncate(fd, entry->status.Length) < 0) {
	close(fd);
	ret = errno;
	rx_EndCall(call, 0);
	goto out;
    }

    ret = copyrx2fd (call, fd, entry->fetched_length, sizefs);
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
		      partition, STATISTICS_REQTYPE_FETCHDATA, sizefs);

    entry->fetched_length += sizefs;
    sizediff = entry->fetched_length - entry->length;
    entry->length = entry->fetched_length;
    usedbytes += sizediff;

    fcache_counter.fetch_data++;
    
    update_entry (entry, &status, &callback, &volsync,
		  conn, ce->cred);

    entry->tokens |= NNPFS_DATA_R | NNPFS_DATA_W | NNPFS_OPEN_NR | NNPFS_OPEN_NW;

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
    int ret;
    uint32_t sizefs;
    size_t have_len;
    int fd;
    struct stat statinfo;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fs_server_context context;
    struct collect_stat collectstat;

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

    have_len = new_fetched_length(entry, sizefs);
    if (entry->status.Length < have_len)
	entry->status.Length = have_len;

    /*
     *
     */

    fcache_update_length (entry, have_len, have_len);
    if (connected_mode != CONNECTED) {
	close (fd);
	return 0;
    }

    ret = init_fs_context(entry, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

	call = rx_NewCall (conn->connection);
	if (call == NULL) {
	    arla_warnx (ADEBMISC, "rx_NewCall failed");
	    ret = ENOMEM;
	    break;
	}

	collectstats_start(&collectstat);
	ret = StartRXAFS_StoreData (call, &entry->fid.fid,
				    storestatus,
				    0,
				    have_len,
				    entry->status.Length);
	if (host_downp(ret)) {
	    rx_EndCall(call, ret);
	    continue;
	} else if (ret) {
	    arla_warn (ADEBFCACHE, ret, "store-data");
	    rx_EndCall(call, 0);
	    break;
	}

	ret = copyfd2rx (fd, call, 0, have_len);
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
	collectstats_stop(&collectstat, entry, conn,
			  find_partition(&context),
			  STATISTICS_REQTYPE_STOREDATA, sizefs);
	break;
    }

    if (conn != NULL) {
	if (ret == 0) {
	    fcache_counter.store_data++;
	    update_entry (entry, &status, NULL, &volsync,
			  conn, ce->cred);
	} else {
	    ftruncate (fd, 0);
	    usedbytes -= entry->length; 
	    entry->length = 0;
	    entry->wanted_length = 0;
	    entry->fetched_length = 0;
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
truncate_file (FCacheEntry *entry, off_t size, 
	       AFSStoreStatus *storestatus, CredCacheEntry *ce)
{
    fs_server_context context;
    ConnCacheEntry *conn;
    struct rx_call *call;
    AFSFetchStatus status;
    AFSVolSync volsync;
    size_t have_len;
    int ret;

    AssertExclLocked(&entry->lock);

    if (connected_mode != CONNECTED)
	return 0;

    have_len = new_fetched_length(entry, size);

    ret = init_fs_context(entry, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

	call = rx_NewCall (conn->connection);
	if (call == NULL) {
	    arla_warnx (ADEBMISC, "rx_NewCall failed");
	    ret = ENOMEM;
	    break;
	}

	ret = StartRXAFS_StoreData (call,
				    &entry->fid.fid, 
				    storestatus,
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
	if (ret)
	    arla_warn (ADEBFCACHE, ret, "rx_EndCall");

	break;
    }

    if (ret == 0) {
	int fd;

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
	
	fcache_update_length (entry, size, have_len);
	
	fcache_counter.store_data++;
	update_entry (entry, &status, NULL, &volsync,
		      conn, ce->cred);
    }

    free_fs_server_context (&context);

    if (host_downp(ret))
	ret = ENETDOWN;

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
    ConnCacheEntry *conn = NULL;
    int ret;
    AFSFetchStatus status;
    AFSVolSync volsync;

    AssertExclLocked(&entry->lock);

    /* Don't write attributes to deleted files */
    if (entry->flags.silly)
	return 0;

    if (connected_mode == CONNECTED) {
	fs_server_context context;
	struct collect_stat collectstat;

	ret = init_fs_context(entry, ce, &context);
	if (ret)
	    return ret;

	for (conn = find_first_fs (&context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, ret)) {

	    collectstats_start(&collectstat);
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
		conn = NULL;
		goto out;
	    }
	    conn_ref(conn);
	    break;
	}

	if (ret == 0)
	    collectstats_stop(&collectstat, entry, conn,
			      find_partition(&context),
			      STATISTICS_REQTYPE_STORESTATUS, 1);


	free_fs_server_context (&context);

	if (host_downp(ret)) {
	    ret = ENETDOWN;
	    goto out;
	}
	update_entry (entry, &status, NULL, &volsync, conn, ce->cred);

    } else {
	assert (conn == NULL);

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
	ret = 0;
    }

 out:
    if (conn)
	conn_free(conn);
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
    ConnCacheEntry *conn = NULL;
    int ret;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    int fd;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode == CONNECTED) {
	fs_server_context context;

	ret = init_fs_context(dir_entry, ce, &context);
	if (ret)
	    return ret;

	for (conn = find_first_fs (&context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, ret)) {

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
		conn = NULL;
		goto out;
	    }
	    conn_ref(conn);
	    break;
	}

	free_fs_server_context (&context);

	if (host_downp(ret)) {
	    ret = ENETDOWN;
	    goto out;
	}

	update_entry (dir_entry, &status, &callback, &volsync,
		      conn, ce->cred);

    } else {
	static int fakefid = 1001;

	if (conn != NULL)
		exit(-1);

	ret = 0;

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
	fetch_attr->DataVersionHigh  = 0;
	fetch_attr->LockCount        = 0;
	fetch_attr->LengthHigh       = 0;
	fetch_attr->ErrorCode        = 0;
    }

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  conn, ce->cred);

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

    child_entry->tokens |= NNPFS_ATTR_R | NNPFS_DATA_R | NNPFS_DATA_W;
	
    fcache_release(child_entry);

 out:
    if (conn)
	conn_free(conn);

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
    ConnCacheEntry *conn = NULL;
    int ret;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;


    AssertExclLocked(&dir_entry->lock);

    if (connected_mode == CONNECTED) {
	fs_server_context context;

	ret = init_fs_context(dir_entry, ce, &context);
	if (ret)
	    return ret;

	for (conn = find_first_fs (&context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, ret)) {

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
		conn = NULL;
		goto out;
	    }
	    conn_ref(conn);
	    break;
	}
	free_fs_server_context (&context);

	if (host_downp(ret)) {
	    ret = ENETDOWN;
	    goto out;
	}

	update_entry (dir_entry, &status, &callback, &volsync,
		      conn, ce->cred);
    } else {
	static int fakedir = 1000;

	ret = 0;

	if (conn != NULL)
		exit(-1);

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
	fetch_attr->DataVersionHigh  = 0;
	fetch_attr->LockCount        = 0;
	fetch_attr->LengthHigh       = 0;
	fetch_attr->ErrorCode        = 0;
    }

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    assert(child_entry->length == 0);

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  conn, ce->cred);

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;

    ret = adir_mkdir (child_entry, child_fid->fid, dir_entry->fid.fid);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "adir_mkdir");
	fcache_release(child_entry);
	goto out;
    }

    child_entry->tokens |= NNPFS_ATTR_R | NNPFS_DATA_R | NNPFS_DATA_W;
	
    fcache_release(child_entry);

 out:
    if (conn)
	conn_free(conn);
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
    int ret;
    ConnCacheEntry *conn;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSVolSync volsync;
    AFSFetchStatus new_status;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = init_fs_context(dir_entry, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

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
	    conn = NULL;
	    goto out;
	}
	conn_ref(conn);
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &new_status, NULL, &volsync,
		  conn, ce->cred);

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  conn, ce->cred);

    /* 
     * flags.kernelp is set in cm_symlink since the symlink
     * might be a mountpoint and this entry is never install
     * into the kernel.
     */

    child_entry->flags.attrp = TRUE;
    child_entry->tokens |= NNPFS_ATTR_R;
	
    fcache_release(child_entry);

 out:
    if (conn)
	conn_free(conn);
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
    ConnCacheEntry *conn = NULL;
    int ret;
    AFSFetchStatus new_status;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = init_fs_context(dir_entry, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

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
	    conn = NULL;
	    goto out;
	}
	conn_ref(conn);
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &status, NULL, &volsync,
		  conn, ce->cred);

    update_entry (existing_entry, &new_status, NULL, NULL,
		  conn, ce->cred);

 out:
    if (conn)
	conn_free(conn);
    AssertExclLocked(&dir_entry->lock);
    return ret;
}

/*
 * Remove a file from a directory.
 */

int
remove_file (FCacheEntry *dir_entry, const char *name, CredCacheEntry *ce)
{
    int ret;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode == CONNECTED) {

	ret = init_fs_context(dir_entry, ce, &context);
	if (ret)
	    return ret;

	for (conn = find_first_fs (&context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn, ret)) {
	    
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
		conn = NULL;
		goto out;
	    }
	    conn_ref(conn);
	    break;
	}
	free_fs_server_context (&context);
	
	if (host_downp(ret))
	    ret = ENETDOWN;

    } else {
	fbuf the_fbuf;
	VenusFid child_fid;
	int fd;

	status = dir_entry->status;
	
	conn = NULL;

	ret = fcache_get_fbuf (dir_entry, &fd, &the_fbuf,
			       O_RDONLY, FBUF_READ|FBUF_SHARED);
	if (ret)
	    goto out;
	
	ret = fdir_lookup(&the_fbuf, &dir_entry->fid, name, &child_fid);
	if (ret == 0) {
	    FCacheEntry *child_entry = NULL;
	    uint32_t disco_id = 0;

	    ret = fcache_find(&child_entry, child_fid);
	    if (ret == 0)
		disco_id = child_entry->disco_id;

	    disco_id = disco_unlink(&dir_entry->fid, &child_fid,
				    name, disco_id);

	    if (child_entry) {
		child_entry->disco_id = disco_id;
		fcache_release(child_entry);
	    }
	    ret = 0;
	}
	    
	fbuf_end (&the_fbuf);
	close (fd);
    }

    if (ret == 0)
	update_entry (dir_entry, &status, NULL, &volsync,
		      conn, ce->cred);

 out:
    if (conn)
	conn_free(conn);
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
    int ret;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fs_server_context context;

    AssertExclLocked(&dir_entry->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = init_fs_context(dir_entry, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

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
	    conn = NULL;
	    goto out;
	}
	conn_ref(conn);
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (dir_entry, &status, NULL, &volsync,
		  conn, ce->cred);

 out:
    if (conn)
	conn_free(conn);
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
    fs_server_context context;

    AssertExclLocked(&old_dir->lock);
    AssertExclLocked(&new_dir->lock);

    if (connected_mode != CONNECTED)
	return EINVAL;

    ret = init_fs_context(old_dir, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

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
	    conn = NULL;
	    goto out;
	}
	conn_ref(conn);
	break;
    }
    free_fs_server_context (&context);

    if (host_downp(ret)) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (old_dir, &orig_status, NULL, &volsync,
		  conn, ce->cred);

    update_entry (new_dir, &new_status, NULL, &volsync,
		  conn, ce->cred);

 out:
    if (conn)
	conn_free(conn);
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
    int ret;
    const char *this_cell = cell_getthiscell ();
    int32_t this_cell_id;

    if (dynroot_enablep()) {
	this_cell = "dynroot";
	this_cell_id = dynroot_cellid();
    } else {
	this_cell_id = cell_name2num (this_cell);
	if (this_cell_id == -1)
	    arla_errx (1, ADEBERROR, "cell %s does not exist", this_cell);
    }

    ret = volcache_getbyname (root_volume, this_cell_id, ce, &ve, NULL);
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
    VolCacheEntry *vol;
    int i, error;

    *res = NULL;

    old = find_entry (fid);
    if (old) {
	assert (old->flags.usedp);
	*res = old;
	return 0;
    }

    error = volcache_getbyid (fid.fid.Volume, fid.Cell, ce, &vol, NULL);
    if (error) {
	if (connected_mode == DISCONNECTED && error == ENOENT)
	    return ENETDOWN;
	return error;
    }

    e = find_free_entry ();
    if (e == NULL) {
	    arla_warnx(ADEBMISC, "find_free_entry failed");
	    return(-1);
    }

    old = find_entry (fid);
    if (old) {
	AssertExclLocked(&e->lock);
	ReleaseWriteLock (&e->lock);

	e->lru_le = listaddtail (lrulist, e);
	if (e->lru_le == NULL)
		exit(-1);

	if (!old->flags.usedp) 
		exit(-1);
	*res = old;
	return 0;
    }

    e->fid     	       = fid;
    e->refcount        = 0;
    e->host	       = 0;
    e->length          = 0;
    e->wanted_length   = 0;
    e->fetched_length  = 0;
    memset (&e->status,   0, sizeof(e->status));
    memset (&e->callback, 0, sizeof(e->callback));
    memset (&e->volsync,  0, sizeof(e->volsync));
    for (i = 0; i < NACCESS; i++) {
	e->acccache[i].cred = ARLA_NO_AUTH_CRED;
	e->acccache[i].access = 0;
    }
    e->anonaccess      = 0;
    e->flags.usedp     = TRUE;
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
    if(!e->lru_le)
	    exit(-1);
    e->invalid_ptr     = -1;
    e->volume	       = vol;
    e->priority	       = fprio_get(fid);
    e->hits	       = 0;
    e->cleanergen      = 0;
    
    hashtabadd (hashtab, e);

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
    struct nnpfs_message_installnode node;
    struct nnpfs_message_installattr attr;
} nnpfs_message_install_node_attr;

static int
bulkstat_help_func (VenusFid *fid, const char *name, void *ptr)
{
    struct bulkstat *bs = (struct bulkstat *) ptr;
    AccessEntry *ae;
    FCacheEntry key;
    FCacheEntry *e;

    /* Is bs full ? */
    if (bs->len > fcache_bulkstatus_num)
	return 0;

    /* Ignore . and .. */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	return 0;

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
	return 0;
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
	return 0;
    }

    if (fcache_enable_bulkstatus == 2) {
	/* cache the name for the installnode */
	bs->names[bs->len] = strdup (name);
	if (bs->names[bs->len] == NULL)
	    return 0;
    } else {
	bs->names[bs->len] = NULL;
    }
    

    bs->fids[bs->len] = fid->fid;
    bs->len++;

    return 0;
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
    ConnCacheEntry *conn = NULL;
    struct bulkstat bs;
    AFSBulkStats stats;
    AFSVolSync sync;
    AFSCBFids fids;
    fbuf the_fbuf;
    int ret, fd;
    AFSCBs cbs;
    int i;
    int len;
    struct collect_stat collectstat;

    arla_warnx (ADEBFCACHE, "get_attr_bulk");

    if (fcache_enable_bulkstatus == 0)
	return -1;

    if (parent_entry->length == 0) {
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
			parent_entry->fid,
			NULL);
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

    ret = init_fs_context(parent_entry, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

	stats.val = NULL;
	cbs.val   = NULL;
	stats.len = cbs.len = 0;

	collectstats_start(&collectstat);
	ret = RXAFS_BulkStatus (conn->connection, &fids, &stats, &cbs, &sync);
	collectstats_stop(&collectstat, parent_entry, conn,
			  find_partition(&context),
			  STATISTICS_REQTYPE_BULKSTATUS, fids.len);
	if (ret) {
	    free (stats.val);
	    free (cbs.val);
	}

	if (host_downp(ret)) {
	    continue;
	} else if (ret) {
	    free_fs_server_context(&context);
	    arla_warn(ADEBFCACHE, ret, "BulkStatus");
	    conn = NULL;
	    goto out_names;
	}
	conn_ref(conn);
	break;
    }

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
	VenusFid fid;

	fcache_counter.fetch_attr_bulk += len;

	fid.Cell = parent_entry->fid.Cell;
	for (i = 0; i < len && ret == 0; i++) {

	    fid.fid = fids.val[i];
	    
	    if (VenusFid_cmp(prefered_fid, &fid) == 0) {
		e = prefered_entry;
	    } else {
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
			       conn,
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
	nnpfs_message_install_node_attr msg[AFSCBMAX];
	struct nnpfs_msg_node *node;
	nnpfs_handle *parent;
	FCacheEntry *e;
	VenusFid fid;
	int j;

	fid.Cell = parent_entry->fid.Cell;
	for (i = 0 , j = 0; i < len && ret == 0; i++) {
	    u_int tokens;

	    fid.fid = fids.val[i];
	    
	    if (VenusFid_cmp(prefered_fid, &fid) == 0) {
		e = prefered_entry;
	    } else {
		e = find_entry_nolock (fid);
		if (e != NULL && CheckLock(&e->lock) != 0)
		    continue;

		ret = fcache_get (&e, fid, ce);
		if (ret)
		    break;
	    }


	    arla_warnx (ADEBFCACHE, "installing %d.%d.%d\n",
			e->fid.fid.Volume,
			e->fid.fid.Vnode,
			e->fid.fid.Unique);
	    assert_flag(e,kernelp);
	    e->flags.attrusedp 	= TRUE;
	    
	    /*
	     * Its its already installed, just update with installattr
	     */
	    
	    e->tokens			|= NNPFS_ATTR_R;
	    tokens				= e->tokens;
	    if (!e->flags.kernelp || !e->flags.datausedp)
		tokens			&= ~NNPFS_DATA_MASK;
	    
	    if (e->flags.kernelp) {
		msg[j].attr.header.opcode	= NNPFS_MSG_INSTALLATTR;
		node			= &msg[j].attr.node;
		parent			= NULL;
	    } else {
		msg[j].node.header.opcode	= NNPFS_MSG_INSTALLNODE;
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
		fcacheentry2nnpfsnode (&e->fid,
				       &e->fid,
				       &stats.val[i],
				       node, 
				       parent_entry->acccache,
				       FCACHE2NNPFSNODE_ALL);
		
		if (parent)
		    *parent = *(struct nnpfs_handle*) &parent_entry->fid;
		j++;
	    }
	    if (prefered_entry != e)
		fcache_release(e);
	}

	/*
	 * Install if there is no error and we have something to install
	 */
	
	if (ret == 0 && j != 0)
	    ret = nnpfs_send_message_multiple_list (kernel_fd,
						    (struct nnpfs_message_header *) msg,
						    sizeof (msg[0]),
						    j);
	/* We have what we wanted, ignore errors */
  	if (ret && i > 0 && prefered_entry)
	    ret = 0;
    }
    
    free (stats.val);
    free (cbs.val);

 out_names:
    for (i = 0 ; i < bs.len && ret == 0; i++)
	free (bs.names[i]);

    if (conn)
	conn_free(conn);

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
fcache_verify_attr (FCacheEntry *entry, FCacheEntry *parent,
		    const char *prefered_name, CredCacheEntry* ce)
{
    AccessEntry *ae;

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
	entry->tokens |= NNPFS_ATTR_R;
	entry->flags.attrp = TRUE;
	return 0;
    }

    if (connected_mode == DISCONNECTED) {
	if (entry->flags.attrp) {
	    AccessEntry *ae;
	    findaccess(ce->cred, entry->acccache, &ae);
	    ae->cred = ce->cred;
	    ae->access = 0x7f; /* XXXDISCO */
	    return 0;
	}
	else
	    return ENETDOWN;
    }

    /*
     * If there is no parent, `entry' is a root-node, or the parent is
     * un-initialized, don't bother bulkstatus.
     */
    if (parent			    != NULL
	&& entry->fid.fid.Vnode     != 1
	&& entry->fid.fid.Unique    != 1
	&& !entry->flags.mountp
	&& !entry->flags.fake_mp
	&& entry->parent.Cell       != 0
	&& entry->parent.fid.Volume != 0
	&& entry->parent.fid.Vnode  != 0
	&& entry->parent.fid.Unique != 0)
    {
	/*
	 * Check if the entry is used, that means that
	 * there is greater chance that we we'll succeed
	 * when doing bulkstatus.
	 */

	if (parent->hits++ > fcache_bulkstatus_num &&
	    parent->flags.datausedp) {
	    int error;
	
	    arla_warnx (ADEBFCACHE, "fcache_get_attr: doing bulk get_attr");

	    error = get_attr_bulk (parent,
				   entry, &entry->fid,
				   prefered_name, ce);
	    /* magic calculation when we are going to do next bulkstat */
	    parent->hits = 0;

	    if (error == 0)
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
	return ENETDOWN;

    ret = init_fs_context(e, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {
	ret = read_data (e, conn, ce, find_partition(&context));
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
	if (e->wanted_length <= e->fetched_length) {
	    fcache_counter.fetch_data_cached++;
	    return 0;
	} else
	    return do_read_data (e, ce);
    } else {
	ret = do_read_attr (e, ce, &conn, &context);
	if (ret)
	    return ret;
	if (e->wanted_length <= e->fetched_length) {
	    fcache_counter.fetch_data_cached++;
	    free_fs_server_context (&context);
	    return 0;
	}
    }
    ret = read_data (e, conn, ce, find_partition(&context));
    free_fs_server_context (&context);
    return ret;
}

/*
 * Fetch `fid' with data, returning the cache entry in `res'.
 * note that `fid' might change.
 */

int
fcache_get_data (FCacheEntry **e, CredCacheEntry **ce,
		 size_t wanted_length)
{
    int ret;

    if ((*e)->flags.fake_mp) {
	VenusFid new_fid;
	FCacheEntry *new_root;

	ret = resolve_mp (e, &new_fid, ce);
	if (ret) {
	    return ret;
	}
	ret = fcache_get (&new_root, new_fid, *ce);
	if (ret) {
	    return ret;
	}
	ret = fcache_verify_attr (new_root, NULL, NULL, *ce);
	if (ret) {
	    fcache_release (new_root);
	    return ret;
	}
	(*e)->flags.fake_mp   = FALSE;
	(*e)->flags.mountp    = TRUE;
	(*e)->status.FileType = TYPE_LINK;
	update_fid ((*e)->fid, *e, new_fid, new_root);
	fcache_release (*e);
	*e  = new_root;
	install_attr (*e, FCACHE2NNPFSNODE_ALL);
    }

    if (wanted_length) {
	(*e)->wanted_length = wanted_length;
    } else {
	/*
	 * XXX remove this case, attr should either be known already
	 * here, or we should just fetch `whole file'/next block.
	 */

        ret = fcache_verify_attr (*e, NULL, NULL, *ce);
        if (ret) {
            return ret;
        }
        if ((*e)->length == 0 || !uptodatep(*e)) {
            (*e)->wanted_length = (*e)->status.Length;
        }
    }
	
    ret = fcache_verify_data (*e, *ce);
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
	     uint32_t *volid, VolCacheEntry **ve)
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
	else if ((*ve)->entry.flags & VLF_RWEXISTS)
	    result_type = RWVOL;
	else {
	    volcache_free (*ve);
	    return ENOENT;
	}
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
    uint32_t volid;
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

    e->parent = *parent;
    ret = fcache_verify_attr (e, parent_e, NULL, *ce);
    if (ret) {
	fcache_release(e);
	return ret;
    }

    if (e->flags.mountp)
	ret = resolve_mp (&e, fid, ce);
     
    fcache_release(e);
    return ret;
}

/*
 * actually resolve a mount-point
 */

static int
resolve_mp (FCacheEntry **e, VenusFid *ret_fid, CredCacheEntry **ce)
{
    VenusFid fid = (*e)->fid;
    int ret;
    fbuf the_fbuf;
    char *buf;
    int fd;
    uint32_t length;

    assert ((*e)->flags.fake_mp || (*e)->flags.mountp);
    AssertExclLocked(&(*e)->lock);

    (*e)->wanted_length = (*e)->status.Length;

    ret = fcache_verify_data (*e, *ce);
    if (ret)
	return ret;

    length = (*e)->status.Length;

    fd = fcache_open_file (*e, O_RDONLY);
    if (fd < 0)
	return errno;

    ret = fbuf_create (&the_fbuf, fd, length,
		       FBUF_READ|FBUF_WRITE|FBUF_PRIVATE);
    if (ret) {
	close (fd);
	return ret;
    }
    buf = fbuf_buf (&the_fbuf);

    ret = get_root_of_volume (&fid, &(*e)->parent, (*e)->volume, 
			      ce, buf, length);

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
	     e->length != 0 ?" data":"",
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
	     (long)usedbytes, (long)lowbytes, (long)highbytes);
    hashtabforeach (hashtab, print_entry, NULL);
}

/*
 *
 */

void
fcache_update_length (FCacheEntry *e, size_t len, size_t have_len)
{
    AssertExclLocked(&e->lock);

    assert (len >= e->length || e->length - len <= usedbytes);
    assert (have_len <= len);

    usedbytes = usedbytes - e->length + len;
    e->length = len;
    e->wanted_length = min(have_len,len);
    e->fetched_length = have_len;
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

    ret = init_fs_context(dire, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {

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
		      conn, ce->cred);
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

    ret = init_fs_context(dire, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {
	ret = RXAFS_StoreACL (conn->connection, &fid.fid,
			      opaque, &status, &volsync);
	if (!try_next_fs (ret, &fid))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "StoreACL");

    if (ret == 0)
	update_entry (dire, &status, NULL, &volsync,
		      conn, ce->cred);
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
	   char *volumename, size_t volumenamesz,
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

    ret = init_fs_context(dire, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {
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
    if (ret == 0 && volumename[0] == '\0') {
	if (volcache_getname (fid.fid.Volume, fid.Cell,
			      volumename, volumenamesz) == -1)
	    strlcpy(volumename, "<unknown>", volumenamesz);
    }

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

    ret = init_fs_context(dire, ce, &context);
    if (ret)
	return ret;

    for (conn = find_first_fs (&context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn, ret)) {
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
    int64_t *a = arg;
    FCacheEntry *e = listdata (li);

    *a += e->length;
    
    return FALSE;
}


int64_t
fcache_calculate_usage (void)
{
    int64_t size = 0;

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
