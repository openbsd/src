/*	$OpenBSD: fcache.c,v 1.1.1.1 1998/09/14 21:52:56 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
RCSID("$KTH: fcache.c,v 1.114 1998/08/13 21:32:38 assar Exp $") ;

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
 * List of entries to be invalidated.
 */

static List *invalid_list;

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

/* This is just temporary for allocating "inode"-numbers. */

static int inode_count;		/* XXX */

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
 * Globalnames 
 */

char arlasysname[SYSNAMEMAXLEN];

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
	entries[i].lru_le     = listaddhead (lrulist, &entries[i]);
	entries[i].invalid_le = NULL;
	entries[i].volume     = NULL;
	entries[i].refcount   = 0;
	entries[i].anonaccess = 0;
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

    assert (entry->flags.datap && entry->flags.usedp
	    && CheckLock(&entry->lock) == -1);

    fd = fcache_open_file (entry, O_WRONLY | O_TRUNC, 0);
    if (fd < 0) {
	arla_warn (ADEBFCACHE, errno, "fcache_open_file");
	return;
    }
    close (fd);
    if (entry->flags.extradirp) {
	char fname[MAXPATHLEN];

	fcache_extra_file_name (entry, fname, sizeof(fname));
	unlink (fname);
    }
    assert(usedbytes >= entry->status.Length);
    usedbytes -= entry->status.Length;
    entry->flags.datap = FALSE;
    entry->flags.extradirp = FALSE;
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

    if (entry->invalid_le != NULL) {
	listdel (invalid_list, entry->invalid_le);
	entry->invalid_le = NULL;
    }

    if (entry->flags.attrp && entry->host) {
	ce = cred_get (entry->fid.Cell, 0, CRED_NONE);
	assert (ce != NULL);
	
	conn = conn_get (entry->fid.Cell, entry->host, afsport,
			 FS_SERVICE_ID, ce);
	cred_free (ce);

	fids.len = cbs.len = 1;
	fids.val = &entry->fid.fid;
	cbs.val  = &entry->callback;
	ret = RXAFS_GiveUpCallBacks (conn->connection, &fids, &cbs);
	conn_free (conn);
	if (ret)
	    arla_warn (ADEBFCACHE, ret, "RXAFS_GiveUpCallBacks");
    }
    volcache_free (entry->volume);
    entry->volume = NULL;
/*    entry->inode  = 0;*/
    entry->flags.attrp = FALSE;
    entry->flags.usedp = FALSE;
    --usedvnodes;
}

/*
 * Return the next inode-number.
 */

static ino_t
nextinode (void)
{
     return inode_count++;
}

/*
 *
 */

static void
create_nodes (char *arg)
{
    Listitem *item;
    FCacheEntry *entry;
    int fd;
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
	    && entry->inode == 0) {
	    struct timeval tv;

	    ObtainWriteLock (&entry->lock);
	    entry->inode = nextinode ();
	    fd = fcache_open_file (entry, O_RDWR | O_CREAT | O_TRUNC, 0666);
	    close (fd);
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
 *
 */

static void
invalidator (char *arg)
{
    for (;;) {
	Listitem *item, *next;
	struct timeval tv;

	while (listemptyp (invalid_list))
	    LWP_WaitProcess (invalid_list);

	gettimeofday (&tv, NULL);

	for (item = listhead (invalid_list);
	     item;
	     item = next) {
	    FCacheEntry *entry = (FCacheEntry *)listdata(item);

	    assert (entry->invalid_le == item);

	    next = listnext (invalid_list, item);
	    if (tv.tv_sec < entry->callback.ExpirationTime) {
		IOMGR_Sleep (entry->callback.ExpirationTime - tv.tv_sec);
		break;
	    }
	    
	    ObtainWriteLock (&entry->lock);
	    listdel (invalid_list, item);
	    entry->invalid_le = NULL;
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
    Listitem *item;

    if (e->invalid_le != NULL) {
	listdel (invalid_list, e->invalid_le);
	e->invalid_le = NULL;
    }

    for (item = listhead (invalid_list);
	 item;
	 item = listnext (invalid_list, item)) {
	FCacheEntry *this_entry = (FCacheEntry *)listdata (item);

	if (e->callback.ExpirationTime < this_entry->callback.ExpirationTime) {
	    e->invalid_le = listaddbefore (invalid_list,
					   item,
					   e);
	    return;
	}
    }
    e->invalid_le = listaddhead (invalid_list, e);
    LWP_NoYieldSignal (invalid_list);
}

/*
 * Remove the entry least-recently used and return it or NULL if none
 * is found.
 */

static FCacheEntry *
unlink_lru_entry (void)
{
     FCacheEntry *entry = NULL;
     Listitem *item;
     
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

     assert (!listemptyp (lrulist));
     for (item = listtail (lrulist);
	  item;
	  item = listprev (lrulist, item)) {

	  entry = (FCacheEntry *)listdata (item);
	  if (entry->flags.usedp
	      && entry->refcount == 0
	      && CheckLock(&entry->lock) == 0) {
	      ObtainWriteLock (&entry->lock);
	      listdel (lrulist, entry->lru_le);
	      entry->lru_le = NULL;
	      if (entry->flags.kernelp)
		  break_callback (entry->fid);
	      throw_entry (entry);
	      return entry;
	  }
     }

     return NULL;
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
	 
	 for (item = listtail (lrulist);
	      item && usedbytes + sz > highbytes;
	      item = listprev (lrulist, item)) {
	     entry = (FCacheEntry *)listdata (item);
	     if (entry->flags.datap
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
	    close (fd);
	    return errno;
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

    fd = open ("fcache", O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return;
    n = 0;
    while (read (fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
	CredCacheEntry *ce;
	FCacheEntry *e;
	int i;
	VolCacheEntry *vol;

	ce = cred_get (tmp.fid.Cell, 0, 0);
	assert (ce != NULL);

	vol = volcache_getbyid (tmp.fid.fid.Volume, tmp.fid.Cell, ce);
	cred_free (ce);
	if (vol == NULL)
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
	e->inode = tmp.inode;
	inode_count = max(inode_count, tmp.inode + 1);
	e->flags.usedp = TRUE;
	e->flags.attrp = tmp.flags.attrp;
	e->flags.datap = tmp.flags.datap;
	e->flags.attrusedp = FALSE;
	e->flags.datausedp = FALSE;
	e->flags.kernelp   = FALSE;
	e->flags.extradirp = tmp.flags.extradirp;
	e->flags.mountp = tmp.flags.mountp;
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
    close (fd);
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
 * Find the file server we should be talking to for a given fid and
 * user and return the connection.
 */

static ConnCacheEntry *
findconn (FCacheEntry *e, CredCacheEntry *ce)
{
    ConnCacheEntry *conn;
    VolCacheEntry  *ve = e->volume;
    int i;
    u_long addr = 0;
    int type = -1, bit;

    for (i = RWVOL; i <= BACKVOL; ++i)
	if (ve->entry.volumeId[i] == e->fid.fid.Volume) {
	    type = i;
	    break;
	}

    switch (type) {
    case RWVOL :
	bit = VLSF_RWVOL;
	break;
    case ROVOL :
	bit = VLSF_ROVOL;
	break;
    case BACKVOL :
	bit = VLSF_BACKVOL;
	break;
    default :
	abort ();
    }

    for (i = 0; i < MAXNSERVERS; ++i)
	if (ve->entry.serverFlags[i] & bit) {
	    addr = htonl(ve->entry.serverNumber[i]);
	    break;
	}

    assert (addr != 0);

    conn = conn_get (e->fid.Cell, addr, afsport, FS_SERVICE_ID, ce);
    if (conn == NULL) {
	  arla_warnx (ADEBFCACHE, "Cannot connect to FS");
	  return NULL;
    }
    return conn;
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

    inode_count    = 1;		/* XXX */
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

    invalid_list = listnew ();
    if (invalid_list == NULL)
	arla_errx (1, ADEBERROR, "fcache: listnew failed");

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
 * If it's found, move it to the front of the lrulist as well.
 */

static FCacheEntry *
find_entry (VenusFid fid)
{
    FCacheEntry key;
    FCacheEntry *e;

    key.fid = fid;
    e = (FCacheEntry *)hashtabsearch (hashtab, (void *)&key);
    if (e != NULL) {
	ObtainWriteLock (&e->lock);
	listdel (lrulist, e->lru_le);
	e->lru_le = listaddhead (lrulist, e);
    }
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

    assert (CheckLock (&e->lock) == 0);

    ObtainWriteLock (&e->lock);
    e->callback = callback;
    e->tokens   = 0;
    if (e->flags.kernelp)
	break_callback (e->fid);
    e->flags.kernelp = FALSE;
    ReleaseWriteLock (&e->lock);
}

/*
 * Call stale on the entry corresponding to `fid', if any.
 */

void
fcache_stale_entry (VenusFid fid, AFSCallBack callback)
{
    FCacheEntry *e;

    e = find_entry (fid);
    if (e == NULL) {
	arla_warnx (ADEBFCACHE,
		    "callback for non-existing file (%d, %u, %u, %u)",
		    fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);
	return;
    }

    ReleaseWriteLock(&e->lock);
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
		stale(e, e->callback);
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
    if (e->host == *host) {
	if (CheckLock (&e->lock) == 0)
	    stale (e, broken_callback);
    }
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
 * return the file name of the cached file for `entry'.
 */

int
fcache_file_name (FCacheEntry *entry, char *s, size_t len)
{
    return snprintf (s, len, "%04X", (unsigned)entry->inode);
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

/*
 * return a fd to the cache file of `entry'
 */

int
fcache_open_file (FCacheEntry *entry, int flag, mode_t mode)
{
    char fname[MAXPATHLEN];

    fcache_file_name (entry, fname, sizeof(fname));
    return open (fname, flag | O_BINARY, mode);
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

    gettimeofday (&tv, NULL);

    entry->status   = *status;
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
 * Fetch the attributes for the file in `entry' from the file_server,
 * using the credentials in `ce' and returning the connection in
 * `ret_conn' */

static int
do_read_attr (FCacheEntry *entry,
	      CredCacheEntry *ce,
	      ConnCacheEntry **ret_conn)
{
    int ret;
    ConnCacheEntry *conn;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;

    assert (CheckLock(&entry->lock) == -1);

    if (connected_mode == DISCONNECTED) {
	*ret_conn = NULL;
	if (entry->flags.attrp == TRUE)
	    return 0;
	return ENETDOWN;
    }

    conn = findconn (entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_FetchStatus (conn->connection,
			     &entry->fid.fid,
			     &status,
			     &callback,
			     &volsync);
    if (ret) {
	if (ret == -1)
	    ret = ENETDOWN;
	conn_free(conn);
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
    
    *ret_conn = conn;
    return 0;
}


/*
 * Read the attributes of `entry' from the file server and store them.
 */

int
read_attr (FCacheEntry *entry, CredCacheEntry *ce)
{
    int ret;
    ConnCacheEntry *conn;

    assert (CheckLock(&entry->lock) == -1);

    ret = do_read_attr (entry, ce, &conn);
    if (ret)
	return ret;
    conn_free (conn);
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

    ret = StartRXAFS_FetchData (call, &entry->fid.fid, 
				0, entry->status.Length);
    if(ret) {
	arla_warn (ADEBFCACHE, ret, "fetch-data");
	goto out;
    }

    ret = rx_Read (call, &sizefs, sizeof(sizefs));
    if (ret != sizeof(sizefs)) {
	ret = rx_Error(call);
	arla_warn (ADEBFCACHE, ret, "Error reading length");
	rx_EndCall(call, 0);
	goto out;
    }
    sizefs = ntohl (sizefs);

    assert (sizefs == entry->status.Length);

    fd = fcache_open_file (entry, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open cache file %u",
		   (unsigned)entry->inode);
	rx_EndCall(call, 0);
	goto out;
    }

    if (copyrx2fd (call, fd, sizefs)) {
	ret = errno;
	rx_EndCall(call, ret);
	arla_warn (ADEBFCACHE, ret, "copyrx2fd");
    }

    usedbytes += sizefs;		/* XXX - sync */

    ret = rx_EndCall (call, EndRXAFS_FetchData (call,
						&status,
						&callback,
						&volsync));
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
    return ret;
}

/*
 * Write the contents of the cache file back to the file server.
 */

int
write_data (FCacheEntry *entry, CredCacheEntry *ce)
{
     ConnCacheEntry *conn;
     struct rx_call *call;
     int ret = 0;
     u_int32_t sizefs;
     int fd;
     struct stat statinfo;
     AFSStoreStatus storestatus;
     AFSFetchStatus status;
     AFSCallBack callback;
     AFSVolSync volsync;

     assert (CheckLock(&entry->lock) == -1);

     conn = findconn (entry, ce);

     if (conn == NULL)
	 return ENETDOWN;

     fd = fcache_open_file (entry, O_RDONLY, 0666);
     if (fd < 0) {
	 ret = errno;
	 arla_warn (ADEBFCACHE, ret, "open cache file %u",
		    (unsigned)entry->inode);
	 goto out;
     }

     if (fstat (fd, &statinfo) < 0) {
	  ret = errno;
	  arla_warn (ADEBFCACHE, ret, "stat cache file %u",
		     (unsigned)entry->inode);
	  goto out;
     }

     sizefs = statinfo.st_size;

     usedbytes = usedbytes - entry->status.Length + sizefs;

     call = rx_NewCall (conn->connection);
     if (call == NULL) {
	 arla_warnx (ADEBMISC, "rx_NewCall failed");
	 ret = ENOMEM;
	 goto out;
     }

     storestatus.Mask = 0; /* Dont save anything */
     ret = StartRXAFS_StoreData (call, &entry->fid.fid,
				 &storestatus, 
				 0,
				 sizefs,
				 sizefs);
     if (ret) {
	 arla_warn (ADEBFCACHE, ret, "store-data");
	 rx_EndCall(call, 0);
	 goto out;
     }

     if (copyfd2rx (fd, call, sizefs)) {
	  ret = errno;
	  rx_EndCall(call, ret);
	  arla_warnx (ADEBFCACHE, "copyfd2rx");
	  goto out;
     }

     ret = rx_EndCall (call, EndRXAFS_StoreData (call,
						 &status,
						 &callback,
						 &volsync));
     if (ret) {
	 arla_warn (ADEBFCACHE, ret, "rx_EndCall");
	 goto out;
     }

     update_entry (entry, &status, &callback, &volsync,
		   rx_HostOf(rx_PeerOf(conn->connection)),
		   ce->cred);

out:
     close (fd);
     conn_free (conn);
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
    u_int32_t sizefs = sizeof(entry->status.Length);
    int fd;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;

    assert (CheckLock(&entry->lock) == -1);

    conn = findconn (entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    fd = fcache_open_file (entry, O_RDWR | O_CREAT, 0);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open fache file %u",
		   (unsigned)entry->inode);
	goto out;
    }

    if(ftruncate (fd, size) < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "ftruncate %ld", (long)size);
	close (fd);
	goto out;
    }
    
    close (fd);

    if (entry->flags.datap) {
	assert (usedbytes >= entry->status.Length);
	usedbytes -= entry->status.Length;
    }
    usedbytes += size;

    entry->status.Length = size;

    call = rx_NewCall (conn->connection);
    if (call == NULL) {
	arla_warnx (ADEBMISC, "rx_NewCall failed");
	ret = ENOMEM;
	goto out;
    }

    storestatus.Mask = 0;
    ret = StartRXAFS_StoreData (call,
				&entry->fid.fid, 
				&storestatus,
				0, 0, entry->status.Length);
    if(ret) {
	arla_warn (ADEBFCACHE, ret, "store-data");
	rx_EndCall(call, 0);
	goto out;
    }

    sizefs = htonl (sizefs);
    if (rx_Write (call, &sizefs, sizeof(sizefs)) != sizeof(sizefs)) {
	ret = rx_Error(call);
	arla_warn (ADEBFCACHE, ret, "writing length");
	rx_EndCall(call, 0);
	goto out;
    }

    if (rx_Write (call, 0, 0) != 0) {
	ret = rx_Error(call);
	arla_warn (ADEBFCACHE, ret, "writing length");
	rx_EndCall(call, 0);
	goto out;
    }

    ret = rx_EndCall (call, EndRXAFS_StoreData (call,
						&status,
						&callback,
						&volsync));
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "rx_EndCall");
	goto out;
    }

    update_entry (entry, &status, &callback, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

out:
    conn_free(conn);
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

    assert (CheckLock(&entry->lock) == -1);

    conn = findconn (entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_StoreStatus (conn->connection,
			     &entry->fid.fid,
			     store_status,
			     &status,
			     &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "store-status");
	goto out;
    }
    
    update_entry (entry, &status, NULL, &volsync,
		  rx_HostOf (rx_PeerOf (conn->connection)),
		  ce->cred);

out:
    conn_free (conn);
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
    ConnCacheEntry *conn;
    AFSFid OutFid;
    FCacheEntry *child_entry;
    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    int fd;

    assert (CheckLock(&dir_entry->lock) == -1);

    conn = findconn (dir_entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_CreateFile (conn->connection,
			    &dir_entry->fid.fid,
			    name,
			    store_attr,
			    &OutFid,
			    fetch_attr,
			    &status,
			    &callback,
			    &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "CreateFile");
	goto out;
    }

    update_entry (dir_entry, &status, &callback, &volsync,
		  rx_HostOf (rx_PeerOf (conn->connection)),
		  ce->cred);

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  rx_HostOf (rx_PeerOf (conn->connection)),
		  ce->cred);

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;

    fd = fcache_open_file (child_entry, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
	ret = errno;
	arla_warn (ADEBFCACHE, ret, "open cache file %u",
		   (unsigned)child_entry->inode);
	ReleaseWriteLock (&child_entry->lock);
	goto out;
    }
    close (fd);

    child_entry->flags.datap = TRUE;
    child_entry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;
	
    ReleaseWriteLock (&child_entry->lock);

out:
    conn_free (conn);
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

    assert (CheckLock(&dir_entry->lock) == -1);

    conn = findconn (dir_entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_MakeDir (conn->connection,
			 &dir_entry->fid.fid,
			 name,
			 store_attr,
			 &OutFid,
			 fetch_attr,
			 &status,
			 &callback,
			 &volsync);

    if (ret) {
	arla_warn (ADEBFCACHE, ret, "MakeDir");
	goto out;
    }

    update_entry (dir_entry, &status, &callback, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;

    ret = adir_mkdir (child_entry, child_fid->fid, dir_entry->fid.fid);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "adir_mkdir");
	ReleaseWriteLock (&child_entry->lock);
	goto out;
    }

    usedbytes += child_entry->status.Length;

    child_entry->flags.datap = TRUE;
    child_entry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;
	
    ReleaseWriteLock (&child_entry->lock);

out:
    conn_free (conn);
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

    assert (CheckLock(&dir_entry->lock) == -1);

    conn = findconn (dir_entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_Symlink (conn->connection,
			 &dir_entry->fid.fid,
			 name,
			 contents,
			 store_attr,
			 &OutFid,
			 fetch_attr,
			 &new_status,
			 &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "Symlink");
	goto out;
    }

    usedbytes = usedbytes - dir_entry->status.Length + new_status.Length;

    update_entry (dir_entry, &new_status, NULL, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    child_fid->Cell = dir_entry->fid.Cell;
    child_fid->fid  = OutFid;

    ret = fcache_get (&child_entry, *child_fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	goto out;
    }

    update_entry (child_entry, fetch_attr, NULL, NULL,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    child_entry->flags.attrp = TRUE;
    child_entry->flags.kernelp = TRUE;
    child_entry->tokens |= XFS_ATTR_R;
	
    ReleaseWriteLock (&child_entry->lock);

out:
    conn_free (conn);
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

    assert (CheckLock(&dir_entry->lock) == -1);

    conn = findconn (dir_entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_Link (conn->connection,
		      &dir_entry->fid.fid,
		      name,
		      &existing_entry->fid.fid,
		      &new_status,
		      &status,
		      &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "Link");
	goto out;
    }

    usedbytes = usedbytes - dir_entry->status.Length + status.Length;

    update_entry (dir_entry, &status, NULL, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    update_entry (existing_entry, &new_status, NULL, NULL,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

out:
    conn_free (conn);
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

    assert (CheckLock(&dir_entry->lock) == -1);

    conn = findconn (dir_entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_RemoveFile (conn->connection,
			    &dir_entry->fid.fid,
			    name,
			    &status,
			    &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "RemoveFile");
	goto out;
    }

    usedbytes = usedbytes - dir_entry->status.Length + status.Length;

    update_entry (dir_entry, &status, NULL, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

out:
    conn_free (conn);
    return ret;
}

/*
 * Remove a file from a directory.
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

    assert (CheckLock(&dir_entry->lock) == -1);

    conn = findconn (dir_entry, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_RemoveDir (conn->connection,
			   &dir_entry->fid.fid,
			   name,
			   &status,
			   &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "RemoveDir");
	goto out;
    }

    usedbytes = usedbytes - dir_entry->status.Length + status.Length;

    update_entry (dir_entry, &status, NULL, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

out:
    conn_free (conn);
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

    assert (CheckLock(&old_dir->lock) == -1
	    && CheckLock(&new_dir->lock) == -1);

    conn = findconn (old_dir, ce);

    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_Rename (conn->connection,
			&old_dir->fid.fid,
			old_name,
			&new_dir->fid.fid,
			new_name,
			&orig_status,
			&new_status,
			&volsync);

    if (ret) {
	arla_warn (ADEBFCACHE, ret, "Rename");
	goto out;
    }

    usedbytes = usedbytes - old_dir->status.Length + orig_status.Length;

    usedbytes = usedbytes - new_dir->status.Length + new_status.Length;

    update_entry (old_dir, &orig_status, NULL, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

    update_entry (new_dir, &new_status, NULL, &volsync,
		  rx_HostOf(rx_PeerOf(conn->connection)),
		  ce->cred);

out:
    conn_free (conn);
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

     ve = volcache_getbyname (root_volume, 0, ce);
     if (ve == NULL) {
	 arla_warnx (ADEBFCACHE, "Cannot find the root volume");
	 return ENODEV;
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
    FCacheEntry *e;
    VolCacheEntry *vol;

    e = find_entry (fid);
    if (e) {
	assert (e->flags.usedp);
	*res = e;
	return 0;
    }

    if (connected_mode == DISCONNECTED) {
	*res = NULL;
	return ENETDOWN;
    }

    vol = volcache_getbyid (fid.fid.Volume, fid.Cell, ce);
    if (vol == NULL)
	return ENODEV;

    e = find_free_entry ();
    e->fid = fid;
    if (e->inode == 0)
	e->inode = nextinode ();
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
    e->host	       = 0;
    e->priority	       = fprio_get(fid);
    e->volume          = vol;
    
    e->lru_le = listaddhead (lrulist, e);
    hashtabadd (hashtab, e);

    *res = e;
    return 0;
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
	connected_mode != CONNECTEDLOG)
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

/*
 * Make sure that `e' has file data and is up-to-date.
 */

int
fcache_get_data (FCacheEntry *e, CredCacheEntry *ce)
{
    ConnCacheEntry *conn;
    int ret;

    assert (e->flags.usedp);

    assert (CheckLock(&e->lock) == -1);

    if (e->flags.attrp && uptodatep(e)) {
	if (e->flags.datap)
	    return 0;
	else {
	    conn = findconn (e, ce);
	    if (conn == NULL)
		return ENETDOWN;
	}
    } else {
	ret = do_read_attr (e, ce, &conn);
	if (ret)
	    return ret;
	if (e->flags.datap) {
	    conn_free (conn);
	    return 0;
	}
    }
    ret = read_data (e, conn, ce);
    conn_free (conn);
    return ret;
}

/*
 * If this entry is a mount point, set the fid data to
 * the root directory of the volume it's pointing at,
 * otherwise just leave it.
 */

int
followmountpoint (VenusFid *fid, VenusFid *parent, CredCacheEntry **ce)
{
     int fd;
     fbuf the_fbuf;
     char *buf;
     long cell;
     long type;
     FCacheEntry *e;
     VenusFid oldfid = *fid;
     int res;
     u_int32_t length;

     res = fcache_get (&e, *fid, *ce);
     if (res)
	 return res;

     res = fcache_get_attr (e, *ce);
     if (res) {
	 ReleaseWriteLock (&e->lock);
	 return res;
     }

     if (e->status.FileType != TYPE_LINK) {
	 ReleaseWriteLock (&e->lock);
	 return 0;
     }

     res = fcache_get_data (e, *ce);
     if (res) {
	 ReleaseWriteLock (&e->lock);
	 return res;
     }

     fd = fcache_open_file (e, O_RDONLY, 0);
     if (fd < 0) {
	 ReleaseWriteLock (&e->lock);
	 return errno;
     }

     length = e->status.Length;
     res = fbuf_create (&the_fbuf, fd, length, FBUF_READ);
     if (res) {
	 close (fd);
	 ReleaseWriteLock (&e->lock);
	 return res;
     }
     buf = (char *)(the_fbuf.buf);
     switch (*buf) {
     case '#' :
     case '%' : {
	 int founderr;
	 char *dot;

	 dot = buf + e->status.Length - 1;
	 *dot = '\0';
	 dot = strchr (buf, ':');
	 if (dot) {
	     *dot++ = '\0';
	     cell   = cell_name2num (buf + 1);
	 } else {
	     cell = fid->Cell;
	     dot  = buf + 1;
	 }
	 if (*buf == '%')
	     type = RWVOL;
	 else
	     type = gettype (fid->fid.Volume, e->volume);

	 founderr = 0;

	 /*
	  * If this is a cross-cell mountpoint we need new credentials.
	  */

	 if ((*ce)->cell != cell) {
	     CredCacheEntry *new_ce;

	     new_ce = cred_get (cell, (*ce)->cred, (*ce)->type);
	     if (new_ce == NULL) {
		 new_ce = cred_get(cell, (*ce)->cred, CRED_ANY);
	     }
	     if (new_ce == NULL) {
		 ReleaseWriteLock (&e->lock);
		 return ENOMEM;
	     }
	     cred_free (*ce);
	     *ce = new_ce;
	 }

	 /* is the cell is invalid the rest should be bougs too */
	 if (cell== -1)
	     founderr = -1;  
	 else {
	     VolCacheEntry *ve;

	     ve = volcache_getbyname (dot, cell, *ce);
	     if (ve == NULL)
		 founderr = -1;
	     else {
		 switch (type) {
		 case ROVOL :
		     if (ve->entry.flags & VLF_ROEXISTS) {
			 fid->fid.Volume = ve->entry.volumeId[ROVOL];
			 break;
		     }
		     /* fall through */
		 case RWVOL :
		     if (ve->entry.flags & VLF_RWEXISTS)
			 fid->fid.Volume = ve->entry.volumeId[RWVOL];
		     else
			 founderr = -1;
		     break;
		 case BACKVOL :
		     if (ve->entry.flags & VLF_BOEXISTS)
			 fid->fid.Volume = ve->entry.volumeId[BACKVOL];
		     else
			 founderr = -1;
		     break;
		 default :
		     abort ();
		 }
		 volcache_free (ve);
	     }
	 }
	 if (founderr) {
	     res = ENODEV;
	     break;
	 }
	 fid->Cell = cell;
	 fid->fid.Vnode = fid->fid.Unique = 1;

	 ReleaseWriteLock (&e->lock);
	 res = fcache_get (&e, *fid, *ce);
	 if (res)
	     break;
	 ++e->refcount;
	 e->flags.mountp = TRUE;
	 e->realfid = oldfid;
	 e->parent = *parent;
     }
     }
     ReleaseWriteLock (&e->lock);
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
    FILE *f = (FILE *)arg;

    fprintf (f, "(%d, %u, %u, %u)\n",
	     e->fid.Cell,
	     e->fid.fid.Volume, e->fid.fid.Vnode, e->fid.fid.Unique);
    return FALSE;
}


/*
 *
 */

void
fcache_status (FILE *f)
{
    fprintf (f, "%lu (%lu-%lu) files\n"
	     "%lu (%lu-%lu) bytes\n",
	     usedvnodes, lowvnodes, highvnodes,
	     usedbytes, lowbytes, highbytes);
    hashtabforeach (hashtab, print_entry, f);
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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return ret;
    }

    conn = findconn (dire, ce);
    if (conn == NULL)
	return ENETDOWN;

    ret = RXAFS_FetchACL (conn->connection, &fid.fid,
			  opaque, &status, &volsync);
    conn_free (conn);
    throw_entry (dire);
    
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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    conn = findconn (dire, ce);
    if (conn == NULL)
      return ENETDOWN;

    ret = RXAFS_StoreACL (conn->connection, &fid.fid,
			    opaque, &status, &volsync);
    
    conn_free (conn);
    throw_entry (dire);
    
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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    conn = findconn (dire, ce);
    if (conn == NULL)
      return ENETDOWN;

    ret = RXAFS_GetVolumeStatus (conn->connection, fid.fid.Volume,
				 volstat, volumename, offlinemsg,
				 motd);
    conn_free (conn);
    throw_entry (dire);
    
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
  
    ret = fcache_get (&dire, fid, ce);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "fcache_get");
	return EINVAL;
    }

    conn = findconn (dire, ce);
    if (conn == NULL)
      return ENETDOWN;

    ret = RXAFS_SetVolumeStatus (conn->connection, fid.fid.Volume,
				 volstat, volumename, offlinemsg,
				 motd);
    conn_free (conn);
    throw_entry (dire);
    
    return ret;
}
