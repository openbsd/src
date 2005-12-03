#ifndef _M88K_MUTEX_H_
#define _M88K_MUTEX_H_
/*	$OpenBSD: mutex.h,v 1.1 2005/12/03 19:01:14 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct mutex {
	volatile int mtx_lock;	/* mutex.S relies upon this field being first */
	int mtx_wantipl;
	int mtx_oldipl;
	void *mtx_cpu;
};

#define	MUTEX_INITIALIZER(ipl)		{ 0, (ipl), IPL_NONE, NULL }

#ifdef DIAGNOSTIC

#define	MUTEX_ASSERT_LOCKED(mtx)					\
do {									\
	if ((mtx)->mtx_lock == 0 || (mtx)->mtx_cpu != curcpu())		\
		panic("mutex %p not held in %s", (mtx), __func__);	\
} while (0)

#define	MUTEX_ASSERT_UNLOCKED(mtx)					\
do {									\
	if ((mtx)->mtx_lock != 0)					\
		panic("mutex %p held in %s", (mtx), __func__);		\
} while (0)

#else

#define	MUTEX_ASSERT_LOCKED(mtx)	do { /* nothing */ } while (0)
#define	MUTEX_ASSERT_UNLOCKED(mtx)	do { /* nothing */ } while (0)

#endif

#define MUTEX_OLDIPL(mtx)	(mtx)->mtx_oldipl

#endif	/* _M88K_MUTEX_H_ */
