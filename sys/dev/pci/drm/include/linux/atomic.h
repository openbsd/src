/* $OpenBSD: atomic.h,v 1.5 2019/08/17 06:07:22 jsg Exp $ */
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

#ifndef _DRM_LINUX_ATOMIC_H_
#define _DRM_LINUX_ATOMIC_H_

#include <sys/types.h>
#include <sys/mutex.h>
#include <machine/intr.h>
#include <machine/atomic.h>
#include <linux/types.h>

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
#define atomic_or(n, p)		atomic_setbits_int(p, n)
#define atomic_cmpxchg(p, o, n)	__sync_val_compare_and_swap(p, o, n)
#define cmpxchg(p, o, n)	__sync_val_compare_and_swap(p, o, n)
#define atomic_set_release(p, v)	atomic_set((p), (v))

static inline int
atomic_xchg(volatile int *v, int n)
{
	__sync_synchronize();
	return __sync_lock_test_and_set(v, n);
}

#define xchg(v, n)	__sync_lock_test_and_set(v, n)

static inline int
atomic_add_unless(volatile int *v, int n, int u)
{
	int o = *v;

	do {
		o = *v;
		if (o == u)
			return 0;
	} while (__sync_val_compare_and_swap(v, o, o +n) != o);

	return 1;
}

static inline int
atomic_dec_if_positive(volatile int *v)
{
	int r, o;

	do {
		o = *v;
		r = o - 1;
		if (r < 0)
			break;
	} while (__sync_val_compare_and_swap(v, o, r) != o);

	return r;
}

#define atomic_long_read(p)	(*(p))

#ifdef __LP64__
typedef int64_t atomic64_t;

#define atomic64_set(p, v)	(*(p) = (v))
#define atomic64_read(p)	(*(p))

static inline int64_t
atomic64_xchg(volatile int64_t *v, int64_t n)
{
	__sync_synchronize();
	return __sync_lock_test_and_set(v, n);
}

#define atomic64_add(n, p)	__sync_fetch_and_add_8(p, n)
#define atomic64_sub(n, p)	__sync_fetch_and_sub_8(p, n)
#define atomic64_inc(p)		__sync_fetch_and_add_8(p, 1)
#define atomic64_add_return(n, p) __sync_add_and_fetch_8(p, n)
#define atomic64_inc_return(p)	__sync_add_and_fetch_8(p, 1)

#else

typedef struct {
	volatile int64_t val;
	struct mutex lock;
} atomic64_t;

static inline void
atomic64_set(atomic64_t *v, int64_t i)
{
	mtx_init(&v->lock, IPL_HIGH);
	v->val = i;
}

static inline int64_t
atomic64_read(atomic64_t *v)
{
	int64_t val;

	mtx_enter(&v->lock);
	val = v->val;
	mtx_leave(&v->lock);

	return val;
}

static inline int64_t
atomic64_xchg(atomic64_t *v, int64_t n)
{
	int64_t val;

	mtx_enter(&v->lock);
	val = v->val;
	v->val = n;
	mtx_leave(&v->lock);

	return val;
}

static inline void
atomic64_add(int i, atomic64_t *v)
{
	mtx_enter(&v->lock);
	v->val += i;
	mtx_leave(&v->lock);
}

#define atomic64_inc(p)		atomic64_add(p, 1)

static inline int64_t
atomic64_add_return(int i, atomic64_t *v)
{
	int64_t val;

	mtx_enter(&v->lock);
	val = v->val + i;
	v->val = val;
	mtx_leave(&v->lock);

	return val;
}

#define atomic64_inc_return(p)		atomic64_add_return(p, 1)

static inline void
atomic64_sub(int i, atomic64_t *v)
{
	mtx_enter(&v->lock);
	v->val -= i;
	mtx_leave(&v->lock);
}
#endif

#ifdef __LP64__
typedef int64_t atomic_long_t;
#define atomic_long_set(p, v)		atomic64_set(p, v)
#define atomic_long_xchg(v, n)		atomic64_xchg(v, n)
#define atomic_long_cmpxchg(p, o, n)	atomic_cmpxchg(p, o, n)
#else
typedef int32_t atomic_long_t;
#define atomic_long_set(p, v)		atomic_set(p, v)
#define atomic_long_xchg(v, n)		atomic_xchg(v, n)
#define atomic_long_cmpxchg(p, o, n)	atomic_cmpxchg(p, o, n)
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
#define atomic_set_mask(bits, p)	atomic_setbits_int(p,bits)
#define atomic_clear_int(p, bits)	atomic_clearbits_int(p,bits)
#define atomic_clear_mask(bits, p)	atomic_clearbits_int(p,bits)
#define atomic_andnot(bits, p)		atomic_clearbits_int(p,bits)
#define atomic_fetchadd_int(p, n) __sync_fetch_and_add(p, n)
#define atomic_fetchsub_int(p, n) __sync_fetch_and_sub(p, n)
#define atomic_fetch_inc(p) __sync_fetch_and_add(p, 1)
#define atomic_fetch_xor(n, p) __sync_fetch_and_xor(p, n)

static inline atomic_t
test_and_set_bit(u_int b, volatile void *p)
{
	unsigned int m = 1 << (b & 0x1f);
	unsigned int prev = __sync_fetch_and_or((volatile u_int *)p + (b >> 5), m);
	return (prev & m) != 0;
}

static inline void
clear_bit(u_int b, volatile void *p)
{
	atomic_clear_int(((volatile u_int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static inline void
set_bit(u_int b, volatile void *p)
{
	atomic_set_int(((volatile u_int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static inline void
__clear_bit(u_int b, volatile void *p)
{
	volatile u_int *ptr = (volatile u_int *)p;
	ptr[b >> 5] &= ~(1 << (b & 0x1f));
}

static inline void
__set_bit(u_int b, volatile void *p)
{
	volatile u_int *ptr = (volatile u_int *)p;
	ptr[b >> 5] |= (1 << (b & 0x1f));
}

static inline int
test_bit(u_int b, const volatile void *p)
{
	return !!(((volatile u_int *)p)[b >> 5] & (1 << (b & 0x1f)));
}

static inline int
__test_and_set_bit(u_int b, volatile void *p)
{
	unsigned int m = 1 << (b & 0x1f);
	volatile u_int *ptr = (volatile u_int *)p;
	unsigned int prev = ptr[b >> 5];
	ptr[b >> 5] |= m;
	
	return (prev & m) != 0;
}

static inline int
test_and_clear_bit(u_int b, volatile void *p)
{
	unsigned int m = 1 << (b & 0x1f);
	unsigned int prev = __sync_fetch_and_and((volatile u_int *)p + (b >> 5), ~m);
	return (prev & m) != 0;
}

static inline int
__test_and_clear_bit(u_int b, volatile void *p)
{
	volatile u_int *ptr = (volatile u_int *)p;
	int rv = !!(ptr[b >> 5] & (1 << (b & 0x1f)));
	ptr[b >> 5] &= ~(1 << (b & 0x1f));
	return rv;
}

static inline int
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

static inline int
find_next_zero_bit(volatile void *p, int max, int b)
{
	volatile u_int *ptr = (volatile u_int *)p;

	for (; b < max; b += 32) {
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

static inline int
find_first_bit(volatile void *p, int max)
{
	int b;
	volatile u_int *ptr = (volatile u_int *)p;

	for (b = 0; b < max; b += 32) {
		if (ptr[b >> 5] != 0) {
			for (;;) {
				if (ptr[b >> 5] & (1 << (b & 0x1f)))
					return b;
				b++;
			}
		}
	}
	return max;
}

static inline int
find_next_bit(volatile void *p, int max, int b)
{
	volatile u_int *ptr = (volatile u_int *)p;

	for (; b < max; b+= 32) {
		if (ptr[b >> 5] != 0) {
			for (;;) {
				if (ptr[b >> 5] & (1 << (b & 0x1f)))
					return b;
				b++;
			}
		}
	}
	return max;
}

#define for_each_set_bit(b, p, max) \
	for ((b) = find_first_bit((p), (max));			\
	     (b) < (max);					\
	     (b) = find_next_bit((p), (max), (b) + 1))

#define for_each_clear_bit(b, p, max) \
	for ((b) = find_first_zero_bit((p), (max));		\
	     (b) < (max);					\
	     (b) = find_next_zero_bit((p), (max), (b) + 1))

#if defined(__i386__)
#define rmb()	__asm __volatile("lock; addl $0,-4(%%esp)" : : : "memory", "cc")
#define wmb()	__asm __volatile("lock; addl $0,-4(%%esp)" : : : "memory", "cc")
#define mb()	__asm __volatile("lock; addl $0,-4(%%esp)" : : : "memory", "cc")
#define smp_mb()	__asm __volatile("lock; addl $0,-4(%%esp)" : : : "memory", "cc")
#define smp_rmb()	__asm __volatile("" : : : "memory")
#define smp_wmb()	__asm __volatile("" : : : "memory")
#define __smp_store_mb(var, value)	do { (void)xchg(&var, value); } while (0)
#define smp_mb__after_atomic()	do { } while (0)
#define smp_mb__before_atomic()	do { } while (0)
#elif defined(__alpha__)
#define rmb()	alpha_mb();
#define wmb()	alpha_wmb();
#define mb()	alpha_mb();
#elif defined(__amd64__)
#define rmb()	__asm __volatile("lfence" : : : "memory")
#define wmb()	__asm __volatile("sfence" : : : "memory")
#define mb()	__asm __volatile("mfence" : : : "memory")
#define smp_mb()	__asm __volatile("lock; addl $0,-4(%%rsp)" : : : "memory", "cc");
#define smp_rmb()	__asm __volatile("" : : : "memory")
#define smp_wmb()	__asm __volatile("" : : : "memory")
#define __smp_store_mb(var, value)	do { (void)xchg(&var, value); } while (0)
#define smp_mb__after_atomic()	do { } while (0)
#define smp_mb__before_atomic()	do { } while (0)
#elif defined(__aarch64__)
#define rmb()	__membar("dsb ld")
#define wmb()	__membar("dsb st")
#define mb()	__membar("dsb sy")
#elif defined(__mips64__)
#define rmb()	mips_sync() 
#define wmb()	mips_sync()
#define mb()	mips_sync()
#elif defined(__powerpc__)
#define rmb()	__asm __volatile("sync" : : : "memory");
#define wmb()	__asm __volatile("sync" : : : "memory");
#define mb()	__asm __volatile("sync" : : : "memory");
#elif defined(__sparc64__)
#define rmb()	membar_sync()
#define wmb()	membar_sync()
#define mb()	membar_sync()
#endif

#ifndef smp_rmb
#define smp_rmb()	rmb()
#endif

#ifndef smp_wmb
#define smp_wmb()	wmb()
#endif

#ifndef mmiowb
#define mmiowb()	wmb()
#endif

#ifndef smp_mb__before_atomic
#define smp_mb__before_atomic()	mb()
#endif

#ifndef smp_mb__after_atomic
#define smp_mb__after_atomic()	mb()
#endif

#ifndef smp_store_mb
#define smp_store_mb(x, v)	do { x = v; mb(); } while (0)
#endif

#endif
