/*	$OpenBSD: volcache.c,v 1.1.1.1 1998/09/14 21:52:57 art Exp $	*/
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
 * Manage our cache of volume information.
 */

#include "arla_local.h"
RCSID("$KTH: volcache.c,v 1.39 1998/07/29 21:31:06 assar Exp $") ;

/*
 * Suffixes for volume names.
 */

static char *volsuffixes[] = {
"",
ROSUFFIX,
BACKSUFFIX
};

static const char *root_volume_name = "root.afs";

/*
 * Return the root volume name.
 */

const char *
volcache_get_rootvolume (void)
{
    return root_volume_name;
}

/*
 * Set the current root volume name.
 */

void
volcache_set_rootvolume (const char *volname)
{
    root_volume_name = volname;
}

#define VOLCACHE_SIZE 2048
#define VOLCACHE_INC 300

/* Hashtable of entries by name */
static Hashtab *volnamehashtab;

/* Hashtable of entries by number */
static Hashtab *volidhashtab;

/* A list with all entries */
static List *lrulist;

/* # of entries */
static unsigned nvolcacheentries = 0;

/* # of active entries */
static unsigned nactive_volcacheentries = 0;

/*
 * VolCacheEntries are indexed by (name, cell) in volnamehashtab
 */

static int
volnamecmp (void *a, void *b)
{
    struct name_ptr *n1 = (struct name_ptr *)a;
    struct name_ptr *n2 = (struct name_ptr *)b;

    return strcmp (n1->name, n2->name)
	|| n1->cell != n2->cell;
}

static unsigned
volnamehash (void *a)
{
     struct name_ptr *n = (struct name_ptr *)a;

     return hashadd (n->name) + n->cell;
}

/*
 * and by (volid, cell) in volidhashtab
 */

static int
volidcmp (void *a, void *b)
{
    struct num_ptr *n1 = (struct num_ptr *)a;
    struct num_ptr *n2 = (struct num_ptr *)b;

    return n1->cell != n2->cell || n1->vol != n2->vol;
}

static unsigned
volidhash (void *a)
{
    struct num_ptr *n = (struct num_ptr *)a;

    return n->cell + n->vol;
}

/*
 * Create `n' entries and add at the end of `lrulist'
 */

static void
create_new_entries (unsigned n)
{
    VolCacheEntry *entries;
    int i;

    entries = (VolCacheEntry *)calloc (n, sizeof(VolCacheEntry));
    if (entries == NULL)
	arla_errx (1, ADEBERROR, "volcache: calloc failed");
    for (i = 0; i < n; ++i) {
	entries[i].cell        = -1;
	listaddtail (lrulist, &entries[i]);
    }
    
    nvolcacheentries += n;
}

/*
 * Re-cycle an entry:
 * remove it from the hashtab, clear it out.
 */

static void
recycle_entry (VolCacheEntry *e)
{
    int i;

    for (i = 0; i < MAXTYPES; ++i) {
	if (e->name_ptr[i].ptr != NULL) {
	    FCacheEntry *fe;
	    int ret;
	    VenusFid fid;

	    assert (e->num_ptr[i].ptr != NULL);

	    fid.Cell       = e->cell;
	    fid.fid.Vnode  = fid.fid.Unique = 1;
	    fid.fid.Volume = e->num_ptr[i].vol;
	    ret = fcache_find (&fe, fid);
	    if (ret == 0 && fe != NULL) {
		assert (fe->refcount > 0 && fe->flags.mountp);
		--fe->refcount;
	    }

	    hashtabdel (volnamehashtab, &e->name_ptr[i]);
	    hashtabdel (volidhashtab, &e->num_ptr[i]);
	}
    }
    memset (e, 0, sizeof(*e));
}

/*
 * Get and return a free entry.
 * Place it at the head of the lrulist.
 */

static VolCacheEntry *
get_free_entry (void)
{
    Listitem *item;
    VolCacheEntry *e;

    assert (!listemptyp(lrulist));

    for(item = listtail (lrulist);
	item;
	item = listprev (lrulist, item)) {
	e = (VolCacheEntry *)listdata(item);
	if (e->refcount == 0) {
	    listdel (lrulist, item);
	    recycle_entry (e);
	    listaddhead (lrulist, e);
	    return e;
	}
    }

    create_new_entries (VOLCACHE_INC);

    e = (VolCacheEntry *)listdeltail (lrulist);
    assert (e != NULL && e->refcount == 0);
    listaddhead (lrulist, e);
    return e;
}

/*
 *
 */

static Bool
volume_uptodatep (VolCacheEntry *e)
{
    if (connected_mode != CONNECTED)
	return TRUE;

    return e->flags.validp;
}

/*
 *
 */

static void
volcache_recover_state (void)
{
    int fd;
    VolCacheEntry tmp;
    unsigned n;

    fd = open ("volcache", O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return;
    n = 0;
    while (read (fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
	VolCacheEntry *e = get_free_entry ();
	int i;

	++n;

	e->entry    = tmp.entry;
	e->volsync  = tmp.volsync;
	e->cell     = tmp.cell;
	e->refcount = tmp.refcount;
	for (i = 0; i < MAXTYPES; ++i) {
	    if (tmp.name_ptr[i].ptr != NULL) {
		e->name_ptr[i].cell = tmp.name_ptr[i].cell;
		strcpy (e->name_ptr[i].name, tmp.name_ptr[i].name);
		e->name_ptr[i].ptr  = e;
		hashtabadd (volnamehashtab, (void *)&e->name_ptr[i]);
	    }
	    if (tmp.num_ptr[i].ptr != NULL) {
		e->num_ptr[i].cell  = tmp.num_ptr[i].cell;
		e->num_ptr[i].vol   = tmp.num_ptr[i].vol;
		e->num_ptr[i].ptr   = e;
		hashtabadd (volidhashtab, (void *)&e->num_ptr[i]);
	    }
	}
	e->flags.validp = FALSE;
    }
    close (fd);
    arla_warnx (ADEBVOLCACHE, "recovered %u entries to volcache", n);
}

/*
 *
 */

int
volcache_store_state (void)
{
    Listitem *item;
    int fd;
    unsigned n;

    fd = open ("volcache.new", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0)
	return errno;
    n = 0;
    for (item = listtail (lrulist);
	 item;
	 item = listprev (lrulist, item)) {
	VolCacheEntry *entry = (VolCacheEntry *)listdata (item);

	if (entry->cell == -1)
	    continue;
	if (write (fd, entry, sizeof(*entry)) != sizeof(*entry)) {
	    close (fd);
	    return errno;
	}
	++n;
    }

    if(close (fd))
	return errno;
    if (rename ("volcache.new", "volcache"))
	return errno;

    arla_warnx (ADEBVOLCACHE, "wrote %u entries to volcache", n);

    return 0;
}

/*
 * Initialize the volume cache with `nentries' in the free list.
 * Try to recover state iff `recover'
 */

void
volcache_init (unsigned nentries, Bool recover)
{
    volnamehashtab = hashtabnew (VOLCACHE_SIZE, volnamecmp, volnamehash);
    if (volnamehashtab == NULL)
	arla_errx (1, ADEBERROR, "volcache_init: hashtabnew failed");

    volidhashtab = hashtabnew (VOLCACHE_SIZE, volidcmp, volidhash);
    if (volidhashtab == NULL)
	arla_errx (1, ADEBERROR, "volcache_init: hashtabnew failed");

    lrulist = listnew ();
    if (lrulist == NULL)
	arla_errx (1, ADEBERROR, "volcache_init: listnew failed");
    nvolcacheentries = 0;
    create_new_entries (nentries);
    if (recover)
	volcache_recover_state ();
}

/*
 *
 */

static int
get_info (VolCacheEntry *e, const char *volname, CredCacheEntry *ce)
{
    ConnCacheEntry *conn;
    u_long addr = cell_finddbserver (e->cell).s_addr;
    int error, i;

    conn = conn_get (e->cell, addr, afsvldbport, VLDB_SERVICE_ID, ce);
    if (conn == NULL) {
	struct in_addr ia;

	ia.s_addr = addr;
	arla_warnx (ADEBVOLCACHE,
		    "volcache: failed to make rx-connection to vldb %s",
		    inet_ntoa(ia));
	return -1;
    }

    error = VL_GetEntryByName(conn->connection, volname, &e->entry);
    conn_free (conn);
    if (error) {
	arla_warn (ADEBVOLCACHE, error,
		   "VL_GetEntryByName(%s)",
		    volname);
	return -1;
    }
    
    i = min(strlen(volname), VLDB_MAXNAMELEN);
    if (strncmp(volname, e->entry.name, i)) {
	arla_warnx (ADEBERROR, "get_info: we asked for %s and got %s",
		    volname, e->entry.name);
    }


    if ((e->entry.volumeId[RWVOL] == e->entry.volumeId[ROVOL] &&
	 e->entry.flags & VLF_RWEXISTS && e->entry.flags & VLF_ROEXISTS) ||
	(e->entry.volumeId[ROVOL] == e->entry.volumeId[BACKVOL] &&
	 e->entry.flags & VLF_ROEXISTS && e->entry.flags & VLF_BOEXISTS) ||
	(e->entry.volumeId[RWVOL] == e->entry.volumeId[BACKVOL] &&
	 e->entry.flags & VLF_RWEXISTS && e->entry.flags & VLF_BOEXISTS)) {
      
	arla_warnx (ADEBERROR, "get_info: same id on diffrent volumes: %s",
		  volname);
	return -1;
    }

    if (e->entry.flags & VLF_RWEXISTS) {
	e->num_ptr[RWVOL].cell = e->cell;
	e->num_ptr[RWVOL].vol  = e->entry.volumeId[RWVOL];
	e->num_ptr[RWVOL].ptr  = e;
	hashtabadd (volidhashtab, (void *)&e->num_ptr[RWVOL]);

	e->name_ptr[RWVOL].cell = e->cell;
	snprintf (e->name_ptr[RWVOL].name, VLDB_MAXNAMELEN,
		  "%s%s", e->entry.name, volsuffixes[RWVOL]);
	e->name_ptr[RWVOL].ptr  = e;
	hashtabadd (volnamehashtab, (void *)&e->name_ptr[RWVOL]);

    }
    if (e->entry.flags & VLF_ROEXISTS) {
	e->num_ptr[ROVOL].cell = e->cell;
	e->num_ptr[ROVOL].vol  = e->entry.volumeId[ROVOL];
	e->num_ptr[ROVOL].ptr  = e;
	hashtabadd (volidhashtab, (void *)&e->num_ptr[ROVOL]);

	e->name_ptr[ROVOL].cell = e->cell;
	snprintf (e->name_ptr[ROVOL].name, VLDB_MAXNAMELEN,
		  "%s%s", e->entry.name, volsuffixes[ROVOL]);
	e->name_ptr[ROVOL].ptr  = e;
	hashtabadd (volnamehashtab, (void *)&e->name_ptr[ROVOL]);
    }
    if (e->entry.flags & VLF_BOEXISTS) {
	e->num_ptr[BACKVOL].cell = e->cell;
	e->num_ptr[BACKVOL].vol  = e->entry.volumeId[BACKVOL];
	e->num_ptr[BACKVOL].ptr  = e;
	hashtabadd (volidhashtab, (void *)&e->num_ptr[BACKVOL]);

	e->name_ptr[BACKVOL].cell = e->cell;
	snprintf (e->name_ptr[BACKVOL].name, VLDB_MAXNAMELEN,
		  "%s%s", e->entry.name, volsuffixes[BACKVOL]);
	e->name_ptr[BACKVOL].ptr  = e;
	hashtabadd (volnamehashtab, (void *)&e->name_ptr[BACKVOL]);
    }
    e->flags.validp = TRUE;
    return 0;
}

/*
 * Add an entry for (volname, cell) to the hash table.
 */

static VolCacheEntry *
add_entry (const char *volname, int32_t cell, CredCacheEntry *ce)
{
    VolCacheEntry *e;

    e = get_free_entry ();

    e->cell = cell;
    e->refcount = 0;

    if(get_info (e, volname, ce) == 0) {
	++nactive_volcacheentries;
	return e;
    } else {
	return NULL;
    }
}

/*
 * Retrieve the entry for (volname, cell).  If it's not in the cache,
 * add it.
 */

VolCacheEntry *
volcache_getbyname (const char *volname, int32_t cell, CredCacheEntry *ce)
{
    VolCacheEntry *e;
    struct name_ptr *n;
    struct name_ptr key;

    key.cell = cell;
    strncpy (key.name, volname, VLDB_MAXNAMELEN);
    key.name[VLDB_MAXNAMELEN - 1] = '\0';

    n = (struct name_ptr *)hashtabsearch (volnamehashtab, (void *)&key);
    if (n == NULL)
	e = add_entry (volname, cell, ce);
    else
	e = n->ptr;

    if (e != NULL && !volume_uptodatep (e)) {
	recycle_entry (e);
	e->cell = cell;
	if (get_info (e, volname, ce))
	    e = NULL;
    }

    if (e != NULL)
	++e->refcount;
    return e;
}

/*
 * Retrieve the entry for (volume-id, cell). If it's not in the cache,
 * add it.
 */

VolCacheEntry *
volcache_getbyid (u_int32_t id, int32_t cell, CredCacheEntry *ce)
{
    VolCacheEntry *e;
    struct num_ptr *n;
    struct num_ptr key;
    char s[11];

    snprintf (s, sizeof(s), "%u", id);

    key.cell = cell;
    key.vol  = id;

    n = (struct num_ptr *)hashtabsearch (volidhashtab, (void *)&key);
    if (n == NULL) {
	e = add_entry (s, cell, ce);
    } else {
	e = n->ptr;
    }

    if (e != NULL && !volume_uptodatep (e)) {
	recycle_entry (e);
	e->cell = cell;
	if (get_info (e, s, ce))
	    e = NULL;
    }

    if (e != NULL)
	++e->refcount;
    return e;
}

/*
 * Invalidate the volume entry for `id'
 */

void
volcache_invalidate (u_int32_t id, int32_t cell)
{
    struct num_ptr *n;
    struct num_ptr key;

    key.cell = cell;
    key.vol  = id;

    n = (struct num_ptr *)hashtabsearch (volidhashtab, (void *)&key);
    if (n != NULL) {
	VolCacheEntry *e = n->ptr;

	e->flags.validp = FALSE;
    }
}

/*
 * Save `volsync'
 */

void
volcache_update_volsync (VolCacheEntry *e, AFSVolSync volsync)
{
    e->volsync = volsync;
}


/*
 * Decrement the references and possibly remove this entry.
 */

void
volcache_free (VolCacheEntry *e)
{
    --e->refcount;
    if (e->refcount == 0)
	--nactive_volcacheentries;
}

/*
 * Print the entry `ptr' to the FILE `arg'
 */

static Bool
print_entry (void *ptr, void *arg)
{
    struct num_ptr *n = (struct num_ptr *)ptr;
    VolCacheEntry *e = n->ptr;
    FILE *f = (FILE *)arg;
    int i;
    struct in_addr tmp;

    if (n->vol != e->entry.volumeId[RWVOL])
	return FALSE;

    fprintf (f, "cell = %d (%s)\n"
	     "name = \"%s\", volumeType = %d(%s), nServers = %d\n",
	     e->cell, cell_num2name (e->cell),
	     e->entry.name,
	     e->entry.volumeType, volsuffixes[e->entry.volumeType],
	     e->entry.nServers);
    for (i = 0; i < e->entry.nServers; ++i) {
	tmp.s_addr = htonl(e->entry.serverNumber[i]);
	fprintf (f, "%d: server = %s, part = %d(%c), flags = %d\n",
		 i, inet_ntoa(tmp), e->entry.serverPartition[i],
		 'a' + e->entry.serverPartition[i],
		 e->entry.serverFlags[i]);
    }
    if (e->entry.flags & VLF_RWEXISTS)
	fprintf (f, "rw clone: %d\n", e->entry.volumeId[RWVOL]);
    if (e->entry.flags & VLF_ROEXISTS)
	fprintf (f, "ro clone: %d\n", e->entry.volumeId[ROVOL]);
    if (e->entry.flags & VLF_BACKEXISTS)
	fprintf (f, "rw clone: %d\n", e->entry.volumeId[BACKVOL]);
    fprintf (f, "refcount = %u\n\n", e->refcount);
    return FALSE;
}

/*
 * Print some status on the volume cache on `f'.
 */

void
volcache_status (FILE *f)
{
    fprintf (f, "%u(%u) volume entries\n",
	     nactive_volcacheentries, nvolcacheentries);
    hashtabforeach (volidhashtab, print_entry, f);
}
