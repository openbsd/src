/*	$OpenBSD: asm_macro.h,v 1.3 2005/12/03 16:52:16 miod Exp $ */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
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

#ifndef __M88K_ASM_MACRO_H__
#define __M88K_ASM_MACRO_H__

/*
 * Various compiler macros used for speed and efficiency.
 * Anyone can include.
 */

#include <machine/asm.h>

/*
 * Flushes the data pipeline.
 */
#define	flush_pipeline() \
	__asm__ __volatile__ (FLUSH_PIPELINE_STRING)

/*
 * Sets the PSR.
 */
static __inline__ void set_psr(u_int psr)
{
	__asm__ __volatile__ ("stcr %0, cr1" :: "r" (psr));
	__asm__ __volatile__ (FLUSH_PIPELINE_STRING);
}

/*
 * Gets the PSR.
 */
static __inline__ u_int get_psr(void)
{
	u_int psr;
	__asm__ __volatile__ ("ldcr %0, cr1" : "=r" (psr));
	return (psr);
}

#define	disable_interrupt(psr)	set_psr(((psr) = get_psr()) | PSR_IND)

/*
 * Provide access from C code to the assembly instruction ff1
 */
static __inline__ unsigned ff1(unsigned val)
{
	__asm__ __volatile__ ("ff1 %0, %0" : "=r" (val) : "0" (val));
	return (val);
}

static __inline__ u_int get_cpu_pid(void)
{
	u_int pid;
	__asm__ __volatile__ ("ldcr %0, cr0" : "=r" (pid));
	return (pid);
}

#endif /* __M88K_ASM_MACRO_H__ */
