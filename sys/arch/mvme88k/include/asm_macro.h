/*	$OpenBSD: asm_macro.h,v 1.4 1999/02/09 06:36:25 smurph Exp $ */
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
/*
 * HISTORY
 * $Log: asm_macro.h,v $
 * Revision 1.4  1999/02/09 06:36:25  smurph
 * Added kernel support for user debugging.  Fixed file ID's
 *
 * Revision 1.3  1997/03/03 20:20:46  rahnds
 * Cleanup after import. This also seems to bring up the current version.
 *
 * Revision 1.1.1.1  1995/10/18 10:54:22  deraadt
 * initial 88k import; code by nivas and based on mach luna88k
 *
 * Revision 2.2  93/01/26  18:07:26  danner
 * 	Created.
 * 	[93/01/24            jfriedl]
 * 
 */

#ifndef __M88K_ASM_MACRO_H__
#define __M88K_ASM_MACRO_H__

/*
 ** Various compiler macros used for speed and efficiency.
 ** Anyone can include.
  */

/*
 * PSR_TYPE is the type of the Process Status Register.
 */
typedef unsigned long m88k_psr_type;

/*
 * disable_interrupts_return_psr()
 *
 *    The INTERRUPT_DISABLE bit is set in the PSR and the *PREVIOUS*
 *    PSR is returned.  Intended to be used with set_psr() [below] as in:
 *
 *	{
 *	    m88k_psr_type psr;
 *	        .
 *	        .
 *	    psr = disable_interrupts_return_psr();
 *	        .
 *   		SHORT [time-wise] CRITICAL SECTION HERE
 *	        .
 *	    set_psr(psr);
 *	        .
 *	        .
 */
static inline m88k_psr_type disable_interrupts_return_psr(void)
{
    m88k_psr_type temp, oldpsr;
    asm volatile (
	"ldcr %0, cr1      \n"
	"set  %1, %0, 1<1> \n"
	"stcr %1, cr1      \n"
	"tcnd ne0, r0, 0     " : "=r" (oldpsr), "=r" (temp));
    return oldpsr;
}
#define disable_interrupt() (void)disable_interrupts_return_psr()

/*
 * Sets the PSR. See comments above.
 */
static inline void set_psr(m88k_psr_type psr)
{
    asm volatile ("stcr %0, cr1" :: "r" (psr));
}

/*
 * Enables interrupts.
 */
static inline m88k_psr_type enable_interrupts_return_psr(void)
{
    m88k_psr_type temp, oldpsr; /* need a temporary register */
    asm volatile (
	"ldcr %0, cr1      \n"
        "clr  %1, %0, 1<1> \n"
        "stcr %1, cr1        " : "=r" (oldpsr), "=r" (temp));
    return oldpsr;
}
#define enable_interrupt() (void)enable_interrupts_return_psr()

#define db_enable_interrupt enable_interrupt
#define db_disable_interrupt disable_interrupt

/*
 * flushes the data pipeline.
 */
static inline void flush_pipeline()
{
    asm volatile ("tcnd ne0, r0, 0");
}
#define db_flush_pipeline flush_pipeline

#endif /* __M88K_ASM_MACRO_H__ */
