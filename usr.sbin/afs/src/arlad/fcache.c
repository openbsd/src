/*	$OpenBSD: fcache.c,v 1.2 1999/04/30 01:59:07 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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
RCSID("$KTH: fcache.c,v 1.189 1999/04/20 20:58:08 map Exp $") ;

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

static u_long lowvnodes, highvnodes, lowbytes, highbytes;

/* current values */

static u_long usedbytes, usedvnodes;

/* 
 * This is how far the cleaner will go to clean out entries.
 * The higher this is, the higher is the risk that you will
 * lose any file that you feel is important to disconnected
 * operation. 
 */

unsigned fprioritylevel;

static int node_count;		/* XXX */

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
fcache_highvnodes(void)
{
    return highvnodes;
}

u_long
fcache_usedvnodes(void)
{
    return usedvnodes;
}

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

/*
 * Globalnames 
 */

char arlasysname[SYSNAMEMAXLEN];

/*
 * return the file name of the cached file for `entry'.
 */

int
fcache_file_name (FCacheEntry *entry, char *s, size_t len)
{
    return snprintf (s, len, "%04X", (unsigned)entry->index);
}

/*
 * return the file name of the converted directory for `entry'.
 */

int
fcache_extra_file_name (FCacheEntry *entry, char *s, size_t len)
{
    int ret;

    assert (entry->flags.datap &&
	    entry->flags.extradirp &&
	    entry->status.FileType == TYPE_DIR);

    ret = fcache_file_name (entry, s, len);
    *s += 0x10;
    return ret;
}

static int fhopen_working;

/*
 * open file by handle
 */

static int
fhopen (xfs_cache_handle *handle, int flags)
{
    struct ViceIoctl vice_ioctl;

    vice_ioctl.in      = (caddr_t)handle;
    vice_ioctl.in_size = sizeof(*handle);

    vice_ioctl.out      = NULL;
    vice_ioctl.out_size = 0;

    return k_pioctl (NULL, VIOC_FHOPEN, &vice_ioctl, flags);
}

/*
 * get the handle of `filename'
 */

int
fcache_fhget (char *filename, xfs_cache_handle *handle)
{
    struct ViceIoctl vice_ioctl;

    if (!fhopen_working)
	return 0;

    vice_ioctl.in      = NULL;
    vice_ioctl.in_size = 0;

    vice_ioctl.out      = (caddr_t)handle;
    vice_ioctl.out_size = sizeof(*handle);

    return k_pioctl (filename, VIOC_FHGET, &vice_ioctl, 0);
}

/*
 * create a new cache vnode
 */

int
fcache_create_file (FCacheEntry *entry)
{
    char fname[MAXPATHLEN];
    int fd;
    int ret;

    fcache_file_name (entry, fname, sizeof(fname));
    fd = open (fname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
    assert (fd >= 0);
    ret = close (fd);
    assert (ret == 0);
    return fcache_fhget (fname, &entry->handle);
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
	ret = fhopen (&entry->handle, flag);
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
 * Create `n' new entries
 */

static void
create_new_entries (unsigned n)
{
    FCacheEntry *entries;
    unsigned int i, j;

    entries = calloc (n, sizeof(FCacheEntry));
    if (entries == NULL)
	arla_errx (1, ADEBERROR, "fcache: calloc failed");
    
    for (i = 0; i < n; ++i) {
	entries[i].lru_le      = listaddhead (lrulist, &entries[i]);
	entries[i].invalid_ptr = -1;
	entries[i].volume      = NULL;
	entries[i].refcount    = 0;
	entries[i].anonaccess  = 0;
	for (j = 0; j < NACCESS; j++) {
	    entries[i].acccache[j].cred = ARLA_NO_AUTH_CRED;
	    entries[i].acccache[j].access = 0;
	}
	Lock_Init(&entries[i].lock);
    }
}


/*
 * Discard the data cached for `entry'.
 */

static void
throw_data (FCacheEntry *entry)
{
    int fd;
    struct stat sb;
    int ret;

    assert (entry->flags.datap && entry->flags.usedp
	    && CheckLock(&entry->lock) == -1);

    fd = fcache_open_file (entry, O_WRONLY);
    if (fd < 0) {
	arla_warn (ADEBFCACHE, errno, "fcache_open_file");
	return;
    }
    if (fstat (fd, &sb) < 0) {
	arla_warn (ADEBFCACHE, errno, "fstat");
	ret = close (fd);
	assert (ret == 0);
	return;
    }
    if (entry->status.FileType != TYPE_DIR) {
	assert (sb.st_size == entry->status.Length);
    }
    if (ftruncate (fd, 0) < 0) {
	arla_warn (ADEBFCACHE, errno, "ftruncate");
	ret = close (fd);
	assert (ret == 0);
	return;
    }
    ret = close (fd);
    assert (ret == 0);
    if (entry->flags.extradirp) {
	char fname[MAXPATHLEN];

	fcache_extra_file_name (entry, fname, sizeof(fname));
	unlink (fname);
    }
    assert(usedbytes >= entry->status.Length);
    usedbytes -= sb.st_size;
    entry->flags.datap = FALSE;
    entry->flags.extradirp = FALSE;
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

    assert (entry->flags.usedp && entry->volume != NULL
	    && CheckLock(&entry->lock) == -1);

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
    volcache_free (entry->volume);
    entry->volume = NULL;
/*    entry->inode  = 0;*/
    entry->flags.attrp = FALSE;
    entry->flags.usedp = FALSE;
    --usedvnodes;
    LWP_NoYieldSignal (lrulist);
}

/*
 * Return the next cache node number.
 */

static ino_t
next_cache_index (void)
{
     return node_count++;
}

/*
 * Allocate a cache file for `e'
 */

static void
create_node (FCacheEntry *e)
{
    assert (CheckLock (&e->lock) == -1);
    e->index = next_cache_index ();
    fcache_create_file (e);
}

/*
 * Pre-create cache nodes for all entries in lrulist that doesn't have any.
 */

static void
create_nodes (char *arg)
{
    Listitem *item;
    FCacheEntry *entry;
    unsigned count = 0;

    arla_warnx (ADEBFCACHE,
		"pre-creating nodes");

    for (item = listtail (lrulist);
	 item;
	 item = listprev (lrulist, item)) {

	entry = (FCacheEntry *)listdata (item);
	assert (entry->lru_le == item);
	if (!entry->flags.usedp
	    && CheckLock(&entry->lock) == 0
	    && entry->index == 0) {
	    struct timeval tv;

	    ObtainWriteLock (&entry->lock);
	    create_node (entry);
	    ReleaseWriteLock (&entry->lock);
	    ++count;
	    tv.tv_sec = 0;
	    tv.tv_usec = 1000;
	    IOMGR_Select(0, NULL, NULL, NULL, &tv);
	}
    }

    arla_warnx (ADEBFCACHE,
		"pre-created %u nodes", count);
}

/*
 *
 */

static void
cleaner (char *arg)
{
    for (;;) {
	Listitem *item, *prev;
	FCacheEntry *entry;
	int prio;

	arla_warnx(ADEBCLEANER,
		   "running cleaner: "
		   "%lu (%lu-%lu) files, "
		   "%lu (%lu-%lu) bytes",
		   usedvnodes, lowvnodes, highvnodes,
		   usedbytes, lowbytes, highbytes);

	for (prio = 0 ; prio <= fprioritylevel ; prio += 10) {

	    while (usedvnodes > lowvnodes) {

		for (item = listtail (lrulist);
		     item && usedvnodes > lowvnodes;
		     item = prev) {
		    prev = listprev (lrulist, item);
		    entry = (FCacheEntry *)listdata (item);
		    assert (entry->lru_le == item);
		    if (entry->flags.usedp
			&& !entry->flags.attrusedp
			&& !entry->flags.datausedp
			&& entry->priority < prio
			&& entry->refcount == 0
			&& CheckLock(&entry->lock) == 0) {
			listdel (lrulist, item);
			ObtainWriteLock (&entry->lock);
			throw_entry (entry);
			ReleaseWriteLock (&entry->lock);
			entry->lru_le = listaddtail (lrulist, entry);
			break;
		    }
		}
		if (item == NULL)
		    break;
	    }

	    for (item = listtail (lrulist);
		 item && usedbytes > lowbytes;
		 item = listprev (lrulist, item)) {
		entry = (FCacheEntry *)listdata (item);
		assert (entry->lru_le == item);
		if (entry->flags.datap
		    && !entry->flags.datausedp
		    && entry->priority < prio
		    && entry->refcount == 0
		    && CheckLock(&entry->lock) == 0) {
		    ObtainWriteLock (&entry->lock);
		    throw_data (entry);
		    ReleaseWriteLock (&entry->lock);
		}
	    }
	}

	arla_warnx(ADEBCLEANER,
		   "cleaner done: "
		   "%lu (%lu-%lu) files, "
		   "%lu (%lu-%lu) bytes",
		   usedvnodes, lowvnodes, highvnodes,
		   usedbytes, lowbytes, highbytes);

	IOMGR_Sleep (CLEANER_SLEEP);
    }
}

/*
 * XXX: will not work if an entry with shorter invalidation time
 *      than the shortest existing invalidation time is inserted.
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
	    heap_remove_head (invalid_heap);
	    entry->invalid_ptr = -1;
	    if (entry->flags.kernelp)
		break_callback (entry->fid);
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
    heap_insert (invalid_heap, (const void *)e, &e->invalid_ptr);
    LWP_NoYieldSignal (invalid_heap);
}

/*
 * Remove the entry least-recently used and return it.  Sleep until
 * there's an entry.
 */

static FCacheEntry *
unlink_lru_entry (void)
{
     FCacheEntry *entry = NULL;
     Listitem *item;
     
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
	 LWP_WaitProcess (lrulist);
     }
}

/*
 *
 */

static void
emergency_remove_data (size_t sz)
{
     FCacheEntry *entry = NULL;
     Listitem *item;
     int prio;

     for (prio = 0 ; prio <= fprioritylevel ; prio += 10) {
     
	 for (item = listtail (lrulist);
	      item && usedbytes + sz > highbytes;
	      item = listprev (lrulist, item)) {
	     entry = (FCacheEntry *)listdata (item);
	     if (entry->flags.datap
		 && !entry->flags.datausedp
		 && entry->priority < prio
		 && CheckLock(&entry->lock) == 0) {
		 ObtainWriteLock (&entry->lock);
		 throw_data (entry);
		 ReleaseWriteLock (&entry->lock);
	     }
	 }
     }
}

/*
 * Return a usable entry.
 */

static FCacheEntry *
find_free_entry (void)
{
    FCacheEntry *entry;

    entry = unlink_lru_entry ();
    if (entry == NULL)
	arla_warnx (ADEBFCACHE, "All vnode entries in use");
    else {
	assert (CheckLock(&entry->lock) == -1);
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

    fd = open ("fcache.new", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0)
	return errno;
    n = 0;
    for (item = listtail (lrulist);
	 item;
	 item = listprev (lrulist, item)) {
	FCacheEntry *entry = (FCacheEntry *)listdata (item);

	if (!entry->flags.usedp)
	    continue;
	if (write (fd, entry, sizeof(*entry)) != sizeof(*entry)) {
	    int save_errno = errno;
	    int ret;

	    ret = close (fd);
	    assert (ret == 0);
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
    int ret;

    fd = open ("fcache", O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return;
    n = 0;
    while (read (fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
	CredCacheEntry *ce;
	FCacheEntry *e;
	int i;
	VolCacheEntry *vol;
	int res;
	int32_t type;

	ce = cred_get (tmp.fid.Cell, 0, 0);
	assert (ce != NULL);

	res = volcache_getbyid (tmp.fid.fid.Volume, tmp.fid.Cell,
				ce, &vol, &type);
	cred_free (ce);
	if (res)
	    continue;

	e = find_free_entry ();

	++n;

	e->fid      = tmp.fid;
	e->host     = 0;
	e->status   = tmp.status;
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
	e->flags.sentenced = FALSE;
	e->tokens = tmp.tokens;
	e->parent = tmp.parent;
	e->realfid = tmp.realfid;
	e->priority = tmp.priority;
	e->lru_le = listaddhead (lrulist, e);
	e->volume = vol;
	hashtabadd (hashtab, e);
	if (e->flags.datap)
	    usedbytes += e->status.Length;
	ReleaseWriteLock (&e->lock);
    }
    ret = close (fd);
    assert (ret == 0);
    arla_warnx (ADEBFCACHE, "recovered %u entries to fcache", n);
}

/*
 * Search for `cred' in `ae' and return a pointer in `pos'.  If it
 * already exists return TRUE, else return FALSE and set pos to a
 * random slot.
 */

Bool
findaccess (pag_t cred, AccessEntry *ae, AccessEntry **pos)
{
     int i;

     for(i = 0; i < NACCESS ; ++i)
	  if(ae[i].cred == cred) {
	      *pos = &ae[i];
	      return TRUE;
	  }

     assert (i == NACCESS);
     i = rand() % NACCESS;
     *pos = &ae[i];
     
     return FALSE;
}

/*
 * Find the next fileserver for the request in `context'.
 * Returns a ConnCacheEntry or NULL.
 */

ConnCacheEntry *
find_next_fs (fs_server_context *context, ConnCacheEntry *prev_conn)
{
    if (prev_conn != NULL)
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
    int bit = 0;
    int num_clones;
    int cell = e->fid.Cell;
    int ret;

    if (connected_mode == DISCONNECTED) {
	context->num_conns = 0;
	return NULL;
    }

    ret = volume_make_uptodate (ve, ce);
    if (ret)
	return NULL;


    if (ve->entry.volumeId[RWVOL] == e->fid.fid.Volume
	&& ve->entry.flags & VLF_RWEXISTS)
	bit = VLSF_RWVOL;

    if (ve->entry.volumeId[ROVOL] == e->fid.fid.Volume
	&& ve->entry.flags & VLF_ROEXISTS)
	bit = VLSF_ROVOL;

    if (ve->entry.volumeId[BACKVOL] == e->fid.fid.Volume
	&& ve->entry.flags & VLF_BACKEXISTS)
	bit = VLSF_RWVOL;

    assert (bit);

    num_clones = 0;
    for (i = 0; i < NMAXNSERVERS; ++i) {
	u_long addr = htonl(ve->entry.serverNumber[i]);

	if (ve->entry.serverFlags[i] & bit
	    && addr != 0) {
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

    return find_next_fs (context, NULL);
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

    fhopen_working = k_hasafs ();

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

    invalid_heap = heap_new (highvnodes, expiration_time_cmp);
    if (invalid_heap == NULL)
	arla_errx (1, ADEBERROR, "fcache: heap_new failed");

    create_new_entries (highvnodes);

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
 *
 */

int
fcache_reinit(u_long alowvnodes, 
	      u_long ahighvnodes, 
	      u_long alowbytes,
	      u_long ahighbytes)
{
    arla_warnx (ADEBFCACHE, "fcache_reinit");

    if (ahighvnodes > highvnodes) {
	create_new_entries(ahighvnodes - highvnodes);
	highvnodes = ahighvnodes;
    } else
	return EINVAL;

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
	    break_callback (e->fid);
	e->flags.kernelp   = FALSE;
	e->flags.attrp     = FALSE;
	e->flags.datap     = FALSE;
	e->flags.attrusedp = FALSE;
	e->flags.datausedp = FALSE;
	ReleaseWriteLock (&e->lock);
    }
}

/*
 * Call stale on the entry corresponding to `fid', if any.
 */

void
fcache_stale_entry (VenusFid fid, AFSCallBack callback)
{
    FCacheEntry *e;

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
    pag_t pag;
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
		    install_attr (e);
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
fcache_purge_cred (pag_t pag, int32_t cell)
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

    if (e->fid.Cell == fid->Cell &&
	e->fid.fid.Volume == fid->fid.Volume) {
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
 * Mark `entry' as not being used. Wake up any threads sleeping in
 * unlink_lru_entry
 */

void
fcache_unused (FCacheEntry *entry)
{
    entry->flags.datausedp = entry->flags.attrusedp = FALSE;
    LWP_NoYieldSignal (lrulist);
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
	      pag_t cred)
{
    struct timeval tv;
    AccessEntry *ae;
    unsigned long bitmask = 0141777; /* REG, DIR, STICKY, USR, GRP, OTH */

    if (cell_issuid_by_num (entry->volume->cell))
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
	volcache_update_volsync (entry->volume, *volsync);
    }
    entry->host     = host;

    entry->anonaccess = status->AnonymousAccess;
    findaccess (cred, entry->acccache, &ae);
    ae->cred   = cred;
    ae->access = status->CallerAccess;
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

	if (entry->flags.attrp && entry->host != 0) {
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

	if (entry->flags.usedp && entry->host != 0) {
	    CredCacheEntry *ce;	
	    ConnCacheEntry *conn;
	    AFSFetchStatus status;
	    AFSCallBack callback;
	    AFSVolSync volsync;

	    ce = cred_get (entry->fid.Cell, 0, CRED_ANY);
	    assert (ce != NULL);

	    conn = conn_get (entry->fid.Cell, entry->host, afsport,
			     FS_SERVICE_ID, fs_probe, ce);
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
	    }
	}
    }
    return 0;			/* XXX */
}

/*
 * Return true iff there's any point in trying the next fs.
 */

static Bool
try_next_fs (int error)
{
    switch (error) {
    case RX_CALL_DEAD :
    case ARLA_VSALVAGE :
    case ARLA_VNOSERVICE :
    case ARLA_VOFFLINE :
    case ARLA_VBUSY :
    case ARLA_VIO :
	return TRUE;
    case ARLA_VMOVED :
    case ARLA_VNOVOL :
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
 */

static int
do_read_attr (FCacheEntry *entry,
	      CredCacheEntry *ce,
	      ConnCacheEntry **ret_conn,
	      fs_server_context *ret_context)
{
    int ret = RX_CALL_DEAD;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;

    assert (CheckLock(&entry->lock) == -1);

    for (conn = find_first_fs (entry, ce, ret_context);
	 conn != NULL;
	 conn = find_next_fs (ret_context, conn)) {
	ret = RXAFS_FetchStatus (conn->connection,
				 &entry->fid.fid,
				 &status,
				 &callback,
				 &volsync);

	if (!try_next_fs (ret))
	    break;
    }
    assert (CheckLock(&entry->lock) == -1);

    if (ret) {
	if (ret == RX_CALL_DEAD) {
	    if (connected_mode == DISCONNECTED && entry->flags.attrp)
		return 0;
	    ret = ENETDOWN;
	}
	arla_warn (ADEBFCACHE, ret, "fetch-status");
	return ret;
    }

    if (entry->flags.datap
	&& entry->status.DataVersion != status.DataVersion) {
	throw_data (entry);
	entry->tokens &= ~(XFS_DATA_R|XFS_DATA_W);
    }

    update_entry (entry, &status, &callback, &volsync,
		  rx_HostOf (rx_PeerOf (conn->connection)),
		  ce->cred);

    entry->tokens |= XFS_ATTR_R;
    entry->flags.attrp = TRUE;
    
    assert (CheckLock(&entry->lock) == -1);

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

    assert (CheckLock(&entry->lock) == -1);

    ret = do_read_attr (entry, ce, &conn, &context);
    free_fs_server_context (&context);
    if (ret)
	return ret;
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

    arla_warnx (ADEBMISC, "read_data");

    assert (CheckLock(&entry->lock) == -1);

    if (connected_mode == DISCONNECTED) 
	return ENETDOWN;

    if (usedbytes + entry->status.Length > highbytes)
	emergency_remove_data (entry->status.Length);

    if (usedbytes + entry->status.Length > highbytes) {
	ret = ENOSPC;
	goto out;
    }

    call = rx_NewCall (conn->connection);
    if (call == NULL) {
	arla_warnx (ADEBMISC, "rx_NewCall failed");
	ret = ENOMEM;
	goto out;
    }

    assert (CheckLock(&entry->lock) == -1);

    ret = StartRXAFS_FetchData (call, &entry->fid.fid, 
				0, entry->status.Length);

    assert (CheckLock(&entry->lock) == -1);

    if(ret) {
	arla_warn (ADEBFCACHE, ret, "fetch-data");
	goto out;
    }

    assert (CheckLock(&entry->lock) == -1);

    ret = rx_Read (call, &sizefs, sizeof(sizefs));

    assert (CheckLock(&entry->lock) == -1);

    if (ret != sizeof(sizefs)) {
	ret = rx_Error(call);
	arla_warn (ADEBFCACHE, ret, "Error reading length");
	rx_EndCall(call, 0);
	goto out;
    }
    sizefs = ntohl (sizefs);

    assert (sizefs == entry->status.Length);

    assert (CheckLock(&entry->lock) == -1);

    fd = fcache_open_file (entry, O_RDWR);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open cache file %u",
		   (unsigned)entry->index);
	rx_EndCall(call, 0);
	goto out;
    }

    assert (CheckLock(&entry->lock) == -1);

    ret = copyrx2fd (call, fd, sizefs);
    close (fd);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "copyrx2fd");
	rx_EndCall(call, ret);
	goto out;
    }

    assert (CheckLock(&entry->lock) == -1);

    usedbytes += sizefs;		/* XXX - sync */

    ret = rx_EndCall (call, EndRXAFS_FetchData (call,
						&status,
						&callback,
						&volsync));
    assert (CheckLock(&entry->lock) == -1);

    if(ret) {
	arla_warn (ADEBFCACHE, ret, "rx_EndCall");
	goto out;
    }
    
    update_entry (entry, &status, &callback, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    entry->flags.datap = TRUE;
    entry->tokens |= XFS_DATA_R | XFS_DATA_W | XFS_OPEN_NR | XFS_OPEN_NW;

out:
    assert (CheckLock(&entry->lock) == -1);

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
     int ret = 0;
     int close_ret;
     u_int32_t sizefs;
     int fd;
     struct stat statinfo;
     AFSFetchStatus status;
     AFSCallBack callback;
     AFSVolSync volsync;
     fs_server_context context;

     assert (CheckLock(&entry->lock) == -1);

     fd = fcache_open_file (entry, O_RDONLY);
     if (fd < 0) {
	 ret = errno;
	 arla_warn (ADEBFCACHE, ret, "open cache file %u",
		    (unsigned)entry->index);
	 return ret;
     }

     if (fstat (fd, &statinfo) < 0) {
	 int close_ret;

	 ret = errno;
	 close_ret = close (fd);
	 assert (close_ret == 0);
	 arla_warn (ADEBFCACHE, ret, "stat cache file %u",
		    (unsigned)entry->index);
	 return ret;
     }

     sizefs = statinfo.st_size;

     fcache_update_length (entry, sizefs);
     if (connected_mode != CONNECTED) {
	 int close_ret;

	 close_ret = close (fd);
	 assert (close_ret == 0);
	 return 0;
     }

     for (conn = find_first_fs (entry, ce, &context);
	  conn != NULL;
	  conn = find_next_fs (&context, conn)) {

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
	 if (ret == RX_CALL_DEAD) {
	     rx_EndCall(call, ret);
	     continue;
	 } else if (ret) {
	     arla_warn (ADEBFCACHE, ret, "store-data");
	     rx_EndCall(call, 0);
	     break;
	 }

	 ret = copyfd2rx (fd, call, sizefs);
	 if (ret) {
	     rx_EndCall(call, ret);
	     arla_warn (ADEBFCACHE, ret, "copyfd2rx");
	     break;
	 }

	 ret = EndRXAFS_StoreData (call,
				   &status,
				   &callback,
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
	 if (ret == 0)
	     update_entry (entry, &status, &callback, &volsync,
			   rx_HostOf(rx_PeerOf(conn->connection)),
			   ce->cred);
     } else {
	 ret = ENETDOWN;
     }
     free_fs_server_context (&context);
     close_ret = close (fd);
     assert(close_ret == 0);
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
    int ret = 0;
    AFSStoreStatus storestatus;
    u_int32_t sizefs;
    int fd;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    fs_server_context context;
    int close_ret;

    assert (CheckLock(&entry->lock) == -1);

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
	close_ret = close (fd);
	assert (close_ret == 0);
	return ret;
    }
    
    close_ret = close (fd);
    assert (close_ret == 0);

    if (!entry->flags.datap)
	entry->status.Length = 0;

    fcache_update_length (entry, size);

    if (connected_mode != CONNECTED)
	return 0;

    for (conn = find_first_fs (entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

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
	if (ret == RX_CALL_DEAD) {
	    rx_EndCall(call, ret);
	    continue;
	} else if(ret) {
	    arla_warn (ADEBFCACHE, ret, "store-data");
	    rx_EndCall(call, 0);
	    break;
	}

	sizefs = htonl (0);
	if (rx_Write (call, &sizefs, sizeof(sizefs)) != sizeof(sizefs)) {
	    ret = rx_Error(call);
	    arla_warn (ADEBFCACHE, ret, "writing length");
	    rx_EndCall(call, 0);
	    break;
	}

	ret = EndRXAFS_StoreData (call,
				  &status,
				  &callback,
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
	    update_entry (entry, &status, &callback, &volsync,
			  rx_HostOf(rx_PeerOf(conn->connection)),
			  ce->cred);

	    assert (entry->status.Length == size);
	}
    } else {
	ret = ENETDOWN;
    } 
    free_fs_server_context (&context);
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
    ConnCacheEntry *conn;
    int ret = 0;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fs_server_context context;
    u_int32_t host;

    assert (CheckLock(&entry->lock) == -1);

    for (conn = find_first_fs (entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

	host = rx_HostOf (rx_PeerOf (conn->connection));

	ret = RXAFS_StoreStatus (conn->connection,
				 &entry->fid.fid,
				 store_status,
				 &status,
				 &volsync);
	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    arla_warn (ADEBFCACHE, ret, "store-status");
	    goto out;
	}
	break;
    }

    if (conn == NULL) {
	ret = ENETDOWN;
	goto out;
    }

    update_entry (entry, &status, NULL, &volsync, host, ce->cred);

out:
    free_fs_server_context (&context);
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
    int ret = 0;
    int close_ret;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    int fd;
    fs_server_context context;
    u_int32_t host;

    assert (CheckLock(&dir_entry->lock) == -1);

    if (connected_mode == CONNECTED) {
	ConnCacheEntry *conn;

	for (conn = find_first_fs (dir_entry, ce, &context);
	     conn != NULL;
	     conn = find_next_fs (&context, conn)) {

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
	    if (ret == RX_CALL_DEAD) {
		continue;
	    } else if (ret) {
		free_fs_server_context (&context);
		arla_warn (ADEBFCACHE, ret, "CreateFile");
		goto out;
	    }
	    break;
	}
	free_fs_server_context (&context);

	if (conn == NULL)
	    return ENETDOWN;

	status.Length = dir_entry->status.Length;
	update_entry (dir_entry, &status, &callback, &volsync,
		      host, ce->cred);
    } else {
	static int fakefid = 100;

	OutFid.Volume = dir_entry->fid.fid.Volume;
	OutFid.Vnode  = fakefid;
	OutFid.Unique = fakefid;
	++fakefid;

	fetch_attr->ClientModTime    = store_attr->ClientModTime;
	fetch_attr->Owner            = store_attr->Owner;
	fetch_attr->Group            = store_attr->Group;
	fetch_attr->UnixModeBits     = store_attr->UnixModeBits;
	fetch_attr->SegSize          = store_attr->SegSize;
	fetch_attr->ServerModTime    = store_attr->ClientModTime;
	fetch_attr->FileType         = TYPE_FILE;
	fetch_attr->DataVersion      = 1;
	fetch_attr->InterfaceVersion = 1;
	fetch_attr->Author           = fetch_attr->Owner;
	fetch_attr->LinkCount        = 1;
	fetch_attr->SyncCount        = 0;
	fetch_attr->spare1           = 0;
	fetch_attr->spare2           = 0;
	fetch_attr->spare3           = 0;
	fetch_attr->spare4           = 0;
	fetch_attr->ParentVnode      = dir_entry->fid.fid.Vnode;
	fetch_attr->ParentUnique     = dir_entry->fid.fid.Unique;
	fetch_attr->CallerAccess     = dir_entry->status.CallerAccess;
	fetch_attr->AnonymousAccess  = dir_entry->status.AnonymousAccess;
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
	close_ret = close (fd);
	assert (close_ret == 0);
	fcache_release(child_entry);
	goto out;
    }
    close_ret = close (fd);
    assert (close_ret == 0);

    child_entry->flags.datap = TRUE;
    child_entry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;
	
    fcache_release(child_entry);

out:
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
    int ret = 0;
    ConnCacheEntry *conn;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    assert (CheckLock(&dir_entry->lock) == -1);

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

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

	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "MakeDir");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;

    status.Length = dir_entry->status.Length;
    update_entry (dir_entry, &status, &callback, &volsync,
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

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;

    ret = adir_mkdir (child_entry, child_fid->fid, dir_entry->fid.fid);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "adir_mkdir");
	fcache_release(child_entry);
	goto out;
    }

    usedbytes += child_entry->status.Length;

    child_entry->flags.datap = TRUE;
    child_entry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;
	
    fcache_release(child_entry);

out:
    return ret;
}

/*
 * Create a symbolic link.
 */

int
create_symlink (FCacheEntry *dir_entry,
		const char *name, AFSStoreStatus *store_attr,
		VenusFid *child_fid, AFSFetchStatus *fetch_attr,
		const char *contents,
		CredCacheEntry *ce)
{
    int ret = 0;
    ConnCacheEntry *conn;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSVolSync volsync;
    AFSFetchStatus new_status;
    u_int32_t host;
    fs_server_context context;

    assert (CheckLock(&dir_entry->lock) == -1);

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

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
	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    arla_warn (ADEBFCACHE, ret, "Symlink");
	    free_fs_server_context (&context);
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;

    new_status.Length = dir_entry->status.Length;
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
    usedbytes += child_entry->status.Length;

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;
    child_entry->tokens |= XFS_ATTR_R;
	
    fcache_release(child_entry);

out:
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

    assert (CheckLock(&dir_entry->lock) == -1);

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_Link (conn->connection,
			  &dir_entry->fid.fid,
			  name,
			  &existing_entry->fid.fid,
			  &new_status,
			  &status,
			  &volsync);
	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "Link");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;

    status.Length = dir_entry->status.Length;

    update_entry (dir_entry, &status, NULL, &volsync,
		  host, ce->cred);

    update_entry (existing_entry, &new_status, NULL, NULL,
		  host, ce->cred);

out:
    return ret;
}

/*
 * Remove a file from a directory.
 */

int
remove_file (FCacheEntry *dir_entry, const char *name, CredCacheEntry *ce)
{
    int ret = 0;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    assert (CheckLock(&dir_entry->lock) == -1);

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_RemoveFile (conn->connection,
				&dir_entry->fid.fid,
				name,
				&status,
				&volsync);
	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "RemoveFile");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;

    status.Length = dir_entry->status.Length;

    update_entry (dir_entry, &status, NULL, &volsync,
		  host, ce->cred);

out:
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
    int ret = 0;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    assert (CheckLock(&dir_entry->lock) == -1);

    for (conn = find_first_fs (dir_entry, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_RemoveDir (conn->connection,
			       &dir_entry->fid.fid,
			       name,
			       &status,
			       &volsync);
	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "RemoveDir");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;

    status.Length = dir_entry->status.Length;

    update_entry (dir_entry, &status, NULL, &volsync,
		  host, ce->cred);

out:
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
    int ret = 0;
    ConnCacheEntry *conn;
    AFSFetchStatus orig_status, new_status;
    AFSVolSync volsync;
    u_int32_t host;
    fs_server_context context;

    assert (CheckLock(&old_dir->lock) == -1
	    && CheckLock(&new_dir->lock) == -1);

    for (conn = find_first_fs (old_dir, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {

	host = rx_HostOf(rx_PeerOf(conn->connection));

	ret = RXAFS_Rename (conn->connection,
			    &old_dir->fid.fid,
			    old_name,
			    &new_dir->fid.fid,
			    new_name,
			    &orig_status,
			    &new_status,
			    &volsync);
	if (ret == RX_CALL_DEAD) {
	    continue;
	} else if (ret) {
	    free_fs_server_context (&context);
	    arla_warn (ADEBFCACHE, ret, "Rename");
	    goto out;
	}
	break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;

    orig_status.Length = old_dir->status.Length;
    new_status.Length  = new_dir->status.Length;

    update_entry (old_dir, &orig_status, NULL, &volsync,
		  host, ce->cred);

    update_entry (new_dir, &new_status, NULL, &volsync,
		  host, ce->cred);

out:
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
     int32_t type;
     int ret;

     ret = volcache_getbyname (root_volume, 0, ce, &ve, &type);
     if (ret) {
	 arla_warnx (ADEBFCACHE, "Cannot find the root volume");
	 return ret;
     }

     fid.Cell = 0;
     if (ve->entry.flags & VLF_ROEXISTS) {
	 fid.fid.Volume = ve->entry.volumeId[ROVOL];
     } else if (ve->entry.flags & VLF_RWEXISTS) {
	 arla_warnx(ADEBERROR,
		    "getroot: %s is missing a RO clone, not good",
		    root_volume);
	 fid.fid.Volume = ve->entry.volumeId[RWVOL];
     } else {
	 arla_errx(1, ADEBERROR,
		   "getroot: %s has no RW or RO clone?",
		   root_volume);
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
gettype (int32_t volid, VolCacheEntry *ve)
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
    int32_t type;
    int error;

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
	e->lru_le = listaddtail (lrulist, e);

	assert (old->flags.usedp);
	*res = old;
	return 0;
    }

    e->fid     = fid;
    e->realfid = fid;
    if (e->index == 0)
	create_node (e);
    e->anonaccess      = 0;
    e->tokens          = 0;
    e->flags.usedp     = TRUE;
    e->flags.datap     = FALSE;
    e->flags.attrp     = FALSE;
    e->flags.attrusedp = FALSE;
    e->flags.datausedp = FALSE;
    e->flags.extradirp = FALSE;
    e->flags.mountp    = FALSE;
    e->flags.kernelp   = FALSE;
    e->flags.sentenced = FALSE;
    e->host	       = 0;
    e->priority	       = fprio_get(fid);
    e->volume          = NULL;
    
    e->lru_le = listaddhead (lrulist, e);
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
    assert (CheckLock (&e->lock) == -1);

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
 * Make sure that `e' has attributes and that they are up-to-date.
 * `e' must be write-locked.
 */

int
fcache_get_attr (FCacheEntry *e, CredCacheEntry *ce)
{
    AccessEntry *ae;
    assert (e->flags.usedp);

    assert (CheckLock(&e->lock) == -1);

    if (e->flags.attrp && uptodatep(e) &&
	findaccess(ce->cred, e->acccache, &ae) != FALSE)
	return 0;

    return read_attr (e, ce);
}

static int
do_read_data (FCacheEntry *e, CredCacheEntry *ce)
{
    int ret = RX_CALL_DEAD;
    fs_server_context context;
    ConnCacheEntry *conn;

    for (conn = find_first_fs (e, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {
	ret = read_data (e, conn, ce);
	if (!try_next_fs (ret))
	    break;
    }
    free_fs_server_context (&context);

    if (conn == NULL)
	return ENETDOWN;
    return ret;
}

/*
 * Make sure that `e' has file data and is up-to-date.
 */

int
fcache_get_data (FCacheEntry *e, CredCacheEntry *ce)
{
    ConnCacheEntry *conn = NULL;
    int ret;
    fs_server_context context;

    assert (e->flags.usedp);

    assert (CheckLock(&e->lock) == -1);

    if (e->flags.attrp && uptodatep(e)) {
	if (e->flags.datap)
	    return 0;
	else
	    return do_read_data (e, ce);
    } else {
	ret = do_read_attr (e, ce, &conn, &context);
	if (ret) {
	    free_fs_server_context (&context);
	    return ret;
	}
	if (e->flags.datap) {
	    free_fs_server_context (&context);
	    return 0;
	}
    }
    ret = read_data (e, conn, ce);
    free_fs_server_context (&context);
    return ret;
}

/*
 * Helper function for followmountpoint.
 * Given the contents of a mount-point, figure out the cell and volume name.
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
	     u_int32_t *volid)
{
    VolCacheEntry *ve;
    int result_type;
    int this_type;
    int res;

    res = volcache_getbyname (volname, cell, ce, &ve, &this_type);
    if (res)
	return res;

    assert (this_type == RWVOL ||
	    this_type == ROVOL ||
	    this_type == BACKVOL);

    if (this_type == ROVOL) {
	assert (ve->entry.flags & VLF_ROEXISTS);
	result_type = ROVOL;
    } else if (this_type == BACKVOL && parent_type == BACKVOL) {
	volcache_free (ve);
	return ENOENT;
    } else if (this_type == BACKVOL) {
	assert (ve->entry.flags & VLF_BOEXISTS);
	result_type = BACKVOL;
    } else if (this_type == RWVOL &&
	       parent_type != RWVOL &&
	       mount_symbol == '#') {
	if (ve->entry.flags & VLF_ROEXISTS)
	    result_type = ROVOL;
	else
	    result_type = RWVOL;
    } else {
	if (ve->entry.flags & VLF_RWEXISTS)
	    result_type = RWVOL;
	else if (ve->entry.flags & VLF_ROEXISTS)
	    result_type = ROVOL;
	else {
	    volcache_free (ve);
	    return ENOENT;
	}
    }
    *volid = ve->entry.volumeId[result_type];
    volcache_free (ve);
    return 0;
}

/*
 * Set `fid' to point to the root of the volume pointed to by the
 * mount-point in (buf, len).
 */

static int
get_root_of_volume (VenusFid *fid, VenusFid *parent,
		    VolCacheEntry *volume, CredCacheEntry **ce,
		    char *buf, size_t len)
{
    VenusFid oldfid = *fid;
    char *volname;
    int32_t cell;
    u_int32_t volid;
    int res;
    long parent_type;
    char mount_symbol;
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

	new_ce = cred_get (cell, (*ce)->cred, (*ce)->type);
	if (new_ce == NULL)
	    new_ce = cred_get(cell, (*ce)->cred, CRED_ANY);
	if (new_ce == NULL)
	    return ENOMEM;
	cred_free (*ce);
	*ce = new_ce;
    }

    parent_type = gettype (fid->fid.Volume, volume);
    mount_symbol = *buf;

    res = find_volume (volname, cell, *ce, mount_symbol,
		       parent_type, &volid);
    if (res)
	return res;

    /*
     * Create the new fid. The root of a volume always has
     * (Vnode, Unique) = (1,1)
     */

    fid->Cell = cell;
    fid->fid.Volume = volid;
    fid->fid.Vnode = fid->fid.Unique = 1;

    res = fcache_get (&e, *fid, *ce);
    if (res)
	return res;

    /*
     * Mount points are a little bit special.  We keep track of
     * their parent in `parent' so that `..' can be handled
     * properly.  Also, increment the refcount so that this entry
     * doesn't get gc:ed away under out feet.
     */

    ++e->refcount;
    e->flags.mountp = TRUE;
    e->realfid = oldfid;
    e->parent = *parent;
    fcache_release (e);
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
followmountpoint (VenusFid *fid, VenusFid *parent, CredCacheEntry **ce)
{
     int fd;
     fbuf the_fbuf;
     char *buf;
     FCacheEntry *e;
     int res;
     u_int32_t length;
     int close_ret;

     /*
      * Get the node for `fid' and verify that it's a symbolic link
      * with the correct contents.  Otherwise, just return the old
      * `fid' without any change.
      */

     res = fcache_get (&e, *fid, *ce);
     if (res)
	 return res;

     res = fcache_get_attr (e, *ce);
     if (res) {
	 fcache_release(e);
	 return res;
     }

     if (e->status.FileType != TYPE_LINK
	 || e->status.Length == 0) {
	 fcache_release(e);
	 return 0;
     }

     res = fcache_get_data (e, *ce);
     if (res) {
	 fcache_release(e);
	 return res;
     }
     
     length = e->status.Length;
     if (length == 0) {
	 fcache_release(e);
	 return 0;
     }

     fd = fcache_open_file (e, O_RDONLY);
     if (fd < 0) {
	 fcache_release(e);
	 return errno;
     }

     res = fbuf_create (&the_fbuf, fd, length, FBUF_READ);
     if (res) {
	 close_ret = close (fd);
	 assert (close_ret == 0);
	 fcache_release(e);
	 return res;
     }
     buf = (char *)(the_fbuf.buf);
     if (*buf == '#' || *buf == '%')
	 res = get_root_of_volume (fid, parent, e->volume, ce,
				   buf, e->status.Length);
     fcache_release(e);
     fbuf_end (&the_fbuf);
     return res;
}

/*
 *
 */

static Bool
print_entry (void *ptr, void *arg)
{
    FCacheEntry *e = (FCacheEntry *)ptr;

    arla_log(ADEBVLOG, "(%d, %u, %u, %u)%s%s%s%s%s%s%s%s%s length: %ld\n",
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
	     e->status.Length);
    return FALSE;
}


/*
 *
 */

void
fcache_status (void)
{
    arla_log(ADEBVLOG, "%lu (%lu-%lu) files\n"
	     "%lu (%lu-%lu) bytes\n",
	     usedvnodes, lowvnodes, highvnodes,
	     usedbytes, lowbytes, highbytes);
    hashtabforeach (hashtab, print_entry, NULL);
}

/*
 *
 */

void
fcache_update_length (FCacheEntry *e, size_t len)
{
    int fd;
    struct stat sb;
    int close_ret;

    fd = fcache_open_file (e, O_RDONLY);
    if (fd < 0) {
	arla_warn (ADEBFCACHE, errno, "fcache_open_file");
	assert(FALSE);
    }

    if (fstat (fd, &sb) < 0) {
	arla_warn (ADEBFCACHE, errno, "fstat");
	close_ret = close (fd);
	assert (close_ret);
	assert (FALSE);
    }
    close_ret = close(fd);
    assert(close_ret == 0);

    assert (len == sb.st_size);

    assert (usedbytes + len >= e->status.Length);

    usedbytes = usedbytes - e->status.Length + len;
    e->status.Length = len;
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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return ret;
    }

    ret = RX_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {
	ret = RXAFS_FetchACL (conn->connection, &fid.fid,
			      opaque, &status, &volsync);
	if (!try_next_fs (ret))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "FetchACL");
    free_fs_server_context (&context);

    if (conn == NULL)
	ret = ENETDOWN;

    fcache_release (dire);
    return ret;
}

/*
 * Store the ACL read from opaque
 */

int
setacl(VenusFid fid,
       CredCacheEntry *ce,
       AFSOpaque *opaque)
{
    FCacheEntry *dire;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSVolSync volsync;
    int ret;
    fs_server_context context;
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    ret = RX_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {
	ret = RXAFS_StoreACL (conn->connection, &fid.fid,
			      opaque, &status, &volsync);
	if (!try_next_fs (ret))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "StoreACL");
    free_fs_server_context (&context);

    if (conn == NULL)
	ret = ENETDOWN;

    if (ret == 0 && dire->flags.kernelp) {
	break_callback (dire->fid);
	dire->flags.kernelp = FALSE;
    }

    fcache_release (dire);
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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    ret = RX_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {
	ret = RXAFS_GetVolumeStatus (conn->connection, fid.fid.Volume,
				     volstat, volumename, offlinemsg,
				     motd);
	if (!try_next_fs (ret))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "GetVolumeStatus");
    free_fs_server_context (&context);

    if (conn == NULL)
	ret = ENETDOWN;

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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    ret = RX_CALL_DEAD;

    for (conn = find_first_fs (dire, ce, &context);
	 conn != NULL;
	 conn = find_next_fs (&context, conn)) {
	ret = RXAFS_SetVolumeStatus (conn->connection, fid.fid.Volume,
				     volstat, volumename, offlinemsg,
				     motd);
	if (!try_next_fs (ret))
	    break;
    }
    if (ret)
	arla_warn (ADEBFCACHE, ret, "SetVolumeStatus");
    free_fs_server_context (&context);

    if (conn == NULL)
	ret = ENETDOWN;

    fcache_release (dire);
    return ret;
}
