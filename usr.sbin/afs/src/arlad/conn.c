/*	$OpenBSD: conn.c,v 1.1.1.1 1998/09/14 21:52:55 art Exp $	*/
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
 * Cache of connections
 */


#include "arla_local.h"
#ifdef RCSID
RCSID("$KTH: conn.c,v 1.39 1998/07/13 19:16:55 assar Exp $") ;
#endif

#define CONNCACHESIZE 101

#define CONNFREELISTINC 17

/* Hashtable of connections */
static Hashtab *connhtab;

/* A list with free connections */
static List *connfreelist;

/* # of connections */
static unsigned nconnections;

/* # of active connections */
static unsigned nactive_connections;

/*
 * Functions for handling entries into the connection cache.
 */

static int
conncmp (void *a, void *b)
{
     ConnCacheEntry *c1 = (ConnCacheEntry*)a;
     ConnCacheEntry *c2 = (ConnCacheEntry*)b;

     return  c1->cred          != c2->cred
	  || c1->host          != c2->host
	  || c1->service       != c2->service
	  || c1->port          != c2->port
	  || c1->securityindex != c2->securityindex;
}

static unsigned int
connhash (void *a)
{
     ConnCacheEntry *c = (ConnCacheEntry*)a;

     return c->cred + c->host + c->service + c->port + c->securityindex;
}


/*
 * Create `n' ConnCacheEntry's and add them to `connfreelist'
 */

static void
create_new_connections (unsigned n)
{
     unsigned i;
     ConnCacheEntry *entries;

     entries = (ConnCacheEntry*)calloc (n, sizeof (ConnCacheEntry));
     if (entries == NULL)
	 arla_errx (1, ADEBERROR, "conncache: calloc failed");
     for (i = 0; i < n; ++i) {
	  entries[i].connection = NULL;
	  entries[i].refcount = 0 ;
	  listaddhead (connfreelist, &entries[i]);
     }
     nconnections += n;
}

/* 
 * Initialize the connection cache.
 */

void
conn_init (unsigned nentries)
{
     arla_warnx (ADEBCONN, "initconncache");

     connhtab = hashtabnew (CONNCACHESIZE, conncmp, connhash);
     if (connhtab == NULL)
	 arla_errx (1, ADEBERROR, "conn_init: hashtabnew failed");
     connfreelist = listnew ();
     if (connfreelist == NULL)
	 arla_errx (1, ADEBERROR, "conn_init: listnew failed");
     nconnections = 0;
     create_new_connections (nentries);
}

/*
 * Re-cycle an entry:
 * remove it from the hashtab, clear it out and place it on the freelist.
 */

static void
recycle_conn (ConnCacheEntry *e)
{
    hashtabdel (connhtab, e);
    rx_DestroyConnection (e->connection);
    memset (e, 0, sizeof(*e));
    listaddhead (connfreelist, e);
    --nactive_connections;
}

/*
 * Remove this connection from the hashtab and add it to the freelist
 * iff refcount == 0.
 */

static Bool
clear_conn (void *ptr, void *arg)
{
    ConnCacheEntry *e = (ConnCacheEntry *)ptr;

    if (e->refcount == 0)
	recycle_conn (e);
    return FALSE;
}

/*
 * Get a free connection to use.  Try to pick it from `connfreelist'.
 * If there are no there, it's time to go through `connhtab' and GC
 * unused connections.  If that fails, allocate some more.
 * And if that fails, give up.
 */

static ConnCacheEntry *
get_free_connection (void)
{
    ConnCacheEntry *e;

    e = (ConnCacheEntry *)listdelhead (connfreelist);
    if (e != NULL)
	return e;

    hashtabforeach (connhtab, clear_conn, NULL);

    e = (ConnCacheEntry *)listdelhead (connfreelist);
    if (e != NULL)
	return e;

    create_new_connections (CONNFREELISTINC);

    e = (ConnCacheEntry *)listdelhead (connfreelist);
    if (e != NULL)
	return e;

    arla_errx (1, ADEBERROR,
	       "conncache: there was no way of getting a connection");
}

/*
 * Get a free connection, fill in all parameters and create a
 * rx_connection.
 */

static ConnCacheEntry *
new_connection (int32_t cell,
		u_int32_t host,
		u_int16_t port,
		u_int16_t service,
		pag_t cred,
		int securityindex,
		struct rx_securityClass *securityobject)
{
    ConnCacheEntry *e;

    e = get_free_connection ();

    e->cell          = cell;
    e->host          = host;
    e->port          = port;
    e->service       = service;
    e->flags.alivep  = TRUE;
    e->refcount      = 0;
    e->cred          = cred;
    e->securityindex = securityindex;

    e->connection   = rx_NewConnection (host,
					htons (port),
					service,
					securityobject,
					securityindex);
    if (e->connection == NULL)
	arla_errx (1, ADEBERROR, "rx_NewConnection failed");
    return e;
}

/*
 * Create a new connection and add it to `connhtab'.
 */

static ConnCacheEntry *
add_connection(int32_t cell,
	       u_int32_t host,
	       u_int16_t port,
	       u_int16_t service,
	       CredCacheEntry *ce)
{
    ConnCacheEntry *e;
    struct rx_securityClass *securityobj;
    int securityindex;
    pag_t cred;

    if (ce) {
	securityindex = ce->securityindex;
	cred = ce->cred;

	switch (ce->type) {
#ifdef KERBEROS
	case CRED_KRB4 :
	case CRED_KRB5 : {
	    krbstruct *krbdata = (krbstruct *)ce->cred_data;
	    
	    securityobj = rxkad_NewClientSecurityObject(
		rxkad_auth,
		&krbdata->c.session,
		krbdata->c.kvno,
		krbdata->c.ticket_st.length,
		krbdata->c.ticket_st.dat);
	    break;
	}
#endif
	case CRED_NONE :
	    securityobj = rxnull_NewClientSecurityObject ();
	    break;
	default :
	    abort();
	}
    } else {
	securityobj = rxnull_NewClientSecurityObject ();
	securityindex = 0;
	cred = 0;
    }

    e = new_connection (cell, host, port, service,
			cred, securityindex, securityobj);

    hashtabadd (connhtab, (void *)e);
    ++nactive_connections;

    return e;
}


/*
 * Find a connection from the cache given:
 * (cell, host, port, service, cred).
 * If there's no connection at all, create one.
 */

ConnCacheEntry *
conn_get (int32_t cell,
	  u_int32_t host,
	  u_int16_t port,
	  u_int16_t service,
	  CredCacheEntry *ce)
{
    ConnCacheEntry *e;
    ConnCacheEntry key;

    if (connected_mode == DISCONNECTED)
	return NULL;

    key.host          = host;
    key.port          = port;
    key.service       = service;
    key.cred          = ce->cred;
    key.securityindex = ce->securityindex;

    e = (ConnCacheEntry *)hashtabsearch (connhtab, (void *)&key);

    if (e == NULL) {
	if (ce->securityindex || ce->cred) {
	    key.cred = 0;
	    key.securityindex = 0;
	    e = (ConnCacheEntry *)hashtabsearch (connhtab, (void *)&key);
	    if (e == NULL)
		add_connection (cell, host, port, service, NULL);
	}

	e = add_connection (cell, host, port, service, ce);
    }

    ++e->refcount;
    return e;
}

/*
 * Free a reference to a ConnCacheEntry.
 * If refcount drops to zero, it makes it eligible for re-use.
 */

void
conn_free (ConnCacheEntry *e)
{
    if (e == NULL)  /* When in disconnected mode conn sets to NULL */
	return;

    --e->refcount;
    if (e->refcount == 0 && e->flags.killme)
	recycle_conn (e);
}

/*
 * Given a host try to figure out what cell it's in.
 */

int32_t
conn_host2cell (u_int32_t host, u_int16_t port, u_int16_t service)
{
    ConnCacheEntry *e;
    ConnCacheEntry key;

    key.host          = host;
    key.port          = port;
    key.service       = service;
    key.cred          = 0;
    key.securityindex = 0;

    e = (ConnCacheEntry *)hashtabsearch(connhtab, (void *)&key);
    if (e == NULL)
	return -1;
    else
	return e->cell;
}

/*
 * Is the service at (cell, host, port, service) up?
 */

Bool
conn_serverupp (u_int32_t host, u_int16_t port, u_int16_t service)
{
    ConnCacheEntry *e;
    ConnCacheEntry key;

    key.host          = host;
    key.port          = port;
    key.service       = service;
    key.cred          = 0;
    key.securityindex = 0;

    e = (ConnCacheEntry *)hashtabsearch (connhtab, (void *)&key);
    if (e != NULL)
	return e->flags.alivep;
    else
	return TRUE;
}

/*
 * Print an entry.
 */

static Bool
print_conn (void *ptr, void *arg)
{
    ConnCacheEntry *e = (ConnCacheEntry *)ptr;
    FILE *f = (FILE *)arg;
    struct in_addr tmp;

    tmp.s_addr = e->host;

    fprintf (f, "host = %s, port = %d, service = %d, "
	     "cell = %d (%s), "
	     "securityindex = %d, cred = %u, "
	     "conn = %p, alive = %d, "
	     "killme = %d, refcount = %d\n\n",
	     inet_ntoa(tmp), e->port, e->service, e->cell,
	     cell_num2name (e->cell),
	     e->securityindex, e->cred, e->connection,
	     e->flags.alivep, e->flags.killme, e->refcount);

    return FALSE;
}

/*
 * Print the status of the connection cache.
 */

void
conn_status (FILE *f)
{
    fprintf (f, "%u(%u) connections\n",
	     nactive_connections, nconnections);
    hashtabforeach (connhtab, print_conn, f);
}

struct clear_state {
    int32_t cell;
    pag_t cred;
    int securityindex;
};

static Bool
clear_cred (void *ptr, void *arg)
{
    ConnCacheEntry *e = (ConnCacheEntry *)ptr;
    struct clear_state *s = (struct clear_state *)arg;

    if (e->cred == s->cred
	&& (s->cell == 0 || e->cell == s->cell)
	&& e->securityindex == s->securityindex) {
	if (e->refcount > 0)
	    e->flags.killme = 1;
	else
	    recycle_conn (e);
    }
    return FALSE;
}

/*
 * Remove all connections matching (cell, cred, securityindex).
 */

void
conn_clearcred(int32_t cell, pag_t cred, int securityindex)
{
    struct clear_state s;

    s.cell          = cell;
    s.cred          = cred;
    s.securityindex = securityindex;

    hashtabforeach (connhtab, clear_cred, (void *)&s);
}
