/*	$OpenBSD: atomic.h,v 1.9 2014/07/19 05:27:17 dlg Exp $	*/

/* Public Domain */

#ifndef _VAX_ATOMIC_H_
#define _VAX_ATOMIC_H_

#if defined(_KERNEL)

#include <machine/mtpr.h>
#include <machine/intr.h>

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	int s;

	s = splhigh();
	*uip |= v;
	splx(s);
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	int s;

	s = splhigh();
	*uip &= ~v;
	splx(s);
}

static __inline unsigned int
atomic_add_int_nv_sp(volatile unsigned int *uip, unsigned int v)
{
	int s;
	unsigned int nv;

	s = splhigh();
	*uip += v;
	nv = *uip;
	splx(s);

	return nv;
}

static __inline unsigned int
atomic_sub_int_nv_sp(volatile unsigned int *uip, unsigned int v)
{
	int s;
	unsigned int nv;

	s = splhigh();
	*uip -= v;
	nv = *uip;
	splx(s);

	return nv;
}

static inline unsigned int
atomic_cas_uint_sp(unsigned int *p, unsigned int o, unsigned int n)
{
	int s;
	unsigned int ov;

	s = splhigh();
	ov = *p;
	if (ov == o)
		*p = n;
	splx(s);

	return ov;
}

static inline unsigned int
atomic_swap_uint_sp(unsigned int *p, unsigned int v)
{
	int s;
	unsigned int ov;

	s = splhigh();
	ov = *p;
	*p = v;
	splx(s);

	return ov;
}

#define	atomic_add_int_nv	atomic_add_int_nv_sp
#define	atomic_sub_int_nv	atomic_sub_int_nv_sp
#define	atomic_cas_uint		atomic_cas_uint_sp
#define	atomic_swap_uint	atomic_swap_uint_sp

#define	atomic_add_long_nv(p,v) \
	((unsigned long)atomic_add_int_nv((unsigned int *)p, (unsigned int)v))
#define	atomic_sub_long_nv(p,v) \
	((unsigned long)atomic_sub_int_nv((unsigned int *)p, (unsigned int)v))

#define	atomic_cas_ulong(p,o,n) \
	((unsigned long)atomic_cas_uint((unsigned int *)p, (unsigned int)o, \
	 (unsigned int)n))
#define	atomic_cas_ptr(p,o,n) \
	((void *)atomic_cas_uint((unsigned int *)p, (unsigned int)o, \
	 (unsigned int)n))

#define	atomic_swap_ulong(p,o) \
	((unsigned long)atomic_swap_uint((unsigned int *)p, (unsigned int)o)
#define	atomic_swap_ptr(p,o) \
	((void *)atomic_swap_uint((unsigned int *)p, (unsigned int)o))

static inline void
__sync_synchronize(void)
{
	__asm__ volatile ("" ::: "memory");
}

#endif /* defined(_KERNEL) */
#endif /* _VAX_ATOMIC_H_ */
