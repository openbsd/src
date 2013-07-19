/*	$OpenBSD: scheduler_proc.c,v 1.1 2013/07/19 21:34:31 eric Exp $	*/

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
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static pid_t		 pid;
static struct imsgbuf	 ibuf;
static struct imsg	 imsg;
static size_t		 rlen;
static char		*rdata;

static const char *execpath = "/usr/libexec/smtpd/backend-scheduler";

static void
scheduler_proc_call(void)
{
	ssize_t	n;

	if (imsg_flush(&ibuf) == -1) {
		log_warn("warn: scheduler-proc: imsg_flush");
		fatalx("scheduler-proc: exiting");
	}

	while (1) {
		if ((n = imsg_get(&ibuf, &imsg)) == -1) {
			log_warn("warn: scheduler-proc: imsg_get");
			break;
		}
		if (n) {
			rlen = imsg.hdr.len - IMSG_HEADER_SIZE;
			rdata = imsg.data;

			if (imsg.hdr.type != PROC_SCHEDULER_OK) {
				log_warnx("warn: scheduler-proc: bad response");
				break;
			}
			return;
		}

		if ((n = imsg_read(&ibuf)) == -1) {
			log_warn("warn: scheduler-proc: imsg_read");
			break;
		}

		if (n == 0) {
			log_warnx("warn: scheduler-proc: pipe closed");
			break;
		}
	}

	fatalx("scheduler-proc: exiting");
}

static void
scheduler_proc_read(void *dst, size_t len)
{
	if (len > rlen) {
		log_warnx("warn: scheduler-proc: bad msg len");
		fatalx("scheduler-proc: exiting");
	}

	memmove(dst, rdata, len);
	rlen -= len;
	rdata += len;
}

static void
scheduler_proc_end(void)
{
	if (rlen) {
		log_warnx("warn: scheduler-proc: bogus data");
		fatalx("scheduler-proc: exiting");
	}
	imsg_free(&imsg);
}

/*
 * API
 */

static int
scheduler_proc_init(void)
{
	int		sp[2], r;
	uint32_t	version;

	errno = 0;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) < 0) {
		log_warn("warn: scheduler-proc: socketpair");
		goto err;
	}

	if ((pid = fork()) == -1) {
		log_warn("warn: scheduler-proc: fork");
		goto err;
	}

	if (pid == 0) {
		/* child process */
		dup2(sp[0], STDIN_FILENO);
		if (closefrom(STDERR_FILENO + 1) < 0)
			exit(1);

		execl(execpath, "scheduler-proc", NULL);
		err(1, "execl");
	}

	/* parent process */
	close(sp[0]);
	imsg_init(&ibuf, sp[1]);

	version = PROC_SCHEDULER_API_VERSION;
	imsg_compose(&ibuf, PROC_SCHEDULER_INIT, 0, 0, -1,
	    &version, sizeof(version));

	scheduler_proc_call();
	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);

err:
	close(sp[0]);
	close(sp[1]);
	fatalx("scheduler-proc: exiting");

	return (0);
}

static int
scheduler_proc_insert(struct scheduler_info *si)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_INSERT");

	imsg_compose(&ibuf, PROC_SCHEDULER_INSERT, 0, 0, -1, si, sizeof(*si));

	scheduler_proc_call();
	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);
}

static size_t
scheduler_proc_commit(uint32_t msgid)
{
	size_t	s;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_COMMIT");

	imsg_compose(&ibuf, PROC_SCHEDULER_COMMIT, 0, 0, -1,
	    &msgid, sizeof(msgid));

	scheduler_proc_call();
	scheduler_proc_read(&s, sizeof(s));
	scheduler_proc_end();

	return (s);
}

static size_t
scheduler_proc_rollback(uint32_t msgid)
{
	size_t	s;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_ROLLBACK");

	imsg_compose(&ibuf, PROC_SCHEDULER_ROLLBACK, 0, 0, -1,
	    &msgid, sizeof(msgid));

	scheduler_proc_call();
	scheduler_proc_read(&s, sizeof(s));
	scheduler_proc_end();

	return (s);
}

static int
scheduler_proc_update(struct scheduler_info *si)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_UPDATE");

	imsg_compose(&ibuf, PROC_SCHEDULER_UPDATE, 0, 0, -1, si, sizeof(*si));

	scheduler_proc_call();
	scheduler_proc_read(&r, sizeof(r));
	if (r == 1)
		scheduler_proc_read(si, sizeof(*si));
	scheduler_proc_end();

	return (r);
}

static int
scheduler_proc_delete(uint64_t evpid)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_DELETE");

	imsg_compose(&ibuf, PROC_SCHEDULER_DELETE, 0, 0, -1,
	    &evpid, sizeof(evpid));

	scheduler_proc_call();
	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);
}

static int
scheduler_proc_batch(int typemask, struct scheduler_batch *ret)
{
	struct ibuf	*buf;
	uint64_t	*evpids;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_BATCH");

	buf = imsg_create(&ibuf, PROC_SCHEDULER_BATCH, 0, 0,
	    sizeof(typemask) + sizeof(ret->evpcount));
	if (buf == NULL)
		return (-1);
	if (imsg_add(buf, &typemask, sizeof(typemask)) == -1)
		return (-1);
	if (imsg_add(buf, &ret->evpcount, sizeof(ret->evpcount)) == -1)
		return (-1);
	imsg_close(&ibuf, buf);

	evpids = ret->evpids;

	scheduler_proc_call();

	scheduler_proc_read(ret, sizeof(*ret));
	scheduler_proc_read(evpids, sizeof(*evpids) * ret->evpcount);
	scheduler_proc_end();

	ret->evpids = evpids;

	if (ret->type == SCHED_NONE)
		return (0);

	return (1);
}

static size_t
scheduler_proc_messages(uint32_t from, uint32_t *dst, size_t size)
{
	struct ibuf	*buf;
	size_t		 s;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_MESSAGES");

	buf = imsg_create(&ibuf, PROC_SCHEDULER_MESSAGES, 0, 0,
	    sizeof(from) + sizeof(size));
	if (buf == NULL)
		return (-1);
	if (imsg_add(buf, &from, sizeof(from)) == -1)
		return (-1);
	if (imsg_add(buf, &size, sizeof(size)) == -1)
		return (-1);
	imsg_close(&ibuf, buf);

	scheduler_proc_call();

	s = rlen / sizeof(*dst);
	scheduler_proc_read(dst, s * sizeof(*dst));
	scheduler_proc_end();

	return (s);
}

static size_t
scheduler_proc_envelopes(uint64_t from, struct evpstate *dst, size_t size)
{
	struct ibuf	*buf;
	size_t		 s;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_ENVELOPES");

	buf = imsg_create(&ibuf, PROC_SCHEDULER_ENVELOPES, 0, 0,
	    sizeof(from) + sizeof(size));
	if (buf == NULL)
		return (-1);
	if (imsg_add(buf, &from, sizeof(from)) == -1)
		return (-1);
	if (imsg_add(buf, &size, sizeof(size)) == -1)
		return (-1);
	imsg_close(&ibuf, buf);

	scheduler_proc_call();

	s = rlen / sizeof(*dst);
	scheduler_proc_read(dst, s * sizeof(*dst));
	scheduler_proc_end();

	return (s);
}

static int
scheduler_proc_schedule(uint64_t evpid)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_SCHEDULE");

	imsg_compose(&ibuf, PROC_SCHEDULER_SCHEDULE, 0, 0, -1,
	    &evpid, sizeof(evpid));

	scheduler_proc_call();

	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);
}

static int
scheduler_proc_remove(uint64_t evpid)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_REMOVE");

	imsg_compose(&ibuf, PROC_SCHEDULER_REMOVE, 0, 0, -1,
	    &evpid, sizeof(evpid));

	scheduler_proc_call();

	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);
}

static int
scheduler_proc_suspend(uint64_t evpid)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_SUSPEND");

	imsg_compose(&ibuf, PROC_SCHEDULER_SUSPEND, 0, 0, -1,
	    &evpid, sizeof(evpid));

	scheduler_proc_call();

	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);
}

static int
scheduler_proc_resume(uint64_t evpid)
{
	int	r;

	log_debug("debug: scheduler-proc: PROC_SCHEDULER_RESUME");

	imsg_compose(&ibuf, PROC_SCHEDULER_RESUME, 0, 0, -1,
	    &evpid, sizeof(evpid));

	scheduler_proc_call();

	scheduler_proc_read(&r, sizeof(r));
	scheduler_proc_end();

	return (r);
}

struct scheduler_backend scheduler_backend_proc = {
	scheduler_proc_init,
	scheduler_proc_insert,
	scheduler_proc_commit,
	scheduler_proc_rollback,
	scheduler_proc_update,
	scheduler_proc_delete,
	scheduler_proc_batch,
	scheduler_proc_messages,
	scheduler_proc_envelopes,
	scheduler_proc_schedule,
	scheduler_proc_remove,
	scheduler_proc_suspend,
	scheduler_proc_resume,
};
