/*	$OpenBSD: mplock.h,v 1.12 2018/06/08 15:38:15 guenther Exp $	*/

/*
 * Copyright (c) 2004 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MPLOCK_H_
#define _MPLOCK_H_

#include <machine/mplock.h>

#ifdef __USE_MI_MPLOCK

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

void	___mp_lock_init(struct __mp_lock *, const struct lock_type *);
void	___mp_lock(struct __mp_lock * LOCK_FL_VARS);
void	___mp_unlock(struct __mp_lock * LOCK_FL_VARS);
int	___mp_release_all(struct __mp_lock * LOCK_FL_VARS);
int	___mp_release_all_but_one(struct __mp_lock * LOCK_FL_VARS);
void	___mp_acquire_count(struct __mp_lock *, int LOCK_FL_VARS);
int	__mp_lock_held(struct __mp_lock *, struct cpu_info *);

#ifdef WITNESS

#define __mp_lock_init(mpl) do {					\
	static const struct lock_type __lock_type = { .lt_name = #mpl };\
	___mp_lock_init((mpl), &__lock_type);				\
} while (0)

#else /* WITNESS */

#define __mp_lock_init(mpl)	___mp_lock_init((mpl), NULL)

#endif /* WITNESS */

#define __mp_lock(mpl)		___mp_lock((mpl) LOCK_FILE_LINE)
#define __mp_unlock(mpl)	___mp_unlock((mpl) LOCK_FILE_LINE)

#define __mp_release_all(mpl) \
	___mp_release_all((mpl) LOCK_FILE_LINE)
#define __mp_release_all_but_one(mpl) \
	___mp_release_all_but_one((mpl) LOCK_FILE_LINE)
#define __mp_acquire_count(mpl, count) \
	___mp_acquire_count((mpl), (count) LOCK_FILE_LINE)

#endif /* __USE_MI_MPLOCK */

extern struct __mp_lock kernel_lock;

#endif /* !_MPLOCK_H */
