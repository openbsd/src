/*	$OpenBSD: atomic.h,v 1.14 2016/04/25 08:00:43 patrick Exp $	*/

/* Public Domain */

#ifndef _ARM_ATOMIC_H_
#define _ARM_ATOMIC_H_

#if defined(_KERNEL)

#if !defined(CPU_ARMv7)

#include <arm/cpufunc.h>
#include <arm/armreg.h>

/*
 * on pre-v6 arm processors, it is necessary to disable interrupts if
 * in the kernel and atomic updates are necessary without full mutexes
 */

static inline unsigned int
_atomic_cas_uint(volatile unsigned int *uip, unsigned int o, unsigned int n)
{
	unsigned int cpsr;
	unsigned int rv;

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
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

	cpsr = disable_interrupts(PSR_I|PSR_F);
	*uip |= v;
	restore_interrupts(cpsr);
}

static inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int cpsr;

	cpsr = disable_interrupts(PSR_I|PSR_F);
	*uip &= ~v;
	restore_interrupts(cpsr);
}

#else /* !CPU_ARMv7 */

/*
 * Compare and set:
 * ret = *ptr
 * if (ret == expect)
 * 	*ptr = new
 * return (ret)
 */
#define def_atomic_cas(_f, _t)					\
static inline _t						\
_f(volatile _t *p, _t e, _t n)					\
{								\
	_t ret, modified;					\
								\
	__asm volatile (					\
	    "1:	ldrex %0, [%4]		\n\t"			\
	    "	cmp %0, %3		\n\t"			\
	    "	bne 2f			\n\t"			\
	    "	strex %1, %2, [%4]	\n\t"			\
	    "	cmp %1, #0		\n\t"			\
	    "	bne 1b			\n\t"			\
	    "	b 3f			\n\t"			\
	    "2:	clrex			\n\t"			\
	    "3:				\n\t"			\
	    : "=&r" (ret), "=&r" (modified)			\
	    : "r" (n), "r" (e), "r" (p)				\
	    : "memory", "cc"					\
	);							\
	return (ret);						\
}
def_atomic_cas(_atomic_cas_uint, unsigned int)
def_atomic_cas(_atomic_cas_ulong, unsigned long)
#undef def_atomic_cas

#define atomic_cas_uint(_p, _e, _n) _atomic_cas_uint((_p), (_e), (_n))
#define atomic_cas_ulong(_p, _e, _n) _atomic_cas_ulong((_p), (_e), (_n))

static inline void *
_atomic_cas_ptr(volatile void *p, void *e, void *n)
{
	void *ret;
	uint32_t modified;

	__asm volatile (
	    "1:	ldrex %0, [%4]		\n\t"
	    "	cmp %0, %3		\n\t"
	    "	bne 2f			\n\t"
	    "	strex %1, %2, [%4]	\n\t"
	    "	cmp %1, #0		\n\t"
	    "	bne 1b			\n\t"
	    "	b 3f			\n\t"
	    "2:	clrex			\n\t"
	    "3:				\n\t"
	    : "=&r" (ret), "=&r" (modified)
	    : "r" (n), "r" (e), "r" (p)
	    : "memory", "cc"
	);
	return (ret);
}
#define atomic_cas_ptr(_p, _e, _n) _atomic_cas_ptr((_p), (_e), (_n))

/*
 * Swap:
 * ret = *p
 * *p = val
 * return (ret)
 */
#define def_atomic_swap(_f, _t)					\
static inline _t						\
_f(volatile _t *p, _t v)					\
{								\
	_t ret, modified;					\
								\
	__asm volatile (					\
	    "1:	ldrex %0, [%3]		\n\t"			\
	    "	strex %1, %2, [%3]	\n\t"			\
	    "	cmp %1, #0		\n\t"			\
	    "	bne 1b			\n\t"			\
	    : "=&r" (ret), "=&r" (modified)			\
	    : "r" (v), "r" (p)					\
	    : "memory", "cc"					\
	);							\
	return (ret);						\
}
def_atomic_swap(_atomic_swap_uint, unsigned int)
def_atomic_swap(_atomic_swap_ulong, unsigned long)
#undef def_atomic_swap

#define atomic_swap_uint(_p, _v) _atomic_swap_uint((_p), (_v))
#define atomic_swap_ulong(_p, _v) _atomic_swap_ulong((_p), (_v))

static inline void *
_atomic_swap_ptr(volatile void *p, void *v)
{
	void *ret;
	uint32_t modified;

	__asm volatile (
	    "1:	ldrex %0, [%3]		\n\t"
	    "	strex %1, %2, [%3]	\n\t"
	    "	cmp %1, #0		\n\t"
	    "	bne 1b			\n\t"
	    : "=&r" (ret), "=&r" (modified)
	    : "r" (v), "r" (p)
	    : "memory", "cc"
	);
	return (ret);
}
#define atomic_swap_ptr(_p, _v) _atomic_swap_ptr((_p), (_v))

/*
 * Increment returning the new value
 * *p += 1
 * return (*p)
 */
#define def_atomic_inc_nv(_f, _t)				\
static inline _t						\
_f(volatile _t *p)						\
{								\
	_t ret, modified;					\
								\
	__asm volatile (					\
	   "1:	ldrex %0, [%2]		\n\t"			\
	    "	add %0, %0, #1		\n\t"			\
	    "	strex %1, %0, [%2]	\n\t"			\
	    "	cmp %1, #0		\n\t"			\
	    "	bne 1b			\n\t"			\
	    : "=&r" (ret), "=&r" (modified)			\
	    : "r" (p)						\
	    : "memory", "cc"					\
	);							\
	return (ret);						\
}
def_atomic_inc_nv(_atomic_inc_int_nv, unsigned int)
def_atomic_inc_nv(_atomic_inc_long_nv, unsigned long)
#undef def_atomic_inc_nv

#define atomic_inc_int_nv(_p) _atomic_inc_int_nv((_p))
#define atomic_inc_long_nv(_p) _atomic_inc_long_nv((_p))

/*
 * Decrement returning the new value
 * *p -= 1
 * return (*p)
 */
#define def_atomic_dec_nv(_f, _t)				\
static inline _t						\
_f(volatile _t *p)						\
{								\
	_t ret, modified;					\
								\
	__asm volatile (					\
	    "1:	ldrex %0, [%2]		\n\t"			\
	    "	sub %0, %0, #1		\n\t"			\
	    "	strex %1, %0, [%2]	\n\t"			\
	    "	cmp %1, #0		\n\t"			\
	    "	bne 1b			\n\t"			\
	    : "=&r" (ret), "=&r" (modified)			\
	    : "r" (p)						\
	    : "memory", "cc"					\
	);							\
	return (ret);						\
}
def_atomic_dec_nv(_atomic_dec_int_nv, unsigned int)
def_atomic_dec_nv(_atomic_dec_long_nv, unsigned long)
#undef def_atomic_dec_nv

#define atomic_dec_int_nv(_p) _atomic_dec_int_nv((_p))
#define atomic_dec_long_nv(_p) _atomic_dec_long_nv((_p))


/*
 * Addition returning the new value
 * *p += v
 * return (*p)
 */
#define def_atomic_add_nv(_f, _t)				\
static inline _t						\
_f(volatile _t *p, _t v)					\
{								\
	_t ret, modified;					\
								\
	__asm volatile (					\
	    "1:	ldrex %0, [%2]		\n\t"			\
	    "	add %0, %0, %3		\n\t"			\
	    "	strex %1, %0, [%2]	\n\t"			\
	    "	cmp %1, #0		\n\t"			\
	    "	bne 1b			\n\t"			\
	    : "=&r" (ret), "=&r" (modified)			\
	    : "r" (p), "r" (v)					\
	    : "memory", "cc"					\
	);							\
	return (ret);						\
}
def_atomic_add_nv(_atomic_add_int_nv, unsigned int)
def_atomic_add_nv(_atomic_add_long_nv, unsigned long)
#undef def_atomic_add_nv

#define atomic_add_int_nv(_p, _v) _atomic_add_int_nv((_p), (_v))
#define atomic_add_long_nv(_p, _v) _atomic_add_long_nv((_p), (_v))

/*
 * Subtraction returning the new value
 * *p -= v
 * return (*p)
 */
#define def_atomic_sub_nv(_f, _t)				\
static inline _t						\
_f(volatile _t *p, _t v)					\
{								\
	_t ret, modified;					\
								\
	__asm volatile (					\
	    "1:	ldrex %0, [%2]		\n\t"			\
	    "	sub %0, %0, %3		\n\t"			\
	    "	strex %1, %0, [%2]	\n\t"			\
	    "	cmp %1, #0		\n\t"			\
	    "	bne 1b			\n\t"			\
	    : "=&r" (ret), "=&r" (modified)			\
	    : "r" (p), "r" (v)					\
	    : "memory", "cc"					\
	);							\
	return (ret);						\
}
def_atomic_sub_nv(_atomic_sub_int_nv, unsigned int)
def_atomic_sub_nv(_atomic_sub_long_nv, unsigned long)
#undef def_atomic_sub_nv

#define atomic_sub_int_nv(_p, _v) _atomic_sub_int_nv((_p), (_v))
#define atomic_sub_long_nv(_p, _v) _atomic_sub_long_nv((_p), (_v))

/*
 * Set bits
 * *p = *p | v
 */
static inline void
atomic_setbits_int(volatile unsigned int *p, unsigned int v)
{
	unsigned int modified, tmp;

	__asm volatile (
	    "1:	ldrex %0, [%3]		\n\t"
	    "	orr %0, %0, %2		\n\t"
	    "	strex %1, %0, [%3]	\n\t"
	    "	cmp %1, #0		\n\t"
	    "	bne 1b			\n\t"
	    : "=&r" (tmp), "=&r" (modified)
	    : "r" (v), "r" (p)
	    : "memory", "cc"
	);
}

/*
 * Clear bits
 * *p = *p & (~v)
 */
static inline void
atomic_clearbits_int(volatile unsigned int *p, unsigned int v)
{
	unsigned int modified, tmp;

	__asm volatile (
	    "1:	ldrex %0, [%3]		\n\t"
	    "	bic %0, %0, %2		\n\t"
	    "	strex %1, %0, [%3]	\n\t"
	    "	cmp %1, #0		\n\t"
	    "	bne 1b			\n\t"
	    : "=&r" (tmp), "=&r" (modified)
	    : "r" (v), "r" (p)
	    : "memory", "cc"
	);
}

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
