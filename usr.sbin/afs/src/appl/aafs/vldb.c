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

#include <vldb.cs.h>
#include <volumeserver.h>

#include <aafs/aafs_private.h>
#include <aafs/aafs_vldb.h>

#include <ports.h>
#include <service.h>

#include <atypes.h>

#include <stdio.h>
#include <stdlib.h>

#include <roken.h>

struct aafs_vldb_list {
    struct aafs_object obj;
    int nvolumes;
    struct aafs_volume **volumes;
};

struct aafs_vldb_ctx {
    struct aafs_object obj;
    struct aafs_vldb_list *list;
    int current;

};

struct aafs_vldb_query_attrs {
    struct aafs_object obj;
    VldbListByAttributes vlattrs;
};

static void
vldb_destruct(void *ptr, const char *name)
{
    struct aafs_vldb_list *l = ptr;
    if (l->volumes) {
	int i;
	for (i = 0; i < l->nvolumes; i++)
	    aafs_object_unref(l->volumes[i], "volume");
	free(l->volumes);
    }
}


int
aafs_vldb_query(struct aafs_cell *cell,
		struct aafs_vldb_query_attrs *attrs,
		struct aafs_vldb_list **ret_query)
{
    struct aafs_cell_db_ctx *ctx;
    struct aafs_vldb_list *query;
    struct rx_connection *conn;
    int32_t nentries;
    nbulkentries entries;
    int32_t ret;
    int i;

    *ret_query = NULL;
    
    query = aafs_object_create(sizeof(*query), "vldb-query", vldb_destruct);

    conn = aafs_cell_first_db(cell,
			      htons(afsvldbport),
			      VLDB_SERVICE_ID,
			      &ctx);

    while(conn) {
	entries.len = 0;
	entries.val = NULL;
	nentries = 0;

	ret = VL_ListAttributesN(conn, &attrs->vlattrs, &nentries, &entries);
	
	if (!aafs_cell_try_next_db(ret))
	    conn = NULL;
	else
	    conn = aafs_cell_next_db(ctx);

	if (ret && conn == NULL) {
	    if (entries.val)
		free(entries.val);
	    aafs_object_unref(query, "vldb-query");
	    cell_db_free_context(ctx);
	    return ret;
	}
    }

    cell_db_free_context(ctx);

    query->volumes = emalloc(entries.len * sizeof(*query->volumes));
    query->nvolumes = entries.len;

    for (i = 0; i < entries.len; i++)
	aafs_volume_attach(cell, 0, &entries.val[i], &query->volumes[i]);

    if (entries.val)
	free(entries.val);

    *ret_query = query;

    return 0;
}

int
aafs_vldb_query_free(struct aafs_vldb_list *query)
{
    aafs_object_unref(query, "vldb-query");
    return 0;
}

int
aafs_vldb_query_attr_create(struct aafs_vldb_query_attrs **ret_attrs)
{
    struct aafs_vldb_query_attrs *attrs;

    attrs = aafs_object_create(sizeof(*attrs), "vldb-query-attr", NULL);

    *ret_attrs = attrs;

    return 0;
}

int
aafs_vldb_query_attr_set_partition(struct aafs_vldb_query_attrs *attrs,
				   aafs_partition partition)
{
    attrs->vlattrs.Mask |= VLLIST_PARTITION;
    attrs->vlattrs.partition = partition;
    return 0;
}

int
aafs_vldb_query_attr_set_server(struct aafs_vldb_query_attrs *attrs,
				uint32_t server)
{
    attrs->vlattrs.Mask |= VLLIST_SERVER;
    attrs->vlattrs.server = server;
    return 0;
}

#ifdef not_yet
int
aafs_vldb_query_attr_set_uuid(struct aafs_vldb_query_attrs *attrs,
			      afsUUID *uuid)
{
    return 0;
}
#endif

int
aafs_vldb_query_attr_set_volumeid(struct aafs_vldb_query_attrs *attrs,
				  int32_t volumeid)
{
    attrs->vlattrs.Mask |= VLLIST_VOLUMEID;
    attrs->vlattrs.volumeid = volumeid;
    return 0;
}

int
aafs_vldb_query_attr_set_flag(struct aafs_vldb_query_attrs *attrs,
			      int32_t flag)
{
    attrs->vlattrs.Mask |= VLLIST_FLAG;
    attrs->vlattrs.flag = flag;
    return 0;
}

int
aafs_vldb_query_attr_free(struct aafs_vldb_query_attrs *attrs)
{
    aafs_object_unref(attrs, "vldb-query-attr");
    return 0;
}

static void
vldb_iter_destruct(void *ptr, const char *name)
{
    struct aafs_vldb_ctx *c = ptr;
    aafs_object_unref(c->list, "vldb-query");
}

struct aafs_volume *
aafs_vldb_iterate_first(struct aafs_vldb_list *list,
			struct aafs_vldb_ctx **ret_ctx)
{
    struct aafs_vldb_ctx *ctx;

    *ret_ctx = NULL;

    if (list->nvolumes <= 0)
	return NULL;

    ctx = aafs_object_create(sizeof(*ctx), "vldb-iter", vldb_iter_destruct);

    ctx->list = aafs_object_ref(list, "vldb-query");
    ctx->current = -1;
    *ret_ctx = ctx;
    return aafs_vldb_iterate_next(ctx);
}

struct aafs_volume *
aafs_vldb_iterate_next(struct aafs_vldb_ctx *ctx)
{
    ctx->current++;
    if (ctx->current < ctx->list->nvolumes)
	return aafs_object_ref(ctx->list->volumes[ctx->current], "volume");
    return NULL;
}

void
aafs_vldb_iterate_done(struct aafs_vldb_ctx *ctx)
{
    if (ctx)
	aafs_object_unref(ctx, "vldb-iter");
}
