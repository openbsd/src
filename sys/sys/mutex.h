/*
 * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef _SYS_MUTEX_H_
#define _SYS_MUTEX_H_

/*
 * A mutex is:
 *  - owned by a cpu.
 *  - non-recursive.
 *  - spinning.
 *  - not providing mutual exclusion between processes, only cpus.
 *  - providing interrupt blocking when necessary.
 *
 * Different mutexes can be nested, but not interleaved. This is ok:
 * "mtx_enter(foo); mtx_enter(bar); mtx_leave(bar); mtx_leave(foo);"
 * This is _not_ ok:
 * "mtx_enter(foo); mtx_enter(bar); mtx_leave(foo); mtx_leave(bar);"
 */

#ifdef __HAVE_MUTEX
#include <machine/mutex.h>
#else

/*
 * Simple non-mp implementation.
 */
struct mutex {
	int mtx_lock;
	int mtx_wantipl;
	int mtx_oldipl;
};

/*
 * Since the alpha IPL levels are so messed up, we have to do magic to get
 * this right.
 */
#define MUTEX_IPL(ipl) MUTEX_##ipl
#define MUTEX_IPL_NONE		0
#define MUTEX_IPL_SOFTSERIAL	1
#define MUTEX_IPL_SOFTCLOCK	2
#define MUTEX_IPL_SOFTNET	3
#define MUTEX_IPL_NET		4
#define MUTEX_IPL_BIO		5
#define MUTEX_IPL_VM		6
#define MUTEX_IPL_TTY		7
#define MUTEX_IPL_SERIAL	8
#define MUTEX_IPL_AUDIO		9
#define MUTEX_IPL_CLOCK		10
#define MUTEX_IPL_STATCLOCK	11
#define MUTEX_IPL_SCHED		12
#define MUTEX_IPL_HIGH		13

void mtx_init1(struct mutex *, int);
#define mtx_init(mtx, ipl) mtx_init1(mtx, MUTEX_##ipl)

#define MUTEX_INITIALIZER(ipl) { 0, MUTEX_##ipl, 0 }

#ifdef DIAGNOSTIC
#define MUTEX_ASSERT_LOCKED(mtx) do {					\
	if ((mtx)->mtx_lock == 0)					\
		panic("mutex %p not held in %s\n", (mtx), __func__);	\
} while (0)

#define MUTEX_ASSERT_UNLOCKED(mtx) do {					\
	if ((mtx)->mtx_lock != 0)					\
		panic("mutex %p held in %s\n", (mtx), __func__);	\
} while (0)
#else
#define MUTEX_ASSERT_LOCKED(mtx) do { } while (0)
#define MUTEX_ASSERT_UNLOCKED(mtx) do { } while (0)
#endif

#define MUTEX_OLDIPL(mtx)	(mtx)->mtx_oldipl

#endif

/*
 * Some architectures need to do magic for the ipl, so they need a macro.
 */
#ifndef mtx_init
void mtx_init(struct mutex *, int);
#endif
void mtx_enter(struct mutex *);
void mtx_leave(struct mutex *);

#endif
