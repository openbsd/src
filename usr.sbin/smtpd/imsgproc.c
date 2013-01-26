/*	$OpenBSD: imsgproc.c,v 1.1 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <err.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "smtpd.h"

static struct tree	children;

void
imsgproc_init(void)
{
	tree_init(&children);
}

struct imsgproc *
imsgproc_fork(const char *path, const char *name, void (*cb)(struct imsg *, void *),
    void *cb_arg)
{
	struct imsgproc	*proc;
	int		 sp[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) < 0)
		return NULL;

	session_socket_blockmode(sp[0], BM_NONBLOCK);
	session_socket_blockmode(sp[1], BM_NONBLOCK);

	proc = xcalloc(1, sizeof *proc, "proc_new");
	proc->path = xstrdup(path, "proc_new:path");
	proc->name = xstrdup(name, "proc_new:name");
	proc->ibuf = xcalloc(1, sizeof *proc->ibuf, "proc_new:ibuf");
	proc->cb = cb;
	proc->cb_arg = cb_arg;

	if ((proc->pid = fork()) == -1)
		goto err;

	if (proc->pid == 0) {
		/* child process */
		dup2(sp[0], STDIN_FILENO);

		if (closefrom(STDERR_FILENO + 1) < 0)
			exit(1);

		execl(proc->path, proc->name, NULL);
		err(1, "execl");
	}

	/* parent process */
	close(sp[0]);
	imsg_init(proc->ibuf, sp[1]);

	tree_set(&children, proc->pid, proc);

	log_info("info: Started process %s (pid=%d), instance of %s",
	    proc->name, proc->pid, proc->path);
	return proc;

err:
	log_warn("warn: Failed to start process %s, instance of %s",
	    proc->name, proc->path);
	close(sp[0]);
	close(sp[1]);
	return NULL;
}

static void
imsgproc_imsg(int fd, short event, void *p)
{
	struct imsgproc	       *proc = p;
	struct imsg		imsg;
	ssize_t			n;
	short			evflags = EV_READ;

	if (event & EV_READ) {
		n = imsg_read(proc->ibuf);
		if (n == -1)
			fatal("proc_imsg: imsg_read");
		if (n == 0) {
			event_del(&proc->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&proc->ibuf->w) == -1)
			fatal("proc_imsg: msgbuf_write");
		if (proc->ibuf->w.queued)
			evflags |= EV_WRITE;
	}

	for (;;) {
		n = imsg_get(proc->ibuf, &imsg);
		if (n == -1)
			fatalx("proc_imsg: imsg_get");
		if (n == 0)
			break;

		proc->cb(&imsg, proc->cb_arg);

		imsg_free(&imsg);
	}
	event_set(&proc->ev, proc->ibuf->fd, evflags, imsgproc_imsg, proc);
	event_add(&proc->ev, NULL);
}

void
imsgproc_set_write(struct imsgproc *proc)
{
	if (proc->ibuf->w.queued) {
		event_set(&proc->ev, proc->ibuf->fd, EV_WRITE, imsgproc_imsg, proc);
		event_add(&proc->ev, NULL);
	}
}

void
imsgproc_set_read(struct imsgproc *proc)
{
	event_set(&proc->ev, proc->ibuf->fd, EV_READ, imsgproc_imsg, proc);
	event_add(&proc->ev, NULL);
}

void
imsgproc_set_read_write(struct imsgproc *proc)
{
	short	events = EV_READ;

	if (proc->ibuf->w.queued)
		events |= EV_WRITE;

	event_set(&proc->ev, proc->ibuf->fd, events, imsgproc_imsg, proc);
	event_add(&proc->ev, NULL);
}

void
imsgproc_reset_callback(struct imsgproc *proc, void (*cb)(struct imsg *, void *),
    void *cb_arg)
{
	proc->cb = cb;
	proc->cb_arg = cb_arg;
}
