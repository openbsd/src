/*	$OpenBSD: queue_api.c,v 1.7 2015/01/20 17:37:54 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Eric Faurot <eric@openbsd.org>
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

#include <netinet/in.h>
#include <netdb.h>

#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "log.h"

static int (*handler_close)(void);
static int (*handler_message_create)(uint32_t *);
static int (*handler_message_commit)(uint32_t, const char *);
static int (*handler_message_delete)(uint32_t);
static int (*handler_message_fd_r)(uint32_t);
static int (*handler_message_corrupt)(uint32_t);
static int (*handler_envelope_create)(uint32_t, const char *, size_t, uint64_t *);
static int (*handler_envelope_delete)(uint64_t);
static int (*handler_envelope_update)(uint64_t, const char *, size_t);
static int (*handler_envelope_load)(uint64_t, char *, size_t);
static int (*handler_envelope_walk)(uint64_t *, char *, size_t);

static struct imsgbuf	 ibuf;
static struct imsg	 imsg;
static size_t		 rlen;
static char		*rdata;
static struct ibuf	*buf;
static const char	*rootpath = PATH_SPOOL;
static const char	*user = SMTPD_QUEUE_USER;

static void
queue_msg_get(void *dst, size_t len)
{
	if (len > rlen) {
		log_warnx("warn: queue-proc: bad msg len");
		fatalx("queue-proc: exiting");
	}

	if (len == 0)
		return;

	if (dst)
		memmove(dst, rdata, len);

	rlen -= len;
	rdata += len;
}

static void
queue_msg_end(void)
{
	if (rlen) {
		log_warnx("warn: queue-proc: bogus data");
		fatalx("queue-proc: exiting");
	}
	imsg_free(&imsg);
}

static void
queue_msg_add(const void *data, size_t len)
{
	if (buf == NULL)
		buf = imsg_create(&ibuf, PROC_QUEUE_OK, 0, 0, 1024);
	if (buf == NULL) {
		log_warnx("warn: queue-api: imsg_create failed");
		fatalx("queue-api: exiting");
	}
	if (imsg_add(buf, data, len) == -1) {
		log_warnx("warn: queue-api: imsg_add failed");
		fatalx("queue-api: exiting");
	}
}

static void
queue_msg_close(void)
{
	imsg_close(&ibuf, buf);
	buf = NULL;
}

static void
queue_msg_dispatch(void)
{
	uint64_t	 evpid;
	uint32_t	 msgid, version;
	size_t		 n, m;
	char		 buffer[8192], path[SMTPD_MAXPATHLEN];
	int		 r, fd;
	FILE		*ifile, *ofile;

	switch (imsg.hdr.type) {
	case PROC_QUEUE_INIT:
		queue_msg_get(&version, sizeof(version));
		queue_msg_end();

		if (version != PROC_QUEUE_API_VERSION) {
			log_warnx("warn: queue-api: bad API version");
			fatalx("queue-api: exiting");
		}

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, NULL, 0);
		break;

	case PROC_QUEUE_CLOSE:
		queue_msg_end();

		if (handler_close)
			r = handler_close();
		else
			r = 1;

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_QUEUE_MESSAGE_CREATE:
		queue_msg_end();

		r = handler_message_create(&msgid);

		queue_msg_add(&r, sizeof(r));
		if (r == 1)
			queue_msg_add(&msgid, sizeof(msgid));
		queue_msg_close();
		break;

	case PROC_QUEUE_MESSAGE_DELETE:
		queue_msg_get(&msgid, sizeof(msgid));
		queue_msg_end();

		r = handler_message_delete(msgid);

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_QUEUE_MESSAGE_COMMIT:
		queue_msg_get(&msgid, sizeof(msgid));
		queue_msg_end();

		/* XXX needs more love */
		r = -1;
		(void)snprintf(path, sizeof path, "/tmp/message.XXXXXXXXXX");
		fd = mkstemp(path);
		if (fd == -1) {
			log_warn("warn: queue-api: mkstemp");
		}
		else {
			ifile = fdopen(imsg.fd, "r");
			ofile = fdopen(fd, "w");
			m = n = 0;
			if (ifile && ofile) {
				while (!feof(ifile)) {
					n = fread(buffer, 1, sizeof(buffer),
					    ifile);
					m = fwrite(buffer, 1, n, ofile);
					if (m != n)
						break;
					fflush(ofile);
				}
				if (m != n)
					r = 0;
				else
					r = handler_message_commit(msgid, path);
			}
			if (ifile)
				fclose(ifile);
			else
				close(imsg.fd);
			if (ofile)
				fclose(ofile);
			else
				close(fd);
		}

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_QUEUE_MESSAGE_FD_R:
		queue_msg_get(&msgid, sizeof(msgid));
		queue_msg_end();

		fd = handler_message_fd_r(msgid);

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, fd, NULL, 0);
		break;

	case PROC_QUEUE_MESSAGE_CORRUPT:
		queue_msg_get(&msgid, sizeof(msgid));
		queue_msg_end();

		r = handler_message_corrupt(msgid);

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_QUEUE_ENVELOPE_CREATE:
		queue_msg_get(&msgid, sizeof(msgid));
		r = handler_envelope_create(msgid, rdata, rlen, &evpid);
		queue_msg_get(NULL, rlen);
		queue_msg_end();

		queue_msg_add(&r, sizeof(r));
		if (r == 1)
			queue_msg_add(&evpid, sizeof(evpid));
		queue_msg_close();
		break;

	case PROC_QUEUE_ENVELOPE_DELETE:
		queue_msg_get(&evpid, sizeof(evpid));
		queue_msg_end();

		r = handler_envelope_delete(evpid);

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_QUEUE_ENVELOPE_LOAD:
		queue_msg_get(&evpid, sizeof(evpid));
		queue_msg_end();

		r = handler_envelope_load(evpid, buffer, sizeof(buffer));

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, buffer, r);
		break;

	case PROC_QUEUE_ENVELOPE_UPDATE:
		queue_msg_get(&evpid, sizeof(evpid));
		r = handler_envelope_update(evpid, rdata, rlen);
		queue_msg_get(NULL, rlen);
		queue_msg_end();

		imsg_compose(&ibuf, PROC_QUEUE_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_QUEUE_ENVELOPE_WALK:
		queue_msg_end();

		r = handler_envelope_walk(&evpid, buffer, sizeof(buffer));

		queue_msg_add(&r, sizeof(r));
		if (r > 0) {
			queue_msg_add(&evpid, sizeof(evpid));
			queue_msg_add(buffer, r);
		}
		queue_msg_close();
		break;

	default:
		log_warnx("warn: queue-api: bad message %d", imsg.hdr.type);
		fatalx("queue-api: exiting");
	}
}

void
queue_api_on_close(int(*cb)(void))
{
	handler_close = cb;
}

void
queue_api_on_message_create(int(*cb)(uint32_t *))
{
	handler_message_create = cb;
}

void
queue_api_on_message_commit(int(*cb)(uint32_t, const char *))
{
	handler_message_commit = cb;
}

void
queue_api_on_message_delete(int(*cb)(uint32_t))
{
	handler_message_delete = cb;
}

void
queue_api_on_message_fd_r(int(*cb)(uint32_t))
{
	handler_message_fd_r = cb;
}

void
queue_api_on_message_corrupt(int(*cb)(uint32_t))
{
	handler_message_corrupt = cb;
}

void
queue_api_on_envelope_create(int(*cb)(uint32_t, const char *, size_t, uint64_t *))
{
	handler_envelope_create = cb;
}

void
queue_api_on_envelope_delete(int(*cb)(uint64_t))
{
	handler_envelope_delete = cb;
}

void
queue_api_on_envelope_update(int(*cb)(uint64_t, const char *, size_t))
{
	handler_envelope_update = cb;
}

void
queue_api_on_envelope_load(int(*cb)(uint64_t, char *, size_t))
{
	handler_envelope_load = cb;
}

void
queue_api_on_envelope_walk(int(*cb)(uint64_t *, char *, size_t))
{
	handler_envelope_walk = cb;
}

void
queue_api_no_chroot(void)
{
	rootpath = NULL;
}

void
queue_api_set_chroot(const char *path)
{
	rootpath = path;
}

void
queue_api_set_user(const char *username)
{
	user = username;
}

int
queue_api_dispatch(void)
{
	struct passwd	*pw = NULL;
	ssize_t		 n;

	if (user) {
		pw = getpwnam(user);
		if (pw == NULL) {
			log_warn("queue-api: getpwnam");
			fatalx("queue-api: exiting");
		}
	}

	if (rootpath) {
		if (chroot(rootpath) == -1) {
			log_warn("queue-api: chroot");
			fatalx("queue-api: exiting");
		}
		if (chdir("/") == -1) {
			log_warn("queue-api: chdir");
			fatalx("queue-api: exiting");
		}
	}

	if (pw &&
	   (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))) {
		log_warn("queue-api: cannot drop privileges");
		fatalx("queue-api: exiting");
	}

	imsg_init(&ibuf, 0);

	while (1) {
		n = imsg_get(&ibuf, &imsg);
		if (n == -1) {
			log_warn("warn: queue-api: imsg_get");
			break;
		}

		if (n) {
			rdata = imsg.data;
			rlen = imsg.hdr.len - IMSG_HEADER_SIZE;
			queue_msg_dispatch();
			imsg_flush(&ibuf);
			continue;
		}

		n = imsg_read(&ibuf);
		if (n == -1) {
			log_warn("warn: queue-api: imsg_read");
			break;
		}
		if (n == 0) {
			log_warnx("warn: queue-api: pipe closed");
			break;
		}
	}

	return (1);
}
