/*	$OpenBSD: aliases.c,v 1.25 2009/11/03 20:55:23 gilles Exp $	*/

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
#include <db.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

#include "smtpd.h"

int aliases_expand_include(struct aliaseslist *, char *);
int alias_is_filter(struct alias *, char *, size_t);
int alias_is_username(struct alias *, char *, size_t);
int alias_is_address(struct alias *, char *, size_t);
int alias_is_filename(struct alias *, char *, size_t);
int alias_is_include(struct alias *, char *, size_t);

int
aliases_exist(struct smtpd *env, objid_t mapid, char *username)
{
	char buf[MAXLOGNAME];
	int ret;
	DBT key;
	DBT val;
	DB *aliasesdb;
	struct map *map;

	map = map_find(env, mapid);
	if (map == NULL)
		return 0;

	aliasesdb = dbopen(map->m_config, O_RDONLY, 0600, DB_HASH, NULL);
	if (aliasesdb == NULL) {
		log_warn("aliases_exist: dbopen: %s", map->m_config);
		return 0;
	}

	lowercase(buf, username, sizeof(buf));

	key.data = buf;
	key.size = strlen(key.data) + 1;

	ret = aliasesdb->get(aliasesdb, &key, &val, 0);
	if (ret == -1)
		log_warn("aliases_exist");
	aliasesdb->close(aliasesdb);

	return (ret == 0);
}

int
aliases_get(struct smtpd *env, objid_t mapid, struct aliaseslist *aliases, char *username)
{
	char buf[MAXLOGNAME];
	int ret;
	DBT key;
	DBT val;
	DB *aliasesdb;
	size_t nbaliases, nbsave;
	struct alias alias;
	struct alias *aliasp;
	struct alias *nextalias;
	struct map *map;

	map = map_find(env, mapid);
	if (map == NULL)
		return 0;

	aliasesdb = dbopen(map->m_config, O_RDONLY, 0600, DB_HASH, NULL);
	if (aliasesdb == NULL) {
		log_warn("aliases_get: dbopen: %s", map->m_config);
		return 0;
	}

	lowercase(buf, username, sizeof(buf));

	key.data = buf;
	key.size = strlen(key.data) + 1;

	if ((ret = aliasesdb->get(aliasesdb, &key, &val, 0)) != 0) {
		if (ret == -1)
			log_warn("aliases_get");
		aliasesdb->close(aliasesdb);
		return 0;
	}

	nbsave = nbaliases = val.size / sizeof(struct alias);
	if (nbaliases == 0) {
		aliasesdb->close(aliasesdb);
		return 0;
	}

	nextalias = (struct alias *)val.data;
	do {
		alias = *nextalias;
		++nextalias;
		if (alias.type == ALIAS_INCLUDE) {
			aliases_expand_include(aliases, alias.u.filename);
		}
		else {
			aliasp = calloc(1, sizeof(struct alias));
			if (aliasp == NULL)
				fatal("aliases_get: calloc");
			*aliasp = alias;
			TAILQ_INSERT_HEAD(aliases, aliasp, entry);
		}
	} while (--nbaliases);
	aliasesdb->close(aliasesdb);
	return nbsave;
}

int
aliases_vdomain_exists(struct smtpd *env, objid_t mapid, char *hostname)
{
	int	ret;
	DBT	key;
	DBT	val;
	DB     *vtable;
	struct map *map;
	char	strkey[MAX_LINE_SIZE];

	map = map_find(env, mapid);
	if (map == NULL)
		return 0;

	vtable = dbopen(map->m_config, O_RDONLY, 0600, DB_HASH, NULL);
	if (vtable == NULL) {
		log_warn("aliases_vdomain_exists: dbopen: %s", map->m_config);
		return 0;
	}

	if (! bsnprintf(strkey, sizeof(strkey), "%s", hostname)) {
		vtable->close(vtable);
		return 0;
	}
	lowercase(strkey, strkey, sizeof(strkey));

	key.data = strkey;
	key.size = strlen(key.data) + 1;

	ret = vtable->get(vtable, &key, &val, 0);
	if (ret == -1)
		log_warn("aliases_vdomain_exists");

	vtable->close(vtable);

	return (ret == 0);
}

int
aliases_virtual_exist(struct smtpd *env, objid_t mapid, struct path *path)
{
	int ret;
	DBT key;
	DBT val;
	DB *aliasesdb;
	struct map *map;
	char	strkey[MAX_LINE_SIZE];

	map = map_find(env, mapid);
	if (map == NULL)
		return 0;

	aliasesdb = dbopen(map->m_config, O_RDONLY, 0600, DB_HASH, NULL);
	if (aliasesdb == NULL) {
		log_warn("aliases_virtual_exist: dbopen: %s", map->m_config);
		return 0;
	}

	if (! bsnprintf(strkey, sizeof(strkey), "%s@%s", path->user,
		path->domain)) {
		aliasesdb->close(aliasesdb);
		return 0;
	}

	lowercase(strkey, strkey, sizeof(strkey));

	key.data = strkey;
	key.size = strlen(key.data) + 1;

	if ((ret = aliasesdb->get(aliasesdb, &key, &val, 0)) != 0) {
		if (ret == -1)
			log_warn("aliases_virtual_exist");

		if (! bsnprintf(strkey, sizeof(strkey), "@%s", path->domain)) {
			aliasesdb->close(aliasesdb);
			return 0;
		}

		lowercase(strkey, strkey, sizeof(strkey));

		key.data = strkey;
		key.size = strlen(key.data) + 1;

		ret = aliasesdb->get(aliasesdb, &key, &val, 0);
	}
	if (ret == -1)
		log_warn("aliases_virtual_exist");
	aliasesdb->close(aliasesdb);

	return (ret == 0);
}

int
aliases_virtual_get(struct smtpd *env, objid_t mapid,
    struct aliaseslist *aliases, struct path *path)
{
	int ret;
	DBT key;
	DBT val;
	DB *aliasesdb;
	size_t nbaliases, nbsave;
	struct alias alias;
	struct alias *aliasp;
	struct alias *nextalias;
	struct map *map;
	char	strkey[MAX_LINE_SIZE];

	map = map_find(env, mapid);
	if (map == NULL)
		return 0;

	aliasesdb = dbopen(map->m_config, O_RDONLY, 0600, DB_HASH, NULL);
	if (aliasesdb == NULL) {
		log_warn("aliases_virtual_get: dbopen: %s", map->m_config);
		return 0;
	}

	if (! bsnprintf(strkey, sizeof(strkey), "%s@%s", path->user,
		path->domain)) {
		aliasesdb->close(aliasesdb);
		return 0;
	}

	lowercase(strkey, strkey, sizeof(strkey));

	key.data = strkey;
	key.size = strlen(key.data) + 1;

	if ((ret = aliasesdb->get(aliasesdb, &key, &val, 0)) != 0) {
		if (ret == -1)
			log_warn("aliases_virtual_get");

		if (! bsnprintf(strkey, sizeof(strkey), "@%s", path->domain)) {
			aliasesdb->close(aliasesdb);
			return 0;
		}

		lowercase(strkey, strkey, sizeof(strkey));

		key.data = strkey;
		key.size = strlen(key.data) + 1;

		if ((ret = aliasesdb->get(aliasesdb, &key, &val, 0)) != 0) {
			if (ret == -1)
				log_warn("aliases_virtual_get");
			aliasesdb->close(aliasesdb);
			return 0;
		}
	}

	nbsave = nbaliases = val.size / sizeof(struct alias);
	if (nbaliases == 0) {
		aliasesdb->close(aliasesdb);
		return 0;
	}

	nextalias = (struct alias *)val.data;
	do {
		alias = *nextalias;
		++nextalias;
		if (alias.type == ALIAS_INCLUDE) {
			aliases_expand_include(aliases, alias.u.filename);
		}
		else {
			aliasp = calloc(1, sizeof(struct alias));
			if (aliasp == NULL)
				fatal("aliases_virtual_get: calloc");
			*aliasp = alias;
			TAILQ_INSERT_HEAD(aliases, aliasp, entry);
		}
	} while (--nbaliases);
	aliasesdb->close(aliasesdb);
	return nbsave;
}

int
aliases_expand_include(struct aliaseslist *aliases, char *filename)
{
	FILE *fp;
	char *line;
	size_t len;
	size_t lineno = 0;
	char delim[] = { '\\', '#' };
	struct alias alias;
	struct alias *aliasp;

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
		if (! alias_parse(&alias, line)) {
			log_warnx("could not parse include entry \"%s\".", line);
		}

		if (alias.type == ALIAS_INCLUDE) {
			log_warnx("nested inclusion is not supported.");
		}
		else {
			aliasp = calloc(1, sizeof(struct alias));
			if (aliasp == NULL)
				fatal("aliases_expand_include: calloc");
			*aliasp = alias;
			TAILQ_INSERT_TAIL(aliases, aliasp, entry);
		}

		free(line);
	}

	fclose(fp);
	return 1;
}

int
alias_parse(struct alias *alias, char *line)
{
	size_t i;
	int (*f[])(struct alias *, char *, size_t) = {
		alias_is_include,
		alias_is_filter,
		alias_is_filename,
		alias_is_address,
		alias_is_username
	};
	char *wsp;

	/* remove ending whitespaces */
	wsp = line + strlen(line);
	while (wsp != line) {
		if (*wsp != '\0' && !isspace((int)*wsp))
			break;
		*wsp-- = '\0';
	}

	for (i = 0; i < sizeof(f) / sizeof(void *); ++i) {
		bzero(alias, sizeof(struct alias));
		if (f[i](alias, line, strlen(line)))
			break;
	}
	if (i == sizeof(f) / sizeof(void *))
		return 0;

	return 1;
}


int
alias_is_filter(struct alias *alias, char *line, size_t len)
{
	if (strncmp(line, "\"|", 2) == 0 &&
	    line[len - 1] == '"') {
		if (strlcpy(alias->u.filter, line, sizeof(alias->u.filter)) >=
		    sizeof(alias->u.filter))
			return 0;
		alias->type = ALIAS_FILTER;
		return 1;
	}
	return 0;
}

int
alias_is_username(struct alias *alias, char *line, size_t len)
{
	if (strlcpy(alias->u.username, line,
	    sizeof(alias->u.username)) >= sizeof(alias->u.username))
		return 0;

	while (*line) {
		if (!isalnum((int)*line) &&
		    *line != '_' && *line != '.' && *line != '-')
			return 0;
		++line;
	}

	alias->type = ALIAS_USERNAME;
	return 1;
}

int
alias_is_address(struct alias *alias, char *line, size_t len)
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
	strlcpy(alias->u.path.user, line, sizeof(alias->u.path.user));
	strlcpy(alias->u.path.domain, domain, sizeof(alias->u.path.domain));

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

	alias->type = ALIAS_ADDRESS;
	return 1;
}

int
alias_is_filename(struct alias *alias, char *line, size_t len)
{
	if (*line != '/')
		return 0;

	if (strlcpy(alias->u.filename, line,
	    sizeof(alias->u.filename)) >= sizeof(alias->u.filename))
		return 0;
	alias->type = ALIAS_FILENAME;
	return 1;
}

int
alias_is_include(struct alias *alias, char *line, size_t len)
{
	if (strncasecmp(":include:", line, 9) != 0)
		return 0;

	if (! alias_is_filename(alias, line + 9, len - 9))
		return 0;

	alias->type = ALIAS_INCLUDE;
	return 1;
}
