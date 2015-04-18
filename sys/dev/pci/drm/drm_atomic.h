/* $OpenBSD: drm_atomic.h,v 1.12 2015/04/18 14:47:34 jsg Exp $ */
/**
 * \file drm_atomic.h
 * Atomic operations used in the DRM which may or may not be provided by the OS.
 * 
 * \author Eric Anholt <anholt@FreeBSD.org>
 */

/*-
 * Copyright 2004 Eric Anholt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <machine/atomic.h>

typedef uint32_t atomic_t;

#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		__sync_fetch_and_add(p, 1)
#define atomic_dec(p)		__sync_fetch_and_sub(p, 1)
#define atomic_add(n, p)	__sync_fetch_and_add(p, n)
#define atomic_sub(n, p)	__sync_fetch_and_sub(p, n)
#define atomic_add_return(n, p) __sync_add_and_fetch(p, n)
#define atomic_sub_return(n, p) __sync_sub_and_fetch(p, n)
#define atomic_inc_return(v)	atomic_add_return(1, (v))
#define atomic_dec_return(v)	atomic_sub_return(1, (v))
#define atomic_dec_and_test(v)	(atomic_dec_return(v) == 0)
#define atomic_inc_and_test(v)	(atomic_inc_return(v) == 0)

static __inline int
atomic_xchg(volatile int *v, int n)
{
	__sync_synchronize();
	return __sync_lock_test_and_set(v, n);
}

#ifdef __LP64__
typedef uint64_t atomic64_t;

#define atomic64_set(p, v)	(*(p) = (v))
#define atomic64_read(p)	(*(p))

static __inline int64_t
atomic64_xchg(volatile int64_t *v, int64_t n)
{
	__sync_synchronize();
	return __sync_lock_test_and_set(v, n);
}

#else

typedef struct {
	volatile uint64_t val;
	struct mutex lock;
} atomic64_t;

static __inline void
atomic64_set(atomic64_t *v, int64_t i)
{
	mtx_init(&v->lock, IPL_HIGH);
	v->val = i;
}

static __inline int64_t
atomic64_read(atomic64_t *v)
{
	int64_t val;

	mtx_enter(&v->lock);
	val = v->val;
	mtx_leave(&v->lock);

	return val;
}

static __inline int64_t
atomic64_xchg(atomic64_t *v, int64_t n)
{
	int64_t val;

	mtx_enter(&v->lock);
	val = v->val;
	v->val = n;
	mtx_leave(&v->lock);

	return val;
}
#endif

static inline int
atomic_inc_not_zero(atomic_t *p)
{
	if (*p == 0)
		return (0);

	*(p) += 1;
	return (*p);
}

/* FIXME */
#define atomic_set_int(p, bits)		atomic_setbits_int(p,bits)
#define atomic_clear_int(p, bits)	atomic_clearbits_int(p,bits)
#define atomic_clear_mask(bits, p)	atomic_clearbits_int(p,bits)
#define atomic_fetchadd_int(p, n) __sync_fetch_and_add(p, n)
#define atomic_fetchsub_int(p, n) __sync_fetch_and_sub(p, n)

#if defined(__i386__) || defined(__amd64__)
static __inline int
atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src)
{
	int res = exp;

	__asm __volatile (
	"	lock ;			"
	"	cmpxchgl %1,%2 ;	"
	"       setz	%%al ;		"
	"	movzbl	%%al,%0 ;	"
	"1:				"
	"# atomic_cmpset_int"
	: "+a" (res)			/* 0 (result) */
	: "r" (src),			/* 1 */
	  "m" (*(dst))			/* 2 */
	: "memory");				 

	return (res);
}
#else /* __i386__ */
static __inline int
atomic_cmpset_int(__volatile__ u_int *dst, u_int old, u_int new)
{
	int s = splhigh();
	if (*dst==old) {
		*dst = new;
		splx(s);
		return 1;
	}
	splx(s);
	return 0;
}
#endif /* !__i386__ */

static __inline atomic_t
test_and_set_bit(u_int b, volatile void *p)
{
	int s = splhigh();
	unsigned int m = 1<<b;
	unsigned int r = *(volatile int *)p & m;
	*(volatile int *)p |= m;
	splx(s);
	return r;
}

static __inline void
clear_bit(u_int b, volatile void *p)
{
	atomic_clear_int(((volatile u_int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(u_int b, volatile void *p)
{
	atomic_set_int(((volatile u_int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(u_int b, volatile void *p)
{
	return !!(((volatile u_int *)p)[b >> 5] & (1 << (b & 0x1f)));
}

static __inline int
find_first_zero_bit(volatile void *p, int max)
{
	int b;
	volatile u_int *ptr = (volatile u_int *)p;

	for (b = 0; b < max; b += 32) {
		if (ptr[b >> 5] != ~0) {
			for (;;) {
				if ((ptr[b >> 5] & (1 << (b & 0x1f))) == 0)
					return b;
				b++;
			}
		}
	}
	return max;
}
