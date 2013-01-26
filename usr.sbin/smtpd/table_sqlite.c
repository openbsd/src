/*	$OpenBSD: table_sqlite.c,v 1.1 2013/01/26 09:37:24 gilles Exp $	*/

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
#include <sys/param.h>
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
static int table_sqlite_config(struct table *, const char *);
static int table_sqlite_update(struct table *);
static void *table_sqlite_open(struct table *);
static int table_sqlite_lookup(void *, const char *, enum table_service,
    void **);
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

static int table_sqlite_alias(struct table_sqlite_handle *, const char *, void **);
static int table_sqlite_domain(struct table_sqlite_handle *, const char *, void **);
static int table_sqlite_userinfo(struct table_sqlite_handle *, const char *, void **);
static int table_sqlite_credentials(struct table_sqlite_handle *, const char *, void **);
static int table_sqlite_netaddr(struct table_sqlite_handle *, const char *, void **);

static int
table_sqlite_config(struct table *table, const char *config)
{
	void	*cfg;

	/* no config ? broken */
	if (config == NULL)
		return 0;

	cfg = table_config_create();
	if (! table_config_parse(cfg, config, T_HASH))
		goto err;

	/* sanity checks */
	if (table_config_get(cfg, "dbpath") == NULL) {
		log_warnx("table_sqlite: missing 'dbpath' configuration");
		return 0;
	}

	table_set_configuration(table, cfg);
	return 1;

err:
	table_config_destroy(cfg);
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
	void		*cfg;
	const char	*dbpath;

	tsh = xcalloc(1, sizeof *tsh, "table_sqlite_open");
	tsh->table = table;

	cfg = table_get_configuration(table);
	dbpath = table_get(cfg, "dbpath");

	if (sqlite3_open(dbpath, &tsh->ppDb) != SQLITE_OK) {
		log_warnx("table_sqlite: open: %s", sqlite3_errmsg(tsh->ppDb));
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
    void **retp)
{
	struct table_sqlite_handle	*tsh = hdl;

	switch (service) {
	case K_ALIAS:
		return table_sqlite_alias(tsh, key, retp);
	case K_DOMAIN:
		return table_sqlite_domain(tsh, key, retp);
	case K_USERINFO:
		return table_sqlite_userinfo(tsh, key, retp);
	case K_CREDENTIALS:
		return table_sqlite_credentials(tsh, key, retp);
	case K_NETADDR:
		return table_sqlite_netaddr(tsh, key, retp);
	default:
		log_warnx("table_sqlite: lookup: unsupported lookup service");
		return -1;
	}

	return 0;
}

static int
table_sqlite_alias(struct table_sqlite_handle *tsh, const char *key, void **retp)

{
	struct table	       *cfg = table_get_configuration(tsh->table);
	const char	       *query = table_get(cfg, "query_alias");
	sqlite3_stmt	       *stmt;
	struct expand	       *xp = NULL;
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

	if (retp)
		xp = xcalloc(1, sizeof *xp, "table_sqlite_alias");

	nrows = 0;

	sqlite3_bind_text(stmt, 1, key, strlen(key), NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if (retp == NULL) {
			sqlite3_finalize(stmt);
			return 1;
		}
		if (! text_to_expandnode(&xn, sqlite3_column_text(stmt, 0)))
			goto error;
		expand_insert(xp, &xn);
		nrows++;
	}

	sqlite3_finalize(stmt);
	if (retp)
		*retp = xp;
	return nrows ? 1 : 0;

error:
	if (retp)
		*retp = NULL;
	if (xp)
		expand_free(xp);
	return -1;
}

static int
table_sqlite_domain(struct table_sqlite_handle *tsh, const char *key, void **retp)
{
	struct table	       *cfg = table_get_configuration(tsh->table);
	const char	       *query = table_get(cfg, "query_domain");
	sqlite3_stmt	       *stmt;
	struct destination     *domain = NULL;
	
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
		if (retp) {
			domain = xcalloc(1, sizeof *domain, "table_sqlite_domain");
			strlcpy(domain->name, sqlite3_column_text(stmt, 0), sizeof domain->name);
			*retp = domain;
		}
		sqlite3_finalize(stmt);
		return 1;

	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		return 0;

	default:
		sqlite3_finalize(stmt);
	}

	free(domain);
	if (retp)
		*retp = NULL;
	return -1;
}

static int
table_sqlite_userinfo(struct table_sqlite_handle *tsh, const char *key, void **retp)
{
	struct table	       *cfg = table_get_configuration(tsh->table);
	const char	       *query = table_get(cfg, "query_userinfo");
	sqlite3_stmt	       *stmt;
	struct userinfo	       *userinfo = NULL;
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
		if (retp) {
			userinfo = xcalloc(1, sizeof *userinfo, "table_sqlite_userinfo");
			s = strlcpy(userinfo->username, sqlite3_column_text(stmt, 0),
			    sizeof(userinfo->username));
			if (s >= sizeof(userinfo->username))
				goto error;
			userinfo->uid = sqlite3_column_int(stmt, 1);
			userinfo->gid = sqlite3_column_int(stmt, 2);
			s = strlcpy(userinfo->directory, sqlite3_column_text(stmt, 3),
			    sizeof(userinfo->directory));
			if (s >= sizeof(userinfo->directory))
				goto error;
			*retp = userinfo;
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
	free(userinfo);
	if (retp)
		*retp = NULL;
	return -1;
}

static int
table_sqlite_credentials(struct table_sqlite_handle *tsh, const char *key, void **retp)
{
	struct table	       *cfg = table_get_configuration(tsh->table);
	const char	       *query = table_get(cfg, "query_credentials");
	sqlite3_stmt	       *stmt;
	struct credentials     *creds = NULL;
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
		if (retp) {
			creds = xcalloc(1, sizeof *creds, "table_sqlite_credentials");
			s = strlcpy(creds->username, sqlite3_column_text(stmt, 0),
			    sizeof(creds->username));
			if (s >= sizeof(creds->username))
				goto error;
			s = strlcpy(creds->password, sqlite3_column_text(stmt, 1),
			    sizeof(creds->password));
			if (s >= sizeof(creds->password))
				goto error;
			*retp = creds;
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
	free(creds);
	if (retp)
		*retp = NULL;
	return -1;
}


static int
table_sqlite_netaddr(struct table_sqlite_handle *tsh, const char *key, void **retp)
{
	struct table	       *cfg = table_get_configuration(tsh->table);
	const char	       *query = table_get(cfg, "query_netaddr");
	sqlite3_stmt	       *stmt;
	struct netaddr	       *netaddr = NULL;
	
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
		if (retp) {
			netaddr = xcalloc(1, sizeof *netaddr, "table_sqlite_netaddr");
			if (! text_to_netaddr(netaddr, sqlite3_column_text(stmt, 0)))
				goto error;
			*retp = netaddr;
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
	free(netaddr);
	if (retp)
		*retp = NULL;
	return -1;
}
