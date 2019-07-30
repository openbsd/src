/*	$OpenBSD: mutex.h,v 1.14 2018/03/27 08:32:29 mpi Exp $	*/

/*
 * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
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

#include <machine/mutex.h>

#ifdef __USE_MI_MUTEX

#include <sys/_lock.h>

struct mutex {
	volatile void *mtx_owner;
	int mtx_wantipl;
	int mtx_oldipl;
#ifdef WITNESS
	struct lock_object mtx_lock_obj;
#endif
};

/*
 * To prevent lock ordering problems with the kernel lock, we need to
 * make sure we block all interrupts that can grab the kernel lock.
 * The simplest way to achieve this is to make sure mutexes always
 * raise the interrupt priority level to the highest level that has
 * interrupts that grab the kernel lock.
 */
#ifdef MULTIPROCESSOR
#define __MUTEX_IPL(ipl) \
    (((ipl) > IPL_NONE && (ipl) < IPL_MPFLOOR) ? IPL_MPFLOOR : (ipl))
#else
#define __MUTEX_IPL(ipl) (ipl)
#endif

#ifdef WITNESS
#define MUTEX_INITIALIZER_FLAGS(ipl, name, flags) \
	{ NULL, __MUTEX_IPL((ipl)), IPL_NONE, MTX_LO_INITIALIZER(name, flags) }
#else
#define MUTEX_INITIALIZER_FLAGS(ipl, name, flags) \
	{ NULL, __MUTEX_IPL((ipl)), IPL_NONE }
#endif

void __mtx_init(struct mutex *, int);
#define _mtx_init(mtx, ipl) __mtx_init((mtx), __MUTEX_IPL((ipl)))

#ifdef DIAGNOSTIC
#define MUTEX_ASSERT_LOCKED(mtx) do {					\
	if (((mtx)->mtx_owner != curcpu()) && !(panicstr || db_active))	\
		panic("mutex %p not held in %s", (mtx), __func__);	\
} while (0)

#define MUTEX_ASSERT_UNLOCKED(mtx) do {					\
	if (((mtx)->mtx_owner == curcpu()) && !(panicstr || db_active))	\
		panic("mutex %p held in %s", (mtx), __func__);		\
} while (0)
#else
#define MUTEX_ASSERT_LOCKED(mtx) do { } while (0)
#define MUTEX_ASSERT_UNLOCKED(mtx) do { } while (0)
#endif

#define MUTEX_LOCK_OBJECT(mtx)	(&(mtx)->mtx_lock_obj)
#define MUTEX_OLDIPL(mtx)	(mtx)->mtx_oldipl

#endif	/* __USE_MI_MUTEX */


#define MTX_LO_FLAGS(flags) \
	((!((flags) & MTX_NOWITNESS) ? LO_WITNESS : 0) | \
	 ((flags) & MTX_DUPOK ? LO_DUPOK : 0) | \
	 LO_INITIALIZED | (LO_CLASS_MUTEX << LO_CLASSSHIFT))

#define __MTX_STRING(x) #x
#define __MTX_S(x) __MTX_STRING(x)
#define __MTX_NAME __FILE__ ":" __MTX_S(__LINE__)

#define MTX_LO_INITIALIZER(name, flags) \
	{ .lo_type = &(struct lock_type){ .lt_name = __MTX_NAME }, \
	  .lo_name = (name) != NULL ? (name) : __MTX_NAME, \
	  .lo_flags = MTX_LO_FLAGS(flags) }

#define MTX_NOWITNESS	0x01
#define MTX_DUPOK	0x02

#define MUTEX_INITIALIZER(ipl) \
	MUTEX_INITIALIZER_FLAGS(ipl, NULL, 0)

/*
 * Some architectures need to do magic for the ipl, so they need a macro.
 */
#ifndef _mtx_init
void _mtx_init(struct mutex *, int);
#endif

void	__mtx_enter(struct mutex *);
int	__mtx_enter_try(struct mutex *);
void	__mtx_leave(struct mutex *);

#define mtx_init(m, ipl)	mtx_init_flags(m, ipl, NULL, 0)
#define mtx_enter(m)		_mtx_enter(m LOCK_FILE_LINE)
#define mtx_enter_try(m)	_mtx_enter_try(m LOCK_FILE_LINE)
#define mtx_leave(m)		_mtx_leave(m LOCK_FILE_LINE)

#ifdef WITNESS

void	_mtx_init_flags(struct mutex *, int, const char *, int,
	    struct lock_type *);

void	_mtx_enter(struct mutex *, const char *, int);
int	_mtx_enter_try(struct mutex *, const char *, int);
void	_mtx_leave(struct mutex *, const char *, int);

#define mtx_init_flags(m, ipl, name, flags) do {			\
	static struct lock_type __lock_type = { .lt_name = #m };	\
	_mtx_init_flags(m, ipl, name, flags, &__lock_type);		\
} while (0)

#else /* WITNESS */

#define mtx_init_flags(m, ipl, name, flags) do {			\
	(void)(name); (void)(flags);					\
	_mtx_init(m, ipl);						\
} while (0)

#define _mtx_init_flags(m,i,n,f,t)	_mtx_init(m,i)
#define _mtx_enter(m)			__mtx_enter(m)
#define _mtx_enter_try(m)		__mtx_enter_try(m)
#define _mtx_leave(m)			__mtx_leave(m)

#endif /* WITNESS */

#endif
