/*	$OpenBSD: atomic.h,v 1.11 2015/06/29 04:16:32 jsg Exp $	*/

/* Public Domain */

#ifndef _ARM_ATOMIC_H_
#define _ARM_ATOMIC_H_

#if defined(_KERNEL)

#include <arm/cpufunc.h>

/*
 * on pre-v6 arm processors, it is necessary to disable interrupts if
 * in the kernel and atomic updates are necessary without full mutexes
*
 * eventually it would be interesting to have these functions
 * support the V6/V7+ atomic instructions ldrex/strex if available
 * on the CPU.
 */

static inline unsigned int
_atomic_cas_uint(volatile unsigned int *uip, unsigned int o, unsigned int n)
{
	unsigned int cpsr;
	unsigned int rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip;
	if (rv == o)
		*uip = n;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_cas_uint(_p, _o, _n) _atomic_cas_uint((_p), (_o), (_n))

static inline unsigned int
_atomic_cas_ulong(volatile unsigned long *uip, unsigned long o, unsigned long n)
{
	unsigned int cpsr;
	unsigned long rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip;
	if (rv == o)
		*uip = n;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_cas_ulong(_p, _o, _n) _atomic_cas_ulong((_p), (_o), (_n))

static inline void *
_atomic_cas_ptr(volatile void *uip, void *o, void *n)
{
	unsigned int cpsr;
	void * volatile *uipp = (void * volatile *)uip;
	void *rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uipp;
	if (rv == o)
		*uipp = n;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_cas_ptr(_p, _o, _n) _atomic_cas_ptr((_p), (_o), (_n))

static inline unsigned int
_atomic_swap_uint(volatile unsigned int *uip, unsigned int n)
{
	unsigned int cpsr;
	unsigned int rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip;
	*uip = n;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_swap_uint(_p, _n) _atomic_swap_uint((_p), (_n))

static inline unsigned long
_atomic_swap_ulong(volatile unsigned long *uip, unsigned long n)
{
	unsigned int cpsr;
	unsigned long rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip;
	*uip = n;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_swap_ulong(_p, _n) _atomic_swap_ulong((_p), (_n))

static inline void *
_atomic_swap_ptr(volatile void *uip, void *n)
{
	unsigned int cpsr;
	void * volatile *uipp = (void * volatile *)uip;
	void *rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uipp;
	*uipp = n;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_swap_ptr(_p, _n) _atomic_swap_ptr((_p), (_n))

static inline unsigned int
_atomic_add_int_nv(volatile unsigned int *uip, unsigned int v)
{
	unsigned int cpsr;
	unsigned int rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip + v;
	*uip = rv;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_add_int_nv(_p, _v) _atomic_add_int_nv((_p), (_v))

static inline unsigned long
_atomic_add_long_nv(volatile unsigned long *uip, unsigned long v)
{
	unsigned int cpsr;
	unsigned long rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip + v;
	*uip = rv;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_add_long_nv(_p, _v) _atomic_add_long_nv((_p), (_v))

static inline unsigned int
_atomic_sub_int_nv(volatile unsigned int *uip, unsigned int v)
{
	unsigned int cpsr;
	unsigned int rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip - v;
	*uip = rv;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_sub_int_nv(_p, _v) _atomic_sub_int_nv((_p), (_v))

static inline unsigned long
_atomic_sub_long_nv(volatile unsigned long *uip, unsigned long v)
{
	unsigned int cpsr;
	unsigned long rv;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	rv = *uip - v;
	*uip = rv;
	restore_interrupts(cpsr);

	return (rv);
}
#define atomic_sub_long_nv(_p, _v) _atomic_sub_long_nv((_p), (_v))

static inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int cpsr;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	*uip |= v;
	restore_interrupts(cpsr);
}

static inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int cpsr;

	cpsr = disable_interrupts(I32_bit|F32_bit);
	*uip &= ~v;
	restore_interrupts(cpsr);
}

#if defined(CPU_ARMv7)
#define __membar(_f) do { __asm __volatile(_f ::: "memory"); } while (0)

#define membar_enter()		__membar("dmb sy")
#define membar_exit()		__membar("dmb sy")
#define membar_producer()	__membar("dmb st")
#define membar_consumer()	__membar("dmb sy")
#define membar_sync()		__membar("dmb sy")

#define virtio_membar_producer()	__membar("dmb st")
#define virtio_membar_consumer()	__membar("dmb sy")
#define virtio_membar_sync()		__membar("dmb sy")
#endif /* CPU_ARMv7 */

#endif /* defined(_KERNEL) */
#endif /* _ARM_ATOMIC_H_ */
