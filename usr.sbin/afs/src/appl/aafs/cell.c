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

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <netdb.h>

#include <aafs/aafs_cell.h>
#include <aafs/aafs_conn.h>
#include <aafs/aafs_private.h>
#include <ko.h>
#include <fs.h>
#include <ubik.h>
#include <afs_uuid.h>
#include <ports.h>

#include <log.h>

#include <roken.h>

struct aafs_cell {
    struct aafs_object obj;
    char *name;
    int last_good_server;
    struct aafs_security *sec;
};

/* a site is a server/partition combination */ 
struct aafs_site {
    struct aafs_object	obj;
    struct aafs_server	*server;
    aafs_partition	part;
};

struct aafs_site_list {
    int numsl;
    struct aafs_site *sl;
};

struct aafs_site_ctx {
    int current;
    struct aafs_site_list *list;
};

Log_method *aafs_log_method = NULL;


int
aafs_init(Log_method *log_method)
{
    if (log_method == NULL)
	log_method = log_open(getprogname(), "/dev/stdout");

    aafs_log_method = log_method;
    
    cell_init(4711, aafs_log_method);
    
    rx_Init(0);
    ports_init();

    return 0;
}

static void
cell_destruct(void *ptr, const char *name)
{
    struct aafs_cell *c = ptr;
    if (c->name)
	free(c->name);
    if (c->sec)
	aafs_object_unref(c->sec, "sec");
}

int
aafs_cell_create(const char *cellname,
		 aafs_sec_type sec_type,
		 struct aafs_cell **cell)
{
    struct aafs_security *s;
    struct aafs_cell *c;
    cell_entry *e;
    
    *cell = NULL;
    
    if ((s = aafs_security_create(sec_type, cellname)) == NULL)
	return ENOENT;

    if ((e = cell_get_by_name(cellname)) == NULL) {
	aafs_object_unref(s, "sec");
	return ENOENT;
    }
    
    c = aafs_object_create(sizeof(*c), "cell", cell_destruct);
    
    if ((c->name = strdup(cellname)) == NULL) {
	aafs_object_unref(c, "cell");
	return ENOMEM;
    }
    
    if (cell_get_by_name(c->name) == NULL) {
	aafs_object_unref(c, "cell");
	return ENOENT;
    }

    c->sec = s;

    *cell = c;
    
    return 0;
}

int
aafs_cell_create_secobj(const char *cell,
			struct aafs_security *, 
			struct aafs_cell **);


int
aafs_cell_rx_secclass(struct aafs_cell *cell,
		      void *secobj, int *secidx)
{
    return aafs_security_rx_secclass(cell->sec, secobj, secidx);
}


void *
aafs_cell_ref(struct aafs_cell *cell)
{
    return aafs_object_ref(&cell->obj, "cell");
}

void
aafs_cell_unref(struct aafs_cell *cell)
{
    aafs_object_unref(cell, "cell");
}

static void
server_destruct(void *ptr, const char *name)
{
    struct aafs_server *s = ptr;
    if (s->cell)
	aafs_cell_unref(s->cell);
    if (s->addr)
	free(s->addr);
}


int
aafs_server_create_by_name(struct aafs_cell *cell,
			   const char *servername,
			   struct aafs_server **ret_server)
{
    struct aafs_server *server;
    struct addrinfo *res, *res0, hints;
    int ret, num_addr;

    *ret_server = NULL;
    server = aafs_object_create(sizeof(*server), "server", server_destruct);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = 0;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    server->cell = aafs_cell_ref(cell);

    ret = getaddrinfo(servername, NULL, &hints, &res0);
    if (ret) {
	aafs_object_unref(server, "server");
	return ret;
    }

    server->addr = NULL;

    for (res = res0, num_addr = 0; res; res = res->ai_next, num_addr++) {
	server->addr = erealloc(server->addr, 
				(num_addr+1) * sizeof(struct site_addr));
	memcpy(&server->addr[num_addr].addr, res->ai_addr, res->ai_addrlen);
	server->addr[num_addr].addrlen = res->ai_addrlen;
	num_addr += 1;
    }
    server->num_addr = num_addr;

    freeaddrinfo(res0);

    server->flags = SERVER_HAVE_ADDR;

    *ret_server = server;
    return 0;
}

int
aafs_server_create_by_long(struct aafs_cell *cell,
			   uint32_t server_number,
			   struct aafs_server **ret_server)
{
    struct aafs_server *server;
    struct sockaddr_in *sin;

    *ret_server = NULL;
    server = aafs_object_create(sizeof(*server), "server", server_destruct);
    
    server->cell = aafs_cell_ref(cell);

    server->addr = malloc(sizeof(struct site_addr));

    server->addr[0].addrlen = sizeof(*sin);

    sin = (struct sockaddr_in *)&server->addr[0].addr;
    memset(sin, 0, sizeof(*sin));

    sin->sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sin->sin_len = sizeof(*sin);
#endif
    sin->sin_addr.s_addr = server_number;

    server->num_addr = 1;

    server->flags = SERVER_HAVE_ADDR;

    *ret_server = server;
    return 0;
}

char *
aafs_server_get_name(struct aafs_server *server, char *buf, size_t sz)
{
    int i, j, ret;
    int flags[] = { 0 , NI_NUMERICSERV };

    for (j = 0; j < sizeof(flags)/sizeof(flags[0]); j++) {
	for (i = 0; i < server->num_addr; i++) {
	    ret = getnameinfo((struct sockaddr *)&server->addr[i].addr,
			      server->addr[i].addrlen,
			      buf, sz, NULL, 0, flags[j]);
	    if (ret == 0)
		break;
	}
	if (ret == 0)
	    break;
    }
    if (i == server->num_addr)
	snprintf(buf, sz, "<failed to find addr>");
    return buf;
}

int
aafs_server_get_long(struct aafs_server *server,
		     uint32_t *server_number)
{
    struct sockaddr_in *sin;
    int i;

    *server_number = 0;

    if ((server->flags & SERVER_HAVE_ADDR) == 0)
	return ENOENT;

    for (i = 0; i < server->num_addr; i++) {
	sin = (struct sockaddr_in *)&server->addr[0].addr;
	if (sin->sin_family != AF_INET)
	    break;
	*server_number = sin->sin_addr.s_addr;
	break;
    }
    
    if (i == server->num_addr)
	return ENOENT;
    
    return 0;
}

void
aafs_server_free(struct aafs_server *s)
{
    aafs_object_unref(s, "server");
}

/* site commands */

static void
site_destruct(void *ptr, const char *name)
{
    struct aafs_site *s = ptr;
    aafs_object_unref(s->server, "server");
}

int
aafs_site_create(struct aafs_cell *cell, 
		 struct aafs_server *server,
		 aafs_partition partition,
		 struct aafs_site **site)
{
    struct aafs_site *s;

    *site = NULL;

    s = aafs_object_create(sizeof(*s), "site", site_destruct);

    s->server = aafs_object_ref(server, "server");
    s->part = partition;

    *site = s;
    return 0;
}

void
aafs_site_free(struct aafs_site *site)
{
    aafs_object_unref(site, "site");
}

char *
aafs_site_print(struct aafs_site *site, char *buf, size_t sz)
{
    char hn[NI_MAXHOST], pn[12];
    snprintf(buf, sz, "Server: %s\t\tPartition: %s", 
	     aafs_server_get_name(site->server, hn, sizeof(hn)),
	     aafs_partition_name(site->part, pn, sizeof(pn)));
    return buf;
}

struct aafs_server *
aafs_site_server(struct aafs_site *site)
{
    return aafs_object_ref(site->server, "server");
}

aafs_partition
aafs_site_partition(struct aafs_site *site)
{
    return site->part;
}


struct aafs_site *
aafs_site_iterate_first(struct aafs_site_list *,
			struct aafs_site_ctx **);

struct aafs_site *
aafs_site_iterate_enxt(struct aafs_site_ctx *);

void
aafs_site_iterate_done(struct aafs_site_ctx *);

void *
aafs_object_create(size_t sz, const char *type,
		   void (*destruct)(void *, const char *))
{
    void *ptr = malloc(sz);
    struct aafs_object *obj = ptr;
    if (ptr == NULL)
	errx(1, "out of memory when creating object for %s", type);
    memset(ptr, 0, sz);
    obj->type = type;
    obj->refcount = 1;
    obj->destruct = destruct;
    return ptr;
}

void *
aafs_object_ref(void *p, const char *type)
{
    struct aafs_object *obj = p;
    assert(obj);
    assert(strcmp(obj->type, type) == 0);
    assert(obj->refcount > 0);
    obj->refcount++;
    return obj;
}

void
aafs_object_unref(void *p, const char *type)
{
    struct aafs_object *obj = p;
    assert(obj);
    assert(strcmp(obj->type, type) == 0);
    assert(obj->refcount > 0);
    obj->refcount--;
    if (obj->refcount == 0) {
	if (obj->destruct)
	    (*obj->destruct)(obj, type);
	free(obj);
    }
}


/*
 * DB server connection support warpper
 */

struct aafs_cell_db_ctx {
    struct aafs_object obj;
    struct aafs_cell *cell;
    int port;
    int service;
    int nhosts;
    int curhost;
    struct aafs_cell_db_host {
	const char *hostname;
	struct rx_connection *conn;
    } *conns;
};


/*
 * Initialize `context' for a db connection to cell `cell', looping
 * over all db servers if `host' == NULL, and else just try that host.
 * `port', `service', and `auth' specify where and how the connection works.
 * Return a rx connection or NULL
 */

static void
celldb_destruct(void *ptr, const char *name)
{
    struct aafs_cell_db_ctx *c = ptr;
    int i;

    aafs_cell_unref(c->cell);

    if (c->conns) {
	for (i = 0; i < c->nhosts; i++)
	    aafs_conn_free(c->conns[i].conn);
	free(c->conns);
    }
}


struct rx_connection *
aafs_cell_first_db(struct aafs_cell *cell,
		   int port,
		   int service,
		   struct aafs_cell_db_ctx **ret_context)
{
    const cell_entry *c_entry;
    struct aafs_cell_db_ctx *ctx;
    int i;
    
    *ret_context = NULL;

    c_entry = cell_get_by_name(cell->name);
    if (c_entry == NULL) {
	warn("Cannot find cell %s", cell->name);
	return NULL;
    }
    if (c_entry->ndbservers == 0) {
	warn("No DB servers for cell %s", cell->name);
	return NULL;
    }

    ctx = aafs_object_create(sizeof(*ctx), "cell-db", 
			     celldb_destruct);

    ctx->cell    = aafs_cell_ref(cell);
    ctx->port    = port;
    ctx->service = service;
  
    ctx->nhosts = c_entry->ndbservers;

    ctx->conns = malloc(ctx->nhosts * sizeof(struct aafs_cell_db_host));
    if (ctx->conns == NULL) {
	aafs_object_unref(ctx, "cell-db");
	return NULL;
    }

    for (i = 0; i < ctx->nhosts; i++) {
	ctx->conns[i].hostname = c_entry->dbservers[i].name;
	ctx->conns[i].conn = aafs_conn_byname(cell,
					      ctx->conns[i].hostname,
					      port,
					      service);
    }

    ctx->curhost = -1;
    *ret_context = ctx;
    return aafs_cell_next_db(ctx);
}

/*
 * Return next connection from `context' or NULL.
 */

struct rx_connection *
aafs_cell_next_db(struct aafs_cell_db_ctx *ctx)
{
    for (ctx->curhost += 1; ctx->curhost < ctx->nhosts; ctx->curhost += 1) {
	if (ctx->conns[ctx->curhost].conn)
	    return ctx->conns[ctx->curhost].conn;
    }
    return NULL;
}

/*
 * return TRUE iff error makes it worthwhile to try the next db.
 */

int
aafs_cell_try_next_db (int error)
{
    switch (error) {
    case ARLA_CALL_DEAD:
    case UNOTSYNC :
    case ENETDOWN :
	return TRUE;
    case 0 :
    default :
	return FALSE;
    }
}

/*
 * free all memory associated with `context'
 */

void
cell_db_free_context(struct aafs_cell_db_ctx *ctx)
{
    aafs_object_unref(ctx, "cell-db");
}   

