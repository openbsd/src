/*	$OpenBSD: delivery_maildir.c,v 1.13 2014/04/19 17:31:35 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

extern char	**environ;

/* maildir backend */
static void delivery_maildir_open(struct deliver *);

struct delivery_backend delivery_backend_maildir = {
	1, delivery_maildir_open
};


static void
delivery_maildir_open(struct deliver *deliver)
{
	char	 tmp[SMTPD_MAXPATHLEN], new[SMTPD_MAXPATHLEN];
	int	 ch, fd;
	FILE	*fp;
	char	*msg;
	int	 n;

#define error(m)	{ msg = m; goto err; }
#define error2(m)	{ msg = m; goto err2; }

	setproctitle("maildir delivery");
	if (mkdirs(deliver->to, 0700) < 0 && errno != EEXIST)
		error("cannot mkdir maildir");
	if (chdir(deliver->to) < 0)
		error("cannot cd to maildir");
	if (mkdir("cur", 0700) < 0 && errno != EEXIST)
		error("mkdir cur failed");
	if (mkdir("tmp", 0700) < 0 && errno != EEXIST)
		error("mkdir tmp failed");
	if (mkdir("new", 0700) < 0 && errno != EEXIST)
		error("mkdir new failed");
	(void)snprintf(tmp, sizeof tmp, "tmp/%lld.%d.%s",
	    (long long int) time(NULL),
	    getpid(), env->sc_hostname);
	fd = open(tmp, O_CREAT | O_EXCL | O_WRONLY, 0600);
	if (fd < 0)
		error("cannot open tmp file");
	fp = fdopen(fd, "w");
	if (fp == NULL)
		error2("fdopen");
	while ((ch = getc(stdin)) != EOF)
		if (putc(ch, fp) == EOF)
			break;
	if (ferror(stdin))
		error2("read error");
	if (fflush(fp) == EOF || ferror(fp))
		error2("write error");
	if (fsync(fd) < 0)
		error2("fsync");
	if (fclose(fp) == EOF)
		error2("fclose");
	(void)snprintf(new, sizeof new, "new/%s", tmp + 4);
	if (rename(tmp, new) < 0)
		error2("cannot rename tmp->new");
	_exit(0);

err2:
	n = errno;
	unlink(tmp);
	errno = n;
err:
	perror(msg);
	_exit(1);
}
