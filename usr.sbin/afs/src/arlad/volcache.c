/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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
RCSID("$arla: volcache.c,v 1.116 2003/04/10 15:49:13 lha Exp $") ;

static int volcache_timeout = VOLCACHE_TIMEOUT;

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

    entries = (VolCacheEntry *)malloc (n * sizeof(VolCacheEntry));
    if (entries == NULL)
	arla_errx (1, ADEBERROR, "volcache: malloc failed");
    memset(entries, 0, n * sizeof(VolCacheEntry));

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
    memset (&e->status, 0, sizeof(e->status));
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
clones_exists (VolCacheEntry *e)
{
    int i;

    for (i = 0; i < MAXTYPES; ++i)
	if (e->num_ptr[i].ptr != NULL)
	    return TRUE;
    return FALSE;
}

/*
 *
 */

void
volcache_process_marks (VolCacheEntry *ve)
{
    int i, status, inval;

    inval = 0;

    for (i = 0; i < ve->entry.nServers; i++) {
	status = ve->status[i];
	if (status & (VOLCACHE_NOVOL|VOLCACHE_UNAVAILABLE)) {
	    /* already checked ? */
	    if (status & VOLCACHE_CHECKED)
		continue;
	    ve->status[i] |= VOLCACHE_CHECKED;
	    if (ve->flags.stablep)
		inval = 1;
	}
    }
    if (inval)
	volcache_invalidate_ve(ve);
}

/*
 *
 */

static void
volcache_remove_marks (VolCacheEntry *ve)
{
    int i;

    for (i = 0; i < NMAXNSERVERS; i++)
	ve->status[i] = 0;
}

/*
 *
 */

static Bool
volume_uptodatep (VolCacheEntry *e)
{
    if (connected_mode != CONNECTED)
	return e->flags.validp ? TRUE : FALSE;

    if (time(NULL) > e->timeout)
	return FALSE;

    return e->flags.validp && clones_exists(e) == TRUE;
}

/*
 * return it if it's in the hash table.
 */

static VolCacheEntry *
getbyid (uint32_t volid, int32_t cell, int *type)
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
getbyname (const char *volname, int32_t cell)
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
 * Add a clone to `e' of type `type' with suffix `slot_type' in slot
 * slot_type
 */

static void
add_clone (VolCacheEntry *e, int type)
{
    struct num_ptr *num_ptr = &e->num_ptr[type];

    num_ptr->cell = e->cell;
    num_ptr->vol  = e->entry.volumeId[type];
    num_ptr->ptr  = e;
    num_ptr->type = type;
    hashtabadd (volidhashtab, (void *) num_ptr);
}

/*
 * Add all types of the volume entry `e' to volid hashtable. If there
 * isn't a RW volume, use the RO as the RW.
 */

static void
add_clones_to_hashtab (VolCacheEntry *e)
{
    if (e->entry.flags & VLF_RWEXISTS)
	add_clone (e, RWVOL);
    if (e->entry.flags & VLF_ROEXISTS)
	add_clone (e, ROVOL);
    if (e->entry.flags & VLF_BOEXISTS)
	add_clone (e, BACKVOL);
}

/*
 *
 */

static void
remove_clone (VolCacheEntry *e, int type)
{
    struct num_ptr *num_ptr = &e->num_ptr[type];

    if (num_ptr->ptr) {
	hashtabdel (volidhashtab, (void *) num_ptr);
	num_ptr->ptr = NULL;
    }
}

static void
remove_clones_from_hashtab (VolCacheEntry *e)
{
    int i;
    for (i = 0; i < MAXTYPES; ++i)
	remove_clone(e, i);
}

/*
 *
 */

static void
add_name_to_hashtab (VolCacheEntry *e)
{
    e->name_ptr.cell = e->cell;
    strlcpy (e->name_ptr.name, e->entry.name, sizeof(e->name_ptr.name));
    e->name_ptr.ptr  = e;
    hashtabadd (volnamehashtab, (void *)&e->name_ptr);
}

/*
 *
 */


static void
update_entry(VolCacheEntry *e, nvldbentry *entry)
{
    e->flags.stablep = cmp_nvldbentry (entry, &e->entry) == 0;
    e->flags.validp = TRUE;
    e->entry = *entry;

    if (e->flags.stablep == FALSE) {
	volcache_remove_marks (e);
	remove_clones_from_hashtab (e);
	add_clones_to_hashtab (e);
    }
}

/*
 *
 */

struct vstore_context {
    Listitem *item;
    unsigned n;
};

/*
 *
 */

static int
volcache_recover_entry (struct volcache_store *st, void *ptr)
{
    VolCacheEntry *e = get_free_entry ();
    struct vstore_context *c = (struct vstore_context *)ptr;

    e->cell = cell_name2num (st->cell);
    if (e->cell == -1) {
	arla_warnx(ADEBWARN, "can't resolve cell name");
	return(-1);
    }
    e->entry = st->entry;
    e->volsync = st->volsync;
    e->refcount = st->refcount;

    add_name_to_hashtab (e);
    add_clones_to_hashtab (e);

    c->n++;

    return 0;
}

/*
 *
 */

static void
volcache_recover_state (void)
{
    struct vstore_context c;
    Listitem *item;

    c.n = 0;
    c.item = NULL;

    state_recover_volcache ("volcache", volcache_recover_entry, &c);

    for(item = listhead (lrulist);
	item;
	item = listnext (lrulist, item)) {
	VolCacheEntry *e = (VolCacheEntry *)listdata(item);
	VolCacheEntry *parent;

	if (e->cell == -1)
	    continue;

	parent = getbyid (e->parent_fid.fid.Volume,
			  e->parent_fid.Cell,
			  NULL);
	if (parent != NULL)
	    volcache_volref (e, parent);
    }
    arla_warnx (ADEBVOLCACHE, "recovered %u entries to volcache", c.n);
}

/*
 *
 */

static int
volcache_store_entry (struct volcache_store *st, void *ptr)
{
    struct vstore_context *c;
    VolCacheEntry *e;

    c = (struct vstore_context *)ptr;
    if (c->item == NULL)		/* check if done ? */
	return STORE_DONE;

    e = (VolCacheEntry *)listdata (c->item);
    c->item = listprev (lrulist, c->item);

    if (e->cell == -1)
	return STORE_SKIP;
    
    strlcpy(st->cell, cell_num2name(e->cell), sizeof(st->cell));
    st->entry = e->entry;
    st->volsync = e->volsync;
    st->refcount = e->refcount;

    c->n++;
    return STORE_NEXT;
}

/*
 *
 */

int
volcache_store_state (void)
{
    struct vstore_context c;
    int ret;

    c.item = listtail (lrulist);
    c.n = 0;

    ret = state_store_volcache("volcache", volcache_store_entry, &c);
    if (ret)
	arla_warn(ADEBWARN, ret, "failed to store volcache state");
    else
	arla_warnx (ADEBVOLCACHE, "wrote %u entries to volcache", c.n);

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
get_info_common (VolCacheEntry *e, nvldbentry *entry)
{
    if (entry->flags & VLF_DFSFILESET)
	arla_warnx (ADEBWARN,
		    "get_info: %s is really a DFS volume. "
		    "This might not work",
		    entry->name);

    if ((entry->volumeId[RWVOL] == entry->volumeId[ROVOL] &&
	 entry->flags & VLF_RWEXISTS && entry->flags & VLF_ROEXISTS) ||
	(entry->volumeId[ROVOL] == entry->volumeId[BACKVOL] &&
	 entry->flags & VLF_ROEXISTS && entry->flags & VLF_BOEXISTS) ||
	(entry->volumeId[RWVOL] == entry->volumeId[BACKVOL] &&
	 entry->flags & VLF_RWEXISTS && entry->flags & VLF_BOEXISTS)) {
      
	arla_warnx (ADEBERROR, "get_info: same id on different volumes: %s",
		    entry->name);
	return ENOENT;
    }

    e->flags.validp = TRUE;
    e->timeout = volcache_timeout + time(NULL);
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
 * Fill in the vldb entry in `entry'.
 *
 * Return 0 if succesful, else error.
 */

static int
get_info_loop (VolCacheEntry *e, nvldbentry *entry,
	       const char *name, int32_t cell,
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
	dynroot_fetch_root_vldbN (entry);
	return 0;
    }
    
    if (connected_mode == DISCONNECTED)
	return ENETDOWN;

    db_servers = cell_dbservers_by_id (cell, &num_db_servers);
    if (db_servers == NULL || num_db_servers == 0) {
	arla_warnx (ADEBWARN,
		    "Cannot find any db servers in cell %d(%s) while "
		    "getting data for volume `%s'",
		    cell, cell_num2name(cell), name);
	return ENOENT;
    }

    conns = malloc (num_db_servers * sizeof(*conns));
    if (conns == NULL)
	return ENOMEM;

    for (i = 0, j = 0; i < num_db_servers; ++i) {
	ConnCacheEntry *conn;

	conn = conn_get (cell, db_servers[i].addr.s_addr, afsvldbport,
			 VLDB_SERVICE_ID, vl_probe, ce);
	if (conn_isalivep (conn))
	    conn->rtt = rx_PeerOf(conn->connection)->srtt
		+ rand() % RTT_FUZZ - RTT_FUZZ / 2;
	else
	    conn->rtt = INT_MAX / 2;
	conns[j++] = conn;
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
		    vldbentry oldentry;
		    error = VL_GetEntryByName (conns[i]->connection,
					       name, &oldentry);
		    if (error == 0)
			vldb2vldbN(&oldentry, entry);
		} else
		    error = VL_GetEntryByNameN (conns[i]->connection,
						name, entry);
		switch (error) {
		case 0 :
		    sanitize_nvldbentry (entry);
		    try_again = FALSE;
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
		case RXKADBADTICKET:
		case RXKADBADKEY:
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
get_info_byid (VolCacheEntry *e, nvldbentry *entry,
	       uint32_t id, int32_t cell,
	       CredCacheEntry *ce)
{
    int error;
    char s[11];

    snprintf (s, sizeof(s), "%u", id);
    error = get_info_loop (e, entry, s, cell, ce);
    if (error)
	return error;
    return get_info_common (e, entry);
}


/*
 * Retrieve the information for `volname' into `e' using `ce' as the creds.
 * Return 0 or error.
 */

static int
get_info_byname (VolCacheEntry *e, nvldbentry *entry,
		 const char *volname, int32_t cell,
		 CredCacheEntry *ce)
{
    int error;

    error = get_info_loop (e, entry, volname, cell, ce);
    if (error)
	return error;

    /*
     * If the name we looked up is different from the one we got back,
     * replace that one with the canonical looked up name.  Otherwise,
     * we're not going to be able to find the volume in question.
     */

    if (strcmp(volname, entry->name) != 0) {
	arla_warnx (ADEBWARN,
		    "get_info: different volnames: %s - %s",
		    volname, entry->name);

	if (strlcpy (entry->name, volname,
		     sizeof(entry->name)) >= sizeof(entry->name)) {
	    arla_warnx (ADEBWARN,
			"get_info: too long volume (%.*s)",
			(int)strlen(volname), volname);
	    return ENAMETOOLONG;
	}
    }

    return get_info_common (e, entry);
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

    e->cell		= cell;
    e->refcount		= 0;
    e->vol_refs		= 0;
    e->flags.lookupp	= 1;
    strlcpy(e->entry.name, volname, sizeof(e->entry.name));

    add_name_to_hashtab (e);

    error = get_info_byname (e, &e->entry, volname, cell, ce);
    if (error == 0) {
	*ret = e;
	add_clones_to_hashtab (e);
    }

    e->flags.lookupp	= 0;
    if (e->flags.waiting)
	LWP_NoYieldSignal (e);

    return error;
}

/*
 * Retrieve the entry for (volname, cell).  If it's not in the cache,
 * add it.
 */

int
volcache_getbyname (const char *volname, int32_t cell, CredCacheEntry *ce,
		    VolCacheEntry **e, int *ret_type)
{
    int type, error;
    char real_volname[VLDB_MAXNAMELEN];

    strlcpy (real_volname, volname, sizeof(real_volname));
    type = volname_canonicalize (real_volname);
    if (ret_type)
	*ret_type = type;

    for(;;) {
	*e = getbyname (real_volname, cell);
	if (*e == NULL) {
	    error = add_entry_byname (e, real_volname, cell, ce);
	    if (error)
		return error;
	    continue;
	}

	if ((*e)->flags.lookupp) {
	    (*e)->flags.waiting = 1;
	    volcache_ref (*e);
	    LWP_WaitProcess (*e);
	    if (!(*e)->flags.validp) {
		volcache_free (*e);
		return ENOENT;
	    }
	} else
	    volcache_ref (*e);

	if (volume_uptodatep (*e)) {
	    error = 0;
	    break;
	} else {
	    nvldbentry tmp_e;
	    
	    error = get_info_byname (*e, &tmp_e, real_volname, cell, ce);
	    if (error)
		break;
	    update_entry(*e, &tmp_e);
	    break;
	}
    }

    if (error)
	(*e)->flags.validp = 0;
    if ((*e)->flags.waiting)
	LWP_NoYieldSignal (*e);
    (*e)->flags.lookupp = 0;
    (*e)->flags.waiting = 0;
    if (error) {
	volcache_free (*e);
	*e = NULL;
    }
    return error;
}

/*
 * Retrieve the entry for (volume-id, cell). If it's not in the cache,
 * there is no good way of adding it, and thus fail.
 */

int
volcache_getbyid (uint32_t volid, int32_t cell, CredCacheEntry *ce,
		  VolCacheEntry **e, int *type)
{
    int error = 0;
    for(;;) {
	*e = getbyid (volid, cell, type);
	if (*e == NULL) 
	    return ENOENT;

	if ((*e)->flags.lookupp) {
	    (*e)->flags.waiting = 1;
	    volcache_ref (*e);
	    LWP_WaitProcess (*e);
	    if (!(*e)->flags.validp) {
		volcache_free (*e);
		return ENOENT;
	    }
	} else
	    volcache_ref (*e);

	if (volume_uptodatep (*e)) {
	    error = 0;
	    break;
	} else {
	    nvldbentry tmp_e;

	    error = get_info_byid (*e, &tmp_e, volid, cell, ce);
	    if (error)
		break;
	    update_entry(*e, &tmp_e);
	    break;
	}
    }

    if (error)
	(*e)->flags.validp = 0;
    if ((*e)->flags.waiting)
	LWP_NoYieldSignal (*e);
    (*e)->flags.lookupp = 0;
    (*e)->flags.waiting = 0;
    if (error) {
	volcache_free (*e);
	*e = NULL;
    }
    return error;
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

static Bool
inval (void *ptr, void *arg)
{
    struct num_ptr *n = (struct num_ptr *)ptr;
    VolCacheEntry *e  = n->ptr;

    volcache_invalidate_ve (e);
    return FALSE;
}

/*
 * Invalidate all volume entries
 */

void
volcache_invalidate_all (void)
{
    hashtabforeach (volidhashtab, inval, NULL);
}

/*
 * invalidate this volume if id == data->id
 */

static Bool
invalidate_vol (void *ptr, void *arg)
{
    uint32_t id = *((uint32_t *)arg);
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
volcache_invalidate (uint32_t id, int32_t cell)
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
 *
 */

Bool
volume_downp (int error)
{
    switch (error) {
    case ARLA_VNOVOL:
    case ARLA_VMOVED:
	return TRUE;
    default:
	return FALSE;
    }
}

/*
 *
 */

void
volcache_mark_down (VolCacheEntry *ve, int i, int error)
{
    int type;

    assert(i < NMAXNSERVERS && i < ve->entry.nServers);

    switch (error) {
    case ARLA_VNOVOL:
    case ARLA_VMOVED:
	type = VOLCACHE_NOVOL;
	break;
    default:
	type = VOLCACHE_UNAVAILABLE;
	break;
    }

    ve->status[i] |= type;
}


/*
 *
 */

Bool
volcache_reliablep_el (VolCacheEntry *ve, int i)
{
    assert(i < NMAXNSERVERS && i < ve->entry.nServers);

    if (ve->status[i] == 0)
	return TRUE;
    return FALSE;
}

void
volcache_reliable_el (VolCacheEntry *ve, int i)
{
    assert(i < NMAXNSERVERS && i < ve->entry.nServers);
    ve->status[i] = 0;
}

/*
 * Return TRUE if this should be considered reliable (if it's validp,
 * stablep and fresh).
 */

Bool
volcache_reliablep (uint32_t id, int32_t cell)
{
    VolCacheEntry *e = getbyid (id, cell, NULL);

    return e != NULL
	&& e->flags.validp
	&& e->flags.stablep
	&& time(NULL) < e->timeout;
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
    nvldbentry tmp_e;
    int ret;

    if (connected_mode != CONNECTED ||
	volume_uptodatep (e))
	return 0;
    
    ret = get_info_byname (e, &tmp_e, e->entry.name, e->cell, ce);
    if (ret == 0)
	update_entry(e, &tmp_e);
    return ret;
}

/*
 * Get a name for a volume in (name, name_sz).
 * Return 0 if succesful
 */

int
volcache_getname (uint32_t id, int32_t cell,
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
volcache_volid2bit (const VolCacheEntry *ve, uint32_t volid)
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
