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
 * Cache of connections
 */


#include "arla_local.h"
#ifdef RCSID
RCSID("$arla: conn.c,v 1.75 2003/06/10 04:23:20 lha Exp $") ;
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

/* List of connections to probe */
static List *connprobelist;

#ifdef KERBEROS
int conn_rxkad_level = rxkad_auth;
#endif

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

/* 2^MAX_RETRIES is the maximum number of seconds between probes */

#define MAX_RETRIES 8

/*
 * Add this entry again to the probe list but without restarting ntries.
 */

static void
re_probe (ConnCacheEntry *e)
{
    Listitem *item;
    struct timeval tv;

    assert (e->probe != NULL);

    gettimeofday (&tv, NULL);
    if (e->probe_le) {
	listdel (connprobelist, e->probe_le);
	e->probe_next = min(tv.tv_sec + (1 << e->ntries), e->probe_next);
    } else
	e->probe_next = tv.tv_sec + (1 << e->ntries);

    if (e->ntries <= MAX_RETRIES)
	++e->ntries;

    for (item = listhead (connprobelist);
	 item;
	 item = listnext (connprobelist, item)) {
	ConnCacheEntry *this = (ConnCacheEntry *)listdata (item);

	if (e->probe_next < this->probe_next) {
	    e->probe_le = listaddbefore (connprobelist, item, e);
	    LWP_NoYieldSignal (connprobelist);
	    return;
	}
    }
    e->probe_le = listaddtail (connprobelist, e);
    LWP_NoYieldSignal (connprobelist);
}

/*
 * Initial add to probe list.
 */

static void
add_to_probe_list (ConnCacheEntry *e, int ntries)
{
    e->ntries = ntries;
    re_probe (e);
}

/*
 *
 */

#define PINGER_STACKSIZE (16*1024)


static PROCESS pinger_pid;

/*
 * Loop waiting for things servers to probe.
 */

static void
pinger (char *arg)
{
    for (;;) {
	struct timeval tv;
	Listitem *item;
	ConnCacheEntry *e;
	struct in_addr addr;
	const char *port_str;

	arla_warnx(ADEBCONN, "running pinger");

	while (listemptyp (connprobelist))
	    LWP_WaitProcess (connprobelist);

	item = listhead (connprobelist);
	e = (ConnCacheEntry *)listdata (item);

	assert (e->probe_le == item);

	gettimeofday (&tv, NULL);
	if (tv.tv_sec < e->probe_next) {
	    unsigned long t = e->probe_next - tv.tv_sec;

	    arla_warnx(ADEBCONN,
		       "pinger: sleeping %lu second(s)", t);
	    IOMGR_Sleep (t);
	    continue;
	}

	listdel (connprobelist, item);
	e->probe_le = NULL;

	if (e->flags.alivep)
	    continue;

	addr.s_addr = e->host;
	port_str    = ports_num2name (e->port);

	if (port_str != NULL)
	    arla_warnx (ADEBCONN, "pinger: probing %s/%s",
			inet_ntoa(addr), port_str);
	else
	    arla_warnx (ADEBCONN, "pinger: probing %s/%d",
			inet_ntoa(addr), e->port);
	++e->refcount;
	if (e->probe == NULL)
	    arla_warnx(ADEBWARN, "pinger: probe function is NULL, "
		       "host: %s cell: %d port: %d",
		       inet_ntoa(addr), e->cell, e->port);

	if (connected_mode == DISCONNECTED) {
	    arla_warnx(ADEBCONN, "pinger: ignoring host in disconnected mode");
	} else if (e->probe && ((*(e->probe))(e->connection) == 0)) {
	    conn_alive (e);
	} else {
	    re_probe (e);
	}

	conn_free (e);
    }
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
	  entries[i].refcount   = 0;
	  entries[i].parent     = NULL;
	  entries[i].probe_le	= NULL;
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
     connprobelist = listnew ();
     if (connprobelist == NULL)
	 arla_errx (1, ADEBERROR, "conn_init: listnew failed");
     nconnections = 0;

     if (LWP_CreateProcess (pinger, PINGER_STACKSIZE, 1,
			    NULL, "pinger", &pinger_pid))
	 arla_errx (1, ADEBERROR,
		    "conn: cannot create pinger thread");

     create_new_connections (nentries);
}

/*
 * Re-cycle an entry:
 * remove it from the hashtab, clear it out and place it on the freelist.
 */

static void
recycle_conn (ConnCacheEntry *e)
{
    assert (e->refcount == 0);

    if (e->parent != NULL) {
	conn_free (e->parent);
	e->parent = NULL;
    }
    if (e->probe_le != NULL) {
	listdel (connprobelist, e->probe_le);
	e->probe_le = NULL;
    }
    if (!e->flags.killme)
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
		uint32_t host,
		uint16_t port,
		uint16_t service,
		nnpfs_pag_t cred,
		int securityindex,
		int (*probe)(struct rx_connection *),
		struct rx_securityClass *securityobject)
{
    ConnCacheEntry *e;

    assert (probe != NULL);

    e = get_free_connection ();

    e->cell          = cell;
    e->host          = host;
    e->port          = port;
    e->service       = service;
    e->flags.alivep  = TRUE;
    e->flags.old     = FALSE;
    e->refcount      = 0;
    e->cred          = cred;
    e->securityindex = securityindex;
    e->probe	     = probe;

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
	       uint32_t host,
	       uint16_t port,
	       uint16_t service,
	       int (*probe)(struct rx_connection *),
	       CredCacheEntry *ce)
{
    ConnCacheEntry *e;
    struct rx_securityClass *securityobj;
    int securityindex;
    nnpfs_pag_t cred;

    if (ce) {
	securityindex = ce->securityindex;
	cred = ce->cred;

	switch (ce->type) {
#ifdef KERBEROS
	case CRED_KRB4 : {
	    struct cred_rxkad *cred = (struct cred_rxkad *)ce->cred_data;
	    
	    securityobj = rxkad_NewClientSecurityObject(conn_rxkad_level,
							cred->ct.HandShakeKey,
							cred->ct.AuthHandle,
							cred->ticket_len,
							cred->ticket);
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
			cred, securityindex, probe, securityobj);

    hashtabadd (connhtab, (void *)e);
    ++nactive_connections;

    return e;
}


/*
 * Find a connection from the cache given:
 * (cell, host, port, service, cred).
 * If there's no connection at all, create one.
 */

static ConnCacheEntry *
internal_get (int32_t cell,
	      uint32_t host,
	      uint16_t port,
	      uint16_t service,
	      int (*probe)(struct rx_connection *),
	      CredCacheEntry *ce)
{
    ConnCacheEntry *e;
    ConnCacheEntry key;

#if 0
    if (connected_mode == DISCONNECTED)
	return NULL;
#endif

    key.host          = host;
    key.port          = port;
    key.service       = service;
    key.cred          = ce->cred;
    key.securityindex = ce->securityindex;

    e = (ConnCacheEntry *)hashtabsearch (connhtab, (void *)&key);

    if (e == NULL) {
	ConnCacheEntry *parent = NULL;

	if (ce->securityindex || ce->cred) {
	    key.cred = 0;
	    key.securityindex = 0;
	    parent = (ConnCacheEntry *)hashtabsearch (connhtab, (void *)&key);
	    if (parent == NULL) {
		parent = add_connection (cell, host, port, service,
					 probe, NULL);
	    }
	    ++parent->refcount;
	}

	e = add_connection (cell, host, port, service, probe, ce);
	if (parent != NULL)
	    e->parent = parent;
    }

    /*
     * Since we only probe the parent entry (ie noauth), we make sure
     * the status from the parent entry is pushed down to the
     * children.
     */
    if(e->parent != NULL) {
	e->flags.alivep = e->parent->flags.alivep;
    }

    return e;
}

/*
 * Return a connection to (cell, host, port, service, ce)
 */

ConnCacheEntry *
conn_get (int32_t cell,
	  uint32_t host,
	  uint16_t port,
	  uint16_t service,
	  int (*probe)(struct rx_connection *),
	  CredCacheEntry *ce)
{
    ConnCacheEntry *e = internal_get (cell, host, port, service, probe, ce);

    ++e->refcount;
    return e;
}

/*
 * Add a new reference to a connection
 */

void
conn_ref(ConnCacheEntry *e)
{
    assert(e->refcount > 0);
    e->refcount++;
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

    assert (e->refcount > 0);

    --e->refcount;
    if (e->refcount == 0 && e->flags.killme)
	recycle_conn (e);
}

/*
 * Given a host try to figure out what cell it's in.
 */

int32_t
conn_host2cell (uint32_t host, uint16_t port, uint16_t service)
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
 * Mark the server in `e' as being down.
 */

void
conn_dead (ConnCacheEntry *e)
{
    struct in_addr a;
    char buf[10];
    const char *port;

    assert (e->probe != NULL);

    e->flags.alivep = FALSE;
    if (e->parent != NULL) {
	e = e->parent;
	e->flags.alivep = FALSE;
    }
    add_to_probe_list (e, 0);
    a.s_addr = e->host;
    port = ports_num2name (e->port);
    if (port == NULL) {
	snprintf(buf, sizeof(buf), "%d", e->port);
	port = buf;
    }

    arla_warnx (ADEBWARN, "Lost connection to %s/%s in cell %s",
		inet_ntoa(a), port, cell_num2name (e->cell));
}

/*
 * Mark the server in `e' as being up.
 */

void
conn_alive (ConnCacheEntry *e)
{
    struct in_addr a;
    const char *s;

    a.s_addr = e->host;
    s = ports_num2name (e->port);
    if (s != NULL)
	arla_warnx (ADEBWARN, "Server %s/%s up again", inet_ntoa(a), s);
    else
	arla_warnx (ADEBWARN, "Server %s/%d up again", inet_ntoa(a), e->port);
    e->flags.alivep = TRUE;
    if (e->parent != NULL)
	e->parent->flags.alivep = TRUE;
}

/*
 * Is this server known to be up?
 */

Bool
conn_isalivep (ConnCacheEntry *e)
{
    if (e->parent != NULL)
	e->flags.alivep = e->parent->flags.alivep;

    return e->flags.alivep;
}

/*
 * Probe the service in `e'
 */

void
conn_probe (ConnCacheEntry *e)
{
    ++e->refcount;
    {
	struct in_addr a;
	a.s_addr = e->host;
	
	if (e->probe == NULL)
	    arla_warnx(ADEBWARN, "conn_probe: probe function is NULL, "
		       "host: %s cell: %d port: %d",
		       inet_ntoa(a), e->cell, e->port);
    }
    if (e->probe && ((*(e->probe))(e->connection) == 0)) {
	if (!e->flags.alivep)
	    conn_alive (e);
    } else {
	if (e->flags.alivep)
	    conn_dead (e);
    }
    conn_free (e);
}

/*
 * Is the service at (cell, host, port, service) up?
 */

Bool
conn_serverupp (uint32_t host, uint16_t port, uint16_t service)
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
    struct in_addr tmp;

    tmp.s_addr = e->host;

    arla_log(ADEBVLOG, "host = %s, port = %d, service = %d, "
	     "cell = %d (%s), "
	     "securityindex = %d, cred = %u, "
	     "conn = %p, alive = %d, "
	     "killme = %d, refcount = %d",
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
conn_status (void)
{
    arla_log(ADEBVLOG, "%u(%u) connections",
	     nactive_connections, nconnections);
    hashtabforeach (connhtab, print_conn, NULL);
}

struct clear_state {
    clear_state_mask mask;
    int32_t cell;
    nnpfs_pag_t cred;
    int securityindex;
};

static Bool
clear_cred (void *ptr, void *arg)
{
    ConnCacheEntry *e = (ConnCacheEntry *)ptr;
    struct clear_state *s = (struct clear_state *)arg;

    if ((s->mask & CONN_CS_CRED) && s->cred != e->cred)
	return FALSE;
    if ((s->mask & CONN_CS_CELL) && s->cell != e->cell)
	return FALSE;
    if ((s->mask & CONN_CS_SECIDX) && s->securityindex != e->securityindex)
	return FALSE;
    
    if (e->refcount > 0) {
	e->flags.killme = 1;
	hashtabdel (connhtab, e);	
    } else
	recycle_conn (e);

    return FALSE;
}

/*
 * Remove all connections matching mask + (cell, cred, securityindex).
 */

void
conn_clearcred(clear_state_mask mask,
	       int32_t cell, nnpfs_pag_t cred, int securityindex)
{
    struct clear_state s;

    s.mask	    = mask;
    s.cell          = cell;
    s.cred          = cred;
    s.securityindex = securityindex;

    hashtabforeach (connhtab, clear_cred, (void *)&s);
}

/*
 * check if servers are up for cell `cell'
 */

struct down_state {
    int32_t cell;
    uint32_t *hosts;
    int len;
    int i;
    int flags;
};

static Bool
host_down (void *ptr, void *arg)
{
    ConnCacheEntry *e = (ConnCacheEntry *)ptr;
    struct down_state *s = (struct down_state *)arg;
    int i;

    if (s->cell == e->cell) {

	if (!(s->flags & CKSERV_DONTPING)) {
	    conn_probe (e);
	}

	if (e->flags.alivep)
	    return FALSE;
	
	if (s->flags & CKSERV_FSONLY && e->port != afsport)
	    return FALSE;

	for (i = 0; i < s->i; ++i)
	    if (s->hosts[i] == e->host)
		return FALSE;

	s->hosts[s->i] = e->host;
	++s->i;

	if (s->i == s->len)
	    return TRUE;
    }
    return FALSE;
}

/*
 * Check what hosts are down.
 *
 * Flags is VIOCCKFLAGS
 */

void
conn_downhosts(int32_t cell, uint32_t *hosts, int *num, int flags)
{
    struct down_state s;

    if (*num == 0)
	return;

    s.cell          = cell;
    s.hosts	    = hosts;
    s.len           = *num;
    s.i	            = 0;
    s.flags         = flags;

    hashtabforeach (connhtab, host_down, (void *)&s);

    *num = s.i;
}

/*
 * Compare two ConnCacheEntries rtt-wise.  Typically used when sorting
 * entries.
 */

int
conn_rtt_cmp (const void *v1, const void *v2)
{
    ConnCacheEntry **e1 = (ConnCacheEntry **)v1;
    ConnCacheEntry **e2 = (ConnCacheEntry **)v2;
    
    return (*e1)->rtt - (*e2)->rtt;
}

/*
 * Return true iff this error means we should mark the host as down
 * due to network errors
 */

Bool
host_downp (int error)
{
    switch (error) {
    case ARLA_CALL_DEAD :
    case ARLA_INVALID_OPERATION :
    case ARLA_CALL_TIMEOUT :
    case ARLA_EOF :
    case ARLA_PROTOCOL_ERROR :
    case ARLA_USER_ABORT :
    case ARLA_ADDRINUSE :
    case ARLA_MSGSIZE :
    case RXGEN_CC_MARSHAL :
    case RXGEN_CC_UNMARSHAL :
    case RXGEN_SS_MARSHAL :
    case RXGEN_SS_UNMARSHAL :
    case RXGEN_DECODE :
    case RXGEN_OPCODE :
    case RXGEN_SS_XDRFREE :
    case RXGEN_CC_XDRFREE :
	return TRUE;
    default :
	return FALSE;
    }
}

