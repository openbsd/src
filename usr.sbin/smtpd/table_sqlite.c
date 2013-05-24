/*	$OpenBSD: table_sqlite.c,v 1.3 2013/05/24 17:03:14 eric Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

/* sqlite(3) backend */
static int table_sqlite_config(struct table *);
static int table_sqlite_update(struct table *);
static void *table_sqlite_open(struct table *);
static int table_sqlite_lookup(void *, const char *, enum table_service,
    union lookup *);
static void  table_sqlite_close(void *);

struct table_backend table_backend_sqlite = {
	K_ALIAS|K_CREDENTIALS|K_DOMAIN|K_NETADDR|K_USERINFO,
	table_sqlite_config,
	table_sqlite_open,
	table_sqlite_update,
	table_sqlite_close,
	table_sqlite_lookup,
};

struct table_sqlite_handle {
	sqlite3	        *ppDb;	
	struct table	*table;
};

static int table_sqlite_alias(struct table_sqlite_handle *, const char *, union lookup *);
static int table_sqlite_domain(struct table_sqlite_handle *, const char *, union lookup *);
static int table_sqlite_userinfo(struct table_sqlite_handle *, const char *, union lookup *);
static int table_sqlite_credentials(struct table_sqlite_handle *, const char *, union lookup *);
static int table_sqlite_netaddr(struct table_sqlite_handle *, const char *, union lookup *);

static int
table_sqlite_config(struct table *table)
{
	struct table	*cfg;

	/* no config ? broken */
	if (table->t_config[0] == '\0')
		return 0;

	cfg = table_create("static", table->t_name, "conf", table->t_config);
	if (!table_config(cfg))
		goto err;

	/* sanity checks */
	if (table_get(cfg, "dbpath") == NULL) {
		log_warnx("table_sqlite: missing 'dbpath' configuration");
		return 0;
	}

	return 1;

err:
	table_destroy(cfg);
	return 0;
}

static int
table_sqlite_update(struct table *table)
{
	log_info("info: Table \"%s\" successfully updated", table->t_name);
	return 1;
}

static void *
table_sqlite_open(struct table *table)
{
	struct table_sqlite_handle	*tsh;
	struct table	*cfg;
	const char	*dbpath;

	tsh = xcalloc(1, sizeof *tsh, "table_sqlite_open");
	tsh->table = table;

	cfg = table_find(table->t_name, "conf");
	dbpath = table_get(cfg, "dbpath");

	if (sqlite3_open(dbpath, &tsh->ppDb) != SQLITE_OK) {
		log_warnx("table_sqlite: open: %s", sqlite3_errmsg(tsh->ppDb));
		free(tsh);
		return NULL;
	}

	return tsh;
}

static void
table_sqlite_close(void *hdl)
{
	return;
}

static int
table_sqlite_lookup(void *hdl, const char *key, enum table_service service,
    union lookup *lk)
{
	struct table_sqlite_handle	*tsh = hdl;

	switch (service) {
	case K_ALIAS:
		return table_sqlite_alias(tsh, key, lk);
	case K_DOMAIN:
		return table_sqlite_domain(tsh, key, lk);
	case K_USERINFO:
		return table_sqlite_userinfo(tsh, key, lk);
	case K_CREDENTIALS:
		return table_sqlite_credentials(tsh, key, lk);
	case K_NETADDR:
		return table_sqlite_netaddr(tsh, key, lk);
	default:
		log_warnx("table_sqlite: lookup: unsupported lookup service");
		return -1;
	}

	return 0;
}

static int
table_sqlite_alias(struct table_sqlite_handle *tsh, const char *key, union lookup *lk)

{
	struct table	       *cfg = table_find(tsh->table->t_name, "conf");
	const char	       *query = table_get(cfg, "query_alias");
	sqlite3_stmt	       *stmt;
	struct expandnode	xn;
	int			nrows;
	
	if (query == NULL) {
		log_warnx("table_sqlite: lookup: no query configured for aliases");
		return -1;
	}

	if (sqlite3_prepare_v2(tsh->ppDb, query, -1, &stmt, 0) != SQLITE_OK) {
		log_warnx("table_sqlite: prepare: %s", sqlite3_errmsg(tsh->ppDb));
		return -1;
	}

	if (sqlite3_column_count(stmt) != 1) {
		log_warnx("table_sqlite: columns: invalid resultset");
		sqlite3_finalize(stmt);
		return -1;
	}

	if (lk)
		lk->expand = xcalloc(1, sizeof(*lk->expand), "table_sqlite_alias");

	nrows = 0;

	sqlite3_bind_text(stmt, 1, key, strlen(key), NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if (lk == NULL) {
			sqlite3_finalize(stmt);
			return 1;
		}
		if (! text_to_expandnode(&xn, sqlite3_column_text(stmt, 0)))
			goto error;
		expand_insert(lk->expand, &xn);
		nrows++;
	}

	sqlite3_finalize(stmt);
	return nrows ? 1 : 0;

error:
	if (lk && lk->expand)
		expand_free(lk->expand);
	return -1;
}

static int
table_sqlite_domain(struct table_sqlite_handle *tsh, const char *key, union lookup *lk)
{
	struct table	       *cfg = table_find(tsh->table->t_name, "conf");
	const char	       *query = table_get(cfg, "query_domain");
	sqlite3_stmt	       *stmt;
	
	if (query == NULL) {
		log_warnx("table_sqlite: lookup: no query configured for domain");
		return -1;
	}

	if (sqlite3_prepare_v2(tsh->ppDb, query, -1, &stmt, 0) != SQLITE_OK) {
		log_warnx("table_sqlite: prepare: %s", sqlite3_errmsg(tsh->ppDb));
		return -1;
	}

	if (sqlite3_column_count(stmt) != 1) {
		log_warnx("table_sqlite: columns: invalid resultset");
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, key, strlen(key), NULL);

	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		if (lk)
			strlcpy(lk->domain.name, sqlite3_column_text(stmt, 0), sizeof(lk->domain.name));
		sqlite3_finalize(stmt);
		return 1;

	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		return 0;

	default:
		sqlite3_finalize(stmt);
	}

	return -1;
}

static int
table_sqlite_userinfo(struct table_sqlite_handle *tsh, const char *key, union lookup *lk)
{
	struct table	       *cfg = table_find(tsh->table->t_name, "conf");
	const char	       *query = table_get(cfg, "query_userinfo");
	sqlite3_stmt	       *stmt;
	size_t			s;
	
	if (query == NULL) {
		log_warnx("table_sqlite: lookup: no query configured for user");
		return -1;
	}

	if (sqlite3_prepare_v2(tsh->ppDb, query, -1, &stmt, 0) != SQLITE_OK) {
		log_warnx("table_sqlite: prepare: %s", sqlite3_errmsg(tsh->ppDb));
		return -1;
	}

	if (sqlite3_column_count(stmt) != 4) {
		log_warnx("table_sqlite: columns: invalid resultset");
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, key, strlen(key), NULL);

	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		if (lk) {
			s = strlcpy(lk->userinfo.username, sqlite3_column_text(stmt, 0),
			    sizeof(lk->userinfo.username));
			if (s >= sizeof(lk->userinfo.username))
				goto error;
			lk->userinfo.uid = sqlite3_column_int(stmt, 1);
			lk->userinfo.gid = sqlite3_column_int(stmt, 2);
			s = strlcpy(lk->userinfo.directory, sqlite3_column_text(stmt, 3),
			    sizeof(lk->userinfo.directory));
			if (s >= sizeof(lk->userinfo.directory))
				goto error;
		}
		sqlite3_finalize(stmt);
		return 1;

	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		return 0;

	default:
		goto error;
	}

error:
	sqlite3_finalize(stmt);
	return -1;
}

static int
table_sqlite_credentials(struct table_sqlite_handle *tsh, const char *key, union lookup *lk)
{
	struct table	       *cfg = table_find(tsh->table->t_name, "conf");
	const char	       *query = table_get(cfg, "query_credentials");
	sqlite3_stmt	       *stmt;
	size_t			s;
	
	if (query == NULL) {
		log_warnx("table_sqlite: lookup: no query configured for credentials");
		return -1;
	}

	if (sqlite3_prepare_v2(tsh->ppDb, query, -1, &stmt, 0) != SQLITE_OK) {
		log_warnx("table_sqlite: prepare: %s", sqlite3_errmsg(tsh->ppDb));
		return -1;
	}

	if (sqlite3_column_count(stmt) != 2) {
		log_warnx("table_sqlite: columns: invalid resultset");
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, key, strlen(key), NULL);
	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		if (lk) {
			s = strlcpy(lk->creds.username, sqlite3_column_text(stmt, 0),
			    sizeof(lk->creds.username));
			if (s >= sizeof(lk->creds.username))
				goto error;
			s = strlcpy(lk->creds.password, sqlite3_column_text(stmt, 1),
			    sizeof(lk->creds.password));
			if (s >= sizeof(lk->creds.password))
				goto error;
		}
		sqlite3_finalize(stmt);
		return 1;

	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		return 0;

	default:
		goto error;
	}

error:
	sqlite3_finalize(stmt);
	return -1;
}


static int
table_sqlite_netaddr(struct table_sqlite_handle *tsh, const char *key, union lookup *lk)
{
	struct table	       *cfg = table_find(tsh->table->t_name, "conf");
	const char	       *query = table_get(cfg, "query_netaddr");
	sqlite3_stmt	       *stmt;
	
	if (query == NULL) {
		log_warnx("table_sqlite: lookup: no query configured for netaddr");
		return -1;
	}

	if (sqlite3_prepare_v2(tsh->ppDb, query, -1, &stmt, 0) != SQLITE_OK) {
		log_warnx("table_sqlite: prepare: %s", sqlite3_errmsg(tsh->ppDb));
		return -1;
	}

	if (sqlite3_column_count(stmt) != 1) {
		log_warnx("table_sqlite: columns: invalid resultset");
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, key, strlen(key), NULL);
	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		if (lk) {
			if (! text_to_netaddr(&lk->netaddr, sqlite3_column_text(stmt, 0)))
				goto error;
		}
		sqlite3_finalize(stmt);
		return 1;

	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		return 0;

	default:
		goto error;
	}

error:
	sqlite3_finalize(stmt);
	return -1;
}
