/*	$Id: chngproc.c,v 1.7 2016/09/13 17:13:37 deraadt Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

int
chngproc(int netsock, const char *root, int remote)
{
	char		 *tok = NULL, *th = NULL, *fmt = NULL, **fs = NULL;
	size_t		  i, fsz = 0;
	int		  rc = 0, fd = -1, cc;
	long		  lval;
	enum chngop	  op;
	void		 *pp;

	if (chroot(root) == -1) {
		warn("chroot");
		goto out;
	}
	if (chdir("/") == -1) {
		warn("chdir");
		goto out;
	}
	if (pledge("stdio cpath wpath", NULL) == -1) {
		warn("pledge");
		goto out;
	}

	/*
	 * Loop while we wait to get a thumbprint and token.
	 * We'll get this for each SAN request.
	 */

	for (;;) {
		op = CHNG__MAX;
		if (0 == (lval = readop(netsock, COMM_CHNG_OP)))
			op = CHNG_STOP;
		else if (CHNG_SYN == lval)
			op = lval;

		if (CHNG__MAX == op) {
			warnx("unknown operation from netproc");
			goto out;
		} else if (CHNG_STOP == op)
			break;

		assert(CHNG_SYN == op);

		/*
		 * Read the thumbprint and token.
		 * The token is the filename, so store that in a vector
		 * of tokens that we'll later clean up.
		 */

		if (NULL == (th = readstr(netsock, COMM_THUMB)))
			goto out;
		else if (NULL == (tok = readstr(netsock, COMM_TOK)))
			goto out;

		/* Vector appending... */

		pp = reallocarray(fs, (fsz + 1), sizeof(char *));
		if (NULL == pp) {
			warn("realloc");
			goto out;
		}
		fs = pp;
		fs[fsz] = tok;
		tok = NULL;
		fsz++;

		if (-1 == asprintf(&fmt, "%s.%s", fs[fsz - 1], th)) {
			warn("asprintf");
			goto out;
		}

		/*
		 * I use this for testing when letskencrypt is being run
		 * on machines apart from where I'm hosting the
		 * challenge directory.
		 * DON'T DEPEND ON THIS FEATURE.
		 */
		if (remote) {
			puts("RUN THIS IN THE CHALLENGE DIRECTORY");
			puts("YOU HAVE 20 SECONDS...");
			printf("doas sh -c \"echo %s > %s\"\n",
			    fmt, fs[fsz - 1]);
			sleep(20);
			puts("TIME'S UP.");
		} else {
			/*
			 * Create and write to our challenge file.
			 * Note: we use file descriptors instead of FILE
			 * because we want to minimise our pledges.
			 */
			fd = open(fs[fsz - 1], O_WRONLY|O_EXCL|O_CREAT, 0444);
			if (-1 == fd) {
				warn("%s", fs[fsz - 1]);
				goto out;
			} if (-1 == write(fd, fmt, strlen(fmt))) {
				warn("%s", fs[fsz - 1]);
				goto out;
			} else if (-1 == close(fd)) {
				warn("%s", fs[fsz - 1]);
				goto out;
			}
			fd = -1;
		}

		free(th);
		free(fmt);
		th = fmt = NULL;

		dodbg("%s/%s: created", root, fs[fsz - 1]);

		/*
		 * Write our acknowledgement.
		 * Ignore reader failure.
		 */

		cc = writeop(netsock, COMM_CHNG_ACK, CHNG_ACK);
		if (0 == cc)
			break;
		if (cc < 0)
			goto out;
	}

	rc = 1;
out:
	close(netsock);
	if (-1 != fd)
		close(fd);
	for (i = 0; i < fsz; i++) {
		if (-1 == unlink(fs[i]) && ENOENT != errno)
			warn("%s", fs[i]);
		free(fs[i]);
	}
	free(fs);
	free(fmt);
	free(th);
	free(tok);
	return(rc);
}
