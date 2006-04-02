/*
 * Copyright (c) 1999, 2000 Kungliga Tekniska Högskolan
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
 * Callback module
 *
 * This module has no way for the rx module to get out stuff like
 * netmasks and MTU, that should be very interesting to know
 * performance-wise.
 */

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <assert.h>

#include <atypes.h>
#include <fs.h>
#include <cb.h>

#include <hash.h>
#include <heap.h>
#include <list.h>

#include <ports.h>
#include <service.h>

#include <rx/rx.h>
#include <rx/rx_null.h>
#include <rx/rxgencon.h>

#include <cb.cs.h>
#include <fs.ss.h>

#include <err.h>

#include <mlog.h>
#include <mdebug.h>

#include <ropa.h>

RCSID("$arla: ropa.c,v 1.30 2002/02/07 17:59:47 lha Exp $");

#ifdef DIAGNOSTIC
#define DIAGNOSTIC 1
#define DIAGNOSTIC_CLIENT 471114
#define DIAGNOSTIC_CHECK_CLIENT(c) assert((c)->magic == DIAGNOSTIC_CLIENT)
#define DIAGNOSTIC_ADDR 471197
#define DIAGNOSTIC_CHECK_ADDR(a) assert((a)->magic == DIAGNOSTIC_ADDR)
#define DIAGNOSTIC_CALLBACK 471134
#define DIAGNOSTIC_CHECK_CALLBACK(c) assert((c)->magic == DIAGNOSTIC_CALLBACK)
#define DIAGNOSTIC_CCPAIR 471110
#define DIAGNOSTIC_CHECK_CCPAIR(c) assert((c)->magic == DIAGNOSTIC_CCPAIR)
#else
#define DIAGNOSTIC_CHECK_CLIENT(c)
#define DIAGNOSTIC_CHECK_ADDR(a)
#define DIAGNOSTIC_CHECK_CALLBACK(c)
#define DIAGNOSTIC_CHECK_CCPAIR(c)
#endif

#undef NO_CALLBACKS

/*
 * Declarations
 */

#define ROPA_STACKSIZE (16*1024)
#define DEFAULT_TIMEOUT 600

struct ropa_client;

struct ropa_addr {
#ifdef DIAGNOSTIC
    int magic;				/* magic */
#endif
    struct ropa_client *c;		/* pointer to head */
    uint32_t   addr_in;		/* */
    uint32_t 	subnetmask;		/* */
    int		mtu;			/* */
};

typedef enum { ROPAC_FREE,	/* this entry is free for use */
	       ROPAC_LOOKUP_U,	/* pending RXAFSCB_WhoAreYou */
	       ROPAC_LOOKUP,	/* pending RXAFSCB_InitCallBackState */
	       ROPAC_DEAD,	/* is client doesn't respond */
	       ROPAC_PLAIN,	/* is a client w/o a UUID*/
	       ROPAC_UUID	/* is a client w/ a UUID */
} ropa_state_t;

enum { 
    ROPAF_LOOKUP = 0x1,	/* pending a lookup => set WAITING, wait on addr(c) */
    ROPAF_WAITING = 0x2	/* wait on lookup to finish */
};

struct ropa_client {
#ifdef DIAGNOSTIC
    int magic;				/* magic */
#endif
    ropa_state_t	state;
    unsigned		flags;
    int			numberOfInterfaces;
    afsUUID		uuid;
    struct ropa_addr	addr[AFS_MAX_INTERFACE_ADDR];
    uint16_t 		port;		/* port of client in network byte order */
    struct rx_connection *conn;		/* connection to client */
    time_t		lastseen;	/* last got a message */
    List		*callbacks;	/* list of ccpairs */
    int			ref;		/* refence counter */
    Listitem	       	*li; 		/* where on lru */
};

struct ropa_ccpair {			/* heap object */
#ifdef DIAGNOSTIC
    int magic;				/* magic */
#endif
    struct ropa_client 	*client;	/* pointer to client */
    struct ropa_cb	*cb;		/* pointer to callback */
    Listitem		*cb_li;		/* pointer to li on callback */
    time_t		expire;		/* when this cb expire */
    heap_ptr		heap;		/* heap pointer */
    Listitem	       	*li;		/* where on lru */
};

struct ropa_cb {
#ifdef DIAGNOSTIC
    int magic;				/* magic */
#endif
    AFSFid		fid;		/* what fid this callback is on */
    List 		*ccpairs;	/* list of clients holding this cb */
    int 		ref;		/* refence counter */
    Listitem	       	*li;		/* where on lru */
};

static void break_callback (struct ropa_cb *cb,struct ropa_client *caller,
			    Bool break_own);
static void break_ccpair (struct ropa_ccpair *cc, Bool notify_clientp);
static void break_client (struct ropa_client *c, Bool notify_clientp);
static void create_callbacks (unsigned n);
static void create_ccpairs (unsigned n);
static void create_clients (unsigned n);
static int uuid_magic_eq (afsUUID *uuid1, afsUUID *uuid2);
static void debug_print_callbacks(void);

/*
 * Module variables
 */

#define ROPA_MTU       1500

/*
 *
 */

static Hashtab *ht_clients_ip = NULL;
static Hashtab *ht_clients_uuid = NULL;
static Hashtab *ht_callbacks = NULL;
static Hashtab *ht_ccpairs = NULL;
/* least recently used on tail */
static List *lru_clients = NULL;
static List *lru_ccpair = NULL;
static List *lru_callback = NULL;
static unsigned num_clients = 0;
static unsigned num_callbacks = 0;
static unsigned num_ccpair = 0;

static Heap *heap_ccpairs = NULL;
static PROCESS cleaner_pid;

static unsigned debuglevel;

afsUUID server_uuid;

/*
 *
 */

static int
uuid_magic_eq (afsUUID *uuid1, afsUUID *uuid2)
{
    return memcmp (&uuid1->node,
		   &uuid2->node,
		   sizeof (uuid1->node));
}

/*
 * help functions for the hashtab
 */

/*
 * We compare using addresses
 */

static int
clients_cmp_ip (void *p1, void *p2)
{
    struct ropa_client *c1 = ((struct ropa_addr *) p1)->c;
    struct ropa_client *c2 = ((struct ropa_addr *) p2)->c;
    int i,j;

    for (i = 0; i < c1->numberOfInterfaces; i++) {
	for (j = 0; j < c2->numberOfInterfaces; j++) {
	    if (c1->addr[i].addr_in == c2->addr[j].addr_in
		&& c1->port == c2->port) {
		return 0;
	    }
	}
    }
    return 1;
}

/*
 * 
 */

static unsigned
clients_hash_ip (void *p)
{
    struct ropa_addr *a = (struct ropa_addr *)p;

    return a->addr_in + a->c->port;
}

/*
 * We compare using uuid
 */

static int
clients_cmp_uuid (void *p1, void *p2)
{
    return uuid_magic_eq (&((struct ropa_client *) p1)->uuid,
			  &((struct ropa_client *) p2)->uuid);
}

static unsigned
clients_hash_uuid (void *p)
{
    struct ropa_client *c =  (struct ropa_client *) p;

    return 
	c->uuid.node[0] + 
	(c->uuid.node[1] << 6) +
	(c->uuid.node[2] << 12) +
	(c->uuid.node[3] << 18) +
	(c->uuid.node[4] << 24) +
	((c->uuid.node[5] << 30) & 0x3);
}

/*
 * 
 */

static int
callbacks_cmp (void *p1, void *p2)
{
    struct ropa_cb *c1 = (struct ropa_cb *) p1;
    struct ropa_cb *c2 = (struct ropa_cb *) p2;

    return c1->fid.Volume - c2->fid.Volume
	|| c1->fid.Vnode - c2->fid.Vnode
	|| c1->fid.Unique - c2->fid.Unique;
}

/*
 *
 */

static unsigned
callbacks_hash (void *p)
{
    struct ropa_cb *c = (struct ropa_cb *)p;
    
    return c->fid.Volume + c->fid.Vnode +
	c->fid.Unique;
}


/*
 * 
 */

static int
ccpairs_cmp (void *p1, void *p2)
{
    struct ropa_ccpair *c1 = (struct ropa_ccpair *) p1;
    struct ropa_ccpair *c2 = (struct ropa_ccpair *) p2;

    return c1->cb - c2->cb
	|| c1->client - c2->client;
}

/*
 *
 */

static unsigned
ccpairs_hash (void *p)
{
    struct ropa_ccpair *c = (struct ropa_ccpair *) p;
    
    return (unsigned) c->client + callbacks_hash (c->cb);
}

/*
 *
 */

static int
ccpair_cmp_time (const void *p1, const void *p2)
{
    struct ropa_ccpair *c1 = (struct ropa_ccpair *) p1;
    struct ropa_ccpair *c2 = (struct ropa_ccpair *) p2;

    return c1->expire - c2->expire;
}

/*
 *
 */

static Bool
client_inuse_p (struct ropa_client *c)
{
    assert (c);
    if (c->state == ROPAC_FREE)
	return FALSE;
    if (c->state == ROPAC_DEAD && listemptyp(c->callbacks))
	return FALSE;
    return TRUE;
}

/*
 *
 */

static Bool
callback_inuse_p (struct ropa_cb *cb)
{
    assert (cb);
    return !listemptyp (cb->ccpairs) ;
}

/*
 *
 */

static Bool
ccpairs_inuse_p (struct ropa_ccpair *cc)
{
    assert (cc);
    return cc->client == NULL ? FALSE : TRUE ;
}

/*
 *
 */

static void
create_clients (unsigned n)
{
    struct ropa_client *c;
    unsigned long i;

    c = calloc (n, sizeof (*c));
    if (c == NULL)
	err (1, "create_clients: calloc");

    for (i = 0 ; i < n; i++) {
#ifdef DIAGNOSTIC
	c[i].magic = DIAGNOSTIC_CLIENT;
	{
	    int j;
	    for (j = 0; j < sizeof(c->addr)/sizeof(c->addr[0]); j++)
		c[i].addr[j].magic = DIAGNOSTIC_ADDR;
	}
#endif
		 
	c[i].port = 0;
	c[i].lastseen = 0;
	c[i].callbacks = listnew();
	if (c[i].callbacks == NULL)
	    err (1, "create_clients: listnew");
	c[i].ref = 0;
	c[i].state = ROPAC_FREE;
	c[i].li = listaddtail (lru_clients, &c[i]);
    }
    num_clients += n;
}

/*
 *
 */

static void
create_callbacks (unsigned n)
{
    struct ropa_cb *c;
    int i;

    c = malloc (n * sizeof (*c));
    if (c == NULL)
	err (1, "create_callbacks: malloc");
    memset (c, 0, n * sizeof (*c));

    for (i = 0; i < n ; i++) {
#ifdef DIAGNOSTIC
	c[i].magic = DIAGNOSTIC_CALLBACK;
#endif
	c[i].ccpairs = listnew();
	if (c[i].ccpairs == NULL)
	    err (1, "create_callbacks: listnew");
	c[i].li = listaddtail (lru_callback, &c[i]);
    }
    num_callbacks += n;
}

/*
 *
 */

static void
create_ccpairs (unsigned n) 
{
    struct ropa_ccpair *c;
    int i;

    c = malloc (n * sizeof (*c));
    if (c == NULL)
	err (1, "create_ccpairs: malloc");
    memset (c, 0, n * sizeof (*c));

    for (i = 0; i < n ; i++) {
#ifdef DIAGNOSTIC
	c[i].magic = DIAGNOSTIC_CCPAIR;
#endif
	c[i].li = listaddtail (lru_ccpair, &c[i]);
    }
    num_ccpair += n;
}

/*
 *
 */

static void
client_ref (struct ropa_client *c)
{
    assert (c->ref >= 0);
    if (c->li)
	listdel (lru_clients, c->li);
    c->ref++;
    if (c->li)
	c->li = listaddhead (lru_clients, c);
}

/*
 *
 */

static void
callback_ref (struct ropa_cb *cb)
{
    assert (cb->ref >= 0);
    if (cb->li)
	listdel (lru_callback, cb->li);
    cb->ref++;

    mlog_log (MDEBROPA, "cb_ref: %x.%x.%x (%d)",
		cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique, cb->ref);

    if (cb->li)
	cb->li = listaddhead (lru_callback, cb);
}

/*
 *
 */

static void
clear_addr (struct ropa_addr *addr)
{
    DIAGNOSTIC_CHECK_ADDR(addr);
    addr->c		= NULL;
    addr->addr_in	= 0;
    addr->subnetmask	= 0;
    addr->mtu		= 0;
}

/*
 * Update `c' with new host information.
 */

static void
client_update_interfaces (struct ropa_client *c, uint32_t host,
			  uint16_t port, interfaceAddr *addr)
{
    int i;
    int found_addr = 0;

    if (addr->numberOfInterfaces > AFS_MAX_INTERFACE_ADDR)
	addr->numberOfInterfaces = AFS_MAX_INTERFACE_ADDR;
    
    for (i = 0; i < c->numberOfInterfaces; i++) {
	hashtabdel (ht_clients_ip, &c->addr[i]);
	DIAGNOSTIC_CHECK_ADDR(&c->addr[i]);
    }

    for (i = 0; i < addr->numberOfInterfaces; i++) {
	DIAGNOSTIC_CHECK_ADDR(&c->addr[i]);
	c->addr[i].c		= c;
	c->addr[i].addr_in	= addr->addr_in[i];
	c->addr[i].subnetmask	= addr->subnetmask[i];
	c->addr[i].mtu		= addr->mtu[i];
	hashtabadd (ht_clients_ip, &c->addr[i]);
	if (host == addr->addr_in[i])
	    found_addr = 1;
    }
    if (!found_addr && i < AFS_MAX_INTERFACE_ADDR) {
	DIAGNOSTIC_CHECK_ADDR(&c->addr[i]);
	c->addr[i].c            = c;
	c->addr[i].addr_in      = host;
	c->addr[i].subnetmask   = 0xffffff00;
	c->addr[i].mtu          = ROPA_MTU;
	hashtabadd (ht_clients_ip, &c->addr[i]);
	i++;
    }
    c->numberOfInterfaces = i;
    for (; i < AFS_MAX_INTERFACE_ADDR; i++)
	clear_addr (&c->addr[i]);
}

/*
 * Initialize the client `c' with `host'/`port' (or use `addr' if that
 * is available). Add the client to hashtables.
 *
 * Note that this function can be called more then one time on the
 * same client to update the address/uuid information.
 */

static void
client_init (struct ropa_client *c, uint32_t host, uint16_t port,
	     afsUUID *uuid, interfaceAddr *addr)
{
    c->port = port;
    if (addr) {
	client_update_interfaces (c, host, port, addr);
    } else {
	int i;

	for (i = 0; i < c->numberOfInterfaces; i++) {
	    hashtabdel (ht_clients_ip, &c->addr[i]);
	    DIAGNOSTIC_CHECK_ADDR(&c->addr[i]);
	}

	c->numberOfInterfaces = 1;
	DIAGNOSTIC_CHECK_ADDR(&c->addr[0]);
	c->addr[0].c = c;
	c->addr[0].addr_in = host;
	c->addr[0].subnetmask = 0xffffff00;
	c->addr[0].mtu = ROPA_MTU;
	hashtabadd (ht_clients_ip, &c->addr[0]);
    }
    if (uuid) {
	c->uuid = *uuid;
	hashtabadd (ht_clients_uuid, c);
    }
}

/*
 * Free client `c' and remove from alla data-structures.
 */

static void
disconnect_client (struct ropa_client *c)
{
    int ret;
    int i;

    assert (c->ref == 0);

    if (c->li) {
	listdel (lru_clients, c->li);
	c->li = NULL;
    }

    for (i = 0 ; i < c->numberOfInterfaces ; i++) {
	int ret = hashtabdel (ht_clients_ip, &c->addr[i]);
	assert (ret == 0);
	clear_addr (&c->addr[i]);
    }
    c->numberOfInterfaces = 0;
    c->port = 0;
    c->state = ROPAC_FREE;
    
    ret = hashtabdel (ht_clients_uuid, c);
    assert (ret == 0);
    c->li = listaddtail (lru_clients, c);
}

/*
 *
 */

static void
client_deref (struct ropa_client *c)
{
    c->ref--;

    if (c->ref == 0)
	disconnect_client (c);
}

/*
 *
 */

static void
callback_deref (struct ropa_cb *cb)
{
    cb->ref--;

    mlog_log (MDEBROPA, "cb_deref: %x.%x.%x (%d)",
		cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique, cb->ref);

    if (cb->ref == 0) {
	int ret = hashtabdel (ht_callbacks, cb);

	mlog_log (MDEBROPA, "cb_deref: removing %x.%x.%x",
		cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique);

	assert (ret == 0);

	if (cb->li) 
	    listdel (lru_callback, cb->li);

	assert (listemptyp (cb->ccpairs));
	memset (&cb->fid, 0, sizeof(cb->fid));
	cb->li = listaddtail (lru_callback, cb);
    }
}

/*
 * 
 */

static struct ropa_ccpair *
add_client (struct ropa_cb *cb, struct ropa_client *c)
{
    struct timeval tv;
    struct ropa_ccpair cckey, *cc;

    assert (cb && c);

    cckey.client = c;
    cckey.cb = cb;
    
    cc = hashtabsearch (ht_ccpairs, &cckey);

    if (cc) {
	listdel (lru_ccpair, cc->li);
	cc->li = listaddhead (lru_ccpair, cc);
	return cc;
    }

    /* The reverse of these are in break_ccpair */
    callback_ref (cb);
    client_ref (c);

    cc = listdeltail (lru_ccpair);
    DIAGNOSTIC_CHECK_CCPAIR(cc);    
    cc->li = NULL;

    if (ccpairs_inuse_p (cc))
	break_ccpair (cc, TRUE);

    /* XXX  do it for real */
    gettimeofday(&tv, NULL);
    cc->expire = tv.tv_sec + 3600;
    
    heap_insert (heap_ccpairs, cc, &cc->heap);
    LWP_NoYieldSignal (heap_ccpairs);
    cc->cb_li = listaddtail (cb->ccpairs, cc);
    
    cc->client = c;
    cc->cb = cb;
    cc->li = listaddhead (lru_ccpair, cc);
    hashtabadd (ht_ccpairs, cc);

    mlog_log (MDEBROPA, "add_client: added %x to callback %x.%x.%x",
	    c->addr[0].addr_in, cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique);

    return cc;
}

/*
 *
 */

static void
uuid_init_simple (afsUUID *uuid, uint32_t host)
{
    uuid->node[0] = 0xff & host;
    uuid->node[1] = 0xff & (host >> 8);
    uuid->node[2] = 0xff & (host >> 16);
    uuid->node[3] = 0xff & (host >> 24);
    uuid->node[4] = 0xaa;
    uuid->node[5] = 0x77;
}

/*
 *
 */

static struct ropa_client *
client_query_notalkback (uint32_t host, uint16_t port)
{
    struct ropa_client ckey;
    struct ropa_addr *addr;
    ckey.numberOfInterfaces = 1;
    ckey.addr[0].c= &ckey;
    ckey.addr[0].addr_in = host;
    ckey.port = port;
    addr = hashtabsearch (ht_clients_ip, &ckey.addr[0]);
    if (addr) {
	assert (addr->c->numberOfInterfaces);
	return addr->c;
    }
    return NULL;
}

/*
 *
 */

static struct ropa_client *
obtain_client (void)
{
    struct ropa_client *c;

    c = listdeltail (lru_clients);
    DIAGNOSTIC_CHECK_CLIENT(c);
    c->li = NULL;
    if (client_inuse_p (c))
	break_client (c, TRUE);

    if (c->li) {
	listdel(lru_clients, c->li);
	c->li = NULL;
    }
    return c;
}

/*
 *
 */

static struct ropa_client *
client_query (uint32_t host, uint16_t port)
{
    struct ropa_client *c, *c_new;
    int ret;

    c = client_query_notalkback(host, port);
    if (c == NULL) {
	interfaceAddr remote;
	struct rx_connection *conn = NULL;

	c = obtain_client();
	assert (c->state == ROPAC_FREE && c->li == NULL);
	c->state = ROPAC_LOOKUP_U;
	c->flags |= ROPAF_LOOKUP;
	client_init (c, host, port, NULL, NULL);
	
	conn = rx_NewConnection (host, port, CM_SERVICE_ID,
				 rxnull_NewClientSecurityObject(),
				 0);
	if (conn == NULL) {
	    free(c);
	    return NULL;
	}
    retry:
	switch (c->state) {
	case ROPAC_DEAD:
	    c->li = listaddtail (lru_clients, c);
	    ret = ENETDOWN;
	    break;
	case ROPAC_LOOKUP_U:
	    ret = RXAFSCB_WhoAreYou (conn, &remote);
	    if (ret == RXGEN_OPCODE) {
		c->state = ROPAC_LOOKUP;
		goto retry;
	    } else if (ret == RX_CALL_DEAD) {
		c->state = ROPAC_DEAD;
		goto retry;
	    } else {
		struct ropa_client ckey;
		
		ckey.uuid = remote.uuid;
		c_new = hashtabsearch (ht_clients_uuid, &ckey);
		if (c_new == NULL) {
		    client_init (c, host, port, &remote.uuid, NULL);
		    ret = RXAFSCB_InitCallBackState3(conn, &server_uuid);
		} else {
		    client_update_interfaces (c_new, host, port, &remote);
		    disconnect_client (c);
		    c = c_new;
		    listdel(lru_clients, c->li);
		    c->li = NULL;
		}
	    }
	    break;
	case ROPAC_LOOKUP: {
	    afsUUID uuid;
	    ret = RXAFSCB_InitCallBackState(conn);
	    if (ret == RX_CALL_DEAD) {
		c->state = ROPAC_DEAD;
		goto retry;
	    }
	    uuid_init_simple (&uuid, host);
	    client_init (c, host, port, &uuid, NULL);
	    break;
	}
	default:
	     exit(-1);
	}
	
	rx_DestroyConnection (conn);
	
	if ((c->flags & ROPAF_WAITING) != 0)
	    LWP_NoYieldSignal (c);
	c->flags &= ~(ROPAF_LOOKUP|ROPAF_WAITING);

	if (ret) {
	    assert (c->li != NULL);
	    return NULL;
	}

	assert (c->li == NULL);
	c->li = listaddhead (lru_clients, c);

    } else { /* c != NULL */
	if ((c->flags & ROPAF_LOOKUP) != 0) {
	    c->flags |= ROPAF_WAITING;
	    LWP_WaitProcess (c);
	}
	assert (c->li != NULL);
    }

    return c;
}

/*
 *
 */

#if 0
static struct ropa_client *
uuid_query_simple (uint32_t host)
{
    struct ropa_client ckey;
    uuid_init_simple (&ckey.uuid, host);
    return hashtabsearch (ht_clients_uuid, &ckey);
}
#endif

/*
 * Update `callback' of `type' to expire at `time'.
 */

static void
update_callback_time (int32_t time, AFSCallBack *callback, int32_t type)
{
    callback->CallBackVersion = CALLBACK_VERSION;
    callback->ExpirationTime = time;
    callback->CallBackType = type;
}

/*
 * Update the `callback' of `type' from `cc'.
 */

static void
update_callback (struct ropa_ccpair *cc, AFSCallBack *callback, int32_t type)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    update_callback_time (cc->expire - tv.tv_sec, callback, type);
}
    

/*
 * ropa_getcallback will obtain a callback for the socketpair
 * `host'/`port' and `fid', then result is returned in `callback'.
 */

int
ropa_getcallback (uint32_t host, uint16_t port, const struct AFSFid *fid,
		  AFSCallBack *callback, int32_t voltype)
{
    struct ropa_client *c;
    struct ropa_cb cbkey, *cb;
    struct ropa_ccpair *cc ;
    struct AFSFid callback_fid;

    debug_print_callbacks();

    c = client_query (host, port);
    if (c == NULL) {
	mlog_log (MDEBROPA, "ropa_getcallback: didn't find client %x/%d",
		  host, port);
	update_callback_time (DEFAULT_TIMEOUT, callback, CBSHARED);
	return 0;
    }

    /*
     * At this point the client should be firmly set
     * in the ropa client database.
     */

#if 0
    if (c->have_outstanding_callbacks)
	break_outstanding_callbacks (c);
#endif

    if (voltype == RWVOL) {
	callback_fid = *fid;
    } else {
	callback_fid.Volume = fid->Volume;
	callback_fid.Vnode = 0;
	callback_fid.Unique = 0;
    }

    cbkey.fid = callback_fid;

    cb = hashtabsearch (ht_callbacks, &cbkey);
    if (cb == NULL) {
	cb = listdeltail (lru_callback);
	DIAGNOSTIC_CHECK_CALLBACK(cb);
	cb->li = NULL;
	if (callback_inuse_p (cb)) {
	    break_callback (cb, NULL, FALSE);
	    callback_ref(cb);
	} else {
	    callback_ref(cb);
	    cb->li = listaddhead (lru_callback, cb);
	}
	cb->fid = callback_fid;
	hashtabadd (ht_callbacks, cb);

	mlog_log (MDEBROPA, "ropa_getcallback: added callback %x.%x.%x:%x",
		  callback_fid.Volume, callback_fid.Vnode,
		  callback_fid.Unique, host);
    } else {
	mlog_log (MDEBROPA, "ropa_getcallback: found callback %x.%x.%x:%x",
		  callback_fid.Volume, callback_fid.Vnode,
		  callback_fid.Unique, host);
	callback_ref(cb);
    }

    cc = add_client (cb, c);

    callback_deref (cb);

    update_callback (cc, callback, CBSHARED);

    debug_print_callbacks();

    return 0;
}

/*
 * Notify client
 */

static int
notify_client (struct ropa_client *c, AFSCBFids *fids, AFSCBs *cbs)
{
#ifdef NO_CALLBACKS
    return 0;
#else
    int i, ret;
    if (c->conn) {
	ret = RXAFSCB_CallBack (c->conn, fids, cbs);
	if (ret == 0)
	    return ret;
    }
    for (i = 0; i < c->numberOfInterfaces ; i++) {
	uint16_t port = c->port;
	DIAGNOSTIC_CHECK_ADDR(&c->addr[i]);

	c->conn = rx_NewConnection (c->addr[i].addr_in,
				 port,
				 CM_SERVICE_ID,
				 rxnull_NewClientSecurityObject(),
				 0);
	mlog_log (MDEBROPA, "notify_client: notifying %x", c->addr[i].addr_in);

	ret = RXAFSCB_CallBack (c->conn, fids, cbs);
	if (ret)
	    rx_DestroyConnection (c->conn);
	else
	    break;

	/* XXX warn */
    }

    return ret;
#endif
}

/*
 * Break the callback `cc' that that clients holds.  The client doesn't
 * need to be removed from the callbacks list of ccpairs before this
 * function is called.
 */

static void
break_ccpair (struct ropa_ccpair *cc, Bool notify_clientp)
{
    AFSCBFids fids;
    AFSCBs cbs;
    int ret;

    debug_print_callbacks();

    cc->expire = 0;

    if (cc->li)
	listdel (lru_ccpair, cc->li);

    if (notify_clientp) {
	fids.len = 1;
	fids.val = &cc->cb->fid;
	cbs.len = 0;
	notify_client (cc->client, &fids, &cbs);
    }

    if (cc->cb_li) {
	listdel (cc->cb->ccpairs, cc->cb_li);
	cc->cb_li = NULL;
    }

    /* The reverse of these are in add_client */
    ret = hashtabdel (ht_ccpairs, cc);
    assert (ret == 0);
    client_deref (cc->client);
    cc->client = NULL;
    callback_deref (cc->cb);
    cc->cb = NULL;

    heap_remove (heap_ccpairs, cc->heap);
    
    cc->li = listaddtail (lru_ccpair, cc);

    debug_print_callbacks();
}

static int
add_to_cb (AFSFid *fid, AFSCallBack *cb, AFSCBFids *fids, AFSCBs *cbs)
{
    AFSFid      *n_fid;
    AFSCallBack *n_cb;

    n_fid = realloc (fids->val, fids->len + 1);
    if (n_fid == NULL)
	return ENOMEM;
    fids->val = n_fid;
    n_cb  = realloc (cbs->val, cbs->len + 1);
    if (n_cb == NULL)
	return ENOMEM;
    cbs->val = n_cb;
    fids->val[fids->len] = *fid;
    cbs->val[cbs->len] = *cb;
    ++fids->len;
    ++cbs->len;
    return 0;
}

static void
break_ccpairs (struct ropa_client *c, Bool notify_clientp)
{
    AFSCBFids fids;
    AFSCBs    cbs;
    struct ropa_ccpair *cc;
    AFSCallBack callback;

    DIAGNOSTIC_CHECK_CLIENT(c);

    fids.val = NULL;
    fids.len = 0;

    cbs.val = NULL;
    cbs.len = 0;

    while ((cc = listdeltail (c->callbacks)) != NULL) {
	DIAGNOSTIC_CHECK_CCPAIR(cc);
	update_callback (cc, &callback, CBDROPPED);
	add_to_cb (&cc->cb->fid, &callback, &fids, &cbs);
	break_ccpair (cc, FALSE);
    }
    
    if (notify_clientp)
	notify_client (c, &fids, &cbs);
}


/*
 * break the callback and remove it from the chain of clients.
 * it the clients responsible for the callback is the caller
 * don't break the callback.
 */

static void
break_callback (struct ropa_cb *cb, struct ropa_client *caller, Bool break_own)
{
    struct ropa_ccpair *cc;
    struct ropa_ccpair *own_cc = NULL;

    assert (cb);
    assert (cb->ccpairs);

    mlog_log (MDEBROPA, "break_callback: breaking callback %x.%x.%x (%d)",
	    cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique, break_own);

    callback_ref (cb);
    if (caller)
	client_ref (caller);

    while ((cc = listdelhead (cb->ccpairs)) != 0) {
	assert (cc->cb == cb);
	cc->cb_li = NULL;
	if (break_own)
	    break_ccpair (cc, cc->client != caller);
	else
	    if (cc->client == caller)
		own_cc = cc;
	    else
		break_ccpair (cc, TRUE);
    }

    if (own_cc)
	own_cc->cb_li = listaddhead (cb->ccpairs, own_cc);

    callback_deref (cb);
    if (caller)
	client_deref (caller);
}

static int
break_fid (const struct AFSFid *fid, struct ropa_client *c,
	   Bool break_own)
{
    struct ropa_cb cbkey, *cb;

    cbkey.fid = *fid;
    
    cb = hashtabsearch (ht_callbacks, &cbkey);
    if (cb == NULL) {
	return -1;
    }

    break_callback (cb, c, break_own);

    return 0;
}

/*
 *
 */

void
ropa_break_callback (uint32_t addr, uint16_t port,
		     const struct AFSFid *fid, Bool break_own)
{
    struct ropa_client *c = NULL;
    
    debug_print_callbacks();
    
    c = client_query_notalkback (addr, port);
    if (c == NULL) {
	mlog_log (MDEBROPA, "ropa_break_callback: didn't find client %x/%d",
		  addr, addr);
 	return; /* XXX */
    }

    if (break_fid(fid, c, break_own)) {
	mlog_log (MDEBROPA, "ropa_break_callback: "
		  "didn't find callback %x.%x.%x:%x/%d",
		  fid->Volume, fid->Vnode, fid->Unique, addr, port);
    }

    debug_print_callbacks();
}

void
ropa_break_volume_callback(int32_t volume)
{
    struct AFSFid fid;

    fid.Volume = volume;
    fid.Vnode = 0;
    fid.Unique = 0;

    break_fid(&fid, NULL, FALSE);
}

/*
 * ropa_drop_callbacks
 */

int
ropa_drop_callbacks (uint32_t addr, uint16_t port,
		     const AFSCBFids *a_cbfids_p, const AFSCBs *a_cbs_p)
{
    struct ropa_client *c;
    struct ropa_cb cbkey, *cb;
    struct ropa_ccpair cckey, *cc;
    int i; 
    
    debug_print_callbacks();

    if (a_cbfids_p->len > AFSCBMAX)
	return EINVAL;

    c = client_query (addr, port);
    if (c == NULL) {
	mlog_log (MDEBROPA, "ropa_drop_callbacks: didn't find client %x/%d",
		  addr, port);
	return 0;
    }

    for (i = 0; i < a_cbfids_p->len; i++) {
	cbkey.fid = a_cbfids_p->val[i];
	
	cb = hashtabsearch (ht_callbacks, &cbkey);
	if (cb != NULL) {
	    cckey.client = c;
	    cckey.cb = cb;
	    
	    cc = hashtabsearch (ht_ccpairs, &cckey);
	    if (cc != NULL) {
		mlog_log (MDEBROPA, "ropa_drop: dropping %x.%x.%x:%x/%d",
			  cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique, 
			  addr, port);
		break_ccpair (cc, FALSE);
	    }
	}
    }
    
    debug_print_callbacks();

    return 0;
}


/*
 *
 */

static void
break_client (struct ropa_client *c, Bool notify_clientp)
{
    assert (c);
    
    client_ref (c);
    break_ccpairs (c, notify_clientp);
    client_deref (c);
}

void
ropa_break_client (uint32_t host, uint16_t port)
{
    struct ropa_client *c;

    c = client_query (host, port);
    if (c == NULL) {
	/* XXX warn */
	return;
    }

    break_client (c, TRUE);
}

/*
 *
 */

static void
heapcleaner (char *arg)
{
    const void *head;
    struct timeval tv;

    while (1) {

	while ((head = heap_head (heap_ccpairs)) == NULL)
	    LWP_WaitProcess (heap_ccpairs);
	
	while ((head = heap_head (heap_ccpairs)) != NULL) {
	    struct ropa_ccpair *cc = (struct ropa_ccpair *)head;
	    
	    gettimeofday (&tv, NULL);
	    
	    if (tv.tv_sec < cc->expire) {
		unsigned long t = cc->expire - tv.tv_sec;
		IOMGR_Sleep (t);
	    } else {
/* XXX should this be fixed?
		listdel (cc->cb->ccpairs, cc->cb_li);
		cc->cb_li = NULL; */
		break_ccpair (cc, TRUE); /* will remove it from the heap */
	    }
	}
    }
}


/*
 * init the ropa-module
 */

int
ropa_init (unsigned num_cb, unsigned num_cli, unsigned num_cc,
	   unsigned hashsz_cb, unsigned hashsz_cli, unsigned hashsz_cc)
{
    ht_callbacks = hashtabnew (hashsz_cb, callbacks_cmp, callbacks_hash);
    if (ht_callbacks == NULL)
	errx (1, "ropa_init: failed to create hashtable for callbacks");

    ht_clients_ip = hashtabnew (hashsz_cli, clients_cmp_ip, clients_hash_ip);
    if (ht_clients_ip == NULL)
	errx (1, "ropa_init: failed to create hashtable for clients_ip");
	
    ht_clients_uuid = hashtabnew (hashsz_cli, clients_cmp_uuid,
				  clients_hash_uuid);
    if (ht_clients_uuid == NULL)
	errx (1, "ropa_init: failed to create hashtable for clients_uuid");

    ht_ccpairs = hashtabnew (hashsz_cc, ccpairs_cmp,
				  ccpairs_hash);
    if (ht_ccpairs == NULL)
	errx (1, "ropa_init: failed to create hashtable for ccpairs");
	
    lru_clients = listnew ();
    if (lru_clients == NULL)
	errx (1, "ropa_init: failed to create lru for clients");
	
    lru_ccpair = listnew ();
    if (lru_ccpair == NULL)
	errx (1, "ropa_init: failed to create list for ccpairs");

    lru_callback = listnew ();
    if (lru_callback == NULL)
	errx (1, "ropa_init: failed to create list for callback");

    heap_ccpairs = heap_new (num_cc, ccpair_cmp_time);
    if (heap_ccpairs == NULL)
	errx (1, "ropa_init: failed to create heap for ccpairs");

    create_clients(num_cli);
    create_callbacks(num_cb);
    create_ccpairs(num_cc);

    uuid_init_simple (&server_uuid, 0x82ED305E);

    if (LWP_CreateProcess (heapcleaner, ROPA_STACKSIZE, 1,
			   NULL, "heap-invalidator", &cleaner_pid))
	errx (1, "ropa_init: LWP_CreateProcess failed");

    debuglevel = mlog_log_get_level_num(); /* XXX */

    return 0;
}

static Bool
print_callback_sub (void *ptr, void *arg)
{
    struct ropa_cb *cb = (struct ropa_cb *)ptr;
    mlog_log (MDEBROPA, "\tfound %x.%x.%x (ref=%d)",
	    cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique, cb->ref);
    return FALSE;
}

static void
debug_print_callbacks (void)
{
    if (debuglevel & MDEBROPA) {
	mlog_log (MDEBROPA, "---callbacks left are---");
	
	hashtabforeach (ht_callbacks, print_callback_sub, NULL);
    }
}
