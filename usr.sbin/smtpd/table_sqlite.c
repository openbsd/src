/*	$OpenBSD: table_sqlite.c,v 1.15 2015/01/16 06:40:21 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Eric Faurot <eric@openbsd.org>
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

#include <ctype.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "log.h"

enum {
	SQL_ALIAS = 0,
	SQL_DOMAIN,
	SQL_CREDENTIALS,
	SQL_NETADDR,
	SQL_USERINFO,
	SQL_SOURCE,
	SQL_MAILADDR,
	SQL_ADDRNAME,

	SQL_MAX
};

static int table_sqlite_update(void);
static int table_sqlite_lookup(int, struct dict *, const char *, char *, size_t);
static int table_sqlite_check(int, struct dict *, const char *);
static int table_sqlite_fetch(int, struct dict *, char *, size_t);

static sqlite3_stmt *table_sqlite_query(const char *, int);

#define	DEFAULT_EXPIRE	60
#define	DEFAULT_REFRESH	1000

static char		*config;
static sqlite3		*db;
static sqlite3_stmt	*statements[SQL_MAX];
static sqlite3_stmt	*stmt_fetch_source;
static struct dict	 sources;
static void		*source_iter;
static size_t		 source_refresh = 1000;
static size_t		 source_ncall;
static int		 source_expire = 60;
static time_t		 source_update;

int
main(int argc, char **argv)
{
	int	ch;

	log_init(1);
	log_verbose(~0);

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			log_warnx("warn: table-sqlite: bad option");
			return (1);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		log_warnx("warn: table-sqlite: bogus argument(s)");
		return (1);
	}

	config = argv[0];

	dict_init(&sources);

	if (table_sqlite_update() == 0) {
		log_warnx("warn: table-sqlite: error parsing config file");
		return (1);
	}

	table_api_on_update(table_sqlite_update);
	table_api_on_check(table_sqlite_check);
	table_api_on_lookup(table_sqlite_lookup);
	table_api_on_fetch(table_sqlite_fetch);
	table_api_dispatch();

	return (0);
}

static int
table_sqlite_getconfstr(const char *key, const char *value, char **var)
{
	if (*var) {
		log_warnx("warn: table-sqlite: duplicate %s %s", key, value);
		free(*var);
	}
	*var = strdup(value);
	if (*var == NULL) {
		log_warn("warn: table-sqlite: strdup");
		return (-1);
	}
	return (0);
}

static sqlite3_stmt *
table_sqlite_prepare_stmt(sqlite3 *_db, const char *query, int ncols)
{
	sqlite3_stmt	*stmt;

	if (sqlite3_prepare_v2(_db, query, -1, &stmt, 0) != SQLITE_OK) {
		log_warnx("warn: table-sqlite: sqlite3_prepare_v2: %s",
		    sqlite3_errmsg(_db));
		goto end;
	}
	if (sqlite3_column_count(stmt) != ncols) {
		log_warnx("warn: table-sqlite: columns: invalid columns count for query: %s", query);
		goto end;
	}

	return (stmt);
    end:
	sqlite3_finalize(stmt);
	return (NULL);
}

static int
table_sqlite_update(void)
{
	static const struct {
		const char	*name;
		int		 cols;
	} qspec[SQL_MAX] = {
		{ "query_alias",	1 },
		{ "query_domain",	1 },
		{ "query_credentials",	2 },
		{ "query_netaddr",	1 },
		{ "query_userinfo",	3 },
		{ "query_source",	1 },
		{ "query_mailaddr",	1 },
		{ "query_addrname",	1 },
	};
	sqlite3		*_db;
	sqlite3_stmt	*_statements[SQL_MAX];
	sqlite3_stmt	*_stmt_fetch_source;
	char		*_query_fetch_source;
	char		*queries[SQL_MAX];
	size_t		 flen;
	size_t		 _source_refresh;
	int		 _source_expire;
	FILE		*fp;
	char		*key, *value, *buf, *lbuf, *dbpath;
	const char	*e;
	int		 i, ret;
	long long	 ll;

	dbpath = NULL;
	_db = NULL;
	memset(queries, 0, sizeof(queries));
	memset(_statements, 0, sizeof(_statements));
	_query_fetch_source = NULL;
	_stmt_fetch_source = NULL;

	_source_refresh = DEFAULT_REFRESH;
	_source_expire = DEFAULT_EXPIRE;

	ret = 0;

	/* Parse configuration */

	fp = fopen(config, "r");
	if (fp == NULL)
		return (0);

	lbuf = NULL;
	while ((buf = fgetln(fp, &flen))) {
		if (buf[flen - 1] == '\n')
			buf[flen - 1] = '\0';
		else {
			lbuf = malloc(flen + 1);
			if (lbuf == NULL) {
				log_warn("warn: table-sqlite: malloc");
				return (0);
			}
			memcpy(lbuf, buf, flen);
			lbuf[flen] = '\0';
			buf = lbuf;
		}

		key = buf;
		while (isspace((unsigned char)*key))
			++key;
		if (*key == '\0' || *key == '#')
			continue;
		value = key;
		strsep(&value, " \t:");
		if (value) {
			while (*value) {
				if (!isspace((unsigned char)*value) &&
				    !(*value == ':' && isspace((unsigned char)*(value + 1))))
					break;
				++value;
			}
			if (*value == '\0')
				value = NULL;
		}

		if (value == NULL) {
			log_warnx("warn: table-sqlite: missing value for key %s", key);
			continue;
		}

		if (!strcmp("dbpath", key)) {
			if (table_sqlite_getconfstr(key, value, &dbpath) == -1)
				goto end;
			continue;
		}
		if (!strcmp("fetch_source", key)) {
			if (table_sqlite_getconfstr(key, value, &_query_fetch_source) == -1)
				goto end;
			continue;
		}
		if (!strcmp("fetch_source_expire", key)) {
			e = NULL;
			ll = strtonum(value, 0, INT_MAX, &e);
			if (e) {
				log_warnx("warn: table-sqlite: bad value for %s: %s", key, e);
				goto end;
			}
			_source_expire = ll;
			continue;
		}
		if (!strcmp("fetch_source_refresh", key)) {
			e = NULL;
			ll = strtonum(value, 0, INT_MAX, &e);
			if (e) {
				log_warnx("warn: table-sqlite: bad value for %s: %s", key, e);
				goto end;
			}
			_source_refresh = ll;
			continue;
		}

		for(i = 0; i < SQL_MAX; i++)
			if (!strcmp(qspec[i].name, key))
				break;
		if (i == SQL_MAX) {
			log_warnx("warn: table-sqlite: bogus key %s", key);
			continue;
		}

		if (queries[i]) {
			log_warnx("warn: table-sqlite: duplicate key %s", key);
			continue;
		}

		queries[i] = strdup(value);
		if (queries[i] == NULL) {
			log_warnx("warn: table-sqlite: strdup");
			goto end;
		}
	}

	/* Setup db */

	log_debug("debug: table-sqlite: opening %s", dbpath);

	if (sqlite3_open(dbpath, &_db) != SQLITE_OK) {
		log_warnx("warn: table-sqlite: open: %s",
		    sqlite3_errmsg(_db));
		goto end;
	}

	for (i = 0; i < SQL_MAX; i++) {
		if (queries[i] == NULL)
			continue;
		if ((_statements[i] = table_sqlite_prepare_stmt(_db, queries[i], qspec[i].cols)) == NULL)
			goto end;
	}

	if (_query_fetch_source &&
	    (_stmt_fetch_source = table_sqlite_prepare_stmt(_db, _query_fetch_source, 1)) == NULL)
		goto end;

	/* Replace previous setup */

	for (i = 0; i < SQL_MAX; i++) {
		if (statements[i])
			sqlite3_finalize(statements[i]);
		statements[i] = _statements[i];
		_statements[i] = NULL;
	}
	if (stmt_fetch_source)
		sqlite3_finalize(stmt_fetch_source);
	stmt_fetch_source = _stmt_fetch_source;
	_stmt_fetch_source = NULL;

	if (db)
		sqlite3_close(_db);
	db = _db;
	_db = NULL;

	source_update = 0; /* force update */
	source_expire = _source_expire;
	source_refresh = _source_refresh;

	log_debug("debug: table-sqlite: config successfully updated");
	ret = 1;

    end:

	/* Cleanup */
	for (i = 0; i < SQL_MAX; i++) {
		if (_statements[i])
			sqlite3_finalize(_statements[i]);
		free(queries[i]);
	}
	if (_db)
		sqlite3_close(_db);

	free(dbpath);
	free(_query_fetch_source);

	free(lbuf);
	fclose(fp);
	return (ret);
}

static sqlite3_stmt *
table_sqlite_query(const char *key, int service)
{
	int		 i;
	sqlite3_stmt	*stmt;

	stmt = NULL;
	for(i = 0; i < SQL_MAX; i++)
		if (service == (1 << i)) {
			stmt = statements[i];
			break;
		}

	if (stmt == NULL)
		return (NULL);

	if (sqlite3_bind_text(stmt, 1, key, strlen(key), NULL) != SQLITE_OK) {
		log_warnx("warn: table-sqlite: sqlite3_bind_text: %s",
		    sqlite3_errmsg(db));
		return (NULL);
	}

	return (stmt);
}

static int
table_sqlite_check(int service, struct dict *params, const char *key)
{
	sqlite3_stmt	*stmt;
	int		 r;

	stmt = table_sqlite_query(key, service);
	if (stmt == NULL)
		return (-1);

	r = sqlite3_step(stmt);
	sqlite3_reset(stmt);

	if (r == SQLITE_ROW)
		return (1);

	if (r == SQLITE_DONE)
		return (0);

	return (-1);
}

static int
table_sqlite_lookup(int service, struct dict *params, const char *key, char *dst, size_t sz)
{
	sqlite3_stmt	*stmt;
	const char	*value;
	int		 r, s;

	stmt = table_sqlite_query(key, service);
	if (stmt == NULL)
		return (-1);

	s = sqlite3_step(stmt);
	if (s == SQLITE_DONE) {
		sqlite3_reset(stmt);
		return (0);
	}

	if (s != SQLITE_ROW) {
		log_warnx("warn: table-sqlite: sqlite3_step: %s",
		    sqlite3_errmsg(db));
		sqlite3_reset(stmt);
		return (-1);
	}

	r = 1;

	switch(service) {
	case K_ALIAS:
		memset(dst, 0, sz);
		do {
			value = sqlite3_column_text(stmt, 0);
			if (dst[0] && strlcat(dst, ", ", sz) >= sz) {
				log_warnx("warn: table-sqlite: result too large");
				r = -1;
				break;
			}
			if (strlcat(dst, value, sz) >= sz) {
				log_warnx("warn: table-sqlite: result too large");
				r = -1;
				break;
			}
			s = sqlite3_step(stmt);
		} while (s == SQLITE_ROW);
		if (s !=  SQLITE_ROW && s != SQLITE_DONE) {
			log_warnx("warn: table-sqlite: sqlite3_step: %s",
			    sqlite3_errmsg(db));
			r = -1;
		}
		break;
	case K_CREDENTIALS:
		if (snprintf(dst, sz, "%s:%s",
		    sqlite3_column_text(stmt, 0),
		    sqlite3_column_text(stmt, 1)) >= (ssize_t)sz) {
			log_warnx("warn: table-sqlite: result too large");
			r = -1;
		}
		break;
	case K_USERINFO:
		if (snprintf(dst, sz, "%d:%d:%s",
		    sqlite3_column_int(stmt, 0),
		    sqlite3_column_int(stmt, 1),
		    sqlite3_column_text(stmt, 2)) >= (ssize_t)sz) {
			log_warnx("warn: table-sqlite: result too large");
			r = -1;
		}
		break;
	case K_DOMAIN:
	case K_NETADDR:
	case K_SOURCE:
	case K_MAILADDR:
	case K_ADDRNAME:
		if (strlcpy(dst, sqlite3_column_text(stmt, 0), sz) >= sz) {
			log_warnx("warn: table-sqlite: result too large");
			r = -1;
		}
		break;
	default:
		log_warnx("warn: table-sqlite: unknown service %d", service);
		r = -1;
	}

	sqlite3_reset(stmt);
	return (r);
}

static int
table_sqlite_fetch(int service, struct dict *params, char *dst, size_t sz)
{
	const char	*k;
	int		 s;

	if (service != K_SOURCE)
		return (-1);

	if (stmt_fetch_source == NULL)
		return (-1);

	if (source_ncall < source_refresh &&
	    time(NULL) - source_update < source_expire)
	    goto fetch;

	source_iter = NULL;
	while (dict_poproot(&sources, NULL))
		;

	while ((s = sqlite3_step(stmt_fetch_source)) == SQLITE_ROW)
		dict_set(&sources, sqlite3_column_text(stmt_fetch_source, 0), NULL);

	if (s != SQLITE_DONE)
		log_warnx("warn: table-sqlite: sqlite3_step: %s",
		    sqlite3_errmsg(db));

	sqlite3_reset(stmt_fetch_source);

	source_update = time(NULL);
	source_ncall = 0;

    fetch:

	source_ncall += 1;

        if (! dict_iter(&sources, &source_iter, &k, (void **)NULL)) {
		source_iter = NULL;
		if (! dict_iter(&sources, &source_iter, &k, (void **)NULL))
			return (0);
	}

	if (strlcpy(dst, k, sz) >= sz)
		return (-1);

	return (1);
}
