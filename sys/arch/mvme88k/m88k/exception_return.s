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
/*
 * Exception handler return routines.
 */
/*
 * HISTORY
 * $Log: exception_return.s,v $
 * Revision 1.1.1.1  1995/10/18 10:54:27  deraadt
 * initial 88k import; code by nivas and based on mach luna88k
 *
 * Revision 2.6  93/01/26  18:00:53  danner
 * 	conditionalized define of ASSEMBLER.
 * 	[93/01/22            jfriedl]
 * 
 * Revision 2.5  92/08/03  17:51:58  jfriedl
 * 	Update includes, changed to new style manifiest constants [danner]
 * 
 * Revision 2.4  92/05/04  11:28:03  danner
 * 	Remove debugging cruft. Leave argument save area in call to
 * 	 ast_taken.
 * 	[92/05/03            danner]
 * 	Remove debugging cruft.
 * 	[92/04/12            danner]
 * 	[92/04/12  16:25:32  danner]
 * 
 *	In the case of an ast on a return from exception, a random value
 *	was stored into R2. Fixed.
 * 	[92/04/12            danner]
 * 
 * Revision 2.3  92/04/01  10:56:17  rpd
 * 	Corrections to the ast handling code.
 * 	[92/03/20            danner]
 * 	Corrected typo in ast_taken register reload code.
 * 	[92/03/03            danner]
 * 
 * Revision 2.2  92/02/18  18:03:30  elf
 * 	Created. 
 * 	[92/02/01            danner]
 * 
 */

#include <mach_kdb.h>

#ifndef ASSEMBLER
#  define ASSEMBLER /* this is required for some of the include files */
#endif

#include <assym.s>   	  		/* for PCB_KSP, etc */
#include <machine/asm.h>
#include <motorola/m88k/m88100/m88100.h>
#include <motorola/m88k/m88100/psl.h>
#include <motorola/m88k/trap.h>         	/* for T_ defines */

/*
 * Return from exception - all registers need to be restored.
 * R30 points to the exception frame.
 * R31 is the kernel stack pointer.
 * Any interrupt status is acceptable on entry.
 * All other registers are scratch.
 * Any data and fp faults must be cleared up before this routine
 * is called.
 */
ENTRY(return_from_exception)
 	ld	r10, r30, REG_OFF(EF_EPSR)   ; get old epsr
	ldcr	r2, PSR
	set	r2, r2, 1<PSR_IND_LOG>
	stcr	r2, PSR				     ; disable interrupts
	FLUSH_PIPELINE
	bb1	PSR_IND_LOG, r10, 1f ; no need to check
	bsr	ast_check
1:		
/* current status -

	interrupts disabled. Asts checked for. 
	Ready to restore registers and return from the exception.
	R30 points to the exception frame.
*/
	/* reload r2-r13 */
	ld.d	r2 , r30, GENREG_OFF(2)
	ld.d	r4 , r30, GENREG_OFF(4)
	ld.d	r6 , r30, GENREG_OFF(6)
	ld.d	r8 , r30, GENREG_OFF(8)
	ld.d	r10, r30, GENREG_OFF(10)
	br.n	return_common
	ld.d	r12, r30, GENREG_OFF(12)

/*
 * Return from syscall - registers r3-r13 need not be restored.
 * R30 points to the exception frame.
 * R31 is the kernel stack pointer.
 * All other registers are scratch.
 * Any interrupt status is acceptable on entry.
 */

ENTRY(return_from_syscall)
/* turn off interrupts, check ast */
	ldcr	r3, PSR
	set	r3, r3, 1<PSR_IND_LOG>
	stcr	r3, PSR				     ; disable interrupts
	FLUSH_PIPELINE
	bsr	ast_check
	/* restore r2 */
	ld	r2, r30, GENREG_OFF(2)
	/* current status -
		interrupts disabled. Asts checked for. 
		Ready to restore registers and return from the exception.
		R30 holds the frame pointer
	*/
	/* br return_common */


LABEL(return_common)
/*
	R30 points to the exception frame.
	Interrupts off. 
	r2-r13 need to be preserved.
*/
	/* restore r14-r29 */
	ld.d	r14, r30, GENREG_OFF(14)
	ld.d	r16, r30, GENREG_OFF(16)
	ld.d	r18, r30, GENREG_OFF(18)
	ld.d	r20, r30, GENREG_OFF(20)
	ld.d	r22, r30, GENREG_OFF(22)
	ld.d	r24, r30, GENREG_OFF(24)
	ld.d	r26, r30, GENREG_OFF(26)
	ld.d	r28, r30, GENREG_OFF(28)
	; restore r1, r30, r31 later
	/* turn off shadowing - we are about to trash
  	   our kernel stack pointer, which means this code
	   cannot be tracked by a debuuger */
	; disable shadowing (interrupts already disabled above)
	ldcr	r1, PSR
	set	r1, r1, 1<PSR_SFRZ_LOG>
	stcr	r1, PSR
	FLUSH_PIPELINE

	; reload the control regs
	/*
	 * Note: no need to restore the SXIP.
	 * When the "rte" causes execution to continue
	 * first with the instruction pointed to by the NIP
	 * and then the FIP.
	 *
	 * See MC88100 Risc Processor User's Manual, 2nd Edition,
	 * section 6.4.3.1.2-4
	 */
	ld	r31, r30, REG_OFF(EF_SNIP)
	ld	r1,  r30, REG_OFF(EF_SFIP)
	stcr	r0,  SSBR
	stcr	r31, SNIP
	stcr	r1,  SFIP

	ld	r31, r30, REG_OFF(EF_EPSR)
	ld	r1,  r30, REG_OFF(EF_MODE)
	stcr	r31, EPSR

	/*
	 * restore the mode (cpu flags).
	 * This can't be done directly, because the flags include the
	 * CPU number.  We might now be on a different CPU from when we
	 * first entered the exception handler (due to having been blocked
	 * and then restarted on a different CPU).  Thus, we'll grab the
	 * old flags and put the current cpu number there.
	 */
	clr	r1, r1, FLAG_CPU_FIELD_WIDTH <0> /* clear bits 0..WIDTH */
	ldcr	r31, SR1
	clr	r31, r31, 0<FLAG_CPU_FIELD_WIDTH> /* clear bits WIDTH..31 */
	or	r31, r1, r31
	stcr	r31, SR1	; restore old flags with (maybe new) CPU number

	/* Now restore r1, r30, and r31 */
	ld	r1,  r30, GENREG_OFF(1)
	ld.d	r30, r30, GENREG_OFF(30)

    _LABEL(return_from_exception)
	RTE


LABEL(ast_check)
	/* enter here with interrupts disabled */	
	/*
	 *
	 *    ast_check:
	 *
	 *	if (exception was from user mode && need_ast[cpu_number()])
	 *	{
	 *	    call: ast_taken()(turns interrupts back on,clears need_ast)
	 *	    disable_interrupts 
	 *	    goto check_ast
	 *	}
 	 *    return (with interrupts off)
         *
 	 * Upon entry, 
 	 *           R30 is the exception frame pointer
	 * 	     R31 is the kernel stack pointer
 	 *           R1 is the return address
	 *
	 *	Upon entry to this function, all user register state
	 *	must be up to date in the pcb. In particular, the return
	 *	value for thread_syscall_return has to have been saved.
  	 *
	 * 	If we block, we will return through thread_exception_return.
	 *	
 	 *	This routine clobbers r2-r29.
	 * 
	 */
	ld	r3, r30, REG_OFF(EF_EPSR)
	bb1	PSR_MODE_LOG, r3, 1f
	ldcr	r3, SR1
	mak	r3, r3, FLAG_CPU_FIELD_WIDTH <2>	; r3 = cpu#
	or.u	r3, r3, hi16(_need_ast)
	ld	r4, r3, lo16(_need_ast)			; r4 now need_ast[cpu#]
	bcnd	eq0, r4, 1f
	/* preserve r1, r30 */
	subu	r31, r31, 40
	st	r1,  r31, 32
	bsr.n	_ast_taken				; no arguments
	st	r30, r31, 36
	/* turn interrupts back off */
	ldcr	r1, PSR				     ; get current PSR
	set	r1, r1, 1<PSR_IND_LOG> 		     ; set for disable intr.
	stcr	r1, PSR				     ; install new PSR
	FLUSH_PIPELINE
	/* restore register state */
	ld	r30, r31, 36
	ld	r1, r31, 32
	br.n	ast_check	                     ; check again
	addu	r31, r31, 40
1:			
	/* no ast. Return back to caller */
	jmp	r1
