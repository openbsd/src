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

#ifndef AAFS_VLDB_H
#define AAFS_VLDB_H 1

#include <aafs/aafs_cell.h>
#include <aafs/aafs_volume.h>

struct aafs_vldb_list;
struct aafs_vldb_ctx;

struct aafs_vldb_query_attrs;

int
aafs_vldb_query(struct aafs_cell *cell,
		struct aafs_vldb_query_attrs *attrs,
		struct aafs_vldb_list **query);

int
aafs_vldb_query_free(struct aafs_vldb_list *query);

int
aafs_vldb_query_attr_create(struct aafs_vldb_query_attrs **attrs);

int
aafs_vldb_query_attr_set_partition(struct aafs_vldb_query_attrs *attrs,
				   aafs_partition partition);

int
aafs_vldb_query_attr_set_server(struct aafs_vldb_query_attrs *attrs,
				uint32_t server);

#ifdef not_yet
int
aafs_vldb_query_attr_set_uuid(struct aafs_vldb_query_attrs *attrs,
			      afsUUID *uuid);
#endif

int
aafs_vldb_query_attr_set_volumeid(struct aafs_vldb_query_attrs *attrs,
				  int32_t volumeid);

int
aafs_vldb_query_attr_set_flag(struct aafs_vldb_query_attrs *attrs,
			      int32_t flag);

int
aafs_vldb_query_attr_free(struct aafs_vldb_query_attrs *attrs);

struct aafs_volume *
aafs_vldb_iterate_first(struct aafs_vldb_list *,
			struct aafs_vldb_ctx **);

struct aafs_volume *
aafs_vldb_iterate_next(struct aafs_vldb_ctx *);

void
aafs_vldb_iterate_done(struct aafs_vldb_ctx *);

#endif /* AAFS_VLDB_H */
