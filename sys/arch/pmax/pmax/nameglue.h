/*	$NetBSD: nameglue.h,v 1.3 1996/10/13 03:39:52 christos Exp $	*/

/*
 * Use macros to glue names for "machine-independent" functions 
 *  (e.g., from new-config code modelled after DEC Alpha drivers)
 * to old-style pmax names.
 *
 */

/*
 * What is magic_read ?
 */
#ifdef  pmax
#ifdef MAGIC_READ
#undef MAGIC_READ
#endif
#define MAGIC_READ \
		 MachEmptyWriteBuffer()	/*XXX*/ /*FIXME*/

/* 
 * memory barrier
 */
#define MB() \
	 MachEmptyWriteBuffer()	/*XXX*/ /*FIXME*/
/*
 * Map physical addresses to kernel-virtual addresses.
 */
#define KV(x) ((void *)MIPS_PHYS_TO_KSEG1(x))

/*
 * Print debugging messages only if DEBUG defined on a pmax.
 */

#ifdef DEBUG
#define DPRINTF(x) do { printf x ;} while (0)	/* general debug */
#else
#define DPRINTF(x) /* nothing */		/* general debug */
#endif
#endif /* pmax */
