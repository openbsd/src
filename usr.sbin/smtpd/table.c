/*	$OpenBSD: table.c,v 1.2 2013/02/05 11:45:18 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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

#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

struct table_backend *table_backend_lookup(const char *);

extern struct table_backend table_backend_static;
extern struct table_backend table_backend_db;
extern struct table_backend table_backend_getpwnam;
extern struct table_backend table_backend_sqlite;
extern struct table_backend table_backend_ldap;

static objid_t	last_table_id = 0;

struct table_backend *
table_backend_lookup(const char *backend)
{
	if (!strcmp(backend, "static") || !strcmp(backend, "file"))
		return &table_backend_static;
	if (!strcmp(backend, "db"))
		return &table_backend_db;
	if (!strcmp(backend, "getpwnam"))
		return &table_backend_getpwnam;
	if (!strcmp(backend, "sqlite"))
		return &table_backend_sqlite;
	if (!strcmp(backend, "ldap"))
		return &table_backend_ldap;
	return NULL;
}

struct table *
table_findbyname(const char *name)
{
	return dict_get(env->sc_tables_dict, name);
}

struct table *
table_find(objid_t id)
{
	return tree_get(env->sc_tables_tree, id);
}

int
table_lookup(struct table *table, const char *key, enum table_service kind,
    void **retp)
{
	return table->t_backend->lookup(table->t_handle, key, kind, retp);
}

int
table_fetch(struct table *table, enum table_service kind, char **retp)
{
	return table->t_backend->fetch(table->t_handle, kind, retp);
}

struct table *
table_create(const char *backend, const char *name, const char *config)
{
	struct table		*t;
	struct table_backend	*tb;
	size_t		 n;

	if (name && table_findbyname(name))
		errx(1, "table_create: table \"%s\" already defined", name);

	if ((tb = table_backend_lookup(backend)) == NULL)
		errx(1, "table_create: backend \"%s\" does not exist", backend);

	t = xcalloc(1, sizeof(*t), "table_create");
	t->t_backend = tb;

	/* XXX */
	/*
	 * until people forget about it, "file" really means "static"
	 */
	if (!strcmp(backend, "file"))
		backend = "static";

	if (strlcpy(t->t_src, backend, sizeof t->t_src) >= sizeof t->t_src)
		errx(1, "table_create: table backend \"%s\" too large",
		    t->t_src);

	if (config && *config) {
		if (strlcpy(t->t_config, config, sizeof t->t_config)
		    >= sizeof t->t_config)
			errx(1, "table_create: table config \"%s\" too large",
			    t->t_config);
	}

	if (strcmp(t->t_src, "static") != 0)
		t->t_type = T_DYNAMIC;

	t->t_id = ++last_table_id;
	if (t->t_id == INT_MAX)
		errx(1, "table_create: too many tables defined");

	if (name == NULL)
		snprintf(t->t_name, sizeof(t->t_name), "<dynamic:%u>", t->t_id);
	else {
		n = strlcpy(t->t_name, name, sizeof(t->t_name));
		if (n >= sizeof(t->t_name))
			errx(1, "table_create: table name too long");
	}

	dict_init(&t->t_dict);
	dict_set(env->sc_tables_dict, t->t_name, t);
	tree_set(env->sc_tables_tree, t->t_id, t);

	return (t);
}

void
table_destroy(struct table *t)
{
	void	*p = NULL;

	while (dict_poproot(&t->t_dict, NULL, (void **)&p))
		free(p);

	dict_xpop(env->sc_tables_dict, t->t_name);
	tree_xpop(env->sc_tables_tree, t->t_id);
	free(t);
}

void
table_set_configuration(struct table *t, struct table *config)
{
	strlcpy(t->t_cfgtable, config->t_name, sizeof t->t_cfgtable);
}

struct table *
table_get_configuration(struct table *t)
{
	return table_findbyname(t->t_cfgtable);
}

void
table_set_payload(struct table *t, void *payload)
{
	t->t_payload = payload;
}

void *
table_get_payload(struct table *t)
{
	return t->t_payload;
}

void
table_add(struct table *t, const char *key, const char *val)
{
	if (strcmp(t->t_src, "static") != 0)
		errx(1, "table_add: cannot add to table");
	dict_set(&t->t_dict, key, val ? xstrdup(val, "table_add") : NULL);
}

const void *
table_get(struct table *t, const char *key)
{
	if (strcmp(t->t_src, "static") != 0)
		errx(1, "table_add: cannot get from table");
	return dict_get(&t->t_dict, key);
}

void
table_delete(struct table *t, const char *key)
{
	if (strcmp(t->t_src, "static") != 0)
		errx(1, "map_add: cannot delete from map");
	free(dict_pop(&t->t_dict, key));
}

int
table_check_type(struct table *t, uint32_t mask)
{
	return t->t_type & mask;
}

int
table_check_service(struct table *t, uint32_t mask)
{
	return t->t_backend->services & mask;
}

int
table_check_use(struct table *t, uint32_t tmask, uint32_t smask)
{
	return table_check_type(t, tmask) && table_check_service(t, smask);
}

int
table_open(struct table *t)
{
	t->t_handle = t->t_backend->open(t);
	if (t->t_handle == NULL)
		return 0;
	return 1;
}

void
table_close(struct table *t)
{
	t->t_backend->close(t->t_handle);
}


void
table_update(struct table *t)
{
	t->t_backend->update(t);
}

void *
table_config_create(void)
{
	return table_create("static", NULL, NULL);
}

const char *
table_config_get(void *p, const char *key)
{
	return (const char *)table_get(p, key);
}

void
table_config_destroy(void *p)
{
	table_destroy(p);
}

int
table_config_parse(void *p, const char *config, enum table_type type)
{
	struct table	*t = p;
	FILE	*fp;
	char *buf, *lbuf;
	size_t flen;
	char *keyp;
	char *valp;
	size_t	ret = 0;

	if (strcmp("static", t->t_src) != 0) {
		log_warn("table_config_parser: config table must be static");
		return 0;
	}

	fp = fopen(config, "r");
	if (fp == NULL)
		return 0;

	lbuf = NULL;
	while ((buf = fgetln(fp, &flen))) {
		if (buf[flen - 1] == '\n')
			buf[flen - 1] = '\0';
		else {
			lbuf = xmalloc(flen + 1, "table_stdio_get_entry");
			memcpy(lbuf, buf, flen);
			lbuf[flen] = '\0';
			buf = lbuf;
		}

		keyp = buf;
		while (isspace((int)*keyp))
			++keyp;
		if (*keyp == '\0' || *keyp == '#')
			continue;
		valp = keyp;
		strsep(&valp, " \t:");
		if (valp) {
			while (*valp) {
				if (!isspace(*valp) &&
				    !(*valp == ':' && isspace(*(valp + 1))))
					break;
				++valp;
			}
			if (*valp == '\0')
				valp = NULL;
		}

		/**/
		if (t->t_type == 0)
			t->t_type = (valp == keyp || valp == NULL) ? T_LIST :
			    T_HASH;

		if (!(t->t_type & type))
			goto end;

		if ((valp == keyp || valp == NULL) && t->t_type == T_LIST)
			table_add(t, keyp, NULL);
		else if ((valp != keyp && valp != NULL) && t->t_type == T_HASH)
			table_add(t, keyp, valp);
		else
			goto end;
	}
	ret = 1;
end:
	free(lbuf);
	fclose(fp);
	return ret;
}

int
table_domain_match(const char *s1, const char *s2)
{
	return hostname_match(s1, s2);
}

int
table_mailaddr_match(const char *s1, const char *s2)
{
	struct mailaddr m1;
	struct mailaddr m2;

	if (! text_to_mailaddr(&m1, s1))
		return 0;
	if (! text_to_mailaddr(&m2, s2))
		return 0;

	if (strcasecmp(m1.domain, m2.domain))
		return 0;

	if (m2.user[0])
		if (strcasecmp(m1.user, m2.user))
			return 0;
	return 1;
}

static int table_match_mask(struct sockaddr_storage *, struct netaddr *);
static int table_inet4_match(struct sockaddr_in *, struct netaddr *);
static int table_inet6_match(struct sockaddr_in6 *, struct netaddr *);

int
table_netaddr_match(const char *s1, const char *s2)
{
	struct netaddr n1;
	struct netaddr n2;

	if (strcmp(s1, s2) == 0)
		return 1;
	if (! text_to_netaddr(&n1, s1))
		return 0;
	if (! text_to_netaddr(&n2, s2))
		return 0;
	if (n1.ss.ss_family != n2.ss.ss_family)
		return 0;
	if (n1.ss.ss_len != n2.ss.ss_len)
		return 0;
	return table_match_mask(&n1.ss, &n2);
}

static int
table_match_mask(struct sockaddr_storage *ss, struct netaddr *ssmask)
{
	if (ss->ss_family == AF_INET)
		return table_inet4_match((struct sockaddr_in *)ss, ssmask);

	if (ss->ss_family == AF_INET6)
		return table_inet6_match((struct sockaddr_in6 *)ss, ssmask);

	return (0);
}

static int
table_inet4_match(struct sockaddr_in *ss, struct netaddr *ssmask)
{
	in_addr_t mask;
	int i;

	/* a.b.c.d/8 -> htonl(0xff000000) */
	mask = 0;
	for (i = 0; i < ssmask->bits; ++i)
		mask = (mask >> 1) | 0x80000000;
	mask = htonl(mask);

	/* (addr & mask) == (net & mask) */
	if ((ss->sin_addr.s_addr & mask) ==
	    (((struct sockaddr_in *)ssmask)->sin_addr.s_addr & mask))
		return 1;

	return 0;
}

static int
table_inet6_match(struct sockaddr_in6 *ss, struct netaddr *ssmask)
{
	struct in6_addr	*in;
	struct in6_addr	*inmask;
	struct in6_addr	 mask;
	int		 i;

	bzero(&mask, sizeof(mask));
	for (i = 0; i < ssmask->bits / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = ssmask->bits % 8;
	if (i)
		mask.s6_addr[ssmask->bits / 8] = 0xff00 >> i;

	in = &ss->sin6_addr;
	inmask = &((struct sockaddr_in6 *)&ssmask->ss)->sin6_addr;

	for (i = 0; i < 16; i++) {
		if ((in->s6_addr[i] & mask.s6_addr[i]) !=
		    (inmask->s6_addr[i] & mask.s6_addr[i]))
			return (0);
	}

	return (1);
}

void
table_open_all(void)
{
	struct table	*t;
	void		*iter;

	iter = NULL;
	while (tree_iter(env->sc_tables_tree, &iter, NULL, (void **)&t))
		if (! table_open(t))
			errx(1, "failed to open table %s", t->t_name);
}

void
table_close_all(void)
{
	struct table	*t;
	void		*iter;

	iter = NULL;
	while (tree_iter(env->sc_tables_tree, &iter, NULL, (void **)&t))
		table_close(t);
}
