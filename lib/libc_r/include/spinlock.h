/*
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: spinlock.h,v 1.1 1998/12/21 07:58:55 d Exp $
 * $OpenBSD: spinlock.h,v 1.1 1998/12/21 07:58:55 d Exp $
 *
 * Lock definitions used in both libc and libpthread.
 *
 */

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_
#include <sys/cdefs.h>
#include <sys/types.h>
#include "_spinlock.h"

/*
 * Lock structure with room for debugging information.
 */
typedef struct {
	volatile _spinlock_lock_t access_lock;
	volatile long	lock_owner;
	volatile const char * fname;
	volatile int	lineno;
} spinlock_t;

#define	_SPINLOCK_INITIALIZER	{ _SPINLOCK_UNLOCKED, 0, 0, 0 }

#define _SPINUNLOCK(_lck)	(_lck)->access_lock = _SPINLOCK_UNLOCKED
#ifdef	_LOCK_DEBUG
#define	_SPINLOCK(_lck)		_spinlock_debug(_lck, __FILE__, __LINE__)
#else
#define	_SPINLOCK(_lck)		_spinlock(_lck)
#endif

/*
 * Thread function prototype definitions:
 */
__BEGIN_DECLS
void	_spinlock __P((spinlock_t *));
void	_spinlock_debug __P((spinlock_t *, const char *, int));

/* lock() functions return 0 if lock was acquired. */
/* is_locked functions() return 1 if lock is locked. */
int	_atomic_lock __P((volatile _spinlock_lock_t *));
int	_atomic_is_locked __P((volatile _spinlock_lock_t *));
int	_thread_slow_atomic_lock __P((volatile _spinlock_lock_t *));
int	_thread_slow_atomic_is_locked __P((volatile _spinlock_lock_t *));
__END_DECLS

#endif /* _SPINLOCK_H_ */
