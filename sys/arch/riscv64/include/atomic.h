/*	$OpenBSD: atomic.h,v 1.4 2022/08/29 02:01:18 jsg Exp $	*/

/* Public Domain */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

#define __membar(_f) do {__asm volatile(_f ::: "memory"); } while (0)

#define membar_enter()		__membar("fence w,rw")
#define membar_exit()		__membar("fence rw,w")
#define membar_producer()	__membar("fence w,w")
#define membar_consumer()	__membar("fence r,r")
#define membar_sync()		__membar("fence rw,rw")

#if defined(_KERNEL)

/* virtio needs MP membars even on SP kernels */
#define virtio_membar_producer()	__membar("fence w,w")
#define virtio_membar_consumer()	__membar("fence r,r")
#define virtio_membar_sync()		__membar("fence rw,rw")

/*
 * Set bits
 * *p = *p | v
 */
static inline void
atomic_setbits_int(volatile unsigned int *p, unsigned int v)
{
	__asm volatile("amoor.w zero, %1, %0"
			: "+A" (*p)
			: "r" (v)
			: "memory");
}

static inline void
atomic_store_64(volatile uint64_t *p, uint64_t v)
{
	__asm volatile("amoor.d zero, %1, %0"
			: "+A" (*p)
			: "r" (v)
			: "memory");
}

/*
 * Clear bits
 * *p = *p & (~v)
 */
static inline void
atomic_clearbits_int(volatile unsigned int *p, unsigned int v)
{
	__asm volatile("amoand.w zero, %1, %0"
			: "+A" (*p)
			: "r" (~v)
			: "memory");
}

#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */
