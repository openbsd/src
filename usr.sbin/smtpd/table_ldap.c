/*	$OpenBSD: table_ldap.c,v 1.3 2013/03/08 19:11:52 chl Exp $	*/

/*
 * Copyright (c) 2010-2012 Gilles Chehade <gilles@poolp.org>
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
#include "log.h"

#define MAX_LDAP_IDENTIFIER    	 32
#define MAX_LDAP_URL   	 	 256
#define MAX_LDAP_USERNAME      	 256
#define MAX_LDAP_PASSWORD      	 256
#define MAX_LDAP_BASELEN      	 128
#define MAX_LDAP_FILTERLEN     	 1024
#define MAX_LDAP_FIELDLEN      	 128

static void			*table_ldap_open(struct table *);
static int			 table_ldap_update(struct table *);
static int			 table_ldap_config(struct table *, const char *);
static int			 table_ldap_lookup(void *, const  char *, enum table_service, void **);
static int			 table_ldap_fetch(void *, enum table_service, char **);
static void			 table_ldap_close(void *);
static struct aldap		*ldap_client_connect(const char *);

struct table_backend table_backend_ldap = {
	K_ALIAS|K_CREDENTIALS|K_DOMAIN|K_USERINFO, /* K_NETADDR|K_SOURCE,*/
	table_ldap_config,
	table_ldap_open,
	table_ldap_update,
	table_ldap_close,
	table_ldap_lookup,
	table_ldap_fetch
};

struct table_ldap_handle {
	struct aldap	*aldap;
	struct table	*table;
};

static int	parse_attributes(char **, const char *, size_t);
static int	table_ldap_internal_query(struct aldap *, const char *,
    const char *, char **, char ***, size_t);

static int	table_ldap_alias(struct table_ldap_handle *, const char *, void **);
static int	table_ldap_credentials(struct table_ldap_handle *, const char *, void **);
static int	table_ldap_domain(struct table_ldap_handle *, const char *, void **);
static int	table_ldap_userinfo(struct table_ldap_handle *, const char *, void **);


static int
table_ldap_config(struct table *table, const char *config)
{
	void	*cfg = NULL;

	/* no config ? broken */
	if (config == NULL)
		return 0;

	cfg = table_config_create();
	if (! table_config_parse(cfg, config, T_HASH))
		goto err;

	/* sanity checks */
	if (table_config_get(cfg, "url") == NULL) {
		log_warnx("table_ldap: missing 'url' configuration");
		goto err;
	}

	if (table_config_get(cfg, "basedn") == NULL) {
		log_warnx("table_ldap: missing 'basedn' configuration");
		goto err;
	}

	table_set_configuration(table, cfg);
	return 1;

err:
	table_destroy(cfg);
	return 0;

}

static int
table_ldap_update(struct table *table)
{
	return 1;
}

static void *
table_ldap_open(struct table *table)
{
	struct table			*cfg = NULL;
	struct table_ldap_handle	*tlh = NULL;
	struct aldap_message		*message = NULL;
	char     			*url = NULL;
	char     			*username = NULL;
	char     			*password = NULL;

	cfg = table_get_configuration(table);
	if (table_get(cfg, "url") == NULL ||
	    table_get(cfg, "username") == NULL ||
	    table_get(cfg, "password") == NULL)
		goto err;

	url      = xstrdup(table_get(cfg, "url"), "table_ldap_open");
	username = xstrdup(table_get(cfg, "username"), "table_ldap_open");
	password = xstrdup(table_get(cfg, "password"), "table_ldap_open");

	tlh = xcalloc(1, sizeof(*tlh), "table_ldap_open");
	tlh->table = table;
	tlh->aldap = ldap_client_connect(url);
	if (tlh->aldap == NULL) {
		log_warnx("table_ldap_open: ldap_client_connect error");
		goto err;
	}

	if (aldap_bind(tlh->aldap, username, password) == -1) {
		log_warnx("table_ldap_open: aldap_bind error");
		goto err;
	}

	if ((message = aldap_parse(tlh->aldap)) == NULL) {
		log_warnx("table_ldap_open: aldap_parse");
		goto err;
	}

	switch (aldap_get_resultcode(message)) {
	case LDAP_SUCCESS:
		log_warnx("table_ldap_open: ldap server accepted credentials");
		break;
	case LDAP_INVALID_CREDENTIALS:
		log_warnx("table_ldap_open: ldap server refused credentials");
		goto err;
	default:
		log_warnx("table_ldap_open: failed to bind, result #%d", aldap_get_resultcode(message));
		goto err;
	}

	return tlh;

err:
	if (tlh) {
		if (tlh->aldap != NULL)
			aldap_close(tlh->aldap);
		free(tlh);
	}
	if (message != NULL)
		aldap_freemsg(message);
	return NULL;
}

static void
table_ldap_close(void *hdl)
{
	struct table_ldap_handle	*tlh = hdl;

	aldap_close(tlh->aldap);
	free(tlh);
}

static int
table_ldap_lookup(void *hdl, const char *key, enum table_service service,
		void **retp)
{
	struct table_ldap_handle	*tlh = hdl;

	switch (service) {
	case K_ALIAS:
		return table_ldap_alias(tlh, key, retp);

	case K_CREDENTIALS:
		return table_ldap_credentials(tlh, key, retp);

	case K_DOMAIN:
		return table_ldap_domain(tlh, key, retp);

	case K_USERINFO:
		return table_ldap_userinfo(tlh, key, retp);

	default:
		break;
	}

	return 0;
}

static int
table_ldap_fetch(void *hdl, enum table_service service, char **retp)
{
	/* fetch not support for LDAP at this point */
	return -1;
}

static int
filter_expand(char **expfilter, const char *filter, const char *key)
{
	if (asprintf(expfilter, filter, key) < 0)
		return 0;
	return 1;
}

static int
table_ldap_internal_query(struct aldap *aldap, const char *basedn,
    const char *filter, char **attributes, char ***outp, size_t n)
{
	struct aldap_message	       *m = NULL;
	struct aldap_page_control      *pg = NULL;
	int				ret;
	int				found;
	size_t				i;
	char				basedn__[MAX_LDAP_BASELEN];
	char				filter__[MAX_LDAP_FILTERLEN];

	if (strlcpy(basedn__, basedn, sizeof basedn__)
	    >= sizeof basedn__)
		return -1;
	if (strlcpy(filter__, filter, sizeof filter__)
	    >= sizeof filter__)
		return -1;
	found = 0;
	do {
		if ((ret = aldap_search(aldap, basedn__, LDAP_SCOPE_SUBTREE,
			    filter__, NULL, 0, 0, 0, pg)) == -1) {
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
	log_debug("debug: table_ldap_internal_query: filter=%s, ret=%d", filter, ret);
	return ret;
}


static int
table_ldap_credentials(struct table_ldap_handle *tlh, const char *key, void **retp)
{
	struct aldap		       *aldap = tlh->aldap;
	struct table		       *cfg = table_get_configuration(tlh->table);
	const char		       *filter = NULL;
	const char		       *basedn = NULL;
	struct credentials		credentials;
	char			       *expfilter = NULL;
	char     		       *attributes[4];
	char     		      **ret_attr[4];
	const char     		       *attr;
	char				line[1024];
	int				ret = -1;
	size_t				i;

	bzero(&attributes, sizeof attributes);
	bzero(&ret_attr, sizeof ret_attr);

	basedn = table_get(cfg, "basedn");
	if ((filter = table_get(cfg, "credentials_filter")) == NULL) {
		log_warnx("table_ldap: lookup: no filter configured for credentials");
		goto end;
	}

	if ((attr = table_get(cfg, "credentials_attributes")) == NULL) {
		log_warnx("table_ldap: lookup: no attributes configured for credentials");
		goto end;
	}

	if (! filter_expand(&expfilter, filter, key)) {
		log_warnx("table_ldap: lookup: couldn't expand filter");
		goto end;
	}

	if (! parse_attributes(attributes, attr, 2)) {
		log_warnx("table_ldap: lookup: failed to parse attributes");
		goto end;
	}

	if ((ret = table_ldap_internal_query(aldap, basedn, expfilter, attributes,
		    ret_attr, nitems(attributes))) <= 0)
		goto end;

	if (retp == NULL)
		goto end;

	if (! bsnprintf(line, sizeof line, "%s:%s", ret_attr[0][0], ret_attr[0][1])) {
		ret = -1;
		goto end;
	}

	bzero(&credentials, sizeof credentials);
	if (! text_to_credentials(&credentials, line)) {
		ret = -1;
		goto end;
	}

	*retp = xmemdup(&credentials, sizeof credentials, "table_ldap_credentials");

end:
	for (i = 0; i < nitems(attributes); ++i) {
		free(attributes[i]);
		if (ret_attr[i])
			aldap_free_attr(ret_attr[i]);
	}
	
	free(expfilter);
	log_debug("debug: table_ldap_credentials: ret=%d", ret);
	return ret;
}

static int
table_ldap_domain(struct table_ldap_handle *tlh, const char *key, void **retp)
{
	struct aldap		       *aldap = tlh->aldap;
	struct table		       *cfg = table_get_configuration(tlh->table);
	const char		       *filter = NULL;
	const char		       *basedn = NULL;
	struct destination		destination;
	char			       *expfilter = NULL;
	char     		       *attributes[1];
	char     		      **ret_attr[1];
	const char     		       *attr;
	int				ret = -1;
	size_t				i;

	bzero(&attributes, sizeof attributes);
	bzero(&ret_attr, sizeof ret_attr);

	log_debug("domain: %s", key);
	basedn = table_get(cfg, "basedn");
	if ((filter = table_get(cfg, "domain_filter")) == NULL) {
		log_warnx("table_ldap: lookup: no filter configured for domain");
		goto end;
	}

	if ((attr = table_get(cfg, "domain_attributes")) == NULL) {
		log_warnx("table_ldap: lookup: no attributes configured for domain");
		goto end;
	}

	if (! filter_expand(&expfilter, filter, key)) {
		log_warnx("table_ldap: lookup: couldn't expand filter");
		goto end;
	}

	if (! parse_attributes(attributes, attr, 1)) {
		log_warnx("table_ldap: lookup: failed to parse attributes");
		goto end;
	}

	if ((ret = table_ldap_internal_query(aldap, basedn, expfilter, attributes,
		    ret_attr, nitems(attributes))) <= 0)
		goto end;

	if (retp == NULL)
		goto end;

	bzero(&destination, sizeof destination);
	if (strlcpy(destination.name, ret_attr[0][0], sizeof destination.name)
	    >= sizeof destination.name);
	*retp = xmemdup(&destination, sizeof destination, "table_ldap_destination");

end:
	for (i = 0; i < nitems(attributes); ++i) {
		free(attributes[i]);
		if (ret_attr[i])
			aldap_free_attr(ret_attr[i]);
	}
	free(expfilter);
	log_debug("debug: table_ldap_destination: ret=%d", ret);
	return ret;
}

static int
table_ldap_userinfo(struct table_ldap_handle *tlh, const char *key, void **retp)
{
	struct aldap		       *aldap = tlh->aldap;
	struct table		       *cfg = table_get_configuration(tlh->table);
	const char		       *filter = NULL;
	const char		       *basedn = NULL;
	struct userinfo			userinfo;
	char			       *expfilter = NULL;
	char     		       *attributes[4];
	char     		      **ret_attr[4];
	const char     		       *attr;
	char				line[1024];
	int				ret = -1;
	size_t				i;

	bzero(&attributes, sizeof attributes);
	bzero(&ret_attr, sizeof ret_attr);

	basedn = table_get(cfg, "basedn");
	if ((filter = table_get(cfg, "userinfo_filter")) == NULL) {
		log_warnx("table_ldap: lookup: no filter configured for userinfo");
		goto end;
	}

	if ((attr = table_get(cfg, "userinfo_attributes")) == NULL) {
		log_warnx("table_ldap: lookup: no attributes configured for userinfo");
		goto end;
	}

	if (! filter_expand(&expfilter, filter, key)) {
		log_warnx("table_ldap: lookup: couldn't expand filter");
		goto end;
	}

	if (! parse_attributes(attributes, attr, 4)) {
		log_warnx("table_ldap: lookup: failed to parse attributes");
		goto end;
	}

	if ((ret = table_ldap_internal_query(aldap, basedn, expfilter, attributes,
		    ret_attr, nitems(attributes))) <= 0)
		goto end;

	if (retp == NULL)
		goto end;

	if (! bsnprintf(line, sizeof line, "%s:%s:%s:%s",
		ret_attr[0][0], ret_attr[1][0], ret_attr[2][0], ret_attr[3][0])) {
		ret = -1;
		goto end;
	}

	bzero(&userinfo, sizeof userinfo);
	if (! text_to_userinfo(&userinfo, line)) {
		ret = -1;
		goto end;
	}

	*retp = xmemdup(&userinfo, sizeof userinfo, "table_ldap_userinfo");

end:
	for (i = 0; i < nitems(attributes); ++i) {
		free(attributes[i]);
		if (ret_attr[i])
			aldap_free_attr(ret_attr[i]);
	}
	free(expfilter);
	log_debug("debug: table_ldap_userinfo: ret=%d", ret);
	return ret;
}

static int
table_ldap_alias(struct table_ldap_handle *tlh, const char *key, void **retp)
{
	struct aldap		       *aldap = tlh->aldap;
	struct table		       *cfg = table_get_configuration(tlh->table);
	const char		       *filter = NULL;
	const char		       *basedn = NULL;
	struct expand		       *xp = NULL;
	char			       *expfilter = NULL;
	char     		       *attributes[1];
	char     		      **ret_attr[1];
	const char     		       *attr;
	int				ret = -1;
	size_t				i;

	bzero(&attributes, sizeof attributes);
	bzero(&ret_attr, sizeof ret_attr);

	basedn = table_get(cfg, "basedn");
	if ((filter = table_get(cfg, "alias_filter")) == NULL) {
		log_warnx("table_ldap: lookup: no filter configured for alias");
		goto end;
	}

	if ((attr = table_get(cfg, "alias_attributes")) == NULL) {
		log_warnx("table_ldap: lookup: no attributes configured for alias");
		goto end;
	}

	if (! filter_expand(&expfilter, filter, key)) {
		log_warnx("table_ldap: lookup: couldn't expand filter");
		goto end;
	}

	if (! parse_attributes(attributes, attr, 1)) {
		log_warnx("table_ldap: lookup: failed to parse attributes");
		goto end;
	}

	if ((ret = table_ldap_internal_query(aldap, basedn, expfilter, attributes,
		    ret_attr, nitems(attributes))) <= 0)
		goto end;

	if (retp == NULL)
		goto end;

	xp = xcalloc(1, sizeof *xp, "table_ldap_alias");
	for (i = 0; ret_attr[0][i]; ++i) {
		if (! expand_line(xp, ret_attr[0][i], 1)) {
			ret = -1;
			goto end;
		}
	}
	*retp = xp;

end:
	for (i = 0; i < nitems(attributes); ++i) {
		free(attributes[i]);
		if (ret_attr[i])
			aldap_free_attr(ret_attr[i]);
	}
	if (ret != 1) {
		if (retp)
			*retp = NULL;
		if (xp)
			expand_free(xp);
	}
	free(expfilter);
	log_debug("debug: table_ldap_alias: ret=%d", ret);
	return ret;
}

static struct aldap *
ldap_client_connect(const char *addr)
{
	struct aldap_url	lu;
	struct addrinfo		 hints, *res0, *res;
	int			 error;

	char *url;
	int fd = -1;

	if ((url = strdup(addr)) == NULL)
		err(1, NULL);

	if (aldap_parse_url(url, &lu) != 1) {
		warnx("aldap_parse_url fail");
		goto err;
	}
	url = NULL;

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
		fd = -1;
	}

err:
	free(url);
	return NULL;
}

static int
parse_attributes(char **attributes, const char *line, size_t expect)
{
	char	buffer[1024];
	char   *p;
	size_t	m, n;

	if (strlcpy(buffer, line, sizeof buffer)
	    >= sizeof buffer)
		return 0;

	m = 1;
	for (p = buffer; *p; ++p) {
		if (*p == ',') {
			*p = 0;
			m++;
		}
	}
	if (expect != m)
		return 0;

	p = buffer;
	for (n = 0; n < expect; ++n)
		attributes[n] = NULL;
	for (n = 0; n < m; ++n) {
		attributes[n] = xstrdup(p, "parse_attributes");
		p += strlen(p) + 1;
	}
	return 1;
}
