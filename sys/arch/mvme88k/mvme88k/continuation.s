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
 * Assembler continuation support routines.
 */
/*
 * HISTORY
 * $Log: continuation.s,v $
 * Revision 1.1  1995/10/18 12:32:20  deraadt
 * moved from m88k directory
 *
 * Revision 1.1.1.1  1995/10/18 10:54:27  deraadt
 * initial 88k import; code by nivas and based on mach luna88k
 *
 * Revision 2.7  93/01/26  18:00:29  danner
 * 	changed ;comments to C-style for cpp.
 * 	[93/01/25            jfriedl]
 * 
 * Revision 2.6  93/01/14  17:53:09  danner
 * 	Enhanced debugger support for continuations.
 * 	[92/12/02            jfriedl]
 * 
 * Revision 2.5  92/08/03  17:51:54  jfriedl
 * 	Adjusted references from luna88k/locore --> luna88k
 * 	[92/07/24            jfriedl]
 * 
 * Revision 2.4.1.1  92/05/27  14:48:42  danner
 * 	Updated includes.
 * 	PSR_INTERRUPT_DISABLE_BIT -> PSR_IND_LOG
 * 
 * 
 * Revision 2.4  92/05/04  11:27:58  danner
 * 	Support for gcc 2.x's moptimize-arg-area switch. Simplify
 * 	 Switch_context.
 * 	[92/05/03            danner]
 * 	Performed instruction reordering in Switch_context suggested by
 * 	jfriedl.
 * 	[92/04/26            danner]
 * 	[92/04/12  16:24:48  danner]
 * 
 *	Thread_syscall_return now stores r2 into the pcb. This cannot be
 *	avoided due to asts.
 * 	[92/04/12            danner]
 * 
 * Revision 2.3  92/03/03  15:38:44  rpd
 * 	Save continuation argument as old_thread->swap_func in
 * 	 Switch_context.
 * 	[92/03/02            danner]
 * 
 * 	Added missing stcr in interrupt disabling code.
 * 	[92/03/02            danner]
 * 
 * Revision 2.2  92/02/18  18:03:27  elf
 * 	Created. 
 * 	[92/02/01            danner]
 * 
 */
#ifndef ASSEMBLER /* predefined by ascpp, at least */
#define ASSEMBLER /* this is required for some of the include files */
#endif

#include <assym.s>   	  		/* for PCB_KSP, etc */
#include <machine/asm.h>
#include <motorola/m88k/m88100/m88100.h>
#include <motorola/m88k/m88100/psl.h> 
#include <mach/machine/vm_param.h>
#include <mach_kdb.h>

/*
 * Jump out into user space for the first time.
 * No ast check. Reload registers from continuation,
 * the jump out.
 */
ENTRY(thread_bootstrap_return)
/*
 * Jump out to user space from an exception. Restore
 * all registers.
 * 
 */
ENTRY(thread_exception_return)
	ldcr	r30, SR0		/* get current thread pointer */
	ld	r30, r30, THREAD_PCB	/* get the pcb pointer */
	br.n	_return_from_exception
	addu	r30, r30, PCB_USER	/* point to exception frame */
	
/*
 *
 * Return to user space from a system call. 
 * The value in r2 is the return value, and should be
 * preserved. The other argument registers (r3-r9), as well as
 * the temporary registers (r10-r13) need not be restored.
 * R2 is saved into the pcb in case we get blocked by an ast.
 */
ENTRY(thread_syscall_return)
	ldcr	r30, SR0		/* get current thread pointer */
	ld	r30, r30, THREAD_PCB	/* get the pcb pointer */
	addu	r30, r30, PCB_USER	/* point to exception frame */
	br.n	_return_from_syscall
	st	r2,  r30, GENREG_OFF(2) /* save r2 */

	
/*
 * Call continuation - call the function specified (r2) with no
 * arguments. Reset the stack point to the top of stack first.
 * On the 88k, we leave the top 2 words of the stack availible
 * to hold a pointer to the user exception frame.
 */
ENTRY(call_continuation)
     /* reset the stack pointer to the top of stack. Since stacks
	grow down, this can be accomplished by rounding up the sp
	to the nearest KERNEL_STACK_SIZE quanta. We do this
	carefully to make sure we have a valid stack pointer at 
	all times (in case we take an interrupt).
	32 bytes is also subtracted from the stack pointer to
	allow compilation with gcc 2.x's -moptimize-arg-area
	option
      */
	or	r3,  r0,  KERNEL_STACK_SIZE-1     
     	addu	r30, r31, r3		         /* nsp += KSS-1 */
	and.c	r30, r30, r3			 /* nsp &= ~(KSS-1) */
#if MACH_KDB
	or	r1, r1, 1		/* mark "continuation" return */
#endif
	jmp.n	r2				 /* call continuation */
	subu	r31, r30, (8+32)		 /* sp  = nsp-8 */

/*
 * Assembler support for switch context. The address space switch
 * has already occured. 
 *
 * On entry
 *	r2 - old thread (current_thread)
 *      r3 - continuation for old thread
 *      r4 - new thread 
 *	r5 - &(old->pcb->kernel_state)
 *	r6 - &(new->pcb->kernel_state)
 *
 */
ENTRY(Switch_context)
	/* 
	 *   if a nonnull continuation, we can skip saving the
	 *   current thread state 
         */
	bcnd	ne0, r3,  1f	/* non null continuation */
	/* null continuation; need to save registers */
	or	r11, r0, r5 
	/* save the relevant registers; r1, r14-r31 */
	st	r1, r11,0
	st	r14,r11,4
	st	r15,r11,2*4
	st	r16,r11,3*4
	st	r17,r11,4*4
	st	r18,r11,5*4
	st	r19,r11,6*4
	st	r20,r11,7*4
	st	r21,r11,8*4
	st	r22,r11,9*4
	st	r23,r11,10*4
	st	r24,r11,11*4
	st	r25,r11,12*4
        /* In principle, registers 26-29 are never manipulated in the
           kernel. Maybe we can skip saving them? */
	st	r26,r11,13*4
	st	r27,r11,14*4
	st	r28,r11,15*4
	st	r29,r11,16*4
	st	r30,r11,17*4		/* save frame pointer */
	st	r31,r11,18*4		/* save stack pointer */
    1:
	/* 
	   Saved incoming thread registers, if necessary. 
	   Reload new thread registers
	 */
	/* get pointer to new pcb */
	or	r11, r0, r6
	/* switch stacks */
	ld	r31,r11,18*4

	/*
		current_thread, active_threads and active_stacks have
		all been updated in switch_context. We just switched
		onto this threads stack, so all state is now consistent
		again. Hence its safe to turn interrupts back on */
		
	/* reenable interrupts */
	ldcr	r10, PSR
	clr	r10, r10, 1<PSR_IND_LOG> 
	stcr	r10, PSR
        FLUSH_PIPELINE		
       
        /* restore registers */
       	ld	r1, r11,0
	ld	r14,r11,4
	ld	r15,r11,2*4
	ld	r16,r11,3*4
	ld	r17,r11,4*4
	ld	r18,r11,5*4
	ld	r19,r11,6*4
	ld	r20,r11,7*4
	ld	r21,r11,8*4
	ld	r22,r11,9*4
	ld	r23,r11,10*4
	ld	r24,r11,11*4
	ld	r25,r11,12*4
	ld	r26,r11,13*4
	ld	r27,r11,14*4
	ld	r28,r11,15*4
	ld	r29,r11,16*4
	/* make the call - r2 is still old thread, which
	 * makes it the return value/first argument
	 * Sometimes this call will be actually be a return
	 * up to switch_context, and sometimes it will be
	 * an actual call to a function. Stare at Figure 4
	 * of Draves, et al.  SOSP paper for a few hours to really
	 * understand....
	 */
       	jmp.n	r1
	ld	r30,r11,17*4
