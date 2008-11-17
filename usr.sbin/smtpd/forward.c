/*	$OpenBSD: forward.c,v 1.3 2008/11/17 21:32:23 gilles Exp $	*/

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
#include <sys/stat.h>

#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"

int	alias_parse(struct alias *, char *);
int	forwards_get(struct aliaseslist *, char *);

int
forwards_get(struct aliaseslist *aliases, char *username)
{
	FILE *fp;
	struct alias alias;
	struct alias *aliasp;
	char	pathname[MAXPATHLEN];
	struct passwd *pw;
	char *buf, *lbuf;
	size_t len;
	struct stat sb;

	pw = getpwnam(username);
	if (pw == NULL)
		return 0;

	if (snprintf(pathname, MAXPATHLEN, "%s/.forward", pw->pw_dir)
	    >= MAXPATHLEN)
		return 0;

	fp = fopen(pathname, "r");
	if (fp == NULL)
		return 0;

	log_debug("+ opening forward file %s", pathname);
	/* make sure ~/ is not writable by anyone but owner */
	if (stat(pw->pw_dir, &sb) == -1)
		goto bad;
	if (sb.st_uid != pw->pw_uid || sb.st_mode & (S_IWGRP|S_IWOTH))
		goto bad;

	/* make sure ~/.forward is not writable by anyone but owner */
	if (fstat(fileno(fp), &sb) == -1)
		goto bad;
	if (sb.st_uid != pw->pw_uid || sb.st_mode & (S_IWGRP|S_IWOTH))
		goto bad;

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				fatal("malloc");
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		log_debug("\tforward: %s", buf);
		if (! alias_parse(&alias, buf)) {
			log_debug("bad entry in ~/.forward");
			continue;
		}

		if (alias.type == ALIAS_INCLUDE) {
			log_debug("includes are forbidden in ~/.forward");
			continue;
		}
		aliasp = calloc(1, sizeof(struct alias));
		if (aliasp == NULL)
			fatal("calloc");
		*aliasp = alias;
		TAILQ_INSERT_HEAD(aliases, aliasp, entry);

	}
	free(lbuf);
	fclose(fp);
	return 1;

bad:
	log_debug("+ forward file error, probably bad perms/mode");
	if (fp != NULL)
		fclose(fp);
	return 0;
}
