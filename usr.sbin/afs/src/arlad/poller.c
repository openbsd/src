/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Poller of fileserver
 *
 * Its part of the afs protocol that you need to talk the server now
 * and then to make sure you don't have any droped callbacks. 
 *
 * It is in its own module since mixing it with connection makes the
 * connection code ever harder to read and understand the it is today,
 * and there is enough of strange dependencies between clear conns and
 * auth/crypto conns today and when they are allowed to be gc-ed, etc.
 */


#include "arla_local.h"
#ifdef RCSID
RCSID("$arla: poller.c,v 1.9 2003/03/04 22:31:37 lha Exp $") ;
#endif

#define POLLERCACHESIZE 101

#define POLLERHEAPSIZE 101

/* Hashtable of connections */
static Hashtab *pollerhtab;

/* Heap for poller */
Heap *pollerheap;


int poller_timeout = 300; /* 5 min */
int poller_downhost_timeout = 60; /* 1 min */

static PROCESS poller_pid;

enum { POLLER_RUNNING, POLLER_SLEEPING, POLLER_WAITING } poller_status;

/*
 * Functions for handling entries into the poller cache.
 */

static int
pollercmp (void *a, void *b)
{
    PollerEntry *c1 = (PollerEntry*)a;
    PollerEntry *c2 = (PollerEntry*)b;

    return  c1->server != c2->server;
}

static unsigned int
pollerhash (void *a)
{
    PollerEntry *c = (PollerEntry*)a;

    return c->server;
}

static int
poller_time_cmp (const void *a, const void *b)
{
    PollerEntry *c1 = (PollerEntry*)a;
    PollerEntry *c2 = (PollerEntry*)b;

    return  c1->server - c2->server;
}


/*
 * Add it to heap, use the minium of cell's timeout value and
 * poller_{,downhost_}timeout.
 */

static void
poller_add(PollerEntry *e, int host_dead, cell_entry *c)
{
    time_t timeout = 0;

    if (c)
	timeout = cell_get_poller_time(c);

    if (timeout == 0)
	timeout = max(poller_timeout, poller_downhost_timeout);

    if (host_dead == 0)
	timeout = min(poller_timeout, timeout);
    else
	timeout = min(poller_downhost_timeout, timeout);

    e->timeout = timeout + time(NULL);

    heap_insert(pollerheap, e, &e->heapptr);

    switch(poller_status) {
    case POLLER_SLEEPING:
	/* we can only insert before first entry if its the short
	 * timeout, that is host_dead -> poller_downhost_timeout */
	if (host_dead)
	    IOMGR_Cancel(poller_pid);
	break;
    case POLLER_WAITING:
	LWP_NoYieldSignal(poller_add);
	break;
    case POLLER_RUNNING:
	break;
    default:
	abort();
    }
}


/*
 * Add a poller event for this conn, there is really a fcache entry
 * that will need to be refreshed.
 */

PollerEntry *
poller_add_conn(ConnCacheEntry *conn)
{
    PollerEntry *e, key;
    cell_entry *cell;

    key.server = rx_HostOf(rx_PeerOf(conn->connection));

    cell = cell_get_by_id(conn->cell);

    e = hashtabsearch(pollerhtab, &key);
    if (e == NULL) {
	e = malloc(sizeof(*e));
	if (e == NULL)
	    return NULL;

	e->server = rx_HostOf(rx_PeerOf(conn->connection));
	e->cell = conn->cell;
	e->refcount = 0;
	hashtabadd(pollerhtab, e);
    } else
	heap_remove(pollerheap, e->heapptr);

    e->refcount++;
    poller_add(e, 0, cell);

    return e;
}

void
poller_remove(PollerEntry *e)
{
    assert(e->refcount > 0);

    e->refcount--;
    if (e->refcount == 0) {
	hashtabdel(pollerhtab, e);
	heap_remove(pollerheap, e->heapptr);
	free(e);
    }
}

struct poller_intr_arg {
    poller_iter_func func;
    void *arg;
};

static Bool
poller_foreach_func(void *ptr, void *arg)
{
    PollerEntry *e = ptr;
    struct poller_intr_arg *a = arg;
    e->refcount++;
    (*a->func)(e->cell, e->server, a->arg);
    poller_remove(e);
    return FALSE;
}

int
poller_foreach(poller_iter_func iter_func, void *arg)
{
    struct poller_intr_arg a;

    a.arg = arg;
    a.func = iter_func;
    hashtabforeach(pollerhtab, poller_foreach_func, &a);

    return 0;
}

/*
 *
 */

#define POLLER_STACKSIZE (16*1024)

/*
 * Loop waiting for things servers to probe.
 */

static void
poller (char *arg)
{
    cell_entry *cell;
    PollerEntry *e;
    time_t now;
    int host_dead_p;

    for (;;) {
	poller_status = POLLER_RUNNING;

	arla_warnx(ADEBCONN, "poller waiting");

	now = time(NULL);

	e = (PollerEntry *)heap_head(pollerheap);
	if (e == NULL) {
	    poller_status = POLLER_WAITING;
	    LWP_WaitProcess(poller_add);
	    continue;
	} else if (e->timeout > now) {
	    poller_status = POLLER_SLEEPING;
	    IOMGR_Sleep (e->timeout - now);
	    continue;
	}

	arla_warnx(ADEBCONN, "running poller");

	e->refcount++;

	/* XXX should a dead host break callbacks ? */
	
	host_dead_p = 0;

	if (connected_mode != DISCONNECTED) {
	    ConnCacheEntry *conn;
	    CredCacheEntry *ce;

	    ce = cred_get (e->cell, 0, CRED_NONE);
	    assert (ce != NULL);

	    conn = conn_get (e->cell, e->server, afsport,
			     FS_SERVICE_ID, fs_probe, ce);
	    cred_free (ce);

	    if (conn) {
		if (!conn_isalivep (conn))
		    host_dead_p = 1;
		else if (fs_probe(conn->connection) != 0)
		    host_dead_p = 1;
		conn_free(conn);
	    } else
		host_dead_p = 1;
	}

	cell = cell_get_by_id(e->cell);

	e->refcount--;

	heap_remove(pollerheap, e->heapptr);
	poller_add(e, host_dead_p, cell);

	arla_warnx(ADEBCONN, "poller done");
    }
}

/*
 * Find and return the cell for given `server', if the cell can't be
 * found, return -1.
 */

int32_t
poller_host2cell(uint32_t server)
{
    PollerEntry *e, key;

    key.server = server;

    e = hashtabsearch(pollerhtab, &key);
    if (e == NULL)
	return -1;
    return e->cell;
}

/* 
 * Initialize the poller.
 */

void
poller_init (void)
{
     arla_warnx (ADEBCONN, "initpoller");

     pollerhtab = hashtabnew (POLLERCACHESIZE, pollercmp, pollerhash);
     if (pollerhtab == NULL)
	 arla_errx (1, ADEBERROR, "poller_init: hashtabnew failed");

     pollerheap = heap_new(POLLERHEAPSIZE, poller_time_cmp);
     if (pollerheap == NULL)
	 arla_errx (1, ADEBERROR, "poller_init: heap_new failed");

     poller_status = POLLER_RUNNING;

     if (LWP_CreateProcess (poller, POLLER_STACKSIZE, 1,
			    NULL, "poller", &poller_pid))
	 arla_errx (1, ADEBERROR,
		    "conn: cannot create poller thread");
}
