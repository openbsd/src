/*	$OpenBSD: queue_ram.c,v 1.1 2013/01/26 09:37:23 gilles Exp $	*/

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

static int queue_ram_init(int);
static int queue_ram_message(enum queue_op, uint32_t *);
static int queue_ram_envelope(enum queue_op , uint64_t *, char *, size_t);

struct queue_backend	queue_backend_ram = {
	queue_ram_init,
	queue_ram_message,
	queue_ram_envelope,
};

struct qr_envelope {
	char		*buf;
	size_t		 len;
};

struct qr_message {
	int		 hasfile;
	char		*buf;
	size_t		 len;
	struct tree	 envelopes;
};

static struct tree messages;

static int
queue_ram_init(int server)
{
	tree_init(&messages);

	return (1);
}

static int
queue_ram_message(enum queue_op qop, uint32_t *msgid)
{
	char			 path[MAXPATHLEN];
	uint64_t		 evpid;
	struct qr_envelope	*evp;
	struct qr_message	*msg;
	int			 fd, fd2, ret;
	struct stat		 sb;
	FILE			*f;
	size_t			 n;

	switch (qop) {
	case QOP_CREATE:
		msg = xcalloc(1, sizeof *msg, "queue_ram_message");
		tree_init(&msg->envelopes);
		do {
			*msgid = queue_generate_msgid();
		} while (tree_check(&messages, *msgid));
		tree_xset(&messages, *msgid, msg);
		return (1);

	case QOP_DELETE:
		if ((msg = tree_pop(&messages, *msgid)) == NULL) {
			log_warnx("warn: queue_ram_message: not found");
			return (0);
		}
		while (tree_poproot(&messages, &evpid, (void**)&evp)) {
			stat_decrement("queue.ram.envelope.size", evp->len);
			free(evp->buf);
			free(evp);
		}
		stat_decrement("queue.ram.message.size", msg->len);
		free(msg->buf);
		if (msg->hasfile) {
			queue_message_incoming_path(*msgid, path, sizeof(path));
			if (unlink(path) == -1)
				log_warn("warn: queue_ram_message: unlink");
		}
		free(msg);
		return (1);

	case QOP_COMMIT:
		if ((msg = tree_get(&messages, *msgid)) == NULL) {
			log_warnx("warn: queue_ram_message: not found");
			return (0);
		}
		queue_message_incoming_path(*msgid, path, sizeof(path));
		f = fopen(path, "rb");
		if (f == NULL) {
			log_warn("warn: queue_ram: fopen");
			return (0);
		}
		if (fstat(fileno(f), &sb) == -1) {
			log_warn("warn: queue_ram_message: fstat");
			fclose(f);
			return (0);
		}

		msg->len = sb.st_size;
		msg->buf = xmalloc(msg->len, "queue_ram_message");
		ret = 0;
		n = fread(msg->buf, 1, msg->len, f);
		if (ferror(f))
			log_warn("warn: queue_ram_message: fread");
		else if ((off_t)n != sb.st_size)
			log_warnx("warn: queue_ram_message: bad read");
		else {
			ret = 1;
			stat_increment("queue.ram.message.size", msg->len);
		}
		fclose(f);
		if (unlink(path) == -1)
			log_warn("warn: queue_ram_message: unlink");
		msg->hasfile = 0;
		return (ret);

	case QOP_FD_R:
		if ((msg = tree_get(&messages, *msgid)) == NULL) {
			log_warnx("warn: queue_ram_message: not found");
			return (-1);
		}
		fd = mktmpfile();
		if (fd == -1) {
			log_warn("warn: queue_ram_message: mktmpfile");
			return (-1);
		}
		fd2 = dup(fd);
		if (fd2 == -1) {
			log_warn("warn: queue_ram_message: dup");
			close(fd);
			return (-1);
		}
		f = fdopen(fd2, "w");
		if (f == NULL) {
			log_warn("warn: queue_ram_message: fdopen");
			close(fd);
			close(fd2);
			return (-1);
		}
		n = fwrite(msg->buf, 1, msg->len, f);
		if (n != msg->len) {
			log_warn("warn: queue_ram_message: write");
			close(fd);
			fclose(f);
			return (-1);
		}
		fclose(f);
		lseek(fd, 0, SEEK_SET);
		return (fd);

	case QOP_FD_RW:
		if ((msg = tree_get(&messages, *msgid)) == NULL) {
			log_warnx("warn: queue_ram_message: not found");
			return (-1);
		}
		queue_message_incoming_path(*msgid, path, sizeof(path));
		fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd == -1) {
			log_warn("warn: queue_ram_message: open");
			return (-1);
		}
		msg->hasfile = 1;
		return (fd);

	case QOP_CORRUPT:
		return (queue_ram_message(QOP_DELETE, msgid));

	default:
		fatalx("queue_ram_message: unsupported operation.");
	}

	return (0);
}

static int
queue_ram_envelope(enum queue_op qop, uint64_t *evpid, char *buf, size_t len)
{
	struct qr_envelope	*evp;
	struct qr_message	*msg;
	uint32_t		 msgid;
	char			*tmp;

	if (qop == QOP_WALK)
		return (-1);

	msgid = evpid_to_msgid(*evpid);
	msg = tree_get(&messages, msgid);
	if (msg == NULL) {
		log_warn("warn: queue_ram_envelope: message not found");
		return (0);
	}

	switch (qop) {
	case QOP_CREATE:
		do {
			*evpid = queue_generate_evpid(msgid);
		} while (tree_check(&msg->envelopes, *evpid));
		evp = calloc(1, sizeof *evp);
		if (evp == NULL) {
			log_warn("warn: queue_ram_envelope: calloc");
			return (0);
		}
		evp->len = len;
		evp->buf = malloc(len);
		if (evp->buf == NULL) {
			log_warn("warn: queue_ram_envelope: malloc");
			return (0);
		}
		memmove(evp->buf, buf, len);
		tree_xset(&msg->envelopes, *evpid, evp);
		stat_increment("queue.ram.envelope.size", len);
		return (1);

	case QOP_DELETE:
		if ((evp = tree_pop(&msg->envelopes, *evpid)) == NULL) {
			log_warnx("warn: queue_ram_envelope: not found");
			return (0);
		}
		stat_decrement("queue.ram.envelope.size", evp->len);
		free(evp->buf);
		free(evp);
		if (tree_empty(&msg->envelopes)) {
			tree_xpop(&messages, msgid);
			stat_decrement("queue.ram.message.size", msg->len);
			free(msg->buf);
			free(msg);
		}
		return (1);

	case QOP_LOAD:
		if ((evp = tree_get(&msg->envelopes, *evpid)) == NULL) {
			log_warn("warn: queue_ram_envelope: not found");
			return (0);
		}
		if (len < evp->len) {
			log_warnx("warn: queue_ram_envelope: buffer too small");
			return (0);
		}
		memmove(buf, evp->buf, evp->len);
		return (evp->len);

	case QOP_UPDATE:
		if ((evp = tree_get(&msg->envelopes, *evpid)) == NULL) {
			log_warn("warn: queue_ram_envelope: not found");
			return (0);
		}
		tmp = malloc(len);
		if (tmp == NULL) {
			log_warn("warn: queue_ram_envelope: malloc");
			return (0);
		}
		memmove(tmp, buf, len);
		free(evp->buf);
		evp->len = len;
		evp->buf = tmp;
		stat_decrement("queue.ram.envelope.size", evp->len);
		stat_increment("queue.ram.envelope.size", len);
		return (1);

	default:
		fatalx("queue_ram_envelope: unsupported operation.");
	}

	return (0);
}
