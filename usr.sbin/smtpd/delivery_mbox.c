/*	$OpenBSD: delivery_mbox.c,v 1.11 2015/01/20 17:37:54 deraadt Exp $	*/

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

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"

#define	PATH_MAILLOCAL	"/usr/libexec/mail.local"

extern char	**environ;

/* mbox backend */
static void delivery_mbox_open(struct deliver *);

struct delivery_backend delivery_backend_mbox = {
	1, delivery_mbox_open
};


static void
delivery_mbox_open(struct deliver *deliver)
{
	char	*environ_new[2];

	environ_new[0] = "PATH=" _PATH_DEFPATH;
	environ_new[1] = (char *)NULL;
	environ = environ_new;

	if (deliver->from[0] == '\0')
		(void)strlcpy(deliver->from, "MAILER-DAEMON", sizeof deliver->from);
	execle(PATH_MAILLOCAL, PATH_MAILLOCAL, "-f", deliver->from,
	    deliver->to, (char *)NULL, environ_new);
	perror("execle");
	_exit(1);
}
