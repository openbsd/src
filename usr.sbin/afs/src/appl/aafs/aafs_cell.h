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

#ifndef AAFS_CELL_H
#define AAFS_CELL_H 1

#include <aafs/aafs_security.h>
#include <aafs/aafs_partition.h>

#include <log.h>

struct aafs_cell;
struct aafs_uuid;
struct aafs_server;

/* a site is a server/partition combination */ 
struct aafs_site;
struct aafs_site_list;
struct aafs_site_ctx; /* used when iterating over sites */

struct afsUUID;

int
aafs_init(Log_method *log_method);

int
aafs_cell_create(const char *cell,
		 aafs_sec_type sec_type,
		 struct aafs_cell **);

int
aafs_cell_create_secobj(const char *cell,
			struct aafs_security *, 
			struct aafs_cell **);

int
aafs_cell_rx_secclass(struct aafs_cell *cell,
		      void *secobj, int *secidx);

/* server commands */

int
aafs_server_create_by_name(struct aafs_cell *cell,
			   const char *server,
			   struct aafs_server **ret_server);

int
aafs_server_create_by_long(struct aafs_cell *cell,
			   uint32_t server,
			   struct aafs_server **ret_server);

int
aafs_server_get_long(struct aafs_server *server,
		     uint32_t *server_number);

char *
aafs_server_get_name(struct aafs_server *server, char *buf, size_t sz);

void
aafs_server_free(struct aafs_server *server);


/* site commands */

int
aafs_site_create(struct aafs_cell *, 
		 struct aafs_server *server,
		 aafs_partition partition,
		 struct aafs_site **);

void
aafs_site_free(struct aafs_site *);

char *
aafs_site_print(struct aafs_site *site, char *buf, size_t sz);

struct aafs_server *
aafs_site_server(struct aafs_site *);

aafs_partition
aafs_site_partition(struct aafs_site *);

struct aafs_site *
aafs_site_iterate_first(struct aafs_site_list *,
			struct aafs_site_ctx **);

struct aafs_site *
aafs_site_iterate_next(struct aafs_site_ctx *);

void
aafs_site_iterate_done(struct aafs_site_ctx *);

/*
 * Cell db communication functions
 */

struct rx_connection;
struct aafs_cell_db_ctx;

struct rx_connection *
aafs_cell_first_db(struct aafs_cell *cell,
		   int port,
		   int service,
		   struct aafs_cell_db_ctx **ret_context);

struct rx_connection *
aafs_cell_next_db(struct aafs_cell_db_ctx *ctx);

int
aafs_cell_try_next_db (int error);

void
cell_db_free_context(struct aafs_cell_db_ctx *ctx);

#endif /* AAFS_CELL_H */
