/*	$OpenBSD: aliases.c,v 1.49 2012/09/18 12:54:56 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

#include "smtpd.h"
#include "log.h"

static int aliases_expand_include(struct expandtree *, const char *);
static int alias_is_filter(struct expandnode *, const char *, size_t);
static int alias_is_username(struct expandnode *, const char *, size_t);
static int alias_is_address(struct expandnode *, const char *, size_t);
static int alias_is_filename(struct expandnode *, const char *, size_t);
static int alias_is_include(struct expandnode *, const char *, size_t);

int
aliases_exist(objid_t mapid, char *username)
{
	struct map_alias *map_alias;
	char buf[MAX_LOCALPART_SIZE];

	xlowercase(buf, username, sizeof(buf));
	map_alias = map_lookup(mapid, buf, K_ALIAS);
	if (map_alias == NULL)
		return 0;

	/* XXX - for now the map API always allocate */
	log_debug("aliases_exist: '%s' exists with %zd expansion nodes",
	    username, map_alias->nbnodes);

	expandtree_free_nodes(&map_alias->expandtree);
	free(map_alias);

	return 1;
}

int
aliases_get(objid_t mapid, struct expandtree *expandtree, char *username)
{
	struct map_alias *map_alias;
	struct expandnode *expnode;
	char buf[MAX_LOCALPART_SIZE];
	size_t nbaliases;

	xlowercase(buf, username, sizeof(buf));
	map_alias = map_lookup(mapid, buf, K_ALIAS);
	if (map_alias == NULL)
		return 0;

	/* foreach node in map_alias expandtree, we merge */
	nbaliases = 0;
	RB_FOREACH(expnode, expandtree, &map_alias->expandtree) {
		strlcpy(expnode->as_user, SMTPD_USER, sizeof (expnode->as_user));
		if (expnode->type == EXPAND_INCLUDE)
			nbaliases += aliases_expand_include(expandtree, expnode->u.buffer);
		else {
			expandtree_increment_node(expandtree, expnode);
			nbaliases++;
		}
	}

	expandtree_free_nodes(&map_alias->expandtree);
	free(map_alias);

	log_debug("aliases_get: returned %zd aliases", nbaliases);
	return nbaliases;
}

int
aliases_vdomain_exists(objid_t mapid, char *hostname)
{
	struct map_virtual *map_virtual;
	char buf[MAXHOSTNAMELEN];

	xlowercase(buf, hostname, sizeof(buf));
	map_virtual = map_lookup(mapid, buf, K_VIRTUAL);
	if (map_virtual == NULL)
		return 0;

	/* XXX - for now the map API always allocate */
	log_debug("aliases_vdomain_exist: '%s' exists", hostname);
	expandtree_free_nodes(&map_virtual->expandtree);
	free(map_virtual);

	return 1;
}

int
aliases_virtual_exist(objid_t mapid, struct mailaddr *maddr)
{
	struct map_virtual *map_virtual;
	char buf[MAX_LINE_SIZE];
	char *pbuf = buf;

	if (! bsnprintf(buf, sizeof(buf), "%s@%s", maddr->user,
		maddr->domain))
		return 0;
	xlowercase(buf, buf, sizeof(buf));

	map_virtual = map_lookup(mapid, buf, K_VIRTUAL);
	if (map_virtual == NULL) {
		pbuf = strchr(buf, '@');
		map_virtual = map_lookup(mapid, pbuf, K_VIRTUAL);
	}
	if (map_virtual == NULL)
		return 0;

	log_debug("aliases_virtual_exist: '%s' exists", pbuf);
	expandtree_free_nodes(&map_virtual->expandtree);
	free(map_virtual);

	return 1;
}

int
aliases_virtual_get(objid_t mapid, struct expandtree *expandtree,
    struct mailaddr *maddr)
{
	struct map_virtual *map_virtual;
	struct expandnode *expnode;
	char buf[MAX_LINE_SIZE];
	char *pbuf = buf;
	int nbaliases;

	if (! bsnprintf(buf, sizeof(buf), "%s@%s", maddr->user,
		maddr->domain))
		return 0;
	xlowercase(buf, buf, sizeof(buf));

	map_virtual = map_lookup(mapid, buf, K_VIRTUAL);
	if (map_virtual == NULL) {
		pbuf = strchr(buf, '@');
		map_virtual = map_lookup(mapid, pbuf, K_VIRTUAL);
	}
	if (map_virtual == NULL)
		return 0;

	/* foreach node in map_virtual expandtree, we merge */
	nbaliases = 0;
	RB_FOREACH(expnode, expandtree, &map_virtual->expandtree) {
		strlcpy(expnode->as_user, SMTPD_USER, sizeof (expnode->as_user));
		if (expnode->type == EXPAND_INCLUDE)
			nbaliases += aliases_expand_include(expandtree, expnode->u.buffer);
		else {
			expandtree_increment_node(expandtree, expnode);
			nbaliases++;
		}
	}

	expandtree_free_nodes(&map_virtual->expandtree);
	free(map_virtual);
	log_debug("aliases_virtual_get: '%s' resolved to %d nodes", pbuf, nbaliases);

	return nbaliases;
}

static int
aliases_expand_include(struct expandtree *expandtree, const char *filename)
{
	FILE *fp;
	char *line;
	size_t len;
	size_t lineno = 0;
	char delim[] = { '\\', '#' };
	struct expandnode expnode;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		log_warn("failed to open include file \"%s\".", filename);
		return 0;
	}

	while ((line = fparseln(fp, &len, &lineno, delim, 0)) != NULL) {
		if (len == 0) {
			free(line);
			continue;
		}

		bzero(&expnode, sizeof(struct expandnode));
		if (! alias_parse(&expnode, line)) {
			log_warnx("could not parse include entry \"%s\".", line);
		}

		if (expnode.type == EXPAND_INCLUDE)
			log_warnx("nested inclusion is not supported.");
		else
			expandtree_increment_node(expandtree, &expnode);

		free(line);
	}

	fclose(fp);
	return 1;
}

int
alias_parse(struct expandnode *alias, char *line)
{
	size_t l;
	char *wsp;

	/* remove ending whitespaces */
	wsp = line + strlen(line);
	while (wsp != line) {
		if (*wsp != '\0' && !isspace((int)*wsp))
			break;
		*wsp-- = '\0';
	}

	l = strlen(line);
	if (alias_is_include(alias, line, l) ||
	    alias_is_filter(alias, line, l) ||
	    alias_is_filename(alias, line, l) ||
	    alias_is_address(alias, line, l) ||
	    alias_is_username(alias, line, l))
		return (1);

	return (0);
}


static int
alias_is_filter(struct expandnode *alias, const char *line, size_t len)
{
	if (*line == '|') {
		if (strlcpy(alias->u.buffer, line + 1,
			sizeof(alias->u.buffer)) >= sizeof(alias->u.buffer))
			return 0;
		alias->type = EXPAND_FILTER;
		return 1;
	}
	return 0;
}

static int
alias_is_username(struct expandnode *alias, const char *line, size_t len)
{
	if (strlcpy(alias->u.user, line,
	    sizeof(alias->u.user)) >= sizeof(alias->u.user))
		return 0;

	while (*line) {
		if (!isalnum((int)*line) &&
		    *line != '_' && *line != '.' && *line != '-')
			return 0;
		++line;
	}

	alias->type = EXPAND_USERNAME;
	return 1;
}

static int
alias_is_address(struct expandnode *alias, const char *line, size_t len)
{
	char *domain;

	if (len < 3)	/* x@y */
		return 0;

	domain = strchr(line, '@');
	if (domain == NULL)
		return 0;

	/* @ cannot start or end an address */
	if (domain == line || domain == line + len - 1)
		return 0;

	/* scan pre @ for disallowed chars */
	*domain++ = '\0';
	strlcpy(alias->u.mailaddr.user, line, sizeof(alias->u.mailaddr.user));
	strlcpy(alias->u.mailaddr.domain, domain, sizeof(alias->u.mailaddr.domain));

	while (*line) {
		char allowedset[] = "!#$%*/?|^{}`~&'+-=_.";
		if (!isalnum((int)*line) &&
		    strchr(allowedset, *line) == NULL)
			return 0;
		++line;
	}

	while (*domain) {
		char allowedset[] = "-.";
		if (!isalnum((int)*domain) &&
		    strchr(allowedset, *domain) == NULL)
			return 0;
		++domain;
	}

	alias->type = EXPAND_ADDRESS;
	return 1;
}

static int
alias_is_filename(struct expandnode *alias, const char *line, size_t len)
{
	if (*line != '/')
		return 0;

	if (strlcpy(alias->u.buffer, line,
	    sizeof(alias->u.buffer)) >= sizeof(alias->u.buffer))
		return 0;
	alias->type = EXPAND_FILENAME;
	return 1;
}

static int
alias_is_include(struct expandnode *alias, const char *line, size_t len)
{
	size_t skip;
	
	if (strncasecmp(":include:", line, 9) == 0)
		skip = 9;
	else if (strncasecmp("include:", line, 8) == 0)
		skip = 8;
	else
		return 0;

	if (! alias_is_filename(alias, line + skip, len - skip))
		return 0;

	alias->type = EXPAND_INCLUDE;
	return 1;
}
