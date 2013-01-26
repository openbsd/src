/*	$OpenBSD: queue_null.c,v 1.1 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static int queue_null_init(int);
static int queue_null_message(enum queue_op, uint32_t *);
static int queue_null_envelope(enum queue_op , uint64_t *, char *, size_t);

struct queue_backend queue_backend_null = {
	queue_null_init,
	queue_null_message,
	queue_null_envelope,
};

static int	devnull;

static int
queue_null_init(int server)
{

	devnull = open("/dev/null", O_WRONLY, 0777);
	if (devnull == -1) {
		log_warn("open");
		return (0);
	}

	return (1);
}

static int
queue_null_message(enum queue_op qop, uint32_t *msgid)
{
	switch (qop) {
	case QOP_CREATE:
		*msgid = queue_generate_msgid();
		return (1);
	case QOP_DELETE:
	case QOP_COMMIT:
		return (1);
	case QOP_FD_R:
		return (-1);
	case QOP_FD_RW:
		return dup(devnull);
	case QOP_CORRUPT:
		return (0);
	default:
		fatalx("queue_null_message: unsupported operation.");
	}

	return (0);
}

static int
queue_null_envelope(enum queue_op qop, uint64_t *evpid, char *buf, size_t len)
{
	uint32_t	msgid;

	if (qop == QOP_WALK)
		return (-1);

	switch (qop) {
	case QOP_CREATE:
		msgid = evpid_to_msgid(*evpid);
		*evpid = queue_generate_evpid(msgid);
		return (1);
	case QOP_DELETE:
		return (1);
	case QOP_LOAD:
		return (0);
	case QOP_UPDATE:
		return (1);
	default:
		fatalx("queue_null_envelope: unsupported operation.");
	}

	return (0);
}
