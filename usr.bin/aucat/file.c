/*	$OpenBSD: file.c,v 1.4 2008/10/26 08:49:44 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
/*
 * non-blocking file i/o module: each file can be read or written (or
 * both). To achieve non-blocking io, we simply use the poll() syscall
 * in an event loop. If a read() or write() syscall return EAGAIN
 * (operation will block), then the file is marked as "for polling", else
 * the file is not polled again.
 *
 */
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "conf.h"
#include "file.h"
#include "aproc.h"
#include "abuf.h"

#define MAXFDS 100

extern struct fileops listen_ops, pipe_ops;
struct filelist file_list;

void
file_dprint(int n, struct file *f)
{
	if (debug_level < n)
		return;
	fprintf(stderr, "%s:%s <", f->ops->name, f->name);
	if (f->state & FILE_ROK)
		fprintf(stderr, "ROK");
	if (f->state & FILE_WOK)
		fprintf(stderr, "WOK");
	if (f->state & FILE_EOF)
		fprintf(stderr, "EOF");
	if (f->state & FILE_HUP)
		fprintf(stderr, "HUP");
	fprintf(stderr, ">");
}

struct file *
file_new(struct fileops *ops, char *name, unsigned nfds)
{
	struct file *f;

	LIST_FOREACH(f, &file_list, entry)
		nfds += f->ops->nfds(f);
	if (nfds > MAXFDS)
		err(1, "%s: too many polled files", name);

	f = malloc(ops->size);
	if (f == NULL)
		err(1, "file_new: %s", ops->name);
	f->ops = ops;
	f->name = name;
	f->state = 0;
	f->rproc = NULL;
	f->wproc = NULL;
	f->refs = 0;
	LIST_INSERT_HEAD(&file_list, f, entry);
	DPRINTF("file_new: %s:%s\n", ops->name, f->name);
	return f;
}

void
file_del(struct file *f)
{
	DPRINTF("file_del: ");
	file_dprint(1, f);
	if (f->refs > 0) {
		DPRINTF(": delayed\n");
		f->state |= FILE_ZOMB;
		return;
	} else {
		DPRINTF(": immediate\n");
		LIST_REMOVE(f, entry);
		f->ops->close(f);
		free(f);
	}
}

int
file_poll(void)
{
	nfds_t nfds, n;
	short events, revents;
	struct pollfd pfds[MAXFDS];
	struct file *f, *fnext;
	struct aproc *p;

	/*
	 * fill the pfds[] array with files that are blocked on reading
	 * and/or writing, skipping those that're just waiting
	 */
	DPRINTFN(4, "file_poll:");
	nfds = 0;
	LIST_FOREACH(f, &file_list, entry) {
		events = 0;
		if (f->rproc && !(f->state & FILE_ROK))
			events |= POLLIN;
		if (f->wproc && !(f->state & FILE_WOK))
			events |= POLLOUT;
		DPRINTFN(4, " %s(%x)", f->name, events);
		n = f->ops->pollfd(f, pfds + nfds, events);
		if (n == 0) {
			f->pfd = NULL;
			continue;
		}
		f->pfd = pfds + nfds;
		nfds += n;
	}
	DPRINTFN(4, "\n");

#ifdef DEBUG
	if (nfds == 0 && !LIST_EMPTY(&file_list)) {
		fprintf(stderr, "file_poll: deadlock\n");
		abort();
	}
#endif
	if (LIST_EMPTY(&file_list)) {
		DPRINTF("file_poll: nothing to do...\n");
		return 0;
	}
	if (poll(pfds, nfds, -1) < 0) {
		if (errno == EINTR)
			return 1;
		err(1, "file_poll: poll failed");
	}
	f = LIST_FIRST(&file_list);
	while (f != LIST_END(&file_list)) {
		if (f->pfd == NULL) {
			f = LIST_NEXT(f, entry);
			continue;
		}
		f->refs++;
		revents = f->ops->revents(f, f->pfd);
		if (!(f->state & FILE_ZOMB) && (revents & POLLIN)) {
			revents &= ~POLLIN;
			f->state |= FILE_ROK;
			DPRINTFN(3, "file_poll: %s rok\n", f->name);
			for (;;) {
				p = f->rproc;
				if (!p || !p->ops->in(p, NULL))
					break;
			}
		}
		if (!(f->state & FILE_ZOMB) && (revents & POLLOUT)) {
			revents &= ~POLLOUT;
			f->state |= FILE_WOK;
			DPRINTFN(3, "file_poll: %s wok\n", f->name);
			for (;;) {
				p = f->wproc;
				if (!p || !p->ops->out(p, NULL))
					break;
			}
		}
		if (!(f->state & FILE_ZOMB) && (f->state & FILE_EOF)) {
			DPRINTFN(2, "file_poll: %s: eof\n", f->name);
			p = f->rproc;
			if (p)
				p->ops->eof(p, NULL);
			f->state &= ~FILE_EOF;
		}
		if (!(f->state & FILE_ZOMB) && (f->state & FILE_HUP)) {
			DPRINTFN(2, "file_poll: %s hup\n", f->name);
			p = f->wproc;
			if (p)
				p->ops->hup(p, NULL);
			f->state &= ~FILE_HUP;
		}
		f->refs--;
		fnext = LIST_NEXT(f, entry);
		if (f->state & FILE_ZOMB)
			file_del(f);
		f = fnext;
	}
	if (LIST_EMPTY(&file_list)) {
		DPRINTFN(2, "file_poll: terminated\n");
		return 0;
	}
	return 1;
}

void
filelist_init(void)
{
	sigset_t set;

	sigemptyset(&set);
	(void)sigaddset(&set, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &set, NULL))
		err(1, "sigprocmask");

	LIST_INIT(&file_list);
}

void
filelist_done(void)
{
	struct file *f;

	if (!LIST_EMPTY(&file_list)) {
		fprintf(stderr, "filelist_done: list not empty:\n");
		LIST_FOREACH(f, &file_list, entry) {
			fprintf(stderr, "\t");
			file_dprint(0, f);
			fprintf(stderr, "\n");
		}
		abort();
	}
}

/*
 * close all listening sockets
 *
 * XXX: remove this
 */
void
filelist_unlisten(void)
{
	struct file *f, *fnext;
	
	for (f = LIST_FIRST(&file_list); f != NULL; f = fnext) {
		fnext = LIST_NEXT(f, entry);
		if (f->ops == &listen_ops)
			file_del(f);
	}
}

unsigned
file_read(struct file *file, unsigned char *data, unsigned count)
{
	return file->ops->read(file, data, count);
}

unsigned
file_write(struct file *file, unsigned char *data, unsigned count)
{
	return file->ops->write(file, data, count);
}

void
file_eof(struct file *f)
{
	struct aproc *p;

	if (f->refs == 0) {
		DPRINTFN(2, "file_eof: %s: immediate\n", f->name);
		f->refs++;
		p = f->rproc;
		if (p)
			p->ops->eof(p, NULL);
		f->refs--;
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		DPRINTFN(2, "file_eof: %s: delayed\n", f->name);
		f->state &= ~FILE_ROK;
		f->state |= FILE_EOF;
	}
}

void
file_hup(struct file *f)
{
	struct aproc *p;

	if (f->refs == 0) {
		DPRINTFN(2, "file_hup: %s immediate\n", f->name);
		f->refs++;
		p = f->wproc;
		if (p)
			p->ops->hup(p, NULL);
		f->refs--;
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		DPRINTFN(2, "file_hup: %s: delayed\n", f->name);
		f->state &= ~FILE_WOK;
		f->state |= FILE_HUP;
	}
}
