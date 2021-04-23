/* Public Domain */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

#define __membar() do {__asm __volatile("fence" ::: "memory"); } while (0)

#define membar_enter()		__membar()
#define membar_exit()		__membar()
#define membar_producer()	__membar()
#define membar_consumer()	__membar()
#define membar_sync()		__membar()

#if defined(_KERNEL)

/* virtio needs MP membars even on SP kernels */
#define virtio_membar_producer()	__membar()
#define virtio_membar_consumer()	__membar()
#define virtio_membar_sync()		__membar()

/*
 * Set bits
 * *p = *p | v
 */
static inline void
atomic_setbits_int(volatile unsigned int *p, unsigned int v)
{
	__asm __volatile("amoor.w zero, %1, %0"
			: "+A" (*p)
			: "r" (v)
			: "memory");
}

static inline void
atomic_store_64(volatile uint64_t *p, uint64_t v)
{
	__asm __volatile("amoor.d zero, %1, %0"
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
	__asm __volatile("amoand.w zero, %1, %0"
			: "+A" (*p)
			: "r" (~v)
			: "memory");
}

#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */
