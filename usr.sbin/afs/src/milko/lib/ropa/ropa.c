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

#include <err.h>

#include <mlog.h>
#include <mdebug.h>

#include <ropa.h>

RCSID("$Id: ropa.c,v 1.1 2000/09/11 14:41:16 art Exp $");

#ifndef DIAGNOSTIC
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


/*
 * Declarations
 */

#define ROPA_STACKSIZE (16*1024)

struct ropa_client;

struct ropa_addr {
#ifdef DIAGNOSTIC
    int magic;				/* magic */
#endif
    struct ropa_client *c;		/* pointer to head */
    u_int32_t   addr_in;		/* */
    u_int32_t 	subnetmask;		/* */
    int		mtu;			/* */
};

struct ropa_client {
#ifdef DIAGNOSTIC
    int magic;				/* magic */
#endif
    int			numberOfInterfaces;
    afsUUID		uuid;
    struct ropa_addr	addr[AFS_MAX_INTERFACE_ADDR];
    u_int16_t 		port;		/* port of client in network byte order */
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
    int32_t		version;	/* version of this callback */
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
static void create_callbacks (void);
static void create_ccpairs (void);
static int uuid_magic_eq (afsUUID *uuid1, afsUUID *uuid2);
static void debug_print_callbacks(void);

/*
 * Module variables
 */

#define NUM_CLIENTS	100
#define NUM_CALLBACKS	300
#define NUM_CCPAIR	600

#define ROPA_MTU       1500

/*
 *
 */

static Hashtab *ht_clients_ip = NULL;
static Hashtab *ht_clients_uuid = NULL;
static Hashtab *ht_callbacks = NULL;
/* least recently used on tail */
static List *lru_clients = NULL;
static List *lru_ccpair = NULL;
static List *lru_callback = NULL;
static unsigned long num_clients = 0;
static unsigned long num_callbacks = 0;
static unsigned long num_ccpair = 0;

static Heap *heap_ccpairs = NULL;
static PROCESS cleaner_pid;

static unsigned debuglevel;

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
ccpair_cmp (const void *p1, const void *p2)
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
    return c->port == 0 ? FALSE : TRUE ;
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
create_clients (void)
{
    struct ropa_client *c;
    unsigned long i;

    c = malloc (NUM_CLIENTS * sizeof (*c));
    if (c == NULL)
	err (1, "create_clients: malloc");
    memset (c, 0, NUM_CLIENTS * sizeof (*c));

    for (i = 0 ; i < NUM_CLIENTS; i++) {
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
	c[i].li = listaddtail (lru_clients, &c[i]);
    }
    num_clients += NUM_CLIENTS;
}

/*
 *
 */

static void
create_callbacks (void)
{
    struct ropa_cb *c;
    int i;

    c = malloc (NUM_CALLBACKS * sizeof (*c));
    if (c == NULL)
	err (1, "create_callbacks: malloc");
    memset (c, 0, NUM_CALLBACKS * sizeof (*c));

    for (i = 0; i < NUM_CALLBACKS ; i++) {
#ifdef DIAGNOSTIC
	c[i].magic = DIAGNOSTIC_CALLBACK;
#endif
	c[i].ccpairs = listnew();
	if (c[i].ccpairs == NULL)
	    err (1, "create_callbacks: listnew");
	c[i].li = listaddtail (lru_callback, &c[i]);
    }
    num_callbacks += NUM_CALLBACKS;
}

/*
 *
 */

void
create_ccpairs (void) 
{
    struct ropa_ccpair *c;
    int i;

    c = malloc (NUM_CCPAIR * sizeof (*c));
    if (c == NULL)
	err (1, "create_ccpairs: malloc");
    memset (c, 0, NUM_CCPAIR * sizeof (*c));

    for (i = 0; i < NUM_CCPAIR ; i++) {
#ifdef DIAGNOSTIC
	c[i].magic = DIAGNOSTIC_CCPAIR;
#endif
	c[i].li = listaddtail (lru_ccpair, &c[i]);
    }
    num_ccpair += NUM_CCPAIR;
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
 *
 */

static void
client_deref (struct ropa_client *c)
{
    int i, ret;

    c->ref--;

    if (c->ref == 0) {
	for (i = 0 ; i < c->numberOfInterfaces ; i++) {
	    int ret = hashtabdel (ht_clients_ip, &c->addr[i]);
	    assert (ret == 0);
	    clear_addr (&c->addr[i]);
	}
	c->numberOfInterfaces = 0;
	c->port = 0;

	ret = hashtabdel (ht_clients_uuid, c);
	assert (ret == 0);
	listdel (lru_clients, c->li);
	c->li = listaddtail (lru_clients, c);
    }
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

struct find_client_s {
    struct ropa_ccpair *cc;
    struct ropa_client *c;
};

static Bool
find_client (List *list, Listitem *li, void *arg)
{
    struct find_client_s *fc = (struct find_client_s *)arg;
    struct ropa_ccpair *cc = listdata (li);

    mlog_log (MDEBROPA, "\tclient fc->c = 0x%x cc = 0x%x cc->client = 0x%d",
	      fc->c, cc, cc == NULL ? 0 : cc->client); 
    if (cc == NULL)
	return FALSE;
    if (cc->client == fc->c) {
	fc->cc = cc;
	return TRUE;
    }
    return FALSE;
}

static struct ropa_ccpair *
add_client (struct ropa_cb *cb, struct ropa_client *c)
{
    struct timeval tv;
    struct ropa_ccpair *cc;
    struct find_client_s fc;

    assert (cb && c);

    fc.c = c;
    fc.cc = NULL;
    listiter (cb->ccpairs, find_client, &fc);
    if (fc.cc) {
	listdel (lru_ccpair, fc.cc->li);
	fc.cc->li = listaddhead (lru_ccpair, fc.cc);
	return fc.cc;
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
    cc->version += 1;
    
    heap_insert (heap_ccpairs, cc, &cc->heap);
    LWP_NoYieldSignal (heap_ccpairs);
    cc->cb_li = listaddtail (cb->ccpairs, cc);
    
    cc->client = c;
    cc->cb = cb;
    cc->li = listaddhead (lru_ccpair, cc);

    mlog_log (MDEBROPA, "add_client: added %x to callback %x.%x.%x",
	    c->addr[0].addr_in, cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique);

    return cc;
}

/*
 *
 */

static void
uuid_init_simple (afsUUID *uuid, u_int32_t host)
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
client_query (u_int32_t host, u_int16_t port)
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

#if 0
static struct ropa_client *
uuid_query_simple (u_int32_t host)
{
    struct ropa_client ckey;
    uuid_init_simple (&ckey.uuid, host);
    return hashtabsearch (ht_clients_uuid, &ckey);
}
#endif

/*
 *
 */

static void
client_update_interfaces (struct ropa_client *c, 
                         u_int32_t host, interfaceAddr *addr)
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
 *
 */

static void
client_init (struct ropa_client *c, u_int32_t host, u_int16_t port,
	     afsUUID *uuid, interfaceAddr *addr)
{
    assert (c->numberOfInterfaces == 0);

    c->ref = 0;
    c->port = port;
    if (addr) {
	client_update_interfaces (c, host, addr);
    } else {
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
    } else {
	uuid_init_simple (&c->uuid, host);
    }
    hashtabadd (ht_clients_uuid, c);

}

/*
 * XXX race
 */

int
ropa_getcallback (u_int32_t host, u_int16_t port, const struct AFSFid *fid,
		  AFSCallBack *callback)
{
    struct ropa_client *c;
    struct ropa_cb cbkey, *cb;
    struct ropa_ccpair *cc ;
    int ret;

    debug_print_callbacks();

    c = client_query (host, port);
    if (c == NULL) {
	interfaceAddr remote;
	struct rx_connection *conn;
	
	conn = rx_NewConnection (host, port, CM_SERVICE_ID,
				 rxnull_NewClientSecurityObject(),
				 0);
	
	ret = RXAFSCB_WhoAreYou (conn, &remote);
    
	/* XXX race, entry can be found, and inserted by other thread */

	if (ret == RX_CALL_DEAD) {
	    rx_DestroyConnection (conn);
	    return ENETDOWN;
	} else if (ret == RXGEN_OPCODE) {
	    /*
	     * This is an new client that doen't support  WhoAreYou.
	     * Lets add it after a InitCallbackState
	     */

	    ret = RXAFSCB_InitCallBackState(conn);
	    /* XXX race, entry can be found, and inserted by other thread */
	    rx_DestroyConnection (conn);
	    if (ret)
		return ret;

	    c = listdeltail (lru_clients);
	    DIAGNOSTIC_CHECK_CLIENT(c);
	    c->li = NULL;
	    if (client_inuse_p (c))
		break_client (c, TRUE);
	    /* XXX race, entry can be found, and inserted by other thread */

	    client_init (c, host, port, NULL, NULL);

	    c->li = listaddhead (lru_clients, c);

	} else if (ret == 0) {
	    struct ropa_client ckey;
	    ckey.uuid = remote.uuid;
	    c = hashtabsearch (ht_clients_uuid, &ckey);
	    if (c == NULL) {
		afsUUID uuid;

		/* 
		 * This is a new clint that support WhoAreYou
		 * Lets add it after a InitCallbackState3.
		 */

		c = listdeltail (lru_clients);
		DIAGNOSTIC_CHECK_CLIENT(c);
		if (client_inuse_p (c))
		    break_client (c, TRUE);
		/* XXX race, entry can be found, and inserted by other thread */

		ret = RXAFSCB_InitCallBackState3 (conn, &uuid);
		rx_DestroyConnection (conn);
		if (ret != 0) {
		    c->li = listaddtail (lru_clients, c);
		    return ENETDOWN;
		}
		/* XXX race, entry can be found, and inserted by other thread */
		/* XXX check uuid */

		client_init (c, host, port, &remote.uuid, &remote);
		c->li = listaddhead (lru_clients, c);

		mlog_log (MDEBROPA, "ropa_getcb: new client %x:%x", c, host);
	    } else {
		/*
		 * We didn't find the client in the ip-hash, but the
		 * uuid hash, it have changed addresses. XXX If it's a bad
		 * client, break outstanding callbacks.
		 */
		
		client_update_interfaces (c, host, &remote);

		mlog_log (MDEBROPA, "ropa_getcb: updated %x", c);

#if 0
		if (c->have_outstanding_callbacks)
		    break_outstanding_callbacks (c);
#endif
	    }
	} else {
	    return ENETDOWN; /* XXX some unknown error */
	}
    }

    cbkey.fid = *fid;

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
	cb->fid = *fid;
	hashtabadd (ht_callbacks, cb);

	mlog_log (MDEBROPA, "ropa_getcallback: added callback %x.%x.%x:%x",
		fid->Volume, fid->Vnode, fid->Unique, host);
    } else {
	mlog_log (MDEBROPA, "ropa_getcallback: found callback %x.%x.%x:%x",
		fid->Volume, fid->Vnode, fid->Unique, host);
	callback_ref(cb);
    }

    cc = add_client (cb, c);
    if (cc == NULL)
	abort();

    callback_deref (cb);

    callback->CallBackVersion = cc->version;
    callback->ExpirationTime = cc->expire;
    callback->CallBackType = CBSHARED;

    debug_print_callbacks();

    return 0;
}

/*
 * Notify client
 */

static int
notify_client (struct ropa_client *c, AFSCBFids *fids, AFSCBs *cbs)
{
    struct rx_connection *conn;
    u_int16_t port = c->port;
    int i, ret;

    for (i = 0; i < c->numberOfInterfaces ; i++) {
	DIAGNOSTIC_CHECK_ADDR(&c->addr[i]);
	conn = rx_NewConnection (c->addr[i].addr_in,
				 port,
				 CM_SERVICE_ID,
				 rxnull_NewClientSecurityObject(),
				 0);
	mlog_log (MDEBROPA, "notify_client: notifying %x", c->addr[i].addr_in);

	ret = RXAFSCB_CallBack (conn, fids, cbs);
	rx_DestroyConnection (conn);
	if (ret == 0) 
	    break;

	/* XXX warn */
    }

    return ret;
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

    debug_print_callbacks();

    cc->expire = 0;

    if (cc->li)
	listdel (lru_ccpair, cc->li);

    if (notify_clientp) {
	AFSCallBack callback;
	callback.CallBackVersion = cc->version;
	callback.ExpirationTime  = cc->expire;
	callback.CallBackType    = CBDROPPED;

	fids.len = 1;
	fids.val = &cc->cb->fid;
	cbs.len = 1;
	cbs.val = &callback;
	notify_client (cc->client, &fids, &cbs);
    }

    if (cc->cb_li) {
	listdel (cc->cb->ccpairs, cc->cb_li);
	cc->cb_li = NULL;
    }

    /* The reverse of these are in add_client */
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

    callback.CallBackType = CBDROPPED;
    while ((cc = listdeltail (c->callbacks)) != NULL) {
	DIAGNOSTIC_CHECK_CCPAIR(cc);
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

/*
 *
 */

void
ropa_break_callback (u_int32_t addr, u_int16_t port,
		     const struct AFSFid *fid, Bool break_own)
{
    struct ropa_client *c = NULL;
    struct ropa_cb cbkey, *cb;

    debug_print_callbacks();

    c = client_query (addr, port);
    if (c == NULL) {
	/* XXX warn */
	mlog_log (MDEBROPA, "ropa_break_callback: didn't find client %x", addr);
/* 	return;
	XXX really no need to return, right? */
    }

    cbkey.fid = *fid;
    
    cb = hashtabsearch (ht_callbacks, &cbkey);
    if (cb == NULL) {
	/* XXX warn */
	mlog_log (MDEBROPA, "ropa_break_callback: didn't find callback %x.%x.%x:%x",
		fid->Volume, fid->Vnode, fid->Unique, addr);
	return;
    }

    break_callback (cb, c, break_own);

    debug_print_callbacks();
}

/*
 * ropa_drop_callbacks
 */

int
ropa_drop_callbacks (u_int32_t addr, u_int16_t port,
		     const AFSCBFids *a_cbfids_p, const AFSCBs *a_cbs_p)
{
    struct ropa_client *c;
    struct ropa_cb cbkey, *cb;
    struct find_client_s fc;
    int i; 
    
    debug_print_callbacks();

    if (a_cbfids_p->len > AFSCBMAX) {
/*	|| a_cbfids_p->len > a_cbs_p->len) */
	abort();
	return EINVAL;
    }

    c = client_query (addr, port);
    if (c == NULL) {
	/* XXX warn */
	return EINVAL;
    }

    for (i = 0; i < a_cbfids_p->len; i++) {
	cbkey.fid = a_cbfids_p->val[i];
	
	cb = hashtabsearch (ht_callbacks, &cbkey);
	if (cb == NULL) {
	    /* XXX warn */
/*	    return EINVAL; not necessary? */
	} else {
	    /* XXX check version */
	    
	    fc.c = c;
	    fc.cc = NULL;
	    listiter (cb->ccpairs, find_client, &fc);
	    if (fc.cc == NULL) {
		/* XXX warn */
/*	    return EINVAL; not necessary? */
	    } else {
		mlog_log (MDEBROPA, "ropa_drop: dropping %x.%x.%x:%x",
			cb->fid.Volume, cb->fid.Vnode, cb->fid.Unique, addr);
		break_ccpair (fc.cc, FALSE);
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
ropa_break_client (u_int32_t host, u_int16_t port)
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
ropa_init (unsigned long num_callback, unsigned long num_clients)
{
    ht_callbacks = hashtabnew (num_callback, callbacks_cmp, callbacks_hash);
    if (ht_callbacks == NULL)
	errx (1, "ropa_init: failed to create hashtable for callbacks");

    ht_clients_ip = hashtabnew (num_clients, clients_cmp_ip, clients_hash_ip);
    if (ht_clients_ip == NULL)
	errx (1, "ropa_init: failed to create hashtable for clients_ip");
	
    ht_clients_uuid = hashtabnew (num_clients, clients_cmp_uuid,
				  clients_hash_uuid);
    if (ht_clients_uuid == NULL)
	errx (1, "ropa_init: failed to create hashtable for clients_uuid");
	
    lru_clients = listnew ();
    if (lru_clients == NULL)
	errx (1, "ropa_init: failed to create lru for clients");
	
    lru_ccpair = listnew ();
    if (lru_ccpair == NULL)
	errx (1, "ropa_init: failed to create list for ccpairs");

    lru_callback = listnew ();
    if (lru_callback == NULL)
	errx (1, "ropa_init: failed to create list for callback");

    heap_ccpairs = heap_new (NUM_CCPAIR, ccpair_cmp);
    if (heap_ccpairs == NULL)
	errx (1, "ropa_init: failed to create heap for ccpairs");

    create_clients();
    create_callbacks();
    create_ccpairs();

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
