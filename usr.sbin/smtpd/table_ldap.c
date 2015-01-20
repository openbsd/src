/*	$OpenBSD: table_ldap.c,v 1.13 2015/01/20 17:37:54 deraadt Exp $	*/

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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "log.h"
#include "aldap.h"

#define MAX_LDAP_IDENTIFIER      32
#define MAX_LDAP_URL             256
#define MAX_LDAP_USERNAME        256
#define MAX_LDAP_PASSWORD        256
#define MAX_LDAP_BASELEN         128
#define MAX_LDAP_FILTERLEN       1024
#define MAX_LDAP_FIELDLEN        128


enum {
	LDAP_ALIAS = 0,
	LDAP_DOMAIN,
	LDAP_CREDENTIALS,
	LDAP_NETADDR,
	LDAP_USERINFO,
	LDAP_SOURCE,
	LDAP_MAILADDR,
	LDAP_ADDRNAME,

	LDAP_MAX
};

#define MAX_ATTRS	6

struct query {
	char	*filter;
	char	*attrs[MAX_ATTRS];
	int	 attrn;
};

static int table_ldap_update(void);
static int table_ldap_check(int, struct dict *, const char *);
static int table_ldap_lookup(int, struct dict *, const char *, char *, size_t);
static int table_ldap_fetch(int, struct dict *, char *, size_t);

static int ldap_config(void);
static int ldap_open(void);
static int ldap_query(const char *, char **, char ***, size_t);
static int ldap_parse_attributes(struct query *, const char *, const char *, size_t);
static int ldap_run_query(int type, const char *, char *, size_t);

static char *config;

static char *url;
static char *username;
static char *password;
static char *basedn;

static struct aldap *aldap;
static struct query queries[LDAP_MAX];

int
main(int argc, char **argv)
{
	int	ch;

	log_init(1);
	log_verbose(~0);

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			log_warnx("warn: table-ldap: bad option");
			return (1);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		log_warnx("warn: table-ldap: bogus argument(s)");
		return (1);
	}

	config = argv[0];

	if (!ldap_config()) {
		log_warnx("warn: table-ldap: could not parse config");
		return (1);
	}

	log_debug("debug: table-ldap: done reading config");

	if (!ldap_open()) {
		log_warnx("warn: table-ldap: failed to connect");
		return (1);
	}

	log_debug("debug: table-ldap: connected");

	table_api_on_update(table_ldap_update);
	table_api_on_check(table_ldap_check);
	table_api_on_lookup(table_ldap_lookup);
	table_api_on_fetch(table_ldap_fetch);
	table_api_dispatch();

	return (0);
}

static int
table_ldap_update(void)
{
	return (1);
}

static int
table_ldap_check(int service, struct dict *params, const char *key)
{
	switch(service) {
	case K_ALIAS:
	case K_DOMAIN:
	case K_CREDENTIALS:
	case K_USERINFO:
	case K_MAILADDR:
		return ldap_run_query(service, key, NULL, 0);
	default:
		return (-1);
	}
}

static int
table_ldap_lookup(int service, struct dict *params, const char *key, char *dst, size_t sz)
{
	switch(service) {
	case K_ALIAS:
	case K_DOMAIN:
	case K_CREDENTIALS:
	case K_USERINFO:
	case K_MAILADDR:
		return ldap_run_query(service, key, dst, sz);
	default:
		return (-1);
	}
}

static int
table_ldap_fetch(int service, struct dict *params, char *dst, size_t sz)
{
	return (-1);
}

static struct aldap *
ldap_connect(const char *addr)
{
	struct aldap_url lu;
	struct addrinfo	 hints, *res0, *res;
	char		*buf;
	int		 error, fd = -1;

	if ((buf = strdup(addr)) == NULL)
		return (NULL);

	/* XXX buf leak */

	if (aldap_parse_url(buf, &lu) != 1) {
		log_warnx("warn: table-ldap: ldap_parse_url fail");
		return (NULL);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM; /* DUMMY */
	error = getaddrinfo(lu.host, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (NULL);
	if (error) {
		log_warnx("warn: table-ldap: could not parse \"%s\": %s",
		    lu.host, gai_strerror(error));
		return (NULL);
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
		fd = -1;
	}

	return (NULL);
}

static int
read_value(char **store, const char *key, const char *value)
{
	log_debug("debug: table-ldap: reading key \"%s\" -> \"%s\"",
	    key, value);

	if (*store) {
		log_warnx("warn: table-ldap: duplicate key %s", key);
		return (0);
	}
	
	if ((*store = strdup(value)) == NULL) {
		log_warn("warn: table-ldap: strdup");
		return (0);
	}

	return (1);
}

static int
ldap_parse_attributes(struct query *query, const char *key, const char *line,
    size_t expect)
{
	char	buffer[1024];
	char   *p;
	size_t	m, n;

	log_debug("debug: table-ldap: parsing attribute \"%s\" (%zu) -> \"%s\"",
	    key, expect, line);

	if (strlcpy(buffer, line, sizeof buffer) >= sizeof buffer)
		return (0);

	m = 1;
	for (p = buffer; *p; ++p) {
		if (*p == ',') {
			*p = 0;
			m++;
		}
	}
	if (expect != m)
		return (0);

	p = buffer;
	for (n = 0; n < expect; ++n)
		query->attrs[n] = NULL;
	for (n = 0; n < m; ++n) {
		query->attrs[n] = strdup(p);
		if (query->attrs[n] == NULL) {
			log_warnx("warn: table-ldap: strdup");
			return (0); /* XXX cleanup */
		}
		p += strlen(p) + 1;
		query->attrn++;
	}
	return (1);
}

static int
ldap_config(void)
{
	size_t		 flen;
	FILE		*fp;
	char		*key, *value, *buf, *lbuf;

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
				log_warn("warn: table-ldap: malloc");
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
			log_warnx("warn: table-ldap: missing value for key %s", key);
			continue;
		}

		if (!strcmp(key, "url"))
			read_value(&url, key, value);
		else if (!strcmp(key, "username"))
			read_value(&username, key, value);
		else if (!strcmp(key, "password"))
			read_value(&password, key, value);
		else if (!strcmp(key, "basedn"))
			read_value(&basedn, key, value);

		else if (!strcmp(key, "alias_filter"))
			read_value(&queries[LDAP_ALIAS].filter, key, value);
		else if (!strcmp(key, "alias_attributes")) {
			ldap_parse_attributes(&queries[LDAP_ALIAS],
			    key, value, 1);
		}

		else if (!strcmp(key, "credentials_filter"))
			read_value(&queries[LDAP_CREDENTIALS].filter, key, value);
		else if (!strcmp(key, "credentials_attributes")) {
			ldap_parse_attributes(&queries[LDAP_CREDENTIALS],
			    key, value, 2);
		}

		else if (!strcmp(key, "domain_filter"))
			read_value(&queries[LDAP_DOMAIN].filter, key, value);
		else if (!strcmp(key, "domain_attributes")) {
			ldap_parse_attributes(&queries[LDAP_DOMAIN],
			    key, value, 1);
		}

		else if (!strcmp(key, "userinfo_filter"))
			read_value(&queries[LDAP_USERINFO].filter, key, value);
		else if (!strcmp(key, "userinfo_attributes")) {
			ldap_parse_attributes(&queries[LDAP_USERINFO],
			    key, value, 3);
		}

		else if (!strcmp(key, "mailaddr_filter"))
			read_value(&queries[LDAP_MAILADDR].filter, key, value);
		else if (!strcmp(key, "mailaddr_attributes")) {
			ldap_parse_attributes(&queries[LDAP_MAILADDR],
			    key, value, 1);
		}
		else
			log_warnx("warn: table-ldap: bogus entry \"%s\"", key);
	}

	free(lbuf);
	fclose(fp);
	return (1);
}

static int
ldap_open(void)
{
	struct aldap_message	*amsg = NULL;

	aldap = ldap_connect(url);
	if (aldap == NULL) {
		log_warnx("warn: table-ldap: ldap_connect error");
		goto err;
	}

	if (aldap_bind(aldap, username, password) == -1) {
		log_warnx("warn: table-ldap: aldap_bind error");
		goto err;
	}

	if ((amsg = aldap_parse(aldap)) == NULL) {
		log_warnx("warn: table-ldap: aldap_parse");
		goto err;
	}

	switch (aldap_get_resultcode(amsg)) {
	case LDAP_SUCCESS:
		log_debug("debug: table-ldap: ldap server accepted credentials");
		break;
	case LDAP_INVALID_CREDENTIALS:
		log_warnx("warn: table-ldap: ldap server refused credentials");
		goto err;
	default:
		log_warnx("warn: table-ldap: failed to bind, result #%d",
		    aldap_get_resultcode(amsg));
		goto err;
	}

	if (amsg)
		aldap_freemsg(amsg);
	return (1);

err:
	if (aldap)
		aldap_close(aldap);
	if (amsg)
		aldap_freemsg(amsg);
	return (0);
}

static int
ldap_query(const char *filter, char **attributes, char ***outp, size_t n)
{
	struct aldap_message		*m = NULL;
	struct aldap_page_control	*pg = NULL;
	int				 ret, found;
	size_t				 i;
	char				 basedn__[MAX_LDAP_BASELEN];
	char				 filter__[MAX_LDAP_FILTERLEN];

	if (strlcpy(basedn__, basedn, sizeof basedn__) >= sizeof basedn__)
		return -1;
	if (strlcpy(filter__, filter, sizeof filter__) >= sizeof filter__)
		return -1;
	found = 0;
	do {
		if ((ret = aldap_search(aldap, basedn__, LDAP_SCOPE_SUBTREE,
			    filter__, NULL, 0, 0, 0, pg)) == -1) {
			log_debug("ret=%d", ret);
			return -1;
		}
		if (pg != NULL) {
			aldap_freepage(pg);
			pg = NULL;
		}

		while ((m = aldap_parse(aldap)) != NULL) {
			if (aldap->msgid != m->msgid)
				goto error;
			if (m->message_type == LDAP_RES_SEARCH_RESULT) {
				if (m->page != NULL && m->page->cookie_len)
					pg = m->page;
				aldap_freemsg(m);
				m = NULL;
				break;
			}
			if (m->message_type != LDAP_RES_SEARCH_ENTRY)
				goto error;

			found = 1;
			for (i = 0; i < n; ++i)
				if (aldap_match_attr(m, attributes[i], &outp[i]) != 1)
					goto error;
			aldap_freemsg(m);
			m = NULL;
		}
	} while (pg != NULL);

	ret = found ? 1 : 0;
	goto end;

error:
	ret = -1;

end:
	if (m)
		aldap_freemsg(m);
	log_debug("debug: table_ldap: ldap_query: filter=%s, ret=%d", filter, ret);
	return ret;
}

static int
ldap_run_query(int type, const char *key, char *dst, size_t sz)
{
	struct query	 *q;
	char		**res[4], filter[MAX_LDAP_FILTERLEN];
	int		  ret, i;

	switch (type) {
	case K_ALIAS:		q = &queries[LDAP_ALIAS];	break;
	case K_DOMAIN:		q = &queries[LDAP_DOMAIN];	break;
	case K_CREDENTIALS:	q = &queries[LDAP_CREDENTIALS];	break;
	case K_NETADDR:		q = &queries[LDAP_NETADDR];	break;
	case K_USERINFO:	q = &queries[LDAP_USERINFO];	break;
	case K_SOURCE:		q = &queries[LDAP_SOURCE];	break;
	case K_MAILADDR:	q = &queries[LDAP_MAILADDR];	break;
	case K_ADDRNAME:	q = &queries[LDAP_ADDRNAME];	break;
	default:
		return (-1);
	}

	if (snprintf(filter, sizeof(filter), q->filter, key)
	    >= (int)sizeof(filter)) {
		log_warnx("warn: table-ldap: filter too large");
		return (-1);
	}

	memset(res, 0, sizeof(res));
	ret = ldap_query(filter, q->attrs, res, q->attrn);
	if (ret <= 0 || dst == NULL)
		goto end;

	switch (type) {

	case K_ALIAS:
		memset(dst, 0, sz);
		for (i = 0; res[0][i]; i++) {
			if (i && strlcat(dst, ", ", sz) >= sz) {
				ret = -1;
				break;
			}
			if (strlcat(dst, res[0][i], sz) >= sz) {
				ret = -1;
				break;
			}
		}
		break;
	case K_DOMAIN:
	case K_MAILADDR:
		if (strlcpy(dst, res[0][0], sz) >= sz)
			ret = -1;
		break;
	case K_CREDENTIALS:
		if (snprintf(dst, sz, "%s:%s", res[0][0], res[1][0]) >= (int)sz)
			ret = -1;
		break;
	case K_USERINFO:
		if (snprintf(dst, sz, "%s:%s:%s", res[0][0], res[1][0],
		    res[2][0]) >= (int)sz)
			ret = -1;
		break;
	default:
		log_warnx("warn: table-ldap: unsupported lookup kind");
		ret = -1;
	}

	if (ret == -1)
		log_warnx("warn: table-ldap: could not format result");

end:
	for (i = 0; i < q->attrn; ++i)
		if (res[i])
			aldap_free_attr(res[i]);

	return (ret);
}
