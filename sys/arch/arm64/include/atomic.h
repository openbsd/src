/* $OpenBSD: atomic.h,v 1.1 2016/12/17 23:38:33 patrick Exp $ */

/* Public Domain */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

#if defined(_KERNEL)
//#include <sys/stdatomic.h>

#define __membar(_f) do { __asm __volatile(_f ::: "memory"); } while (0)

/* virtio needs MP membars even on SP kernels */
#define virtio_membar_producer()	__membar("")
#define virtio_membar_consumer()	__membar("")
#define virtio_membar_sync()		__membar("dmb sy") /* XXX dmb? */

static inline void
atomic_setbits_int(__volatile unsigned int *ptr, unsigned int val)
{
#if 0
	//FIXME
	atomic_fetch_or_explicit((atomic_uint *)ptr, val, memory_order_seq_cst);
#else
	*ptr |= val;
#endif
}
static inline void
atomic_clearbits_int(__volatile unsigned int *ptr, unsigned int val)
{
#if 0
	//FIXME
	atomic_fetch_and_explicit((atomic_uint *)ptr, ~val, memory_order_seq_cst);
#else
	*ptr &= ~val;
#endif
}
#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */
