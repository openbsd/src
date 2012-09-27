/*	$OpenBSD: map_stdio.c,v 1.11 2012/09/27 20:34:15 chl Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"


/* stdio(3) backend */
static void *map_stdio_open(struct map *);
static void *map_stdio_lookup(void *, const char *, enum map_kind);
static int   map_stdio_compare(void *, const char *, enum map_kind,
    int (*)(const char *, const char *));
static void  map_stdio_close(void *);

static char *map_stdio_get_entry(void *, const char *, size_t *);
static void *map_stdio_credentials(const char *, char *, size_t);
static void *map_stdio_alias(const char *, char *, size_t);
static void *map_stdio_virtual(const char *, char *, size_t);
static void *map_stdio_netaddr(const char *, char *, size_t);


struct map_backend map_backend_stdio = {
	map_stdio_open,
	map_stdio_close,
	map_stdio_lookup,
	map_stdio_compare
};


static void *
map_stdio_open(struct map *map)
{
	return fopen(map->m_config, "r");
}

static void
map_stdio_close(void *hdl)
{
	FILE *fp = hdl;

	fclose(fp);
}

static void *
map_stdio_lookup(void *hdl, const char *key, enum map_kind kind)
{
	char *line;
	size_t len;
	void *ret;

	line = map_stdio_get_entry(hdl, key, &len);
	if (line == NULL)
		return NULL;

	ret = NULL;
	switch (kind) {
	case K_ALIAS:
		ret = map_stdio_alias(key, line, len);
		break;

	case K_CREDENTIALS:
		ret = map_stdio_credentials(key, line, len);
		break;

	case K_VIRTUAL:
		ret = map_stdio_virtual(key, line, len);
		break;

	case K_NETADDR:
		ret = map_stdio_netaddr(key, line, len);
		break;

	default:
		break;
	}

	free(line);

	return ret;
}

static int
map_stdio_compare(void *hdl, const char *key, enum map_kind kind,
    int (*func)(const char *, const char *))
{
	char *buf, *lbuf;
	size_t flen;
	char *keyp;
	FILE *fp = hdl;
	int ret = 0;

	lbuf = NULL;
	while ((buf = fgetln(fp, &flen))) {
		if (buf[flen - 1] == '\n')
			buf[flen - 1] = '\0';
		else {
			lbuf = xmalloc(flen + 1, "map_stdio_compare");
			memcpy(lbuf, buf, flen);
			lbuf[flen] = '\0';
			buf = lbuf;
		}

		keyp = buf;
		while (isspace((int)*keyp))
			++keyp;
		if (*keyp == '\0' || *keyp == '#')
			continue;

		if (! func(key, keyp))
			continue;

		ret = 1;
		break;
	}
	free(lbuf);

	return ret;
}

static char *
map_stdio_get_entry(void *hdl, const char *key, size_t *len)
{
	char *buf, *lbuf;
	size_t flen;
	char *keyp;
	char *valp;
	FILE *fp = hdl;
	char *result = NULL;

	lbuf = NULL;
	while ((buf = fgetln(fp, &flen))) {
		if (buf[flen - 1] == '\n')
			buf[flen - 1] = '\0';
		else {
			lbuf = xmalloc(flen + 1, "map_stdio_get_entry");
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
		if (valp == NULL || valp == keyp)
			continue;

		if (strcmp(keyp, key) != 0)
			continue;

		result = xstrdup(valp, "map_stdio_get_entry");
		*len = strlen(result);

		break;
	}
	free(lbuf);

	return result;
}


static void *
map_stdio_credentials(const char *key, char *line, size_t len)
{
	struct map_credentials *map_credentials = NULL;
	char *p;

	/* credentials are stored as user:password */
	if (len < 3)
		return NULL;

	/* too big to fit in a smtp session line */
	if (len >= MAX_LINE_SIZE)
		return NULL;

	p = strchr(line, ':');
	if (p == NULL)
		return NULL;

	if (p == line || p == line + len - 1)
		return NULL;
	*p++ = '\0';

	map_credentials = xcalloc(1, sizeof *map_credentials,
	    "map_stdio_credentials");

	if (strlcpy(map_credentials->username, line,
		sizeof(map_credentials->username)) >=
	    sizeof(map_credentials->username))
		goto err;

	if (strlcpy(map_credentials->password, p,
		sizeof(map_credentials->password)) >=
	    sizeof(map_credentials->password))
		goto err;

	return map_credentials;

err:
	free(map_credentials);
	return NULL;
}

static void *
map_stdio_alias(const char *key, char *line, size_t len)
{
	char	       	*subrcpt;
	char	       	*endp;
	struct map_alias	*map_alias = NULL;
	struct expandnode	 xn;

	map_alias = xcalloc(1, sizeof *map_alias, "map_stdio_alias");

	while ((subrcpt = strsep(&line, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace((int)*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace((int)*endp))
			*endp-- = '\0';

		if (! alias_parse(&xn, subrcpt))
			goto error;

		expand_insert(&map_alias->expand, &xn);
		map_alias->nbnodes++;
	}

	return map_alias;

error:
	expand_free(&map_alias->expand);
	free(map_alias);
	return NULL;
}

static void *
map_stdio_virtual(const char *key, char *line, size_t len)
{
	char	       	*subrcpt;
	char	       	*endp;
	struct map_virtual	*map_virtual = NULL;
	struct expandnode	 xn;

	map_virtual = xcalloc(1, sizeof *map_virtual, "map_stdio_virtual");

	/* domain key, discard value */
	if (strchr(key, '@') == NULL)
		return map_virtual;

	while ((subrcpt = strsep(&line, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace((int)*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace((int)*endp))
			*endp-- = '\0';

		if (! alias_parse(&xn, subrcpt))
			goto error;

		expand_insert(&map_virtual->expand, &xn);
		map_virtual->nbnodes++;
	}

	return map_virtual;

error:
	expand_free(&map_virtual->expand);
	free(map_virtual);
	return NULL;
}

static void *
map_stdio_netaddr(const char *key, char *line, size_t len)
{
	struct map_netaddr	*map_netaddr = NULL;

	map_netaddr = xcalloc(1, sizeof *map_netaddr, "map_stdio_netaddr");

	if (! text_to_netaddr(&map_netaddr->netaddr, line))
	    goto error;

	return map_netaddr;

error:
	free(map_netaddr);
	return NULL;
}
