/*	$OpenBSD: completion.h,v 1.3 2019/12/30 09:30:31 mpi Exp $	*/
/*
 * Copyright (c) 2015, 2018 Mark Kettenis
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

#ifndef _LINUX_COMPLETION_H
#define _LINUX_COMPLETION_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <linux/wait.h>

struct completion {
	u_int done;
	wait_queue_head_t wait;
};

static inline void
init_completion(struct completion *x)
{
	x->done = 0;
	mtx_init(&x->wait.lock, IPL_TTY);
}

static inline u_long
wait_for_completion_timeout(struct completion *x, u_long timo)
{
	int ret;

	KASSERT(!cold);

	mtx_enter(&x->wait.lock);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, 0, "wfct", timo);
		if (ret) {
			mtx_leave(&x->wait.lock);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}
	x->done--;
	mtx_leave(&x->wait.lock);

	return 1;
}

static inline u_long
wait_for_completion_interruptible(struct completion *x)
{
	int ret;

	KASSERT(!cold);

	mtx_enter(&x->wait.lock);
	while (x->done == 0) {
		ret = msleep_nsec(x, &x->wait.lock, PCATCH, "wfci", INFSLP);
		if (ret) {
			mtx_leave(&x->wait.lock);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}
	x->done--;
	mtx_leave(&x->wait.lock);

	return 0;
}

static inline u_long
wait_for_completion_interruptible_timeout(struct completion *x, u_long timo)
{
	int ret;

	KASSERT(!cold);

	mtx_enter(&x->wait.lock);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, PCATCH, "wfcit", timo);
		if (ret) {
			mtx_leave(&x->wait.lock);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}
	x->done--;
	mtx_leave(&x->wait.lock);

	return 1;
}

static inline void
complete_all(struct completion *x)
{
	mtx_enter(&x->wait.lock);
	x->done = UINT_MAX;
	mtx_leave(&x->wait.lock);
	wakeup(x);
}

static inline bool
try_wait_for_completion(struct completion *x)
{
	mtx_enter(&x->wait.lock);
	if (x->done == 0) {
		mtx_leave(&x->wait.lock);
		return false;
	}
	x->done--;
	mtx_leave(&x->wait.lock);
	return true;
}

#endif
