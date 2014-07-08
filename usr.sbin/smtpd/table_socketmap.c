/*	$OpenBSD: table_socketmap.c,v 1.4 2014/07/08 13:49:09 eric Exp $	*/

/*
 * Copyright (c) 2014 Gilles Chehade <gilles@poolp.org>
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
#include <sys/socket.h>
#include <sys/un.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "log.h"

static int table_socketmap_update(void);
static int table_socketmap_lookup(int, struct dict *, const char *, char *, size_t);
static int table_socketmap_check(int, struct dict *, const char *);
static int table_socketmap_fetch(int, struct dict *, char *, size_t);

static int table_socketmap_connect(const char *);

static char	       *config;
static int		sock = -1;
static FILE	       *sockstream;
#define	REPLYBUFFERSIZE	100000
static char		repbuffer[REPLYBUFFERSIZE+1];

enum socketmap_reply{
	SM_OK = 0,
	SM_NOTFOUND,
	SM_TEMP,
	SM_TIMEOUT,
	SM_PERM,
};

int
main(int argc, char **argv)
{
	int	ch;

	log_init(1);
	log_verbose(~0);

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			log_warnx("warn: table-socketmap: bad option");
			return (1);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		log_warnx("warn: table-socketmap: bogus argument(s)");
		return (1);
	}

	config = argv[0];

	if (table_socketmap_connect(config) == 0) {
		log_warnx("warn: table-socketmap: error connecting to %s", config);
		return (1);
	}

	table_api_on_update(table_socketmap_update);
	table_api_on_check(table_socketmap_check);
	table_api_on_lookup(table_socketmap_lookup);
	table_api_on_fetch(table_socketmap_fetch);
	table_api_dispatch();

	return (0);
}

static int
table_socketmap_connect(const char *s)
{
	struct sockaddr_un	sun;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("warn: table-socketmap");
		goto err;
	}

	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, s, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path)) {
		log_warnx("warn: table-socketmap: socket path too long");
		goto err;
	}

	if (connect(sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("warn: table-socketmap");
		goto err;
	}

	if ((sockstream = fdopen(sock, "w+")) == NULL) {
		log_warn("warn: table-socketmap");
		goto err;
	}

	return 1;

err:
	if (sock) {
		close(sock);
		sock = -1;
	}
	return 0;
}

static enum socketmap_reply
table_socketmap_query(const char *name, const char *key)
{
	char   *buf, *lbuf = NULL;
	size_t	len;
	int	ret = SM_PERM;

	memset(repbuffer, 0, sizeof repbuffer);
	fprintf(sockstream, "%s %s\n", name, key);
	fflush(sockstream);

	buf = fgetln(sockstream, &len);
	if (buf == NULL) {
		log_warnx("warn: table-socketmap: socketmap has lost its socket");
		(void)strlcpy(repbuffer, "lost connection to socket", sizeof repbuffer);
		ret = SM_PERM;
		goto err;
	}
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	else {
		if ((lbuf = malloc(len + 1)) == NULL) {
			log_warnx("warn: table-socketmap: memory exhaustion");
			(void)strlcpy(repbuffer, "memory exhaustion", sizeof repbuffer);
			ret = SM_PERM;
			goto err;
		}
		memcpy(lbuf, buf, len);
		lbuf[len] = '\0';
		buf = lbuf;
	}
	free(lbuf);
	lbuf = NULL;

	if (strlcpy(repbuffer, buf, sizeof repbuffer) >= sizeof repbuffer) {
		log_warnx("warn: table-socketmap: socketmap reply too large (>%zu bytes)",
			sizeof repbuffer);
		(void)strlcpy(repbuffer, "socketmap reply too large", sizeof repbuffer);
		ret = SM_PERM;
		goto err;
	}

	if (strncasecmp(repbuffer, "OK ", 3) == 0) {
		ret = SM_OK;
		memmove(repbuffer, repbuffer+3, strlen(repbuffer)-2);
	}
	else if (strncasecmp(repbuffer, "NOTFOUND ", 9) == 0) {
		ret = SM_NOTFOUND;
		memmove(repbuffer, repbuffer+9, strlen(repbuffer)-8);
	}
	else if (strncasecmp(repbuffer, "TEMP ", 5) == 0) {
		ret = SM_TEMP;
		memmove(repbuffer, repbuffer+5, strlen(repbuffer)-4);
	}
	else if (strncasecmp(repbuffer, "TIMEOUT ", 8) == 0) {
		ret = SM_TIMEOUT;
		memmove(repbuffer, repbuffer+8, strlen(repbuffer)-7);
	}
	else if (strncasecmp(repbuffer, "PERM ", 5) == 0) {
		ret = SM_PERM;
		memmove(repbuffer, repbuffer+5, strlen(repbuffer)-4);
	}
	else {
		ret = SM_PERM;
		(void)strlcpy(repbuffer, "unrecognized socketmap reply", sizeof repbuffer);
	}

err:
	if (lbuf)
		free(lbuf);
	return ret;
}

static int
table_socketmap_update(void)
{
	return 1;
}

static int
table_socketmap_check(int service, struct dict *params, const char *key)
{
	return (-1);
}

static int
table_socketmap_lookup(int service, struct dict *params, const char *key, char *dst, size_t sz)
{
	int			r;
	enum socketmap_reply	rep;

	rep = table_socketmap_query(table_api_get_name(), key);
	if (rep == SM_NOTFOUND)
		return 0;
	if (rep != SM_OK) {
		log_warnx("warn: table-socketmap: %s", repbuffer);
		return -1;
	}
	if (strlcpy(dst, repbuffer, sz) >= sz) {
		log_warnx("warn: table-socketmap: result too large");
		return -1;
	}

	r = 1;
	switch(service) {
	case K_ALIAS:
	case K_CREDENTIALS:
	case K_USERINFO:
	case K_DOMAIN:
	case K_NETADDR:
	case K_SOURCE:
	case K_MAILADDR:
	case K_ADDRNAME:
		break;
	default:
		log_warnx("warn: table-socketmap: unknown service %d", service);
		r = -1;
	}

	return (r);
}

static int
table_socketmap_fetch(int service, struct dict *params, char *key, size_t sz)
{
	return (-1);
}
