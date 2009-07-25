/*	$OpenBSD: file.c,v 1.13 2009/07/25 10:52:19 ratchov Exp $	*/
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
 * the module also provides trivial timeout implementation,
 * derived from:
 *
 * 	anoncvs@moule.caoua.org:/cvs/midish/timo.c rev 1.16
 *
 * A timeout is used to schedule the call of a routine (the callback)
 * there is a global list of timeouts that is processed inside the
 * event loop. Timeouts work as follows:
 *
 *	first the timo structure must be initialized with timo_set()
 *
 *	then the timeout is scheduled (only once) with timo_add()
 *
 *	if the timeout expires, the call-back is called; then it can
 *	be scheduled again if needed. It's OK to reschedule it again
 *	from the callback
 *
 *	the timeout can be aborted with timo_del(), it is OK to try to
 *	abort a timout that has expired
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

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "file.h"

#define MAXFDS 100

extern struct fileops listen_ops, pipe_ops;

struct timeval file_tv;
struct filelist file_list;
struct timo *timo_queue;
unsigned timo_abstime;

/*
 * initialise a timeout structure, arguments are callback and argument
 * that will be passed to the callback
 */
void
timo_set(struct timo *o, void (*cb)(void *), void *arg)
{
	o->cb = cb;
	o->arg = arg;
	o->set = 0;
}

/*
 * schedule the callback in 'delta' 24-th of microseconds. The timeout
 * must not be already scheduled
 */
void
timo_add(struct timo *o, unsigned delta)
{
	struct timo **i;
	unsigned val;
	int diff;

#ifdef DEBUG
	if (o->set) {
		fprintf(stderr, "timo_set: already set\n");
		abort();
	}
	if (delta == 0) {
		fprintf(stderr, "timo_set: zero timeout is evil\n");
		abort();
	}
#endif
	val = timo_abstime + delta;
	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		diff = (*i)->val - val;
		if (diff > 0) {
			break;
		}
	}
	o->set = 1;
	o->val = val;
	o->next = *i;
	*i = o;
}

/*
 * abort a scheduled timeout
 */
void
timo_del(struct timo *o)
{
	struct timo **i;

	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		if (*i == o) {
			*i = o->next;
			o->set = 0;
			return;
		}
	}
	DPRINTF("timo_del: not found\n");
}

/*
 * routine to be called by the timer when 'delta' 24-th of microsecond
 * elapsed. This routine updates time referece used by timeouts and
 * calls expired timeouts
 */
void
timo_update(unsigned delta)
{
	struct timo *to;
	int diff;

	/*
	 * update time reference
	 */
	timo_abstime += delta;

	/*
	 * remove from the queue and run expired timeouts
	 */
	while (timo_queue != NULL) {
		/*
		 * there is no overflow here because + and - are
		 * modulo 2^32, they are the same for both signed and
		 * unsigned integers
		 */
		diff = timo_queue->val - timo_abstime;
		if (diff > 0)
			break;
		to = timo_queue;
		timo_queue = to->next;
		to->set = 0;
		to->cb(to->arg);
	}
}

/*
 * initialize timeout queue
 */
void
timo_init(void)
{
	timo_queue = NULL;
	timo_abstime = 0;
}

/*
 * destroy timeout queue
 */
void
timo_done(void)
{
	if (timo_queue != NULL) {
		fprintf(stderr, "timo_done: timo_queue not empty!\n");
		abort();
	}
	timo_queue = (struct timo *)0xdeadbeef;
}

void
file_dprint(int n, struct file *f)
{
#ifdef DEBUG
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
#endif
}

struct file *
file_new(struct fileops *ops, char *name, unsigned nfds)
{
	struct file *f;

	LIST_FOREACH(f, &file_list, entry)
		nfds += f->ops->nfds(f);
	if (nfds > MAXFDS) {
		DPRINTF("file_new: %s: too many polled files\n", name);
		return NULL;
	}
	f = malloc(ops->size);
	if (f == NULL)
		err(1, "file_new: %s", ops->name);
	f->ops = ops;
	f->name = name;
	f->state = 0;
	f->rproc = NULL;
	f->wproc = NULL;
	LIST_INSERT_HEAD(&file_list, f, entry);
	DPRINTF("file_new: %s:%s\n", ops->name, f->name);
	return f;
}

void
file_del(struct file *f)
{
	DPRINTF("file_del: ");
	file_dprint(1, f);
	if (f->state & (FILE_RINUSE | FILE_WINUSE)) {
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
	struct timeval tv;
	long delta_usec;
	int timo;

	/*
	 * Fill the pfds[] array with files that are blocked on reading
	 * and/or writing, skipping those that are just waiting.
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
	if (debug_level >= 4) {
		DPRINTF("file_poll: pfds[] =");
		for (n = 0; n < nfds; n++)
			DPRINTF(" %x", pfds[n].events);
		DPRINTF("\n");
	}
	if (LIST_EMPTY(&file_list)) {
		DPRINTF("file_poll: nothing to do...\n");
		return 0;
	}
	if (nfds > 0) {
		if (timo_queue) {
			timo = (timo_queue->val - timo_abstime) / (2 * 1000);
			if (timo == 0)
				timo = 1;
		} else
			timo = -1;
		if (poll(pfds, nfds, timo) < 0) {
			if (errno == EINTR)
				return 1;
			err(1, "file_poll: poll failed");
		}
		gettimeofday(&tv, NULL);
		delta_usec = 1000000L * (tv.tv_sec - file_tv.tv_sec);
		delta_usec += tv.tv_usec - file_tv.tv_usec;
		if (delta_usec > 0) {
			file_tv = tv;
			timo_update(delta_usec);
		}
	}
	f = LIST_FIRST(&file_list);
	while (f != LIST_END(&file_list)) {
		if (f->pfd == NULL) {
			f = LIST_NEXT(f, entry);
			continue;
		}
		revents = f->ops->revents(f, f->pfd);
		if (!(f->state & FILE_ZOMB) && (revents & POLLIN)) {
			revents &= ~POLLIN;
			f->state |= FILE_ROK;
			DPRINTFN(3, "file_poll: %s rok\n", f->name);
			f->state |= FILE_RINUSE;
			for (;;) {
				p = f->rproc;
				if (!p || !p->ops->in(p, NULL))
					break;
			}
			f->state &= ~FILE_RINUSE;
		}
		if (!(f->state & FILE_ZOMB) && (revents & POLLOUT)) {
			revents &= ~POLLOUT;
			f->state |= FILE_WOK;
			DPRINTFN(3, "file_poll: %s wok\n", f->name);
			f->state |= FILE_WINUSE;
			for (;;) {
				p = f->wproc;
				if (!p || !p->ops->out(p, NULL))
					break;
			}
			f->state &= ~FILE_WINUSE;
		}
		if (!(f->state & FILE_ZOMB) && (revents & POLLHUP)) {
			DPRINTFN(2, "file_poll: %s: disconnected\n", f->name);
			f->state |= (FILE_EOF | FILE_HUP);
		}
		if (!(f->state & FILE_ZOMB) && (f->state & FILE_EOF)) {
			DPRINTFN(2, "file_poll: %s: eof\n", f->name);
			p = f->rproc;
			if (p) {
				f->state |= FILE_RINUSE;
				p->ops->eof(p, NULL);
				f->state &= ~FILE_RINUSE;
			}
			f->state &= ~FILE_EOF;
		}
		if (!(f->state & FILE_ZOMB) && (f->state & FILE_HUP)) {
			DPRINTFN(2, "file_poll: %s hup\n", f->name);
			p = f->wproc;
			if (p) {
				f->state |= FILE_WINUSE;
				p->ops->hup(p, NULL);
				f->state &= ~FILE_WINUSE;
			}
			f->state &= ~FILE_HUP;
		}
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
	timo_init();
	gettimeofday(&file_tv, NULL);
}

void
filelist_done(void)
{
	struct file *f;

	timo_done();
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
 * Close all listening sockets.
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

	if (!(f->state & (FILE_RINUSE | FILE_WINUSE))) {
		DPRINTFN(2, "file_eof: %s: immediate\n", f->name);
		p = f->rproc;
		if (p) {
			f->state |= FILE_RINUSE;
			p->ops->eof(p, NULL);
			f->state &= ~FILE_RINUSE;
		}
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

	if (!(f->state & (FILE_RINUSE | FILE_WINUSE))) {
		DPRINTFN(2, "file_hup: %s immediate\n", f->name);
		p = f->wproc;
		if (p) {
			f->state |= FILE_WINUSE;
			p->ops->hup(p, NULL);
			f->state &= ~FILE_WINUSE;
		}
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		DPRINTFN(2, "file_hup: %s: delayed\n", f->name);
		f->state &= ~FILE_WOK;
		f->state |= FILE_HUP;
	}
}

void
file_close(struct file *f)
{
	struct aproc *p;

	if (!(f->state & (FILE_RINUSE | FILE_WINUSE))) {
		DPRINTFN(2, "file_close: %s: immediate\n", f->name);
		p = f->rproc;
		if (p) {
			f->state |= FILE_RINUSE;
			p->ops->eof(p, NULL);
			f->state &= ~FILE_RINUSE;
		}
		p = f->wproc;
		if (p) {
			f->state |= FILE_WINUSE;
			p->ops->hup(p, NULL);
			f->state &= ~FILE_WINUSE;
		}
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		DPRINTFN(2, "file_close: %s: delayed\n", f->name);
		f->state &= ~(FILE_ROK | FILE_WOK);
		f->state |= (FILE_EOF | FILE_HUP);
	}
}
