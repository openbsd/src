/* $OpenBSD: sftp-glob.c,v 1.27 2015/01/14 13:54:13 djm Exp $ */
/*
 * Copyright (c) 2001-2004 Damien Miller <djm@openbsd.org>
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
#include <sys/stat.h>

#include <dirent.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include "xmalloc.h"
#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"

int remote_glob(struct sftp_conn *, const char *, int,
    int (*)(const char *, int), glob_t *);

struct SFTP_OPENDIR {
	SFTP_DIRENT **dir;
	int offset;
};

static struct {
	struct sftp_conn *conn;
} cur;

static void *
fudge_opendir(const char *path)
{
	struct SFTP_OPENDIR *r;

	r = xcalloc(1, sizeof(*r));

	if (do_readdir(cur.conn, (char *)path, &r->dir)) {
		free(r);
		return(NULL);
	}

	r->offset = 0;

	return((void *)r);
}

static struct dirent *
fudge_readdir(struct SFTP_OPENDIR *od)
{
	static struct dirent ret;

	if (od->dir[od->offset] == NULL)
		return(NULL);

	memset(&ret, 0, sizeof(ret));
	strlcpy(ret.d_name, od->dir[od->offset++]->filename,
	    sizeof(ret.d_name));

	return(&ret);
}

static void
fudge_closedir(struct SFTP_OPENDIR *od)
{
	free_sftp_dirents(od->dir);
	free(od);
}

static int
fudge_lstat(const char *path, struct stat *st)
{
	Attrib *a;

	if (!(a = do_lstat(cur.conn, (char *)path, 1)))
		return(-1);

	attrib_to_stat(a, st);

	return(0);
}

static int
fudge_stat(const char *path, struct stat *st)
{
	Attrib *a;

	if (!(a = do_stat(cur.conn, (char *)path, 1)))
		return(-1);

	attrib_to_stat(a, st);

	return(0);
}

int
remote_glob(struct sftp_conn *conn, const char *pattern, int flags,
    int (*errfunc)(const char *, int), glob_t *pglob)
{
	pglob->gl_opendir = fudge_opendir;
	pglob->gl_readdir = (struct dirent *(*)(void *))fudge_readdir;
	pglob->gl_closedir = (void (*)(void *))fudge_closedir;
	pglob->gl_lstat = fudge_lstat;
	pglob->gl_stat = fudge_stat;

	memset(&cur, 0, sizeof(cur));
	cur.conn = conn;

	return(glob(pattern, flags | GLOB_ALTDIRFUNC, errfunc, pglob));
}
