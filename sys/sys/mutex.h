/*	$OpenBSD: mutex.h,v 1.11 2017/11/29 15:12:52 visa Exp $	*/

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

#include <machine/mutex.h>

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
