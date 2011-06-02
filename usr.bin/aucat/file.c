/*	$OpenBSD: file.c,v 1.25 2011/06/02 16:58:02 ratchov Exp $	*/
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
 * 	anoncvs@moule.caoua.org:/cvs
 *
 *		midish/timo.c rev 1.16
 * 		midish/mdep.c rev 1.69
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

#include <sys/time.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "file.h"
#ifdef DEBUG
#include "dbg.h"
#endif

#define MAXFDS 100
#define TIMER_USEC 10000

struct timespec file_ts;
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
		dbg_puts("timo_add: already set\n");
		dbg_panic();
	}
	if (delta == 0) {
		dbg_puts("timo_add: zero timeout is evil\n");
		dbg_panic();
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
#ifdef DEBUG
	if (debug_level >= 4)
		dbg_puts("timo_del: not found\n");
#endif
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
#ifdef DEBUG
	if (timo_queue != NULL) {
		dbg_puts("timo_done: timo_queue not empty!\n");
		dbg_panic();
	}
#endif
	timo_queue = (struct timo *)0xdeadbeef;
}

#ifdef DEBUG
void
file_dbg(struct file *f)
{
	dbg_puts(f->ops->name);
	dbg_puts("(");
	dbg_puts(f->name);
	dbg_puts("|");
	if (f->state & FILE_ROK)
		dbg_puts("r");
	if (f->state & FILE_RINUSE)
		dbg_puts("R");
	if (f->state & FILE_WOK)
		dbg_puts("w");
	if (f->state & FILE_WINUSE)
		dbg_puts("W");
	if (f->state & FILE_EOF)
		dbg_puts("e");
	if (f->state & FILE_HUP)
		dbg_puts("h");
	if (f->state & FILE_ZOMB)
		dbg_puts("Z");
	dbg_puts(")");
}
#endif

struct file *
file_new(struct fileops *ops, char *name, unsigned nfds)
{
	struct file *f;

	LIST_FOREACH(f, &file_list, entry)
		nfds += f->ops->nfds(f);
	if (nfds > MAXFDS) {
#ifdef DEBUG
		if (debug_level >= 1) {
			dbg_puts(name);
			dbg_puts(": too many polled files\n");
		}
#endif
		return NULL;
	}
	f = malloc(ops->size);
	if (f == NULL)
		err(1, "file_new: %s", ops->name);
	f->ops = ops;
	f->name = name;
	f->state = 0;
#ifdef DEBUG
	f->cycles = 0;
#endif
	f->rproc = NULL;
	f->wproc = NULL;
	LIST_INSERT_HEAD(&file_list, f, entry);
#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(f);
		dbg_puts(": created\n");
	}
#endif
	return f;
}

void
file_del(struct file *f)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(f);
		dbg_puts(": terminating...\n");
	}
#endif
	if (f->state & (FILE_RINUSE | FILE_WINUSE)) {
		f->state |= FILE_ZOMB;
	} else {
		LIST_REMOVE(f, entry);
#ifdef DEBUG
		if (debug_level >= 3) {
			file_dbg(f);
			dbg_puts(": destroyed\n");
		}
#endif
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
	struct timespec ts;
	long delta_nsec;

	if (LIST_EMPTY(&file_list) && timo_queue == NULL) {
#ifdef DEBUG
		if (debug_level >= 3)
			dbg_puts("nothing to do...\n");
#endif
		return 0;
	}
	/*
	 * Fill the pfds[] array with files that are blocked on reading
	 * and/or writing, skipping those that are just waiting.
	 */
#ifdef DEBUG
	dbg_flush();
	if (debug_level >= 4) 
		dbg_puts("poll:");
#endif
	nfds = 0;
	LIST_FOREACH(f, &file_list, entry) {
		events = 0;
		if (f->rproc && !(f->state & FILE_ROK))
			events |= POLLIN;
		if (f->wproc && !(f->state & FILE_WOK))
			events |= POLLOUT;
#ifdef DEBUG
		if (debug_level >= 4) {
			dbg_puts(" ");
			file_dbg(f);
		}
#endif
		n = f->ops->pollfd(f, pfds + nfds, events);
		if (n == 0) {
			f->pfd = NULL;
			continue;
		}
		f->pfd = pfds + nfds;
		nfds += n;
	}
#ifdef DEBUG
	if (debug_level >= 4) {
		dbg_puts("\npfds[] =");
		for (n = 0; n < nfds; n++) {
			dbg_puts(" ");
			dbg_putx(pfds[n].events);
		}
		dbg_puts("\n");
	}
#endif
	if (nfds > 0) {
		if (poll(pfds, nfds, -1) < 0) {
			if (errno == EINTR)
				return 1;
			err(1, "file_poll: poll failed");
		}
		clock_gettime(CLOCK_MONOTONIC, &ts);
		delta_nsec = 1000000000L * (ts.tv_sec - file_ts.tv_sec);
		delta_nsec += ts.tv_nsec - file_ts.tv_nsec;
		if (delta_nsec > 0) {
			file_ts = ts;
			timo_update(delta_nsec / 1000);
		}
	}
	f = LIST_FIRST(&file_list);
	while (f != NULL) {
		if (f->pfd == NULL) {
			f = LIST_NEXT(f, entry);
			continue;
		}
		revents = f->ops->revents(f, f->pfd);
#ifdef DEBUG
		if (revents) {
			f->cycles++;
			if (f->cycles > FILE_MAXCYCLES) {
				file_dbg(f);
				dbg_puts(": busy loop, disconnecting\n");
				revents = POLLHUP;
			}
		}
#endif
		if (!(f->state & FILE_ZOMB) && (revents & POLLIN)) {
			revents &= ~POLLIN;
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(f);
				dbg_puts(": rok\n");
			}
#endif
			f->state |= FILE_ROK;
			f->state |= FILE_RINUSE;
			for (;;) {
				p = f->rproc;
				if (!p)
					break;
#ifdef DEBUG
				if (debug_level >= 4) {
					aproc_dbg(p);
					dbg_puts(": in\n");
				}
#endif
				if (!p->ops->in(p, NULL))
					break;
			}
			f->state &= ~FILE_RINUSE;
		}
		if (!(f->state & FILE_ZOMB) && (revents & POLLOUT)) {
			revents &= ~POLLOUT;
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(f);
				dbg_puts(": wok\n");
			}
#endif
			f->state |= FILE_WOK;
			f->state |= FILE_WINUSE;
			for (;;) {
				p = f->wproc;
				if (!p)
					break;
#ifdef DEBUG
				if (debug_level >= 4) {
					aproc_dbg(p);
					dbg_puts(": out\n");
				}
#endif
				if (!p->ops->out(p, NULL))
					break;
			}
			f->state &= ~FILE_WINUSE;
		}
		if (!(f->state & FILE_ZOMB) && (revents & POLLHUP)) {
#ifdef DEBUG
			if (debug_level >= 3) {
				file_dbg(f);
				dbg_puts(": disconnected\n");
			}
#endif
			f->state |= (FILE_EOF | FILE_HUP);
		}
		if (!(f->state & FILE_ZOMB) && (f->state & FILE_EOF)) {
#ifdef DEBUG
			if (debug_level >= 3) {
				file_dbg(f);
				dbg_puts(": eof\n");
			}
#endif
			p = f->rproc;
			if (p) {
				f->state |= FILE_RINUSE;
#ifdef DEBUG
				if (debug_level >= 3) {
					aproc_dbg(p);
					dbg_puts(": eof\n");
				}
#endif
				p->ops->eof(p, NULL);
				f->state &= ~FILE_RINUSE;
			}
			f->state &= ~FILE_EOF;
		}
		if (!(f->state & FILE_ZOMB) && (f->state & FILE_HUP)) {
#ifdef DEBUG
			if (debug_level >= 3) {
				file_dbg(f);
				dbg_puts(": hup\n");
			}
#endif
			p = f->wproc;
			if (p) {
				f->state |= FILE_WINUSE;
#ifdef DEBUG
				if (debug_level >= 3) {
					aproc_dbg(p);
					dbg_puts(": hup\n");
				}
#endif
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
	if (LIST_EMPTY(&file_list) && timo_queue == NULL) {
#ifdef DEBUG
		if (debug_level >= 3)
			dbg_puts("no files anymore...\n");
#endif
		return 0;
	}
	return 1;
}

/*
 * handler for SIGALRM, invoked periodically
 */
void
file_sigalrm(int i)
{
	/* nothing to do, we only want poll() to return EINTR */
}


void
filelist_init(void)
{
	static struct sigaction sa;
	struct itimerval it;
	sigset_t set;

	sigemptyset(&set);
	(void)sigaddset(&set, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &set, NULL))
		err(1, "sigprocmask");
	LIST_INIT(&file_list);
	if (clock_gettime(CLOCK_MONOTONIC, &file_ts) < 0) {
		perror("clock_gettime");
		exit(1);
	}
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = file_sigalrm;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = TIMER_USEC;
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = TIMER_USEC;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
		perror("setitimer");
		exit(1);
	}
	timo_init();
#ifdef DEBUG
	dbg_sync = 0;
#endif
}

void
filelist_done(void)
{
	struct itimerval it;
#ifdef DEBUG
	struct file *f;

	if (!LIST_EMPTY(&file_list)) {
		LIST_FOREACH(f, &file_list, entry) {
			file_dbg(f);
			dbg_puts(" not closed\n");
		}
		dbg_panic();
	}
	dbg_sync = 1;
	dbg_flush();
#endif
	timerclear(&it.it_value);
	timerclear(&it.it_interval);
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
		perror("setitimer");
		exit(1);
	}
	timo_done();
}

unsigned
file_read(struct file *f, unsigned char *data, unsigned count)
{
	unsigned n;
#ifdef DEBUG
	struct timespec ts0, ts1;
	long us;

	if (!(f->state & FILE_ROK)) {
		file_dbg(f);
		dbg_puts(": read: bad state\n");
		dbg_panic();
	}
	clock_gettime(CLOCK_MONOTONIC, &ts0);
#endif
	n = f->ops->read(f, data, count);
#ifdef DEBUG
	if (n > 0)
		f->cycles = 0;
	clock_gettime(CLOCK_MONOTONIC, &ts1);
	us = 1000000L * (ts1.tv_sec - ts0.tv_sec);
	us += (ts1.tv_nsec - ts0.tv_nsec) / 1000;
	if (debug_level >= 4 || (debug_level >= 2 && us >= 5000)) {
		dbg_puts(f->name);
		dbg_puts(": read ");
		dbg_putu(n);
		dbg_puts(" bytes in ");
		dbg_putu(us);
		dbg_puts("us\n");
	}
#endif
	return n;
}

unsigned
file_write(struct file *f, unsigned char *data, unsigned count)
{
	unsigned n;
#ifdef DEBUG
	struct timespec ts0, ts1;
	long us;

	if (!(f->state & FILE_WOK)) {
		file_dbg(f);
		dbg_puts(": write: bad state\n");
		dbg_panic();
	}
	clock_gettime(CLOCK_MONOTONIC, &ts0);
#endif
	n = f->ops->write(f, data, count);
#ifdef DEBUG
	if (n > 0)
		f->cycles = 0;
	clock_gettime(CLOCK_MONOTONIC, &ts1);
	us = 1000000L * (ts1.tv_sec - ts0.tv_sec);
	us += (ts1.tv_nsec - ts0.tv_nsec) / 1000;
	if (debug_level >= 4 || (debug_level >= 2 && us >= 5000)) {
		dbg_puts(f->name);
		dbg_puts(": wrote ");
		dbg_putu(n);
		dbg_puts(" bytes in ");
		dbg_putu(us);
		dbg_puts("us\n");
	}
#endif
	return n;
}

void
file_eof(struct file *f)
{
	struct aproc *p;

#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(f);
		dbg_puts(": eof requested\n");
	}
#endif
	if (!(f->state & (FILE_RINUSE | FILE_WINUSE))) {
		p = f->rproc;
		if (p) {
			f->state |= FILE_RINUSE;
#ifdef DEBUG
			if (debug_level >= 3) {
				aproc_dbg(p);
				dbg_puts(": eof\n");
			}
#endif
			p->ops->eof(p, NULL);
			f->state &= ~FILE_RINUSE;
		}
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		f->state &= ~FILE_ROK;
		f->state |= FILE_EOF;
	}
}

void
file_hup(struct file *f)
{
	struct aproc *p;

#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(f);
		dbg_puts(": hup requested\n");
	}
#endif
	if (!(f->state & (FILE_RINUSE | FILE_WINUSE))) {
		p = f->wproc;
		if (p) {
			f->state |= FILE_WINUSE;
#ifdef DEBUG
			if (debug_level >= 3) {
				aproc_dbg(p);
				dbg_puts(": hup\n");
			}
#endif
			p->ops->hup(p, NULL);
			f->state &= ~FILE_WINUSE;
		}
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		f->state &= ~FILE_WOK;
		f->state |= FILE_HUP;
	}
}

void
file_close(struct file *f)
{
	struct aproc *p;

#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(f);
		dbg_puts(": closing\n");
	}
#endif
	if (f->wproc == NULL && f->rproc == NULL)
		f->state |= FILE_ZOMB;
	if (!(f->state & (FILE_RINUSE | FILE_WINUSE))) {
		p = f->rproc;
		if (p) {
			f->state |= FILE_RINUSE;
#ifdef DEBUG
			if (debug_level >= 3) {
				aproc_dbg(p);
				dbg_puts(": eof\n");
			}
#endif
			p->ops->eof(p, NULL);
			f->state &= ~FILE_RINUSE;
		}
		p = f->wproc;
		if (p) {
			f->state |= FILE_WINUSE;
#ifdef DEBUG
			if (debug_level >= 3) {
				aproc_dbg(p);
				dbg_puts(": hup\n");
			}
#endif
			p->ops->hup(p, NULL);
			f->state &= ~FILE_WINUSE;
		}
		if (f->state & FILE_ZOMB)
			file_del(f);
	} else {
		f->state &= ~(FILE_ROK | FILE_WOK);
		f->state |= (FILE_EOF | FILE_HUP);
	}
}
