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
 * Cache of credentials
 */

#include "arla_local.h"
RCSID("$KTH: cred.c,v 1.33.2.2 2001/04/21 14:55:34 lha Exp $");

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
internal_get (long cell, xfs_pag_t cred, int type)
{
    CredCacheEntry *e;
    CredCacheEntry key;

    key.cell = cell;
    key.type = type;
    key.cred = cred;

    e = (CredCacheEntry *)hashtabsearch (credhtab, (void *)&key);

    if (e == NULL && type == CRED_NONE) {
	e = cred_add (cred, type, 0, cell, 0, NULL, 0, 0);
    }

    if (e != NULL)
	++e->refcount;

    return e;
}

CredCacheEntry *
cred_get (long cell, xfs_pag_t cred, int type)
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

CredCacheEntry *
cred_add (xfs_pag_t cred, int type, int securityindex, long cell,
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

    old = hashtabsearch (credhtab, e);
    if (old != NULL)
	recycle_entry (old);

    hashtabadd (credhtab, e);
    ++nactive_credentials;
    return e;
}

#if KERBEROS
CredCacheEntry *
cred_add_krb4 (xfs_pag_t cred, uid_t uid, CREDENTIALS *c)
{
    CredCacheEntry *ce;
    char *cellname;
    int cellnum;

    if (*c->instance == '\0')
	cellname = strdup (c->realm);
    else
	cellname = strdup (c->instance);
    if (cellname == NULL)
	return NULL;
    strlwr (cellname);
    cellnum = cell_name2num(cellname);
    free (cellname);
    if (cellnum == -1)
	return NULL;

    ce = cred_add (cred, CRED_KRB4, 2, cellnum, -1, c, sizeof(*c), uid);
    return ce;
}
#endif

/*
 *
 */

void
cred_delete (CredCacheEntry *ce)
{
    if (ce->refcount > 0) {
	ce->flags.killme = 1;
	hashtabdel (credhtab, ce);
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
		    "Credentials for UID %u in cell %s has expired",
		    (unsigned)ce->uid, cell_name);
    else
	arla_warnx (ADEBWARN,
		    "Credentials for UID %u in cell unknown %ld has expired",
		    (unsigned)ce->uid, ce->cell);

    cred_delete (ce);
}

static Bool
remove_entry (void *ptr, void *arg)
{
    CredCacheEntry *ce = (CredCacheEntry *)ptr;
    xfs_pag_t *cred = (xfs_pag_t *)arg;

    if (ce->cred == *cred)
	cred_delete (ce);
    return FALSE;
}

void
cred_remove (xfs_pag_t cred)
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
