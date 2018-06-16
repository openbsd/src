/*	$OpenBSD: table_static.c,v 1.19 2018/06/16 19:41:26 gilles Exp $	*/

/*
 * Copyright (c) 2013 Eric Faurot <eric@openbsd.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

/* static backend */
static int table_static_config(struct table *);
static int table_static_update(struct table *);
static void *table_static_open(struct table *);
static int table_static_lookup(void *, struct dict *, const char *,
    enum table_service, union lookup *);
static int table_static_fetch(void *, struct dict *, enum table_service,
    union lookup *);
static void  table_static_close(void *);

struct table_backend table_backend_static = {
	K_ALIAS|K_CREDENTIALS|K_DOMAIN|K_NETADDR|K_USERINFO|
	K_SOURCE|K_MAILADDR|K_ADDRNAME|K_MAILADDRMAP|K_RELAYHOST|
	K_STRING,
	table_static_config,
	table_static_open,
	table_static_update,
	table_static_close,
	table_static_lookup,
	table_static_fetch
};

static struct keycmp {
	enum table_service	service;
	int		       (*func)(const char *, const char *);
} keycmp[] = {
	{ K_DOMAIN, table_domain_match },
	{ K_NETADDR, table_netaddr_match },
	{ K_MAILADDR, table_mailaddr_match }
};


static int
table_static_config(struct table *t)
{
	FILE	*fp;
	char	*buf = NULL, *p;
	int	 lineno = 0;
	size_t	 sz = 0;
	ssize_t	 flen;
	char	*keyp;
	char	*valp;
	size_t	 ret = 0;

	/* no config ? ok */
	if (*t->t_config == '\0')
		return 1;

	if ((fp = fopen(t->t_config, "r")) == NULL) {
		log_warn("warn: Table \"%s\"", t->t_config);
		return 0;
	}

	while ((flen = getline(&buf, &sz, fp)) != -1) {
		lineno++;
		if (buf[flen - 1] == '\n')
			buf[--flen] = '\0';

		keyp = buf;
		while (isspace((unsigned char)*keyp)) {
			++keyp;
			--flen;
		}
		if (*keyp == '\0')
			continue;
		while (isspace((unsigned char)keyp[flen - 1]))
			keyp[--flen] = '\0';
		if (*keyp == '#') {
			if (t->t_type == T_NONE) {
				keyp++;
				while (isspace((unsigned char)*keyp))
					++keyp;
				if (!strcmp(keyp, "@list"))
					t->t_type = T_LIST;
			}
			continue;
		}

		if (t->t_type == T_NONE) {
			for (p = keyp; *p; p++) {
				if (*p == ' ' || *p == '\t' || *p == ':') {
					t->t_type = T_HASH;
					break;
				}
			}
			if (t->t_type == T_NONE)
				t->t_type = T_LIST;
		}

		if (t->t_type == T_LIST) {
			table_add(t, keyp, NULL);
			continue;
		}

		/* T_HASH */
		valp = keyp;
		strsep(&valp, " \t:");
		if (valp) {
			while (*valp) {
				if (!isspace((unsigned char)*valp) &&
				    !(*valp == ':' &&
				    isspace((unsigned char)*(valp + 1))))
					break;
				++valp;
			}
			if (*valp == '\0')
				valp = NULL;
		}
		if (valp == NULL) {
			log_warnx("%s: invalid map entry line %d", t->t_config,
			    lineno);
			goto end;
		}

		table_add(t, keyp, valp);
	}

	if (ferror(fp)) {
		log_warn("%s: getline", t->t_config);
		goto end;
	}

	/* Accept empty alias files; treat them as hashes */
	if (t->t_type == T_NONE && t->t_backend->services & K_ALIAS)
	    t->t_type = T_HASH;

	ret = 1;
end:
	free(buf);
	fclose(fp);
	return ret;
}

static int
table_static_update(struct table *table)
{
	struct table	*t;
	void		*p = NULL;

	/* no config ? ok */
	if (table->t_config[0] == '\0')
		goto ok;

	t = table_create(env, "static", table->t_name, "update", table->t_config);
	if (!table_config(t))
		goto err;

	/* replace former table, frees t */
	while (dict_poproot(&table->t_dict, (void **)&p))
		free(p);
	dict_merge(&table->t_dict, &t->t_dict);
	table_destroy(env, t);

ok:
	log_info("info: Table \"%s\" successfully updated", table->t_name);
	return 1;

err:
	table_destroy(env, t);
	log_info("info: Failed to update table \"%s\"", table->t_name);
	return 0;
}

static void *
table_static_open(struct table *table)
{
	return table;
}

static void
table_static_close(void *hdl)
{
	return;
}

static int
table_static_lookup(void *hdl, struct dict *params, const char *key,
    enum table_service service, union lookup *lk)
{
	struct table   *m  = hdl;
	char	       *line;
	int		ret;
	int	       (*match)(const char *, const char *) = NULL;
	size_t		i;
	void	       *iter;
	const char     *k;
	char	       *v;

	for (i = 0; i < nitems(keycmp); ++i)
		if (keycmp[i].service == service)
			match = keycmp[i].func;

	line = NULL;
	iter = NULL;
	ret = 0;
	while (dict_iter(&m->t_dict, &iter, &k, (void **)&v)) {
		if (match) {
			if (match(key, k)) {
				line = v;
				ret = 1;
			}
		}
		else {
			if (strcmp(key, k) == 0) {
				line = v;
				ret = 1;
			}
		}
		if (ret)
			break;
	}

	if (lk == NULL)
		return ret ? 1 : 0;

	if (ret == 0)
		return 0;

	return table_parse_lookup(service, key, line, lk);
}

static int
table_static_fetch(void *hdl, struct dict *params,
    enum table_service service, union lookup *lk)
{
	struct table   *t = hdl;
	const char     *k;

	if (!dict_iter(&t->t_dict, &t->t_iter, &k, (void **)NULL)) {
		t->t_iter = NULL;
		if (!dict_iter(&t->t_dict, &t->t_iter, &k, (void **)NULL))
			return 0;
	}

	if (lk == NULL)
		return 1;

	return table_parse_lookup(service, NULL, k, lk);
}
