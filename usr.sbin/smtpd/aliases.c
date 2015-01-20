/*	$OpenBSD: aliases.c,v 1.66 2015/01/20 17:37:54 deraadt Exp $	*/

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
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <util.h>

#include "smtpd.h"
#include "log.h"

static int aliases_expand_include(struct expand *, const char *);

int
aliases_get(struct expand *expand, const char *username)
{
	struct expandnode      *xn;
	char			buf[SMTPD_MAXLOCALPARTSIZE];
	size_t			nbaliases;
	int			ret;
	union lookup		lk;
	struct table	       *mapping = NULL;
	struct table	       *userbase = NULL;

	mapping = expand->rule->r_mapping;
	userbase = expand->rule->r_userbase;
	
	xlowercase(buf, username, sizeof(buf));
	ret = table_lookup(mapping, NULL, buf, K_ALIAS, &lk);
	if (ret <= 0)
		return ret;

	/* foreach node in table_alias expandtree, we merge */
	nbaliases = 0;
	RB_FOREACH(xn, expandtree, &lk.expand->tree) {
		if (xn->type == EXPAND_INCLUDE)
			nbaliases += aliases_expand_include(expand,
			    xn->u.buffer);
		else {
			xn->mapping = mapping;
			xn->userbase = userbase;
			expand_insert(expand, xn);
			nbaliases++;
		}
	}

	expand_free(lk.expand);

	log_debug("debug: aliases_get: returned %zd aliases", nbaliases);
	return nbaliases;
}

int
aliases_virtual_check(struct table *table, const struct mailaddr *maddr)
{
	char			buf[LINE_MAX];
	char		       *pbuf;
	int			ret;

	if (! bsnprintf(buf, sizeof(buf), "%s@%s", maddr->user,
		maddr->domain))
		return 0;	
	xlowercase(buf, buf, sizeof(buf));

	/* First, we lookup for full entry: user@domain */
	ret = table_lookup(table, NULL, buf, K_ALIAS, NULL);
	if (ret < 0)
		return (-1);
	if (ret)
		return 1;

	/* Failed ? We lookup for username only */
	pbuf = strchr(buf, '@');
	*pbuf = '\0';
	ret = table_lookup(table, NULL, buf, K_ALIAS, NULL);
	if (ret < 0)
		return (-1);
	if (ret)
		return 1;

	*pbuf = '@';
	/* Failed ? We lookup for catch all for virtual domain */
	ret = table_lookup(table, NULL, pbuf, K_ALIAS, NULL);
	if (ret < 0)
		return (-1);
	if (ret)
		return 1;

	/* Failed ? We lookup for a *global* catch all */
	ret = table_lookup(table, NULL, "@", K_ALIAS, NULL);
	if (ret <= 0)
		return (ret);

	return 1;
}

int
aliases_virtual_get(struct expand *expand, const struct mailaddr *maddr)
{
	struct expandnode      *xn;
	union lookup		lk;
	char			buf[LINE_MAX];
	char		       *pbuf;
	int			nbaliases;
	int			ret;
	struct table	       *mapping = NULL;
	struct table	       *userbase = NULL;

	mapping = expand->rule->r_mapping;
	userbase = expand->rule->r_userbase;

	if (! bsnprintf(buf, sizeof(buf), "%s@%s", maddr->user,
		maddr->domain))
		return 0;	
	xlowercase(buf, buf, sizeof(buf));

	/* First, we lookup for full entry: user@domain */
	ret = table_lookup(mapping, NULL, buf, K_ALIAS, &lk);
	if (ret < 0)
		return (-1);
	if (ret)
		goto expand;

	/* Failed ? We lookup for username only */
	pbuf = strchr(buf, '@');
	*pbuf = '\0';
	ret = table_lookup(mapping, NULL, buf, K_ALIAS, &lk);
	if (ret < 0)
		return (-1);
	if (ret)
		goto expand;

	*pbuf = '@';
	/* Failed ? We lookup for catch all for virtual domain */
	ret = table_lookup(mapping, NULL, pbuf, K_ALIAS, &lk);
	if (ret < 0)
		return (-1);
	if (ret)
		goto expand;

	/* Failed ? We lookup for a *global* catch all */
	ret = table_lookup(mapping, NULL, "@", K_ALIAS, &lk);
	if (ret <= 0)
		return (ret);

expand:
	/* foreach node in table_virtual expand, we merge */
	nbaliases = 0;
	RB_FOREACH(xn, expandtree, &lk.expand->tree) {
		if (xn->type == EXPAND_INCLUDE)
			nbaliases += aliases_expand_include(expand,
			    xn->u.buffer);
		else {
			xn->mapping = mapping;
			xn->userbase = userbase;
			expand_insert(expand, xn);
			nbaliases++;
		}
	}

	expand_free(lk.expand);

	log_debug("debug: aliases_virtual_get: '%s' resolved to %d nodes",
	    buf, nbaliases);

	return nbaliases;
}

static int
aliases_expand_include(struct expand *expand, const char *filename)
{
	FILE *fp;
	char *line;
	size_t len, lineno = 0;
	char delim[3] = { '\\', '#', '\0' };

	fp = fopen(filename, "r");
	if (fp == NULL) {
		log_warn("warn: failed to open include file \"%s\".", filename);
		return 0;
	}

	while ((line = fparseln(fp, &len, &lineno, delim, 0)) != NULL) {
		expand_line(expand, line, 0);
		free(line);
	}

	fclose(fp);
	return 1;
}
