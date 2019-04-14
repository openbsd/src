/*	$OpenBSD: completion.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
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
_wait_for_completion_timeout(struct completion *x, u_long timo LOCK_FL_VARS)
{
	int ret;

	KASSERT(!cold);

	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, 0, "wfct", timo);
		if (ret) {
			_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}
	x->done--;
	_mtx_leave(&x->wait.lock LOCK_FL_ARGS);

	return 1;
}
#define wait_for_completion_timeout(x, timo)	\
	_wait_for_completion_timeout(x, timo LOCK_FILE_LINE)

static inline u_long
_wait_for_completion_interruptible(struct completion *x LOCK_FL_VARS)
{
	int ret;

	KASSERT(!cold);

	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, PCATCH, "wfci", 0);
		if (ret) {
			_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}
	x->done--;
	_mtx_leave(&x->wait.lock LOCK_FL_ARGS);

	return 0;
}
#define wait_for_completion_interruptible(x)	\
	_wait_for_completion_interruptible(x LOCK_FILE_LINE)

static inline u_long
_wait_for_completion_interruptible_timeout(struct completion *x, u_long timo
    LOCK_FL_VARS)
{
	int ret;

	KASSERT(!cold);

	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, PCATCH, "wfcit", timo);
		if (ret) {
			_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}
	x->done--;
	_mtx_leave(&x->wait.lock LOCK_FL_ARGS);

	return 1;
}
#define wait_for_completion_interruptible_timeout(x, timo)	\
	_wait_for_completion_interruptible_timeout(x, timo LOCK_FILE_LINE)

static inline void
_complete_all(struct completion *x LOCK_FL_VARS)
{
	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	x->done = UINT_MAX;
	_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
	wakeup(x);
}
#define complete_all(x)	_complete_all(x LOCK_FILE_LINE)

static inline bool
_try_wait_for_completion(struct completion *x LOCK_FL_VARS)
{
	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	if (x->done == 0) {
		_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
		return false;
	}
	x->done--;
	_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
	return true;
}
#define try_wait_for_completion(x)	_try_wait_for_completion(x LOCK_FILE_LINE)

#endif
