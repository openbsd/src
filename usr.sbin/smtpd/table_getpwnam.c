/*	$OpenBSD: table_getpwnam.c,v 1.1 2013/01/26 09:37:24 gilles Exp $	*/

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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"


/* getpwnam(3) backend */
static int table_getpwnam_config(struct table *, const char *);
static int table_getpwnam_update(struct table *);
static void *table_getpwnam_open(struct table *);
static int table_getpwnam_lookup(void *, const char *, enum table_service,
    void **);
static void  table_getpwnam_close(void *);

struct table_backend table_backend_getpwnam = {
	K_USERINFO,
	table_getpwnam_config,
	table_getpwnam_open,
	table_getpwnam_update,
	table_getpwnam_close,
	table_getpwnam_lookup,
};


static int
table_getpwnam_config(struct table *table, const char *config)
{
	if (config)
		return 0;
	return 1;
}

static int
table_getpwnam_update(struct table *table)
{
	return 1;
}

static void *
table_getpwnam_open(struct table *table)
{
	return table;
}

static void
table_getpwnam_close(void *hdl)
{
	return;
}

static int
table_getpwnam_lookup(void *hdl, const char *key, enum table_service kind,
    void **ret)
{
	struct userinfo	       *userinfo;
	struct passwd	       *pw;
	size_t			s;

	if (kind != K_USERINFO)
		return -1;

	errno = 0;
	do {
		pw = getpwnam(key);
	} while (pw == NULL && errno == EINTR);

	if (pw == NULL) {
		if (errno)
			return -1;
		return 0;
	}
	if (ret == NULL)
		return 1;

	userinfo = xcalloc(1, sizeof *userinfo, "table_getpwnam_lookup");
	userinfo->uid = pw->pw_uid;
	userinfo->gid = pw->pw_gid;
	s = strlcpy(userinfo->username, pw->pw_name,
	    sizeof(userinfo->username));
	if (s >= sizeof(userinfo->username))
		goto error;
	s = strlcpy(userinfo->directory, pw->pw_dir,
	    sizeof(userinfo->directory));
	if (s >= sizeof(userinfo->directory))
		goto error;

	*ret = userinfo;
	return 1;

error:
	free(userinfo);
	return -1;
}
