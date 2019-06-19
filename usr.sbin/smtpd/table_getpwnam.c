/*	$OpenBSD: table_getpwnam.c,v 1.12 2018/12/27 14:23:41 eric Exp $	*/

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
#include <limits.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"


/* getpwnam(3) backend */
static int table_getpwnam_config(struct table *);
static int table_getpwnam_update(struct table *);
static int table_getpwnam_open(struct table *);
static int table_getpwnam_lookup(struct table *, enum table_service, const char *,
    char **);
static void table_getpwnam_close(struct table *);

struct table_backend table_backend_getpwnam = {
	"getpwnam",
	K_USERINFO,
	table_getpwnam_config,
	NULL,
	NULL,
	table_getpwnam_open,
	table_getpwnam_update,
	table_getpwnam_close,
	table_getpwnam_lookup,
};


static int
table_getpwnam_config(struct table *table)
{
	if (table->t_config[0])
		return 0;
	return 1;
}

static int
table_getpwnam_update(struct table *table)
{
	return 1;
}

static int
table_getpwnam_open(struct table *table)
{
	return 1;
}

static void
table_getpwnam_close(struct table *table)
{
	return;
}

static int
table_getpwnam_lookup(struct table *table, enum table_service kind, const char *key,
    char **dst)
{
	struct passwd	       *pw;

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
	if (dst == NULL)
		return 1;

	if (asprintf(dst, "%d:%d:%s",
	    pw->pw_uid,
	    pw->pw_gid,
	    pw->pw_dir) == -1) {
		*dst = NULL;
		return -1;
	}

	return (1);
}
