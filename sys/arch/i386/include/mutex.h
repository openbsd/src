/*	$OpenBSD: mutex.h,v 1.4 2011/03/23 16:54:35 pirofti Exp $	*/

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
#ifndef _MACHINE_MUTEX_H_
#define _MACHINE_MUTEX_H_

/*
 * XXX - we don't really need the mtx_lock field, we can use mtx_oldipl
 *	 as the lock to save some space.
 */
struct mutex {
	__volatile int mtx_lock;
	int mtx_wantipl;
	int mtx_oldipl;
	void *mtx_owner;
};

#define MUTEX_INITIALIZER(IPL) { 0, (IPL), 0, NULL }

#define MUTEX_ASSERT_LOCKED(mtx) do {					\
	if ((mtx)->mtx_lock != 1 ||					\
	    (mtx)->mtx_owner != curcpu())				\
		panic("mutex %p not held in %s", (mtx), __func__);	\
} while (0)

#define MUTEX_ASSERT_UNLOCKED(mtx) do {					\
	if ((mtx)->mtx_lock == 1 &&					\
	    (mtx)->mtx_owner == curcpu())				\
		panic("mutex %p held in %s", (mtx), __func__);		\
} while (0)

#define MUTEX_OLDIPL(mtx)	(mtx)->mtx_oldipl

#endif
