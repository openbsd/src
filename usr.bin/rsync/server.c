/*	$OpenBSD: server.c,v 1.15 2021/11/03 14:42:12 deraadt Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "extern.h"

static int
fcntl_nonblock(int fd)
{
	int	 fl;

	if ((fl = fcntl(fd, F_GETFL, 0)) == -1)
		ERR("fcntl: F_GETFL");
	else if (fcntl(fd, F_SETFL, fl|O_NONBLOCK) == -1)
		ERR("fcntl: F_SETFL");
	else
		return 1;

	return 0;
}

/*
 * The server (remote) side of the system.
 * This parses the arguments given it by the remote shell then moves
 * into receiver or sender mode depending upon those arguments.
 * Returns exit code 0 on success, 1 on failure, 2 on failure with
 * incompatible protocols.
 */
int
rsync_server(const struct opts *opts, size_t argc, char *argv[])
{
	struct sess	 sess;
	int		 fdin = STDIN_FILENO,
			 fdout = STDOUT_FILENO, rc = 1;

	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;

	/* Begin by making descriptors non-blocking. */

	if (!fcntl_nonblock(fdin) ||
	    !fcntl_nonblock(fdout)) {
		ERRX1("fcntl_nonblock");
		goto out;
	}

	/* Standard rsync preamble, server side. */

	sess.lver = RSYNC_PROTOCOL;
	sess.seed = arc4random();

	if (!io_read_int(&sess, fdin, &sess.rver)) {
		ERRX1("io_read_int");
		goto out;
	} else if (!io_write_int(&sess, fdout, sess.lver)) {
		ERRX1("io_write_int");
		goto out;
	} else if (!io_write_int(&sess, fdout, sess.seed)) {
		ERRX1("io_write_int");
		goto out;
	}

	sess.mplex_writes = 1;

	if (sess.rver < sess.lver) {
		ERRX("remote protocol %d is older than our own %d: unsupported",
		    sess.rver, sess.lver);
		rc = 2;
		goto out;
	}

	LOG2("server detected client version %d, server version %d, seed %d",
	    sess.rver, sess.lver, sess.seed);

	if (sess.opts->sender) {
		LOG2("server starting sender");

		/*
		 * At this time, I always get a period as the first
		 * argument of the command line.
		 * Let's make it a requirement until I figure out when
		 * that differs.
		 * rsync [flags] "." <source> <...>
		 */

		if (strcmp(argv[0], ".")) {
			ERRX("first argument must be a standalone period");
			goto out;
		}
		argv++;
		argc--;
		if (argc == 0) {
			ERRX("must have arguments");
			goto out;
		}

		if (!rsync_sender(&sess, fdin, fdout, argc, argv)) {
			ERRX1("rsync_sender");
			goto out;
		}
	} else {
		LOG2("server starting receiver");

		/*
		 * I don't understand why this calling convention
		 * exists, but we must adhere to it.
		 * rsync [flags] "." <destination>
		 */

		if (argc != 2) {
			ERRX("server receiver mode requires two argument");
			goto out;
		} else if (strcmp(argv[0], ".")) {
			ERRX("first argument must be a standalone period");
			goto out;
		}

		if (!rsync_receiver(&sess, fdin, fdout, argv[1])) {
			ERRX1("rsync_receiver");
			goto out;
		}
	}

#if 0
	/* Probably the EOF. */
	if (io_read_check(&sess, fdin))
		WARNX("data remains in read pipe");
#endif

	rc = 0;
out:
	return rc;
}
