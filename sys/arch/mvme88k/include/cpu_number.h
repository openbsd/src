/*	$OpenBSD: cpu_number.h,v 1.9 2001/12/19 07:04:41 smurph Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

#ifndef	_M88K_CPU_NUMBER_
#define _M88K_CPU_NUMBER_
 
#ifdef	_KERNEL
#ifndef _LOCORE
#include <machine/param.h>
extern unsigned number_cpus;

static unsigned cpu_number __P((void));

static __inline__ unsigned cpu_number(void)
{
	register unsigned cpu;
	if (brdtyp != BRD_188 || number_cpus == 1) return 0;
	__asm__ ("ldcr %0, cr18" : "=r" (cpu));
	return (cpu & 3);
}
#endif /* _LOCORE */
#endif /* _KERNEL */
#endif /* _M88K_CPU_NUMBER_ */
