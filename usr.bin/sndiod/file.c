/*	$OpenBSD: file.c,v 1.8 2015/07/17 09:37:07 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
 * in an event loop and dispatch events to sub-modules.
 *
 * the module also provides trivial timeout implementation,
 * derived from:
 *
 * 	anoncvs@moule.caoua.org:/midish
 *
 *		midish/timo.c rev 1.18
 * 		midish/mdep.c rev 1.71
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
#include <time.h>

#include "file.h"
#include "utils.h"

#define MAXFDS 100
#define TIMER_MSEC 5

void timo_update(unsigned int);
void timo_init(void);
void timo_done(void);

struct timespec file_ts;
struct file *file_list;
struct timo *timo_queue;
unsigned int timo_abstime;
int file_slowaccept = 0, file_nfds;
#ifdef DEBUG
long long file_wtime, file_utime;
#endif

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
timo_add(struct timo *o, unsigned int delta)
{
	struct timo **i;
	unsigned int val;
	int diff;

#ifdef DEBUG
	if (o->set) {
		log_puts("timo_add: already set\n");
		panic();
	}
	if (delta == 0) {
		log_puts("timo_add: zero timeout is evil\n");
		panic();
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
	if (log_level >= 4)
		log_puts("timo_del: not found\n");
#endif
}

/*
 * routine to be called by the timer when 'delta' 24-th of microsecond
 * elapsed. This routine updates time referece used by timeouts and
 * calls expired timeouts
 */
void
timo_update(unsigned int delta)
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
		log_puts("timo_done: timo_queue not empty!\n");
		panic();
	}
#endif
	timo_queue = (struct timo *)0xdeadbeef;
}

#ifdef DEBUG
void
file_log(struct file *f)
{
	static char *states[] = { "ini", "zom" };

	log_puts(f->ops->name);
	if (log_level >= 3) {
		log_puts("(");
		log_puts(f->name);
		log_puts("|");
		log_puts(states[f->state]);
		log_puts(")");
	}
}
#endif

struct file *
file_new(struct fileops *ops, void *arg, char *name, unsigned int nfds)
{
	struct file *f;

	if (file_nfds + nfds > MAXFDS) {
#ifdef DEBUG
		if (log_level >= 1) {
			log_puts(name);
			log_puts(": too many polled files\n");
		}
#endif
		return NULL;
	}
	f = xmalloc(sizeof(struct file));
	f->nfds = nfds;
	f->ops = ops;
	f->arg = arg;
	f->name = name;
	f->state = FILE_INIT;
	f->next = file_list;
	file_list = f;
#ifdef DEBUG
	if (log_level >= 3) {
		file_log(f);
		log_puts(": created\n");
	}
#endif
	file_nfds += f->nfds;
	return f;
}

void
file_del(struct file *f)
{
#ifdef DEBUG
	if (f->state == FILE_ZOMB) {
		log_puts("bad state in file_del()\n");
		panic();
	}
#endif	
	file_nfds -= f->nfds;
	f->state = FILE_ZOMB;
#ifdef DEBUG
	if (log_level >= 3) {
		file_log(f);
		log_puts(": destroyed\n");
	}
#endif
}

int
file_poll(void)
{
	struct pollfd pfds[MAXFDS];
	struct file *f, **pf;
	struct timespec ts;
#ifdef DEBUG
	struct timespec sleepts;
	struct timespec ts0, ts1;
	long us;
	int i, n, nfds;
#endif
	long long delta_nsec;
	int revents, res, immed;

	/*
	 * cleanup zombies
	 */
	pf = &file_list;
	while ((f = *pf) != NULL) {
		if (f->state == FILE_ZOMB) {
			*pf = f->next;
			xfree(f);
		} else
			pf = &f->next;
	}

	if (file_list == NULL && timo_queue == NULL) {
#ifdef DEBUG
		if (log_level >= 3)
			log_puts("nothing to do...\n");
#endif
		return 0;
	}

	log_flush();
	nfds = 0;
	immed = 0;
	for (f = file_list; f != NULL; f = f->next) {
		n = f->ops->pollfd(f->arg, pfds + nfds);
		if (n == 0) {
			f->pfd = NULL;
			continue;
		}
		if (n < 0) {
			immed = 1;
			n = 0;
		}
		f->pfd = pfds + nfds;
		nfds += n;
	}
#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("poll:");
		for (i = 0; i < nfds; i++) {
			log_puts(" ");
			for (f = file_list; f != NULL; f = f->next) {
				if (f->pfd == &pfds[i]) {
					log_puts(f->ops->name);
					log_puts(": ");
				}
			}
			log_putx(pfds[i].events);
		}
		log_puts("\n");
	}
	clock_gettime(CLOCK_MONOTONIC, &sleepts);
	file_utime += 1000000000LL * (sleepts.tv_sec - file_ts.tv_sec);
	file_utime += sleepts.tv_nsec - file_ts.tv_nsec;
#endif
	if (!immed) {
		res = poll(pfds, nfds, TIMER_MSEC);
		if (res < 0 && errno != EINTR)
			err(1, "poll");
	} else
		res = 0;
	clock_gettime(CLOCK_MONOTONIC, &ts);
#ifdef DEBUG
	file_wtime += 1000000000LL * (ts.tv_sec - sleepts.tv_sec);
	file_wtime += ts.tv_nsec - sleepts.tv_nsec;
#endif
	delta_nsec = 1000000000LL * (ts.tv_sec - file_ts.tv_sec);
	delta_nsec += ts.tv_nsec - file_ts.tv_nsec;
#ifdef DEBUG
	if (delta_nsec < 0)
		log_puts("file_poll: negative time interval\n");
#endif
	file_ts = ts;
	if (delta_nsec >= 0 && delta_nsec < 1000000000LL)
		timo_update(delta_nsec / 1000);
	else {
		if (log_level >= 2)
			log_puts("ignored huge clock delta\n");
	}
	if (!immed && res <= 0)
		return 1;

	for (f = file_list; f != NULL; f = f->next) {
		if (f->pfd == NULL)
			continue;
#ifdef DEBUG
		if (log_level >= 3)
			clock_gettime(CLOCK_MONOTONIC, &ts0);
#endif
		revents = (f->state != FILE_ZOMB) ? 
		    f->ops->revents(f->arg, f->pfd) : 0;
		if ((revents & POLLHUP) && (f->state != FILE_ZOMB))
			f->ops->hup(f->arg);
		if ((revents & POLLIN) && (f->state != FILE_ZOMB))
			f->ops->in(f->arg);
		if ((revents & POLLOUT) && (f->state != FILE_ZOMB))
			f->ops->out(f->arg);
#ifdef DEBUG
		if (log_level >= 3) {
			clock_gettime(CLOCK_MONOTONIC, &ts1);
			us = 1000000L * (ts1.tv_sec - ts0.tv_sec);
			us += (ts1.tv_nsec - ts0.tv_nsec) / 1000;
			if (log_level >= 4 || us >= 5000) {
				file_log(f);
				log_puts(": processed in ");
				log_putu(us);
				log_puts("us\n");
			}
		}
#endif
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
	file_list = NULL;
	if (clock_gettime(CLOCK_MONOTONIC, &file_ts) < 0) {
		perror("clock_gettime");
		exit(1);
	}
	log_sync = 0;
	timo_init();
}

void
filelist_done(void)
{
#ifdef DEBUG
	struct file *f;

	if (file_list != NULL) {
		for (f = file_list; f != NULL; f = f->next) {
			file_log(f);
			log_puts(" not closed\n");
		}
		panic();
	}
	log_sync = 1;
	log_flush();
#endif
	timo_done();
}
