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
 * Cache of credentials
 *
 *   Each 'cred,type' pair is collected on a list where the head is in
 *   the cell CRED_ROOT_CELL. This is to enable fast iterations over
 *   all creds for a user (indexed on cred,type).
 */

#include "arla_local.h"
RCSID("$arla: cred.c,v 1.42 2003/06/10 04:23:23 lha Exp $");

#define CRED_ROOT_CELL	(-1)

#define CREDCACHESIZE 101

#define CREDFREEINC 10

/* a hashtable of all credentials */
static Hashtab *credhtab;

/* list of all free entries */
static List *freelist;

/* # of credentials */
static unsigned ncredentials;

/* # of active credentials */
static unsigned nactive_credentials;

static void root_free(CredCacheEntry *);

/*
 * Functions for handling entries into the credentials cache.
 */

static int
credcmp (void *a, void *b)
{
     CredCacheEntry *c1 = (CredCacheEntry*)a;
     CredCacheEntry *c2 = (CredCacheEntry*)b;

     return c1->cred    != c2->cred
	 || c1->type    != c2->type
	 || c1->cell    != c2->cell;
}

static unsigned
credhash (void *a)
{
     CredCacheEntry *c = (CredCacheEntry*)a;

     return c->cred + c->type + c->cell;
}


/*
 * Create `n' credentials and add them to `freelist'
 */

static void
create_new_credentials (unsigned n)
{
    unsigned i;
    CredCacheEntry *entries;

    entries = (CredCacheEntry*)calloc (n, sizeof (CredCacheEntry));
    if (entries == NULL)
	arla_errx (1, ADEBERROR, "credcache: calloc failed");
    for (i = 0; i < n; ++i) {
	entries[i].cred_data = NULL;
	listaddhead (freelist, &entries[i]);
    }
    ncredentials += n;
}

/* 
 * Initialize the cred cache.
 */

void
cred_init (unsigned nentries)
{
     credhtab = hashtabnew (CREDCACHESIZE, credcmp, credhash);
     if (credhtab == NULL)
	 arla_errx (1, ADEBERROR, "cred_init: hashtabnew failed");
     freelist = listnew ();
     if (freelist == NULL)
	 arla_errx (1, ADEBERROR, "cred_init: listnew failed");
     ncredentials = 0;
     create_new_credentials (nentries);
}

static CredCacheEntry *
internal_get (long cell, nnpfs_pag_t cred, int type)
{
    CredCacheEntry *e;
    CredCacheEntry key;

    if (cell == CRED_ROOT_CELL)
	return NULL;

    key.cell = cell;
    key.type = type;
    key.cred = cred;

    e = (CredCacheEntry *)hashtabsearch (credhtab, (void *)&key);

    if (e == NULL && type == CRED_NONE) {
	e = cred_add (cred, type, 0, cell, 0, NULL, 0, 0);
    }

    if (e != NULL) {
	++e->refcount;
	assert(e->cell != CRED_ROOT_CELL);
    }

    return e;
}

struct list_pag {
    int (*func)(CredCacheEntry *, void *);
    void *arg;
};

static Bool
list_pag_func(List *l, Listitem *li, void *arg)
{
    CredCacheEntry *e = listdata(li);
    struct list_pag *d = arg;

    return (d->func)(e, d->arg);
}

int
cred_list_pag(nnpfs_pag_t cred, int type, 
	      int (*func)(CredCacheEntry *, void *), 
	      void *ptr)
{
    CredCacheEntry key, *root_e;
    struct list_pag d;

    key.cell = CRED_ROOT_CELL;
    key.type = type;
    key.cred = cred;

    root_e = (CredCacheEntry *)hashtabsearch (credhtab, (void *)&key);
    if (root_e == NULL)
	return 0;

    d.func = func;
    d.arg = ptr;

    listiter(root_e->pag.list, list_pag_func, &d);

    return 0;
}

CredCacheEntry *
cred_get (long cell, nnpfs_pag_t cred, int type)
{
    if (type == CRED_ANY) {
	CredCacheEntry *e;
	int i;

	for (i = CRED_MAX; i > CRED_NONE; --i) {
	    e = internal_get (cell, cred, i);
	    if (e != NULL)
		return e;
	}

	return internal_get (cell, cred, CRED_NONE);
    } else
	return internal_get (cell, cred, type);
}

static void
recycle_entry (CredCacheEntry *ce)
{
    assert(ce->refcount == 0);

    if (ce->cell != CRED_ROOT_CELL)
	root_free(ce);
    if (!ce->flags.killme)
	hashtabdel (credhtab, ce);
    if (ce->cred_data != NULL)
	free (ce->cred_data);
    memset (ce, 0, sizeof(*ce));
    listaddhead (freelist, ce);
    --nactive_credentials;
}

void
cred_free (CredCacheEntry *ce)
{ 
    if (ce == NULL)
        return;
    assert (ce->cell != CRED_ROOT_CELL);

    --ce->refcount;
    if (ce->flags.killme && ce->refcount == 0)
	recycle_entry (ce);
}

static CredCacheEntry *
get_free_cred (void)
{
    CredCacheEntry *e;

    e = (CredCacheEntry *)listdelhead (freelist);
    if (e != NULL)
	return e;

    create_new_credentials (CREDFREEINC);

    e = (CredCacheEntry *)listdelhead (freelist);
    if (e != NULL)
	return e;

    arla_errx (1, ADEBERROR,
	       "credcache: there was no way of getting a cred");
}

static void
root_free(CredCacheEntry *e)
{
    CredCacheEntry key, *root_e;

    assert(e->cell != CRED_ROOT_CELL);

    if (e->pag.li == NULL)
	return;

    key.cell = CRED_ROOT_CELL;
    key.type = e->type;
    key.cred = e->cred;

    root_e = (CredCacheEntry *)hashtabsearch (credhtab, (void *)&key);
    assert(root_e);

    listdel(root_e->pag.list, e->pag.li);
    e->pag.li = NULL;

    if (listemptyp(root_e->pag.list)) {
	listfree(root_e->pag.list);
	root_e->pag.list = NULL;
	recycle_entry(root_e);
    }
}

static void
add_to_root_entry(CredCacheEntry *e)
{
    CredCacheEntry key, *root_e;

    assert(e->cell != CRED_ROOT_CELL);

    key.cell = CRED_ROOT_CELL;
    key.type = e->type;
    key.cred = e->cred;

    root_e = (CredCacheEntry *)hashtabsearch (credhtab, (void *)&key);
    if (root_e == NULL) {
	root_e = get_free_cred ();

	root_e->cell          = CRED_ROOT_CELL;
	root_e->type          = e->type;
	root_e->cred          = e->cred;
	root_e->securityindex = -1;
	root_e->expire        = 0;
	root_e->cred_data     = NULL;
	root_e->uid           = e->uid;
	root_e->pag.list      = listnew();
	if (root_e->pag.list == NULL)
	    arla_errx (1, ADEBERROR, "add_to_root_entry: out of memory");

	hashtabadd(credhtab, root_e);

	++nactive_credentials;
    }
    e->pag.li = listaddhead (root_e->pag.list, e);
}

CredCacheEntry *
cred_add (nnpfs_pag_t cred, int type, int securityindex, long cell,
	  time_t expire, void *cred_data, size_t cred_data_sz,
	  uid_t uid)
{
    void *data;
    CredCacheEntry *e;
    CredCacheEntry *old;

    if (cred_data != NULL) {
	data = malloc (cred_data_sz);
	if (data == NULL)
	    return NULL;
	memcpy (data, cred_data, cred_data_sz);
    } else
	data = NULL;

    e = get_free_cred ();

    e->cred          = cred;
    e->type          = type;
    e->securityindex = securityindex;
    e->cell          = cell;
    e->expire        = expire;
    e->cred_data     = data;
    e->uid           = uid;
    e->pag.li	     = NULL;

    add_to_root_entry(e);

    old = (CredCacheEntry *)hashtabsearch (credhtab, (void *)e);
    if (old != NULL)
	cred_delete (old);

    hashtabadd (credhtab, e);

    ++nactive_credentials;

    return e;
}

/*
 *
 */

void
cred_delete (CredCacheEntry *ce)
{
    assert(ce->cell != CRED_ROOT_CELL);

    if (ce->refcount > 0) {
	ce->flags.killme = 1;
	hashtabdel (credhtab, ce);
	root_free(ce);
    } else
	recycle_entry (ce);
}

/*
 *
 */

void
cred_expire (CredCacheEntry *ce)
{
    const char *cell_name = cell_num2name (ce->cell);

    if (cell_name != NULL)
	arla_warnx (ADEBWARN,
		    "Credentials for UID %u in cell %s have expired",
		    (unsigned)ce->uid, cell_name);
    else
	arla_warnx (ADEBWARN,
		    "Credentials for UID %u in cell unknown %ld have expired",
		    (unsigned)ce->uid, ce->cell);

    cred_delete (ce);
}

static Bool
remove_entry (void *ptr, void *arg)
{
    CredCacheEntry *ce = (CredCacheEntry *)ptr;
    nnpfs_pag_t *cred = (nnpfs_pag_t *)arg;

    if (ce->cell == CRED_ROOT_CELL)
	return FALSE;

    if (ce->cred == *cred)
	cred_delete (ce);
    return FALSE;
}

void
cred_remove (nnpfs_pag_t cred)
{
    hashtabforeach (credhtab, remove_entry, &cred);
}

static Bool
print_cred (void *ptr, void *arg)
{
    CredCacheEntry *e = (CredCacheEntry *)ptr;

    arla_log(ADEBVLOG, "cred = %u, type = %d, securityindex = %d, "
	     "cell = %ld, refcount = %u, killme = %d, uid = %lu",
	     e->cred, e->type, e->securityindex, e->cell, e->refcount,
	     e->flags.killme, (unsigned long)e->uid);
    return FALSE;
}

void
cred_status (void)
{
    arla_log(ADEBVLOG, "%u(%u) credentials",
	     nactive_credentials, ncredentials);
    hashtabforeach (credhtab, print_cred, NULL);
}
