/*	$OpenBSD: atomic.h,v 1.8 2014/01/30 00:51:13 dlg Exp $	*/
/*
 * Copyright (c) 2007 Artur Grabowski <art@openbsd.org>
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

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

#if defined(_KERNEL)

static inline unsigned int
_atomic_cas_uint(volatile unsigned int *p, unsigned int e, unsigned int n)
{
	__asm __volatile("cas [%2], %3, %0"
	    : "+r" (n), "=m" (*p)
	    : "r" (p), "r" (e), "m" (*p));

	return (n);
}
#define atomic_cas_uint(_p, _e, _n) _atomic_cas_uint((_p), (_e), (_n))

static inline unsigned long
_atomic_cas_ulong(volatile unsigned long *p, unsigned long e, unsigned long n)
{
	__asm __volatile("casx [%2], %3, %0"
	    : "+r" (n), "=m" (*p)
	    : "r" (p), "r" (e), "m" (*p));

	return (n);
}
#define atomic_cas_ulong(_p, _e, _n) _atomic_cas_ulong((_p), (_e), (_n))

static inline void *
_atomic_cas_ptr(volatile void **p, void *e, void *n)
{
	__asm __volatile("casx [%2], %3, %0"
	    : "+r" (n), "=m" (*p)
	    : "r" (p), "r" (e), "m" (*p));

	return (n);
}
#define atomic_cas_ptr(_p, _e, _n) _atomic_cas_ptr((_p), (_e), (_n))

#define def_atomic_swap(_f, _t, _c)					\
static inline _t							\
_f(volatile _t *p, _t v)						\
{									\
	_t e;								\
	_t r;								\
									\
	r = (_t)*p;							\
	do {								\
		e = r;							\
		r = _c(p, e, v);					\
	} while (r != e);						\
									\
	return (r);							\
}

def_atomic_swap(_atomic_swap_uint, unsigned int, atomic_cas_uint)
def_atomic_swap(_atomic_swap_ulong, unsigned long, atomic_cas_ulong)
def_atomic_swap(_atomic_swap_ptr, void *, atomic_cas_ptr)
#undef def_atomic_swap

#define atomic_swap_uint(_p, _v)  _atomic_swap_uint(_p, _v)
#define atomic_swap_ulong(_p, _v)  _atomic_swap_ulong(_p, _v)
#define atomic_swap_ptr(_p, _v)  _atomic_swap_ptr(_p, _v)

#define def_atomic_op_nv(_f, _t, _c, _op)				\
static inline _t							\
_f(volatile _t *p, _t v)						\
{									\
	_t e, r, f;							\
									\
	r = *p;								\
	do {								\
		e = r;							\
		f = e _op v;						\
		r = _c(p, e, f);					\
	} while (r != e);						\
									\
	return (f);							\
}

def_atomic_op_nv(_atomic_add_int_nv, unsigned int, atomic_cas_uint, +)
def_atomic_op_nv(_atomic_add_long_nv, unsigned long, atomic_cas_ulong, +)
def_atomic_op_nv(_atomic_sub_int_nv, unsigned int, atomic_cas_uint, -)
def_atomic_op_nv(_atomic_sub_long_nv, unsigned long, atomic_cas_ulong, -)
#undef def_atomic_opf

#define atomic_add_int_nv(_p, _v)  _atomic_add_int_nv(_p, _v)
#define atomic_add_long_nv(_p, _v)  _atomic_add_long_nv(_p, _v)
#define atomic_sub_int_nv(_p, _v)  _atomic_sub_int_nv(_p, _v)
#define atomic_sub_long_nv(_p, _v)  _atomic_sub_long_nv(_p, _v)

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int e, r;

	r = *uip;
	do {
		e = r;
		r = atomic_cas_uint(uip, e, e | v);
	} while (r != e);
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int e, r;

	r = *uip;
	do {
		e = r;
		r = atomic_cas_uint(uip, e, e & ~v);
	} while (r != e);
}

#if KERN_MM != PSTATE_MM_TSO
#error membar operations only support KERN_MM = PSTATE_MM_TSO
#endif

#define membar_enter()		membar(LoadLoad)
#define membar_exit()		membar(LoadLoad)
#define membar_producer()	membar(0)
#define membar_consumer()	membar(LoadLoad)
#define membar_sync()		membar(LoadLoad)

#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */
