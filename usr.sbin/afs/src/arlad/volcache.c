/*	$OpenBSD: volcache.c,v 1.2 1999/04/30 01:59:10 art Exp $	*/
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
 * Manage our cache of volume information.
 */

#include "arla_local.h"
RCSID("$KTH: volcache.c,v 1.67 1999/04/20 20:58:09 map Exp $") ;

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
	if (e->num_ptr[i].ptr != NULL) {
	    FCacheEntry *fe;
	    int ret;
	    VenusFid fid;

	    fid.Cell       = e->cell;
	    fid.fid.Vnode  = fid.fid.Unique = 1;
	    fid.fid.Volume = e->num_ptr[i].vol;
	    ret = fcache_find (&fe, fid);
	    if (ret == 0 && fe != NULL) {
		if (fe->refcount > 0)
		    --fe->refcount;
		fcache_release (fe);
	    }
	    hashtabdel (volidhashtab, &e->num_ptr[i]);
	}
	if (e->name_ptr[i].ptr != NULL) {
	    hashtabdel (volnamehashtab, &e->name_ptr[i]);
	}
    }
    memset (&e->entry, 0, sizeof(e->entry));
    memset (&e->volsync, 0, sizeof(e->volsync));
    e->flags.validp = FALSE;
    memset (&e->name_ptr, 0, sizeof(e->name_ptr));
    memset (&e->num_ptr, 0, sizeof(e->num_ptr));
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
 * Add a clone to `e' of type `type' with suffix `slot_type' in slot
 * slot_type
 */

static void
add_clone (VolCacheEntry *e, int type, int suffix_type)
{
    struct num_ptr *num_ptr = &e->num_ptr[type];
    struct name_ptr *name_ptr = &e->name_ptr[suffix_type];

    if (type == suffix_type) {
	num_ptr->cell = e->cell;
	num_ptr->vol  = e->entry.volumeId[type];
	num_ptr->ptr  = e;
	num_ptr->type = type;
	hashtabadd (volidhashtab, (void *) num_ptr);
    }

    name_ptr->cell = e->cell;
    snprintf (name_ptr->name, VLDB_MAXNAMELEN,
	      "%s%s", e->entry.name, volsuffixes[suffix_type]);
    name_ptr->ptr  = e;
    name_ptr->type = type;
    hashtabadd (volnamehashtab, (void *) name_ptr);
}

/*
 *
 */

static int
get_info_common (VolCacheEntry *e)
{
    if (e->entry.flags & VLF_DFSFILESET)
	arla_warnx (ADEBWARN,
		    "get_info: %s is really a DFS volume. "
		    "This might not work",
		    e->entry.name);

    if ((e->entry.volumeId[RWVOL] == e->entry.volumeId[ROVOL] &&
	 e->entry.flags & VLF_RWEXISTS && e->entry.flags & VLF_ROEXISTS) ||
	(e->entry.volumeId[ROVOL] == e->entry.volumeId[BACKVOL] &&
	 e->entry.flags & VLF_ROEXISTS && e->entry.flags & VLF_BOEXISTS) ||
	(e->entry.volumeId[RWVOL] == e->entry.volumeId[BACKVOL] &&
	 e->entry.flags & VLF_RWEXISTS && e->entry.flags & VLF_BOEXISTS)) {
      
	arla_warnx (ADEBERROR, "get_info: same id on diffrent volumes: %s",
		    e->entry.name);
	return -1;
    }

    if (e->entry.flags & VLF_RWEXISTS)
	add_clone (e, RWVOL, RWVOL);
    else
	add_clone (e, ROVOL, RWVOL);
    if (e->entry.flags & VLF_ROEXISTS)
	add_clone (e, ROVOL, ROVOL);
    if (e->entry.flags & VLF_BOEXISTS)
	add_clone (e, BACKVOL, BACKVOL);
    e->flags.validp = TRUE;
    return 0;
}

/*
 * A function for checking if a service is up.  Return 0 if succesful.
 */

static int
vl_probe (struct rx_connection *conn)
{
    return VL_Probe (conn);
}

/*
 * Get all the db servers for `e->cell', sort them in order by rtt
 * (with some fuzz) and try to retrieve the entry for `name'.
 *
 * Return 0 if succesful, else error.
 */

static int
get_info_loop (VolCacheEntry *e, const char *name, CredCacheEntry *ce)
{
    const cell_db_entry *db_servers;
    int num_db_servers;
    int num_working_db_servers;
    int error = 0;
    int cell = e->cell;
    ConnCacheEntry **conns;
    int i, j;
    Bool try_again;

    db_servers = cell_dbservers (cell, &num_db_servers);
    if (db_servers == NULL || num_db_servers == 0) {
	arla_warnx (ADEBWARN,
		    "Cannot find any db servers in cell %d(%s)",
		    cell, cell_num2name(cell));
	return ENOENT;
    }

    conns = malloc (num_db_servers * sizeof(*conns));
    if (conns == NULL)
	return ENOMEM;

    for (i = 0, j = 0; i < num_db_servers; ++i) {
	ConnCacheEntry *conn;

	conn = conn_get (cell, db_servers[i].addr.s_addr, afsvldbport,
			 VLDB_SERVICE_ID, vl_probe, ce);
	if (conn != NULL) {
	    conn->rtt = rx_PeerOf(conn->connection)->rtt
		+ rand() % RTT_FUZZ - RTT_FUZZ / 2;
	    conns[j++] = conn;
	}
    }
    num_working_db_servers = j;

    qsort (conns, num_working_db_servers, sizeof(*conns),
	   conn_rtt_cmp);

    try_again = TRUE;

    for (i = 0; i < num_working_db_servers; ++i) {
	if (conns[i] != NULL) {
	retry:
	    if (try_again) {
		if (conns[i]->flags.old) {
		    vldbentry entry;
		    error = VL_GetEntryByName (conns[i]->connection,
					       name, &entry);
		    if (error == 0)
			vldb2vldbN(&entry, &e->entry);
		} else
		    error = VL_GetEntryByNameN (conns[i]->connection,
						name, &e->entry);
		if (error == RX_CALL_DEAD)
		    conn_dead (conns[i]);
		switch (error) {
		case 0 :
		    try_again = FALSE;
		    break;
		case VL_NOENT :
		    error = ENOENT;
		    try_again = FALSE;
		    break;
		case RXGEN_OPCODE:
		    if (conns[i]->flags.old == FALSE) {
			conns[i]->flags.old = TRUE;
			goto retry;
		    }
		    break;
		default :
		    arla_warn (ADEBVOLCACHE, error,
			       "VL_GetEntryByName%s(%s)", 
			       conns[i]->flags.old ? "" : "N",
			       name);
		    break;
		}
	    }
	    conn_free (conns[i]);
	}
    }

    free (conns);

    if (try_again) {
	arla_warnx (ADEBWARN,
		    "Failed to contact any db servers in cell %d(%s)",
		    cell, cell_num2name(cell));
	error = ETIMEDOUT;
    }

    return error;
}

/*
 *
 */

static int
get_info_byid (VolCacheEntry *e, u_int32_t id, CredCacheEntry *ce)
{
    int error;
    char s[11];

    snprintf (s, sizeof(s), "%u", id);
    error = get_info_loop (e, s, ce);
    if (error)
	return error;
    return get_info_common (e);
}


/*
 *
 */

static int
get_info_byname (VolCacheEntry *e, const char *volname, CredCacheEntry *ce)
{
    int error;
    int i;
    size_t entry_name_len;
    int name_matched;

    error = get_info_loop (e, volname, ce);
    if (error)
	return error;

    entry_name_len = strlen(e->entry.name);
    name_matched = FALSE;

    for (i = 0; i < MAXTYPES; ++i) {
	if (strncmp (volname, e->entry.name, entry_name_len) == 0
	    && strcmp (volname + entry_name_len, volsuffixes[i]) == 0) {
	    name_matched = TRUE;
	    break;
	}
    }

    /*
     * If the name we looked up is different from the one we got back,
     * replace that one with the canonical looked up name.  Otherwise,
     * we're not going to be able to find the volume in question.
     */

    if (!name_matched) {
	size_t volname_len = strlen(volname);

	arla_warnx (ADEBWARN,
		    "get_info: different volnames: %s - %s",
		    volname, e->entry.name);

	for (i = MAXTYPES - 1; i >= 0; --i)
	    if (strcmp (volname + volname_len - strlen(volsuffixes[i]),
			volsuffixes[i]) == 0) {
		volname_len -= strlen(volsuffixes[i]);
		break;
	    }

	strncpy (e->entry.name, volname, volname_len);
	e->entry.name[volname_len] = '\0';
    }

    return get_info_common (e);
}

/*
 * Add an entry for (volname, cell) to the hash table.
 */

static int
add_entry_byname (VolCacheEntry **ret, const char *volname,
		  int32_t cell, CredCacheEntry *ce)
{
    VolCacheEntry *e;
    int error;

    e = get_free_entry ();

    e->cell = cell;
    e->refcount = 0;

    error = get_info_byname (e, volname, ce);
    if (error == 0) {
	*ret = e;
	++nactive_volcacheentries;
    }
    return error;
}

/*
 * Add an entry for (volname, cell) to the hash table.
 */

static int
add_entry_byid (VolCacheEntry **ret, u_int32_t id,
		int32_t cell, CredCacheEntry *ce)
{
    int error;
    VolCacheEntry *e;

    e = get_free_entry ();

    e->cell = cell;
    e->refcount = 0;

    error = get_info_byid (e, id, ce);
    if (error == 0) {
	++nactive_volcacheentries;
	*ret = e;
    }
    return error;
}

/*
 * Retrieve the entry for (volname, cell).  If it's not in the cache,
 * add it.
 */

int
volcache_getbyname (const char *volname, int32_t cell, CredCacheEntry *ce,
		    VolCacheEntry **e, int32_t *type)
{
    int error = 0;
    struct name_ptr *n;
    struct name_ptr key;

    key.cell = cell;
    strncpy (key.name, volname, VLDB_MAXNAMELEN);
    key.name[VLDB_MAXNAMELEN - 1] = '\0';

    for(;;) {
	n = (struct name_ptr *)hashtabsearch (volnamehashtab, (void *)&key);
	if (n == NULL) {
	    error = add_entry_byname (e, volname, cell, ce);
	    if (error)
		return error;
	    continue;
	}
	*e = n->ptr;
	*type = n->type;
	++(*e)->refcount;
	if (volume_uptodatep (*e)) {
	    return 0;
	} else {
	    recycle_entry (*e);
	    error = get_info_byname (*e, volname, ce);
	    --(*e)->refcount;
	    if (error)
		return error;
	}
    }
}

/*
 * Retrieve the entry for (volume-id, cell). If it's not in the cache,
 * add it.
 */

int
volcache_getbyid (u_int32_t volid, int32_t cell, CredCacheEntry *ce,
		  VolCacheEntry **e, int32_t *type)
{
    int error = 0;
    struct num_ptr *n;
    struct num_ptr key;

    key.cell = cell;
    key.vol  = volid;

    for(;;) {
	n = (struct num_ptr *)hashtabsearch (volidhashtab, (void *)&key);
	if (n == NULL) {
	    error = add_entry_byid (e, volid, cell, ce);
	    if (error)
		return error;
	    continue;
	}
	*e = n->ptr;
	*type = n->type;
	++(*e)->refcount;
	if (volume_uptodatep (*e)) {
	    return 0;
	} else {
	    recycle_entry (*e);
	    error = get_info_byid (*e, volid, ce);
	    --(*e)->refcount;
	    if (error)
		return error;
	}
    }
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
    int i;
    struct in_addr tmp;

    if (n->vol != e->entry.volumeId[RWVOL])
	return FALSE;

    arla_log(ADEBVLOG, "cell = %d (%s)\n"
	     "name = \"%s\", nServers = %d\n",
	     e->cell, cell_num2name (e->cell),
	     e->entry.name,
	     e->entry.nServers);
    for (i = 0; i < e->entry.nServers; ++i) {
	tmp.s_addr = htonl(e->entry.serverNumber[i]);
	arla_log(ADEBVLOG, "%d: server = %s, part = %d(%c), flags = %d\n",
		 i, inet_ntoa(tmp), e->entry.serverPartition[i],
		 'a' + e->entry.serverPartition[i],
		 e->entry.serverFlags[i]);
    }
    if (e->entry.flags & VLF_RWEXISTS)
	arla_log(ADEBVLOG, "rw clone: %d\n", e->entry.volumeId[RWVOL]);
    if (e->entry.flags & VLF_ROEXISTS)
	arla_log(ADEBVLOG, "ro clone: %d\n", e->entry.volumeId[ROVOL]);
    if (e->entry.flags & VLF_BACKEXISTS)
	arla_log(ADEBVLOG, "rw clone: %d\n", e->entry.volumeId[BACKVOL]);
    arla_log(ADEBVLOG, "refcount = %u\n\n", e->refcount);
    return FALSE;
}

/*
 *
 */

int
volume_make_uptodate (VolCacheEntry *e, CredCacheEntry *ce)
{
    if (connected_mode != CONNECTED ||
	volume_uptodatep (e))
	return 0;
    
    return get_info_byname (e, e->entry.name, ce);
}

/*
 * Print some status on the volume cache on `f'.
 */

void
volcache_status (void)
{
    arla_log(ADEBVLOG, "%u(%u) volume entries\n",
	     nactive_volcacheentries, nvolcacheentries);
    hashtabforeach (volidhashtab, print_entry, NULL);
}
