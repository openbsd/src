/*	$OpenBSD: scheduler_api.c,v 1.7 2015/01/20 17:37:54 deraadt Exp $	*/

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
#include <string.h>
#include <unistd.h>

#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "log.h"

static int (*handler_init)(void);
static int (*handler_insert)(struct scheduler_info *);
static size_t (*handler_commit)(uint32_t);
static size_t (*handler_rollback)(uint32_t);
static int (*handler_update)(struct scheduler_info *);
static int (*handler_delete)(uint64_t);
static int (*handler_hold)(uint64_t, uint64_t);
static int (*handler_release)(int, uint64_t, int);
static int (*handler_batch)(int, int *, size_t *, uint64_t *, int *);
static size_t (*handler_messages)(uint32_t, uint32_t *, size_t);
static size_t (*handler_envelopes)(uint64_t, struct evpstate *, size_t);
static int (*handler_schedule)(uint64_t);
static int (*handler_remove)(uint64_t);
static int (*handler_suspend)(uint64_t);
static int (*handler_resume)(uint64_t);

#define MAX_BATCH_SIZE	1024

static struct imsgbuf	 ibuf;
static struct imsg	 imsg;
static size_t		 rlen;
static char		*rdata;
static struct ibuf	*buf;
static const char	*rootpath = PATH_CHROOT;
static const char	*user = SMTPD_USER;

static void
scheduler_msg_get(void *dst, size_t len)
{
	if (len > rlen) {
		log_warnx("warn: scheduler-proc: bad msg len");
		fatalx("scheduler-proc: exiting");
	}

	if (len == 0)
		return;

	if (dst)
		memmove(dst, rdata, len);

	rlen -= len;
	rdata += len;
}

static void
scheduler_msg_end(void)
{
	if (rlen) {
		log_warnx("warn: scheduler-proc: bogus data");
		fatalx("scheduler-proc: exiting");
	}
	imsg_free(&imsg);
}

static void
scheduler_msg_add(const void *data, size_t len)
{
	if (buf == NULL)
		buf = imsg_create(&ibuf, PROC_SCHEDULER_OK, 0, 0, 1024);
	if (buf == NULL) {
		log_warnx("warn: table-api: imsg_create failed");
		fatalx("table-api: exiting");
	}
	if (imsg_add(buf, data, len) == -1) {
		log_warnx("warn: table-api: imsg_add failed");
		fatalx("table-api: exiting");
	}
}

static void
scheduler_msg_close(void)
{
	imsg_close(&ibuf, buf);
	buf = NULL;
}

static void
scheduler_msg_dispatch(void)
{
	size_t			 n, sz, count;
	struct evpstate		 evpstates[MAX_BATCH_SIZE];
	uint64_t		 evpid, evpids[MAX_BATCH_SIZE], u64;
	uint32_t		 msgids[MAX_BATCH_SIZE], version, msgid;
	struct scheduler_info	 info;
	int			 typemask, r, type, types[MAX_BATCH_SIZE];
	int			 delay;

	switch (imsg.hdr.type) {
	case PROC_SCHEDULER_INIT:
		log_debug("scheduler-api:  PROC_SCHEDULER_INIT");
		scheduler_msg_get(&version, sizeof(version));
		scheduler_msg_end();

		if (version != PROC_SCHEDULER_API_VERSION) {
			log_warnx("warn: scheduler-api: bad API version");
			fatalx("scheduler-api: exiting");
		}

		r = handler_init();

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_INSERT:
		log_debug("scheduler-api:  PROC_SCHEDULER_INSERT");
		scheduler_msg_get(&info, sizeof(info));
		scheduler_msg_end();

		r = handler_insert(&info);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_COMMIT:
		log_debug("scheduler-api:  PROC_SCHEDULER_COMMIT");
		scheduler_msg_get(&msgid, sizeof(msgid));
		scheduler_msg_end();

		n = handler_commit(msgid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &n, sizeof(n));
		break;

	case PROC_SCHEDULER_ROLLBACK:
		log_debug("scheduler-api:  PROC_SCHEDULER_ROLLBACK");
		scheduler_msg_get(&msgid, sizeof(msgid));
		scheduler_msg_end();

		n = handler_rollback(msgid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &n, sizeof(n));
		break;

	case PROC_SCHEDULER_UPDATE:
		log_debug("scheduler-api:  PROC_SCHEDULER_UPDATE");
		scheduler_msg_get(&info, sizeof(info));
		scheduler_msg_end();

		r = handler_update(&info);

		scheduler_msg_add(&r, sizeof(r));
		if (r == 1)
			scheduler_msg_add(&info, sizeof(info));
		scheduler_msg_close();
		break;

	case PROC_SCHEDULER_DELETE:
		log_debug("scheduler-api:  PROC_SCHEDULER_DELETE");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_end();

		r = handler_delete(evpid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_HOLD:
		log_debug("scheduler-api: PROC_SCHEDULER_HOLD");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_get(&u64, sizeof(u64));
		scheduler_msg_end();

		r = handler_hold(evpid, u64);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_RELEASE:
		log_debug("scheduler-api: PROC_SCHEDULER_RELEASE");
		scheduler_msg_get(&type, sizeof(type));
		scheduler_msg_get(&u64, sizeof(u64));
		scheduler_msg_get(&r, sizeof(r));
		scheduler_msg_end();

		r = handler_release(type, u64, r);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_BATCH:
		log_debug("scheduler-api:  PROC_SCHEDULER_BATCH");
		scheduler_msg_get(&typemask, sizeof(typemask));
		scheduler_msg_get(&count, sizeof(count));
		scheduler_msg_end();

		if (count > MAX_BATCH_SIZE)
			count = MAX_BATCH_SIZE;

		r = handler_batch(typemask, &delay, &count, evpids, types);
		scheduler_msg_add(&r, sizeof(r));
		scheduler_msg_add(&delay, sizeof(delay));
		scheduler_msg_add(&count, sizeof(count));
		if (r > 0) {
			scheduler_msg_add(evpids, sizeof(*evpids) * count);
			scheduler_msg_add(types, sizeof(*types) * count);
		}
		scheduler_msg_close();
		break;

	case PROC_SCHEDULER_MESSAGES:
		log_debug("scheduler-api:  PROC_SCHEDULER_MESSAGES");
		scheduler_msg_get(&msgid, sizeof(msgid));
		scheduler_msg_get(&sz, sizeof(sz));
		scheduler_msg_end();

		if (sz > MAX_BATCH_SIZE)
			sz = MAX_BATCH_SIZE;

		n = handler_messages(msgid, msgids, sz);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, msgids,
		    n * sizeof(*msgids));
		break;

	case PROC_SCHEDULER_ENVELOPES:
		log_debug("scheduler-api:  PROC_SCHEDULER_ENVELOPES");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_get(&sz, sizeof(sz));
		scheduler_msg_end();

		if (sz > MAX_BATCH_SIZE)
			sz = MAX_BATCH_SIZE;

		n = handler_envelopes(evpid, evpstates, sz);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, evpstates,
		    n * sizeof(*evpstates));
		break;

	case PROC_SCHEDULER_SCHEDULE:
		log_debug("scheduler-api:  PROC_SCHEDULER_SCHEDULE");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_end();

		r = handler_schedule(evpid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_REMOVE:
		log_debug("scheduler-api:  PROC_SCHEDULER_REMOVE");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_end();

		r = handler_remove(evpid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_SUSPEND:
		log_debug("scheduler-api:  PROC_SCHEDULER_SUSPEND");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_end();

		r = handler_suspend(evpid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	case PROC_SCHEDULER_RESUME:
		log_debug("scheduler-api:  PROC_SCHEDULER_RESUME");
		scheduler_msg_get(&evpid, sizeof(evpid));
		scheduler_msg_end();

		r = handler_resume(evpid);

		imsg_compose(&ibuf, PROC_SCHEDULER_OK, 0, 0, -1, &r, sizeof(r));
		break;

	default:
		log_warnx("warn: scheduler-api: bad message %d", imsg.hdr.type);
		fatalx("scheduler-api: exiting");
	}
}

void
scheduler_api_on_init(int(*cb)(void))
{
	handler_init = cb;
}

void
scheduler_api_on_insert(int(*cb)(struct scheduler_info *))
{
	handler_insert = cb;
}

void
scheduler_api_on_commit(size_t(*cb)(uint32_t))
{
	handler_commit = cb;
}

void
scheduler_api_on_rollback(size_t(*cb)(uint32_t))
{
	handler_rollback = cb;
}

void
scheduler_api_on_update(int(*cb)(struct scheduler_info *))
{
	handler_update = cb;
}

void
scheduler_api_on_delete(int(*cb)(uint64_t))
{
	handler_delete = cb;
}

void
scheduler_api_on_batch(int(*cb)(int, int *, size_t *, uint64_t *, int *))
{
	handler_batch = cb;
}

void
scheduler_api_on_messages(size_t(*cb)(uint32_t, uint32_t *, size_t))
{
	handler_messages = cb;
}

void
scheduler_api_on_envelopes(size_t(*cb)(uint64_t, struct evpstate *, size_t))
{
	handler_envelopes = cb;
}

void
scheduler_api_on_schedule(int(*cb)(uint64_t))
{
	handler_schedule = cb;
}

void
scheduler_api_on_remove(int(*cb)(uint64_t))
{
	handler_remove = cb;
}

void
scheduler_api_on_suspend(int(*cb)(uint64_t))
{
	handler_suspend = cb;
}

void
scheduler_api_on_resume(int(*cb)(uint64_t))
{
	handler_resume = cb;
}

void
scheduler_api_on_hold(int(*cb)(uint64_t, uint64_t))
{
	handler_hold = cb;
}

void
scheduler_api_on_release(int(*cb)(int, uint64_t, int))
{
	handler_release = cb;
}

void
scheduler_api_no_chroot(void)
{
	rootpath = NULL;
}

void
scheduler_api_set_chroot(const char *path)
{
	rootpath = path;
}

void
scheduler_api_set_user(const char *username)
{
	user = username;
}

int
scheduler_api_dispatch(void)
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
			log_warn("warn: scheduler-api: imsg_get");
			break;
		}

		if (n) {
			rdata = imsg.data;
			rlen = imsg.hdr.len - IMSG_HEADER_SIZE;
			scheduler_msg_dispatch();
			imsg_flush(&ibuf);
			continue;
		}

		n = imsg_read(&ibuf);
		if (n == -1) {
			log_warn("warn: scheduler-api: imsg_read");
			break;
		}
		if (n == 0) {
			log_warnx("warn: scheduler-api: pipe closed");
			break;
		}
	}

	return (1);
}
