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
 * Manage our cache of volume information.
 */

#include "arla_local.h"
RCSID("$KTH: volcache.c,v 1.95.2.4 2001/03/04 05:11:19 lha Exp $") ;

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
    assert (volname != NULL);

    root_volume_name = volname;
}

#define VOLCACHE_SIZE	2053
#define VOLCACHE_INC	300

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
 * Compare two `nvldbentry' and return 0 if they are equal.
 */

static int
cmp_nvldbentry (const nvldbentry *n1, const nvldbentry *n2)
{
    int i;

    if (strcmp (n1->name, n2->name) != 0)
	return 1;
    if (n1->nServers != n2->nServers)
	return 1;
    for (i = 0; i < n1->nServers; ++i)
	if (n1->serverNumber[i] != n2->serverNumber[i]
	    || n1->serverPartition[i] != n2->serverPartition[i]
	    || n1->serverFlags[i] != n2->serverFlags[i])
	    return 1;
    if (n1->flags != n2->flags)
	return 1;
    if (n1->flags & VLF_RWEXISTS
	&& n1->volumeId[RWVOL] != n2->volumeId[RWVOL])
	return 1;
    if (n1->flags & VLF_ROEXISTS
	&& n1->volumeId[ROVOL] != n2->volumeId[ROVOL])
	return 1;
    if (n1->flags & VLF_BOEXISTS
	&& n1->volumeId[BACKVOL] != n2->volumeId[BACKVOL])
	return 1;
    if (n1->cloneId != n2->cloneId)
	return 1;
    return 0;
}

/*
 * Do consistency checks and simple clean-ups.
 */

static void
sanitize_nvldbentry (nvldbentry *n)
{
    if (n->nServers > NMAXNSERVERS) {
	arla_warnx (ADEBVOLCACHE, "too many servers %d > %d",
		    n->nServers, NMAXNSERVERS);
	n->nServers = NMAXNSERVERS;
    }
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
	entries[i].cell = -1;
	entries[i].li   = listaddtail (lrulist, &entries[i]);
    }
    
    nvolcacheentries += n;
}

/*
 * mark as not being in use
 */

static void
mark_unused (VolCacheEntry *e)
{
    if (e->refcount == 0 && e->vol_refs == 0) {
	listdel (lrulist, e->li);
	e->li = listaddtail (lrulist, e);
	assert (nactive_volcacheentries > 0);
	assert (nactive_volcacheentries <= nvolcacheentries);
	--nactive_volcacheentries;
    }
}

/*
 * Re-cycle an entry:
 * remove it from the hashtab, clear it out.
 */

static void
recycle_entry (VolCacheEntry *e)
{
    int i;

    assert (e->refcount == 0 && e->vol_refs == 0);

    for (i = 0; i < MAXTYPES; ++i)
	if (e->num_ptr[i].ptr != NULL)
	    hashtabdel (volidhashtab, &e->num_ptr[i]);
    if (e->name_ptr.ptr != NULL)
	hashtabdel (volnamehashtab, &e->name_ptr);
    if (e->parent) {
	volcache_volfree (e->parent);
	e->parent = NULL;
    }

    memset (&e->entry, 0, sizeof(e->entry));
    memset (&e->volsync, 0, sizeof(e->volsync));
    e->flags.validp  = FALSE;
    e->flags.stablep = FALSE;
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
	if (e->refcount == 0 && e->vol_refs == 0) {
	    listdel (lrulist, item);
	    recycle_entry (e);
	    e->li = listaddhead (lrulist, e);
	    return e;
	}
    }

    create_new_entries (VOLCACHE_INC);

    e = (VolCacheEntry *)listdeltail (lrulist);
    assert (e != NULL && e->refcount == 0);
    e->li = listaddhead (lrulist, e);
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
 * return it if it's in the hash table.
 */

static VolCacheEntry *
getbyid (u_int32_t volid, int32_t cell, int *type)
{
    struct num_ptr *n;
    struct num_ptr key;

    key.cell = cell;
    key.vol  = volid;

    n = (struct num_ptr *)hashtabsearch (volidhashtab, (void *)&key);
    if (n == NULL)
	return NULL;
    if (type != NULL)
	*type = n->type;
    return n->ptr;
}

/*
 * return it if it's in the hash table.
 */

static VolCacheEntry *
getbyname (const char *volname, int32_t cell, int *type)
{
    struct name_ptr *n;
    struct name_ptr key;

    key.cell = cell;
    strlcpy (key.name, volname, sizeof(key.name));

    n = (struct name_ptr *)hashtabsearch (volnamehashtab, (void *)&key);
    if (n == NULL)
	return NULL;
    return n->ptr;
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
    u_int32_t u1, u2;
    Listitem *item;

    fd = open ("volcache", O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return;
    if (read (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| read (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	close (fd);
	return;
    }
    if (u1 != VOLCACHE_MAGIC_COOKIE) {
	arla_warnx (ADEBVOLCACHE, "dump file not recognized, ignoring");
	close (fd);
	return;
    }
    if (u2 != VOLCACHE_VERSION) {
	arla_warnx (ADEBVOLCACHE, "unknown dump file version number %u", u2);
	close (fd);
	return;
    }

    n = 0;
    while (read (fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
	VolCacheEntry *e = get_free_entry ();
	int i;

	++n;

	e->entry      = tmp.entry;
	e->volsync    = tmp.volsync;
	e->cell       = tmp.cell;
	e->refcount   = tmp.refcount;
	e->vol_refs   = 0;
	e->mp_fid     = tmp.mp_fid;
	e->parent_fid = tmp.parent_fid;
	e->parent     = NULL;
	if (tmp.name_ptr.ptr != NULL) {
	    e->name_ptr.cell = tmp.name_ptr.cell;
	    strlcpy (e->name_ptr.name, tmp.name_ptr.name,
		     sizeof(e->name_ptr.name));
	    e->name_ptr.ptr  = e;
	    hashtabadd (volnamehashtab, (void *)&e->name_ptr);
	}

	for (i = 0; i < MAXTYPES; ++i) {
	    if (tmp.num_ptr[i].ptr != NULL) {
		e->num_ptr[i].cell  = tmp.num_ptr[i].cell;
		e->num_ptr[i].vol   = tmp.num_ptr[i].vol;
		e->num_ptr[i].ptr   = e;
		hashtabadd (volidhashtab, (void *)&e->num_ptr[i]);
	    }
	}
	e->flags.validp  = FALSE;
	e->flags.stablep = FALSE;
    }
    for(item = listhead (lrulist);
	item;
	item = listnext (lrulist, item)) {
	VolCacheEntry *e = (VolCacheEntry *)listdata(item);
	VolCacheEntry *parent = getbyid (e->parent_fid.fid.Volume,
					 e->parent_fid.Cell,
					 NULL);
	if (parent != NULL)
	    volcache_volref (e, parent);
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
    u_int32_t u1, u2;

    fd = open ("volcache.new", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0)
	return errno;
    u1 = VOLCACHE_MAGIC_COOKIE;
    u2 = VOLCACHE_VERSION;
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

    if (type == suffix_type) {
	num_ptr->cell = e->cell;
	num_ptr->vol  = e->entry.volumeId[type];
	num_ptr->ptr  = e;
	num_ptr->type = type;
	hashtabadd (volidhashtab, (void *) num_ptr);
    }
}

/*
 *
 */

static void
add_to_hashtab (VolCacheEntry *e)
{
    e->name_ptr.cell = e->cell;
    strlcpy (e->name_ptr.name, e->entry.name, sizeof(e->name_ptr.name));
    e->name_ptr.ptr  = e;
    hashtabadd (volnamehashtab, (void *)&e->name_ptr);

    if (e->entry.flags & VLF_RWEXISTS)
	add_clone (e, RWVOL, RWVOL);
    else
	add_clone (e, ROVOL, RWVOL);
    if (e->entry.flags & VLF_ROEXISTS)
	add_clone (e, ROVOL, ROVOL);
    if (e->entry.flags & VLF_BOEXISTS)
	add_clone (e, BACKVOL, BACKVOL);
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
get_info_loop (VolCacheEntry *e, const char *name, int32_t cell,
	       CredCacheEntry *ce)
{
    const cell_db_entry *db_servers;
    int num_db_servers;
    int num_working_db_servers;
    int error = 0;
    ConnCacheEntry **conns;
    int i, j;
    Bool try_again;

    if (dynroot_isvolumep (cell, name)) {
	dynroot_fetch_vldbN (&e->entry);
	return 0;
    }

    db_servers = cell_dbservers_by_id (cell, &num_db_servers);
    if (db_servers == NULL || num_db_servers == 0) {
	arla_warnx (ADEBWARN,
		    "Cannot find any db servers in cell %d(%s) while "
		    "getting data for volume `%s'",
		    cell, cell_num2name(cell), name);
	assert (cell_is_sanep (cell));
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
		switch (error) {
		case 0 :
		    sanitize_nvldbentry (&e->entry);
		    try_again = FALSE;
		    e->last_fetch = time(NULL);
		    break;
		case VL_NOENT :
		    error = ENOENT;
		    try_again = FALSE;
		    break;
#ifdef KERBEROS
		case RXKADEXPIRED :
		    try_again = FALSE;
		    break;
		case RXKADSEALEDINCON:
		case RXKADUNKNOWNKEY:
		    try_again = FALSE;
		    break;
#endif
		case RXGEN_OPCODE:
		    if (conns[i]->flags.old == FALSE) {
			conns[i]->flags.old = TRUE;
			goto retry;
		    }
		    break;
		default :
		    if (host_downp(error))
			conn_dead (conns[i]);
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
 * Retrieve the information for the volume `id' into `e' using `ce' as
 * the creds.
 * Return 0 or error.
 */

static int
get_info_byid (VolCacheEntry *e, u_int32_t id, int32_t cell,
	       CredCacheEntry *ce)
{
    int error;
    char s[11];

    snprintf (s, sizeof(s), "%u", id);
    error = get_info_loop (e, s, cell, ce);
    if (error)
	return error;
    return get_info_common (e);
}


/*
 * Retrieve the information for `volname' into `e' using `ce' as the creds.
 * Return 0 or error.
 */

static int
get_info_byname (VolCacheEntry *e, const char *volname, int32_t cell,
		 CredCacheEntry *ce)
{
    int error;

    error = get_info_loop (e, volname, cell, ce);
    if (error)
	return error;

    /*
     * If the name we looked up is different from the one we got back,
     * replace that one with the canonical looked up name.  Otherwise,
     * we're not going to be able to find the volume in question.
     */

    if (strcmp(volname, e->entry.name) != 0) {
	arla_warnx (ADEBWARN,
		    "get_info: different volnames: %s - %s",
		    volname, e->entry.name);

	if (strlcpy (e->entry.name, volname,
		     sizeof(e->entry.name)) >= sizeof(e->entry.name)) {
	    arla_warnx (ADEBWARN,
			"get_info: too long volume (%.*s)",
			(int)strlen(volname), volname);
	    return ENAMETOOLONG;
	}
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
    e->vol_refs = 0;

    error = get_info_byname (e, volname, cell, ce);
    if (error == 0) {
	add_to_hashtab (e);
	*ret = e;
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
    e->vol_refs = 0;

    error = get_info_byid (e, id, cell, ce);
    if (error == 0) {
	add_to_hashtab (e);
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
		    VolCacheEntry **e, int *type)
{
    int error = 0;
    char real_volname[VLDB_MAXNAMELEN];

    strlcpy (real_volname, volname, sizeof(real_volname));
    *type = volname_canonicalize (real_volname);

    for(;;) {
	*e = getbyname (real_volname, cell, type);
	if (*e == NULL) {
	    error = add_entry_byname (e, real_volname, cell, ce);
	    if (error)
		return error;
	    continue;
	}
	
	volcache_ref (*e);
	if (volume_uptodatep (*e)) {
	    return 0;
	} else {
	    VolCacheEntry tmp_ve;

	    error = get_info_byname (&tmp_ve, real_volname, cell, ce);
	    if (error) {
		volcache_free (*e);
		return error;
	    }
	    (*e)->flags.stablep = cmp_nvldbentry (&tmp_ve.entry,
						  &(*e)->entry) == 0;
	    (*e)->flags.validp = TRUE;
	    (*e)->entry = tmp_ve.entry;
	}
    }
}

/*
 * Retrieve the entry for (volume-id, cell). If it's not in the cache,
 * add it.
 */

int
volcache_getbyid (u_int32_t volid, int32_t cell, CredCacheEntry *ce,
		  VolCacheEntry **e, int *type)
{
    int error = 0;
    for(;;) {
	*e = getbyid (volid, cell, type);
	if (*e == NULL) {
	    error = add_entry_byid (e, volid, cell, ce);
	    if (error)
		return error;
	    continue;
	}
	volcache_ref(*e);
	if (volume_uptodatep (*e)) {
	    return 0;
	} else {
	    VolCacheEntry tmp_ve;

	    error = get_info_byid (&tmp_ve, volid, cell, ce);
	    if (error) {
		volcache_free(*e);
		return error;
	    }
	    (*e)->flags.stablep = cmp_nvldbentry (&tmp_ve.entry,
						  &(*e)->entry) == 0;
	    (*e)->flags.validp = TRUE;
	    (*e)->entry = tmp_ve.entry;
	}
    }
}

/*
 * Invalidate the volume entry `ve'
 */

void
volcache_invalidate_ve (VolCacheEntry *ve)
{
    ve->flags.validp  = FALSE;
    ve->flags.stablep = FALSE;
}


/*
 * invalidate this volume if id == data->id
 */

static Bool
invalidate_vol (void *ptr, void *arg)
{
    u_int32_t id = *((u_int32_t *)arg);
    struct num_ptr *n = (struct num_ptr *)ptr;
    VolCacheEntry *e  = n->ptr;

    if (n->vol == id)
	volcache_invalidate_ve (e);

    return FALSE;
}


/*
 * Invalidate the volume entry for `id'
 */

void
volcache_invalidate (u_int32_t id, int32_t cell)
{
    if (cell == -1) {
	hashtabforeach (volidhashtab, invalidate_vol, &id);
    } else {
	VolCacheEntry *e = getbyid (id, cell, NULL);
	if (e != NULL)
	    volcache_invalidate_ve (e);
    }
}

/*
 * Return TRUE if this should be considered reliable (if it's validp,
 * stablep and fresh).
 */

Bool
volcache_reliable (u_int32_t id, int32_t cell)
{
    VolCacheEntry *e = getbyid (id, cell, NULL);

    return e != NULL
	&& e->flags.validp && e->flags.stablep
	&& (time(NULL) - e->last_fetch) < VOLCACHE_OLD;
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
 * Increment the references to `e'
 */

void
volcache_ref (VolCacheEntry *e)
{
    if (e->refcount == 0 && e->vol_refs == 0)
	++nactive_volcacheentries;
    ++e->refcount;
}

/*
 * Decrement the references and possibly remove this entry.
 */

void
volcache_free (VolCacheEntry *e)
{
    --e->refcount;
    mark_unused (e);
}

/*
 * A parent directory of `e' is `parent'.
 * Record it and bump the vol ref count in `parent' iff e does not
 * already have a parent.
 */

void
volcache_volref (VolCacheEntry *e, VolCacheEntry *parent)
{
    if (e->parent == NULL) {
	if (parent->refcount == 0 && parent->vol_refs == 0)
	    ++nactive_volcacheentries;
	++parent->vol_refs;
	e->parent = parent;
    }
}

/*
 * remove one `volume' reference
 */

void
volcache_volfree (VolCacheEntry *e)
{
    --e->vol_refs;
    mark_unused (e);
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

    arla_log(ADEBVLOG, "cell = %d (%s)"
	     "name = \"%s\", nServers = %d",
	     e->cell, cell_num2name (e->cell),
	     e->entry.name,
	     e->entry.nServers);
    for (i = 0; i < e->entry.nServers; ++i) {
	tmp.s_addr = htonl(e->entry.serverNumber[i]);
	arla_log(ADEBVLOG, "%d: server = %s, part = %d(%c), flags = %d",
		 i, inet_ntoa(tmp), e->entry.serverPartition[i],
		 'a' + e->entry.serverPartition[i],
		 e->entry.serverFlags[i]);
    }
    if (e->entry.flags & VLF_RWEXISTS)
	arla_log(ADEBVLOG, "rw clone: %d", e->entry.volumeId[RWVOL]);
    if (e->entry.flags & VLF_ROEXISTS)
	arla_log(ADEBVLOG, "ro clone: %d", e->entry.volumeId[ROVOL]);
    if (e->entry.flags & VLF_BACKEXISTS)
	arla_log(ADEBVLOG, "rw clone: %d", e->entry.volumeId[BACKVOL]);
    arla_log(ADEBVLOG, "refcount = %u", e->refcount);
    arla_log(ADEBVLOG, "vol_refs = %u", e->vol_refs);
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
    
    return get_info_byname (e, e->entry.name, e->cell, ce);
}

/*
 * Get a name for a volume in (name, name_sz).
 * Return 0 if succesful
 */

int
volcache_getname (u_int32_t id, int32_t cell,
		  char *name, size_t name_sz)
{
    int type;
    VolCacheEntry *e = getbyid (id, cell, &type);

    if (e == NULL)
	return -1;
    volname_specific (e->name_ptr.name, type, name, name_sz);
    return 0;
}

/*
 * Find out what incarnation of a particular volume we're using
 * return one of (VLSF_RWVOL, VLSF_ROVOL, VLSF_BACKVOL or -1 on error)
 */

int
volcache_volid2bit (const VolCacheEntry *ve, u_int32_t volid)
{
    int bit = -1;

    if (ve->entry.flags & VLF_RWEXISTS
	&& ve->entry.volumeId[RWVOL] == volid)
	bit = VLSF_RWVOL;

    if (ve->entry.flags & VLF_ROEXISTS
	&& ve->entry.volumeId[ROVOL] == volid)
	bit = VLSF_ROVOL;

    if (ve->entry.flags & VLF_BACKEXISTS
	&& ve->entry.volumeId[BACKVOL] == volid)
	bit = VLSF_RWVOL;

    return bit;
}

/*
 * Print some status on the volume cache on `f'.
 */

void
volcache_status (void)
{
    arla_log(ADEBVLOG, "%u(%u) volume entries",
	     nactive_volcacheentries, nvolcacheentries);
    hashtabforeach (volidhashtab, print_entry, NULL);
}
