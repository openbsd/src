/*	$OpenBSD: table_passwd.c,v 1.9 2015/01/16 06:40:21 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Gilles Chehade <gilles@poolp.org>
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

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "log.h"

static int table_passwd_update(void);
static int table_passwd_check(int, struct dict *, const char *);
static int table_passwd_lookup(int, struct dict *, const char *, char *, size_t);
static int table_passwd_fetch(int, struct dict *, char *, size_t);
static int parse_passwd_entry(struct passwd *, char *);

static char	       *config;
static struct dict     *passwd;

int
main(int argc, char **argv)
{
	int	ch;

	log_init(1);

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			log_warnx("warn: table-passwd: bad option");
			return (1);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		log_warnx("warn: table-passwd: bogus argument(s)");
		return (1);
	}

	config = argv[0];

	if (table_passwd_update() == 0) {
		log_warnx("warn: table-passwd: error parsing config file");
		return (1);
	}

	table_api_on_update(table_passwd_update);
	table_api_on_check(table_passwd_check);
	table_api_on_lookup(table_passwd_lookup);
	table_api_on_fetch(table_passwd_fetch);
	table_api_dispatch();

	return (0);
}

static int
table_passwd_update(void)
{
	FILE	       *fp;
	char	       *buf, *lbuf = NULL;
	char		tmp[LINE_MAX];
	size_t		len;
	char	       *line;
	struct passwd	pw;
	struct dict    *npasswd;

	/* Parse configuration */
	fp = fopen(config, "r");
	if (fp == NULL)
		return (0);

	npasswd = calloc(1, sizeof *passwd);
	if (npasswd == NULL)
		goto err;

	dict_init(npasswd);

	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (strlcpy(tmp, buf, sizeof tmp) >= sizeof tmp) {
			log_warnx("warn: table-passwd: line too long");
			goto err;
		}
		if (! parse_passwd_entry(&pw, tmp)) {
			log_warnx("warn: table-passwd: invalid entry");
			goto err;
		}
		if ((line = strdup(buf)) == NULL)
			err(1, NULL);
		dict_set(npasswd, pw.pw_name, line);
	}
	free(lbuf);
	fclose(fp);

	/* swap passwd table and release old one*/
	if (passwd)
		while (dict_poproot(passwd, (void**)&buf))
			free(buf);
	passwd = npasswd;

	return (1);

err:
	if (fp)
		fclose(fp);
	free(lbuf);

	/* release passwd table */
	if (npasswd) {
		while (dict_poproot(npasswd, (void**)&buf))
			free(buf);
		free(npasswd);
	}
	return (0);
}

static int
table_passwd_check(int service, struct dict *params, const char *key)
{
	return (-1);
}

static int
table_passwd_lookup(int service, struct dict *params, const char *key, char *dst, size_t sz)
{
	int		r;
	struct passwd	pw;
	char	       *line;
	char		tmp[LINE_MAX];

	line = dict_get(passwd, key);
	if (line == NULL)
		return 0;

	(void)strlcpy(tmp, line, sizeof tmp);
	if (! parse_passwd_entry(&pw, tmp)) {
		log_warnx("warn: table-passwd: invalid entry");
		return -1;
	}

	r = 1;
	switch (service) {
	case K_CREDENTIALS:
		if (snprintf(dst, sz, "%s:%s",
			pw.pw_name, pw.pw_passwd) >= (ssize_t)sz) {
			log_warnx("warn: table-passwd: result too large");
			r = -1;
		}
		break;
	case K_USERINFO:
		if (snprintf(dst, sz, "%d:%d:%s",
			pw.pw_uid, pw.pw_gid, pw.pw_dir)
		    >= (ssize_t)sz) {
			log_warnx("warn: table-passwd: result too large");
			r = -1;
		}
		break;
	default:
		log_warnx("warn: table-passwd: unknown service %d",
		    service);
		r = -1;
	}

	return (r);
}

static int
table_passwd_fetch(int service, struct dict *params, char *dst, size_t sz)
{
	return (-1);
}

static int
parse_passwd_entry(struct passwd *pw, char *buf)
{
	const char     *errstr;
	char	       *p, *q;

	p = buf;

	/* username */
	q = p;
	if ((p = strchr(q, ':')) == NULL)
		return 0;
	*p++ = 0;
	pw->pw_name = q;

	/* password */
	q = p;
	if ((p = strchr(q, ':')) == NULL)
		return 0;
	*p++ = 0;
	pw->pw_passwd = q;

	/* uid */
	q = p;
	if ((p = strchr(q, ':')) == NULL)
		return 0;
	*p++ = 0;
	pw->pw_uid = strtonum(q, 1, UID_MAX, &errstr);
	if (errstr)
		return 0;

	/* gid */
	q = p;
	if ((p = strchr(q, ':')) == NULL)
		return 0;
	*p++ = 0;
	pw->pw_gid = strtonum(q, 1, GID_MAX, &errstr);
	if (errstr)
		return 0;

	/* gecos */
	q = p;
	if ((p = strchr(q, ':')) == NULL)
		return 0;
	*p++ = 0;
	pw->pw_gecos = q;

	/* home */
	q = p;
	if ((p = strchr(q, ':')) == NULL)
		return 0;
	*p++ = 0;
	pw->pw_dir = q;

	/* shell */
	q = p;
	if (strchr(q, ':') != NULL)
		return 0;
	pw->pw_shell = q;

	return 1;
}
