/*	$Id: child.c,v 1.5 2019/02/17 17:19:05 deraadt Exp $ */
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * This is run on the client machine to initiate a connection with the
 * remote machine in --server mode.
 * It does not return, as it executes into the remote shell.
 *
 * Pledges: exec, stdio.
 */
void
rsync_child(const struct opts *opts, int fd, const struct fargs *f)
{
	struct sess	  sess;
	char		**args;
	size_t		  i;

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;

	/* Construct the remote shell command. */

	if ((args = fargs_cmdline(&sess, f)) == NULL) {
		ERRX1(&sess, "fargs_cmdline");
		exit(1);
	}

	for (i = 0; args[i] != NULL; i++)
		LOG2(&sess, "exec[%zu] = %s", i, args[i]);

	/* Make sure the child's stdin is from the sender. */

	if (dup2(fd, STDIN_FILENO) == -1) {
		ERR(&sess, "dup2");
		exit(1);
	} if (dup2(fd, STDOUT_FILENO) == -1) {
		ERR(&sess, "dup2");
		exit(1);
	}

	/* Here we go... */

	execvp(args[0], args);

	ERR(&sess, "%s: execvp", args[0]);
	exit(1);
	/* NOTREACHED */
}
