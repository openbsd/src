/*	$NetBSD: psl.h,v 1.4 1995/11/23 02:36:33 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef __ALPHA_PSL_H__
#define	__ALPHA_PSL_H__

/*
 * Processor Status register definitions.
 */
#define	PSL_U		0x08		/* PS<3> == 1 -> User mode */
#define	PSL_IPL		0x07		/* PS<2:0> -> Interrupt mask */

/*
 * The interrupt priority levels.
 * Other IPL's are configured in software, and are listed below.
 */
#define	PSL_IPL_0	0		/* all interrupts enabled */
#define PSL_IPL_SOFT	1		/* block software interrupts */
#define	PSL_IPL_IO	4		/* block I/O device interrupts */
#define	PSL_IPL_CLOCK	5		/* block clock interrupts */
#define	PSL_IPL_HIGH	6		/* block everything except mchecks */

/*
 * Miscellaneous PSL definitions
 */
#define	PSL_MBZ		(0xfffffffffffffff0)	/* Must be always zero */
#define	PSL_USERSET	(PSL_U)			/* Must be set for user-mode */
#define	PSL_USERCLR	(PSL_MBZ|PSL_IPL)	/* Must be clr for user-mode */
#define	USERMODE(ps)	((ps & PSL_U) != 0)	/* Is it user-mode? */

#ifdef _KERNEL
/*
 * Translation buffer invalidation macro definitions.
 */
#define	TBI_A		-2		/* Flush all TB entries */
#define	TBI_AP		-1		/* Flush all per-process TB entries */
#define	TBI_SI		1		/* Invalidate ITB entry for va */
#define	TBI_SD		2		/* Invalidate DTB entry for va */
#define	TBI_S		3		/* Invalidate all entries for va */

#define	TBIA()		pal_tbi(TBI_A, NULL)
#define	TBIAP()		pal_tbi(TBI_AP, NULL)
#define	TBISI(va)	pal_tbi(TBI_SI, va)
#define	TBISD(va)	pal_tbi(TBI_SD, va)
#define	TBIS(va)	pal_tbi(TBI_S, va)

/*
 * Cache invalidation/flush routines.
 */

/* Flush all write buffers */
static __inline int wbflush() { __asm __volatile("mb"); }	/* XXX? wmb */

#define	IMB()		pal_imb()	/* Sync instruction cache w/data */

void alpha_mb __P((void));		/* Flush all write buffers */
void pal_imb __P((void));		/* Sync instruction cache */
u_int64_t pal_swpipl __P((u_int64_t));	/* write new IPL, return old */
u_int64_t profile_swpipl __P((u_int64_t));	/* pal_swpipl w/o profiling */
void pal_tbi __P((u_int64_t, void *));	/* Invalidate TLB entries */
#endif /* _KERNEL */

#endif /* !__ALPHA_PSL_H__ */
