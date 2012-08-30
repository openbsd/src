/*
 * Copyright (c) 2010 Gilles Chehade <gilles@openbsd.org>
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
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "aldap.h"
#include "map_ldap.h"
#include "log.h"


/* ldap backend */
static void			*map_ldap_open(struct map *);
static void			*map_ldap_lookup(void *, char *, enum map_kind);
static void			 map_ldap_close(void *);

static void			*map_ldap_alias(void *, char *);
static void			*map_ldap_virtual(void *, char *);
static struct ldap_conf		*ldapconf_findbyname(const char *);
static struct aldap		*ldap_client_connect(struct ldap_conf *);
static struct ldap_conf		*ldap_parse_configuration(const char *);

struct ldap_confs ldap_confs = TAILQ_HEAD_INITIALIZER(ldap_confs);

struct map_backend map_backend_ldap = {
	map_ldap_open,
	map_ldap_close,
	map_ldap_lookup,
	NULL
};

enum {
	BASEDN_OPT,
	FILTER_OPT,
	ATTR_OPT,
	URL_OPT,
	USER_OPT,
	PWD_OPT
};

struct conf_option {
	const char* opt_name;
	int			opt_flag;
};

const struct conf_option cf_keywords[] = {
	{ "basedn", BASEDN_OPT },
	{ "filter", FILTER_OPT },
	{ "attribute", ATTR_OPT },
	{ "url", URL_OPT },
	{ "username", USER_OPT },
	{ "password", PWD_OPT }
};

static int
setoptvalue(struct ldap_conf *lconf, int opt, char *value)
{
	switch (opt) {
	case BASEDN_OPT:
		if (strlcpy(lconf->m_ldapbasedn, value, sizeof(lconf->m_ldapbasedn))
		    >= sizeof(lconf->m_ldapbasedn)) {
			warnx("ldap base dn too long");
			return 0;
		}
		break;
	case FILTER_OPT:
		if (strlcpy(lconf->m_ldapfilter, value, sizeof(lconf->m_ldapfilter))
		    >= sizeof(lconf->m_ldapfilter)) {
			warnx("ldap filter too long");
			return 0;
		}
		break;
	case ATTR_OPT:
		if (strlcpy(lconf->m_ldapattr, value, sizeof(lconf->m_ldapattr))
		    >= sizeof(lconf->m_ldapattr)) {
			warnx("ldap attribute too long");
			return 0;
		}
		break;
	case URL_OPT:
		if (strlcpy(lconf->url, value, sizeof(lconf->url))
		    >= sizeof(lconf->url)) {
			warnx("ldap url too long");
			return 0;
		}
		break;
	case USER_OPT:
		if (strlcpy(lconf->username, value, sizeof(lconf->username))
		    >= sizeof(lconf->username)) {
			warnx("ldap username too long");
			return 0;
		}
		break;
	case PWD_OPT:
		if (strlcpy(lconf->password, value, sizeof(lconf->password))
		    >= sizeof(lconf->password)) {
			warnx("ldap password too long");
			return 0;
		}
		break;
	}
	return 1;
}

static struct ldap_conf *
ldap_parse_configuration(const char *path)
{
	char	*buf, *lbuf;
	char	*p;
	size_t	 len, wlen, i;
	FILE	*fp;
	struct  ldap_conf *ldapconf;

	if ((fp = fopen(path, "r")) == NULL) {
		warnx("ldap_parse_configuration: can't open configuration file '%s'", path);
		return NULL;
	}

	if ((ldapconf = malloc(sizeof(struct ldap_conf))) == NULL)
		err(1, "malloc");

	if (strlcpy(ldapconf->identifier, path, sizeof(ldapconf->identifier))
			>= sizeof(ldapconf->identifier)) {
		free(ldapconf);
		err(1, "path name too long");
	}

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		p = buf;
		p += strspn(p, " \t");
		wlen = strcspn(p, "= \t");

		for (i = 0; i < nitems(cf_keywords); ++i) {
			if (!strncmp(cf_keywords[i].opt_name, p, wlen) &&
					strlen(cf_keywords[i].opt_name) == wlen)
				break;
		}

		if (i == nitems(cf_keywords)) {
			warnx("ldap configuration file '%s' syntax error", path);
			goto err;
		}

		p += wlen;
		p += strspn(p, " =\t");
		wlen = strcspn(p, " \t");

		if (wlen == 0) {
			warnx("ldap configuration file '%s' invalid option argument", path);
			goto err;
		}

		*(p+wlen) = '\0';
		if (! setoptvalue(ldapconf, cf_keywords[i].opt_flag, p))
			goto err;
	}
	free(lbuf);

	TAILQ_INSERT_TAIL(&ldap_confs, ldapconf, entry);

	return ldapconf;

err:
	free(ldapconf);
	free(lbuf);
	return NULL;
}

static void *
map_ldap_open(struct map *map)
{
	struct ldap_conf  *ldapconf;
	struct aldap_message *message = NULL;
	struct ldaphandle *ldaphandle = NULL;


	ldapconf = ldapconf_findbyname(map->m_config);
	if (ldapconf == NULL) {
		ldapconf = ldap_parse_configuration(map->m_config);

		if (ldapconf == NULL) {
			warnx("ldap configuration file '%s' parse error", map->m_config);
			return NULL;
		}
	}

	warnx("map_ldap_open: found ldapserverconf '%s' in smtpd.conf", ldapconf->identifier);

	ldaphandle = calloc(1, sizeof(*ldaphandle));
	if (ldaphandle == NULL)
		err(1, "calloc");

	ldaphandle->conf = ldapconf;
	ldaphandle->aldap = ldap_client_connect(ldapconf);
	if (ldaphandle->aldap == NULL) {
		warnx("map_ldap_open: ldap_client_connect error");
		goto err;
	}

	if (aldap_bind(ldaphandle->aldap, ldapconf->username, ldapconf->password) == -1) {
		warnx("map_ldap_open: aldap_bind error");
		goto err;
	}

	if ((message = aldap_parse(ldaphandle->aldap)) == NULL) {
		warnx("map_ldap_open: aldap_parse");
		goto err;
	}

	switch (aldap_get_resultcode(message)) {
	case LDAP_SUCCESS:
		warnx("map_ldap_open: ldap server accepted credentials");
		break;
	case LDAP_INVALID_CREDENTIALS:
		warnx("map_ldap_open: ldap server refused credentials");
		goto err;

	default:
		warnx("map_ldap_open: failed to bind, result #%d", aldap_get_resultcode(message));
		goto err;
	}
	warnx("map_ldap_open: aldap: %p", ldaphandle->aldap);
	return ldaphandle;

err:
	if (ldaphandle->aldap != NULL)
		aldap_close(ldaphandle->aldap);
	free(ldaphandle);
	if (message != NULL)
		aldap_freemsg(message);
	return NULL;
}

static void
map_ldap_close(void *hdl)
{
	struct ldaphandle *ldaphandle = hdl;

	aldap_close(ldaphandle->aldap);
	free(ldaphandle);
}

static void *
map_ldap_lookup(void *hdl, char *key, enum map_kind kind)
{
	void *ret;

	ret = NULL;
	switch (kind) {
	case K_ALIAS:
		ret = map_ldap_alias(hdl, key);
		break;
	case K_VIRTUAL:
		ret = map_ldap_virtual(hdl, key);
		break;
	default:
		break;
	}

	return ret;
}


/* XXX: this should probably be factorized in a map_ldap_getentries function */
static void *
map_ldap_alias(void *hdl, char *key)
{
	struct ldaphandle *ldaphandle = hdl;
	struct aldap *aldap = ldaphandle->aldap;
	struct aldap_message *m = NULL;
	struct map_alias	 *map_alias = NULL;
	struct expandnode	  expnode;
	char *attributes[2];
	char *ldap_attrs[2];
	char **ldapattrsp = ldap_attrs;
	char expandedfilter[MAX_LDAP_FILTERLEN * 2];
	int ret;
	int i;


	bzero(expandedfilter, sizeof(expandedfilter));
	for (i = 0; ldaphandle->conf->m_ldapfilter[i] != '\0'; ++i) {
		if (ldaphandle->conf->m_ldapfilter[i] == '%') {
			if (ldaphandle->conf->m_ldapfilter[i + 1] == 'k') {
				ret = snprintf(expandedfilter, sizeof(expandedfilter), "%s%s", expandedfilter, key);
				if (ret == -1 || ret >= (int)sizeof(expandedfilter))
					return NULL;

				++i;
			}
			continue;
		}
		ret = snprintf(expandedfilter, sizeof(expandedfilter), "%s%c", expandedfilter, ldaphandle->conf->m_ldapfilter[i]);
		if (ret == -1 || ret >= (int)sizeof(expandedfilter))
			return NULL;
	}

	attributes[0] = ldaphandle->conf->m_ldapattr;
	attributes[1] = NULL;

	ret = aldap_search(aldap, ldaphandle->conf->m_ldapbasedn, LDAP_SCOPE_SUBTREE,
	    expandedfilter, attributes, 0, 0, 0);
	if (ret == -1)
		return NULL;

	m = aldap_parse(aldap);
	if (m == NULL)
		return NULL;

	if ((map_alias = calloc(1, sizeof(struct map_alias))) == NULL)
			err(1, NULL);

	if (aldap_match_entry(m, attributes[0], &ldapattrsp) != 1)
		goto error;

	for (i = 0; ldapattrsp[i]; ++i) {
		bzero(&expnode, sizeof(struct expandnode));
		if (!alias_parse(&expnode, ldapattrsp[i]))
			goto error;

		expandtree_increment_node(&map_alias->expandtree, &expnode);
		map_alias->nbnodes++;
	}

	aldap_free_entry(ldapattrsp);
	aldap_freemsg(m);
	return map_alias;

error:
	expandtree_free_nodes(&map_alias->expandtree);
	free(map_alias);
	aldap_freemsg(m);
	return NULL;
}

static void *
map_ldap_virtual(void *hdl, char *key)
{
	struct ldaphandle *ldaphandle = hdl;
	struct aldap *aldap = ldaphandle->aldap;
	struct aldap_message *m = NULL;
	struct map_virtual	 *map_virtual = NULL;
	struct expandnode	  expnode;
	char *attributes[2];
	char *ldap_attrs[2];
	char **ldapattrsp = ldap_attrs;
	char expandedfilter[MAX_LDAP_FILTERLEN * 2];
	int ret;
	int i;


	bzero(expandedfilter, sizeof(expandedfilter));
	for (i = 0; ldaphandle->conf->m_ldapfilter[i] != '\0'; ++i) {
		if (ldaphandle->conf->m_ldapfilter[i] == '%') {
			if (ldaphandle->conf->m_ldapfilter[i + 1] == 'k') {
				ret = snprintf(expandedfilter, sizeof(expandedfilter), "%s%s", expandedfilter, key);
				if (ret == -1 || ret >= (int)sizeof(expandedfilter))
					return NULL;

				++i;
			}
			continue;
		}
		ret = snprintf(expandedfilter, sizeof(expandedfilter), "%s%c", expandedfilter, ldaphandle->conf->m_ldapfilter[i]);
		if (ret == -1 || ret >= (int)sizeof(expandedfilter))
			return NULL;
	}

	attributes[0] = ldaphandle->conf->m_ldapattr;
	attributes[1] = NULL;

	ret = aldap_search(aldap, ldaphandle->conf->m_ldapbasedn, LDAP_SCOPE_SUBTREE,
	    expandedfilter, attributes, 0, 0, 0);
	if (ret == -1)
		return NULL;

	m = aldap_parse(aldap);
	if (m == NULL)
		return NULL;

	if ((map_virtual = calloc(1, sizeof(struct map_virtual))) == NULL)
			err(1, NULL);

	/* domain key, discard value */
	if (strchr(key, '@') == NULL)
		return map_virtual;

	if (aldap_match_entry(m, attributes[0], &ldapattrsp) != 1)
		goto error;

	for (i = 0; ldapattrsp[i]; ++i) {
		bzero(&expnode, sizeof(struct expandnode));
		if (!alias_parse(&expnode, ldapattrsp[i]))
			goto error;

		expandtree_increment_node(&map_virtual->expandtree, &expnode);
		map_virtual->nbnodes++;
	}

	aldap_free_entry(ldapattrsp);
	aldap_freemsg(m);
	return map_virtual;

error:
	expandtree_free_nodes(&map_virtual->expandtree);
	free(map_virtual);
	aldap_freemsg(m);
	return NULL;
}

static struct ldap_conf *
ldapconf_findbyname(const char *identifier)
{
	struct ldap_conf	*ldapconf = NULL;

	TAILQ_FOREACH(ldapconf, &ldap_confs, entry) {
		if (strcmp(ldapconf->identifier, identifier) == 0)
			break;
	}
	return ldapconf;
}

static struct aldap *
ldap_client_connect(struct ldap_conf *addr)
{
	struct aldap_url	lu;
	struct addrinfo		 hints, *res0, *res;
	int			 error;

	char *url;
	int fd = -1;

	if ((url = strdup(addr->url)) == NULL)
		err(1, NULL);

	if (aldap_parse_url(url, &lu) != 1) {
		warnx("aldap_parse_url fail");
		goto err;
	}

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM; /* DUMMY */
	error = getaddrinfo(lu.host, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		goto err;
	if (error) {
		log_warnx("ldap_client_connect: could not parse \"%s\": %s", lu.host,
		    gai_strerror(error));
		goto err;
	}

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET && res->ai_family != AF_INET6)
			continue;

		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd == -1)
			continue;

		if (res->ai_family == AF_INET) {
			struct sockaddr_in sin4 = *(struct sockaddr_in *)res->ai_addr;
			sin4.sin_port = htons(lu.port);
			if (connect(fd, (struct sockaddr *)&sin4, res->ai_addrlen) == 0)
				return aldap_init(fd);
		}
		else if (res->ai_family == AF_INET6) {
			struct sockaddr_in6 sin6 = *(struct sockaddr_in6 *)res->ai_addr;
			sin6.sin6_port = htons(lu.port);
			if (connect(fd, (struct sockaddr *)&sin6, res->ai_addrlen) == 0)
				return aldap_init(fd);
		}

		close(fd);
	}

err:
	if (fd != -1)
		close(fd);
	free(url);
	return NULL;
}
