/*	$OpenBSD: atomic.h,v 1.6 2007/05/25 16:22:11 art Exp $	*/
/*	$NetBSD: atomic.h,v 1.1 2003/04/26 18:39:37 fvdl Exp $	*/

/*
 * Copyright 2002 (c) Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AMD64_ATOMIC_H
#define _AMD64_ATOMIC_H

/*
 * Perform atomic operations on memory. Should be atomic with respect
 * to interrupts and multiple processors.
 *
 * void atomic_setbits_int(volatile u_int *a, u_int mask) { *a |= mask; }
 * void atomic_clearbits_int(volatile u_int *a, u_int mas) { *a &= ~mask; }
 */

#if defined(_KERNEL) && !defined(_LOCORE)

#ifdef MULTIPROCESSOR
#define LOCK "lock"
#else
#define LOCK
#endif

static __inline u_int64_t
x86_atomic_testset_u64(volatile u_int64_t *ptr, u_int64_t val)
{
	__asm__ volatile ("xchgq %0,(%2)" :"=r" (val):"0" (val),"r" (ptr));
	return val;
}

static __inline u_int32_t
x86_atomic_testset_u32(volatile u_int32_t *ptr, u_int32_t val)
{
	__asm__ volatile ("xchgl %0,(%2)" :"=r" (val):"0" (val),"r" (ptr));
	return val;
}

static __inline int32_t
x86_atomic_testset_i32(volatile int32_t *ptr, int32_t val)
{
	__asm__ volatile ("xchgl %0,(%2)" :"=r" (val):"0" (val),"r" (ptr));
	return val;
}



static __inline void
x86_atomic_setbits_u32(volatile u_int32_t *ptr, u_int32_t bits)
{
	__asm __volatile(LOCK " orl %1,%0" :  "=m" (*ptr) : "ir" (bits));
}

static __inline void
x86_atomic_clearbits_u32(volatile u_int32_t *ptr, u_int32_t bits)
{
	__asm __volatile(LOCK " andl %1,%0" :  "=m" (*ptr) : "ir" (~bits));
}

static __inline u_long
x86_atomic_cas_ul(volatile u_long *ptr, u_long expect, u_long set)
{
	u_long res;

	__asm volatile(LOCK " cmpxchgq %2, %1" : "=a" (res), "=m" (*ptr)
	    : "r" (set), "a" (expect), "m" (*ptr) : "memory");

	return (res);
}

/*
 * XXX XXX XXX
 * theoretically 64bit cannot be used as
 * an "i" and thus if we ever try to give
 * these anything from the high dword there
 * is an asm error pending
 */
static __inline void
x86_atomic_setbits_u64(volatile u_int64_t *ptr, u_int64_t bits)
{
	__asm __volatile(LOCK " orq %1,%0" :  "=m" (*ptr) : "ir" (bits));
}

static __inline void
x86_atomic_clearbits_u64(volatile u_int64_t *ptr, u_int64_t bits)
{
	__asm __volatile(LOCK " andq %1,%0" :  "=m" (*ptr) : "ir" (~bits));
}

#define x86_atomic_testset_ul	x86_atomic_testset_u32
#define x86_atomic_testset_i	x86_atomic_testset_i32
#define x86_atomic_setbits_l	x86_atomic_setbits_u32
#define x86_atomic_setbits_ul	x86_atomic_setbits_u32
#define x86_atomic_clearbits_l	x86_atomic_clearbits_u32
#define x86_atomic_clearbits_ul	x86_atomic_clearbits_u32

#define atomic_setbits_int x86_atomic_setbits_u32
#define atomic_clearbits_int x86_atomic_clearbits_u32

#undef LOCK

#endif /* defined(_KERNEL) && !defined(_LOCORE) */
#endif /* _AMD64_ATOMIC_H_ */
