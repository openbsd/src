/*
 * Copyright (c) 2007 Artur Grabowski <art@openbsd.org>
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

#ifndef _MACHINE_MPLOCK_H_
#define _MACHINE_MPLOCK_H_

#include <sys/_lock.h>

struct __mp_lock_cpu {
	u_int			mplc_ticket;
	u_int			mplc_depth;
};

struct __mp_lock {
	struct __mp_lock_cpu	mpl_cpus[MAXCPUS];
	volatile u_int		mpl_ticket;
	u_int			mpl_users;
#ifdef WITNESS
	struct lock_object	mpl_lock_obj;
#endif
};

#ifndef _LOCORE

void ___mp_lock_init(struct __mp_lock *);
void ___mp_lock(struct __mp_lock * LOCK_FL_VARS);
void ___mp_unlock(struct __mp_lock * LOCK_FL_VARS);
int ___mp_release_all(struct __mp_lock * LOCK_FL_VARS);
int ___mp_release_all_but_one(struct __mp_lock * LOCK_FL_VARS);
void ___mp_acquire_count(struct __mp_lock *, int LOCK_FL_VARS);
int __mp_lock_held(struct __mp_lock *);

#ifdef WITNESS

void _mp_lock_init(struct __mp_lock *, struct lock_type *);

#define __mp_lock_init(mpl) do {					\
	static struct lock_type __lock_type = { .lt_name = #mpl };	\
	_mp_lock_init((mpl), &__lock_type);				\
} while (0)

#else /* WITNESS */

#define __mp_lock_init		___mp_lock_init

#endif /* WITNESS */

#define __mp_lock(mpl)		___mp_lock((mpl) LOCK_FILE_LINE)
#define __mp_unlock(mpl)	___mp_unlock((mpl) LOCK_FILE_LINE)

#define __mp_release_all(mpl) \
	___mp_release_all((mpl) LOCK_FILE_LINE)
#define __mp_release_all_but_one(mpl) \
	___mp_release_all_but_one((mpl) LOCK_FILE_LINE)
#define __mp_acquire_count(mpl, count) \
	___mp_acquire_count((mpl), (count) LOCK_FILE_LINE)

#endif

#endif /* !_MACHINE_MPLOCK_H */
