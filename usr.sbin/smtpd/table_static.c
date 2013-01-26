/*	$OpenBSD: table_static.c,v 1.1 2013/01/26 09:37:24 gilles Exp $	*/

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

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

/* static backend */
static int table_static_config(struct table *, const char *);
static int table_static_update(struct table *);
static void *table_static_open(struct table *);
static int table_static_lookup(void *, const char *, enum table_service,
    void **);
static int table_static_fetch(void *, enum table_service, char **);
static void  table_static_close(void *);

static int	table_static_credentials(const char *, char *, size_t, void **);
static int	table_static_alias(const char *, char *, size_t, void **);
static int	table_static_domain(const char *, char *, size_t, void **);
static int	table_static_netaddr(const char *, char *, size_t, void **);
static int	table_static_source(const char *, char *, size_t, void **);
static int	table_static_userinfo(const char *, char *, size_t, void **);
static int	table_static_mailaddr(const char *, char *, size_t, void **);
static int	table_static_addrname(const char *, char *, size_t, void **);

struct table_backend table_backend_static = {
	K_ALIAS|K_CREDENTIALS|K_DOMAIN|K_NETADDR|K_USERINFO|K_SOURCE|K_MAILADDR|K_ADDRNAME,
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
table_static_config(struct table *table, const char *config)
{
	/* no config ? ok */
	if (config == NULL)
		return 1;

	return table_config_parse(table, config, T_LIST|T_HASH);
}

static int
table_static_update(struct table *table)
{
	struct table   *t;
	char		name[MAX_LINE_SIZE];

	/* no config ? ok */
	if (table->t_config[0] == '\0')
		goto ok;

	t = table_create(table->t_src, NULL, table->t_config);
	if (! t->t_backend->config(t, table->t_config))
		goto err;

	/* update successful, swap table names */
	strlcpy(name, table->t_name, sizeof name);
	strlcpy(table->t_name, t->t_name, sizeof table->t_name);
	strlcpy(t->t_name, name, sizeof t->t_name);

	/* swap, table id */
	table->t_id = table->t_id ^ t->t_id;
	t->t_id     = table->t_id ^ t->t_id;
	table->t_id = table->t_id ^ t->t_id;

	/* destroy former table */
	table_destroy(table);

ok:
	log_info("info: Table \"%s\" successfully updated", name);
	return 1;

err:
	table_destroy(t);
	log_info("info: Failed to update table \"%s\"", name);
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
table_static_lookup(void *hdl, const char *key, enum table_service service,
    void **retp)
{
	struct table   *m  = hdl;
	char	       *line;
	size_t		len;
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

	if (retp == NULL)
		return ret ? 1 : 0;

	if (ret == 0) {
		*retp = NULL;
		return 0;
	}

	if ((line = strdup(line)) == NULL)
		return -1;
	len = strlen(line);
	switch (service) {
	case K_ALIAS:
		ret = table_static_alias(key, line, len, retp);
		break;

	case K_CREDENTIALS:
		ret = table_static_credentials(key, line, len, retp);
		break;

	case K_DOMAIN:
		ret = table_static_domain(key, line, len, retp);
		break;

	case K_NETADDR:
		ret = table_static_netaddr(key, line, len, retp);
		break;

	case K_SOURCE:
		ret = table_static_source(key, line, len, retp);
		break;

	case K_USERINFO:
		ret = table_static_userinfo(key, line, len, retp);
		break;

	case K_MAILADDR:
		ret = table_static_mailaddr(key, line, len, retp);
		break;

	case K_ADDRNAME:
		ret = table_static_addrname(key, line, len, retp);
		break;

	default:
		ret = -1;
	}

	free(line);

	return ret;
}

static int
table_static_fetch(void *hdl, enum table_service service, char **retp)
{
	struct table   *t = hdl;
	const char     *k;
	char	       *line;

	if (! dict_iter(&t->t_dict, &t->t_iter, &k, (void **)NULL)) {
		t->t_iter = NULL;
		if (! dict_iter(&t->t_dict, &t->t_iter, &k, (void **)NULL))
			return 0;
	}

	if (retp == NULL)
		return 1;

	if ((line = strdup(k)) == NULL)
		return -1;

	*retp = line;

	return 1;
}

static int
table_static_credentials(const char *key, char *line, size_t len, void **retp)
{
	struct credentials	*creds;
	char			*p;

	/* credentials are stored as user:password */
	if (len < 3)
		return -1;

	/* too big to fit in a smtp session line */
	if (len >= MAX_LINE_SIZE)
		return -1;

	p = strchr(line, ':');
	if (p == NULL)
		return -1;

	if (p == line || p == line + len - 1)
		return -1;
	*p++ = '\0';

	creds = xcalloc(1, sizeof *creds, "table_static_credentials");
	if (strlcpy(creds->username, line, sizeof(creds->username))
	    >= sizeof(creds->username))
		goto err;

	if (strlcpy(creds->password, p, sizeof(creds->password))
	    >= sizeof(creds->password))
		goto err;

	*retp = creds;
	return 1;

err:
	*retp = NULL;
	free(creds);
	return -1;
}

static int
table_static_alias(const char *key, char *line, size_t len, void **retp)
{
	struct expand		*xp;

	xp = xcalloc(1, sizeof *xp, "table_static_alias");
	if (! expand_line(xp, line, 1))
		goto error;
	*retp = xp;
	return 1;

error:
	*retp = NULL;
	expand_free(xp);
	return -1;
}

static int
table_static_netaddr(const char *key, char *line, size_t len, void **retp)
{
	struct netaddr		*netaddr;

	netaddr = xcalloc(1, sizeof *netaddr, "table_static_netaddr");
	if (! text_to_netaddr(netaddr, line))
		goto error;
	*retp = netaddr;
	return 1;

error:
	*retp = NULL;
	free(netaddr);
	return -1;
}

static int
table_static_source(const char *key, char *line, size_t len, void **retp)
{
	struct source	*source = NULL;

	source = xcalloc(1, sizeof *source, "table_static_source");
	if (inet_pton(AF_INET6, line, &source->addr.in6) != 1)
		if (inet_pton(AF_INET, line, &source->addr.in4) != 1)
			goto error;
	*retp = source;
	return 1;

error:
	*retp = NULL;
	free(source);
	return 0;
}

static int
table_static_domain(const char *key, char *line, size_t len, void **retp)
{
	struct destination	*destination;

	destination = xcalloc(1, sizeof *destination, "table_static_domain");
	if (strlcpy(destination->name, line, sizeof destination->name)
	    >= sizeof destination->name)
		goto error;
	*retp = destination;
	return 1;

error:
	*retp = NULL;
	free(destination);
	return -1;
}

static int
table_static_userinfo(const char *key, char *line, size_t len, void **retp)
{
	struct userinfo		*userinfo;

	userinfo = xcalloc(1, sizeof *userinfo, "table_static_userinfo");
	if (! text_to_userinfo(userinfo, line))
	    goto error;
	*retp = userinfo;
	return 1;

error:
	*retp = NULL;
	free(userinfo);
	return -1;
}

static int
table_static_mailaddr(const char *key, char *line, size_t len, void **retp)
{
	struct mailaddr		*mailaddr;

	mailaddr = xcalloc(1, sizeof *mailaddr, "table_static_mailaddr");
	if (! text_to_mailaddr(mailaddr, line))
	    goto error;
	*retp = mailaddr;
	return 1;

error:
	*retp = NULL;
	free(mailaddr);
	return -1;
}

static int
table_static_addrname(const char *key, char *line, size_t len, void **retp)
{
	struct addrname		*addrname;

	addrname = xcalloc(1, sizeof *addrname, "table_static_addrname");

	if (inet_pton(AF_INET6, key, &addrname->addr.in6) != 1)
		if (inet_pton(AF_INET, key, &addrname->addr.in4) != 1)
			goto error;

	if (strlcpy(addrname->name, line, sizeof addrname->name)
	    >= sizeof addrname->name)
		goto error;

	*retp = addrname;
	return 1;

error:
	*retp = NULL;
	free(addrname);
	return -1;
}
