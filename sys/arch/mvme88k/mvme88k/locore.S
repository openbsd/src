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

/* $RCSfile: locore.S,v $ --  asm boot routines
 *
 **********************************************************************
 *****************************************************************RCS**/

#ifndef ASSEMBLER /* predefined by ascpp, at least */
#define ASSEMBLER
#endif

#include "machine/locore.h"
#include "machine/m88100.h"
#include "machine/trap.h"
#include "machine/asm.h"
#include "machine/board.h"	/* lots of stuff (OBIO_PIO*, SYSV_BASE, etc)*/
#include "machine/vmparam.h"      /* INTSTACK_SIZE */
#include "assym.s"

/***********************************************************************/

/*
 *  Arrange for the include file version number to appear directly in
 *  the namelist.
 */
global	_INCLUDE_VERSION
def	_INCLUDE_VERSION, INCLUDE_VERSION
#ifndef NBPG
#define	NBPG 4096
#endif /* NBPG */

#ifndef UADDR
#define	UADDR		0xFFEE0000	/* address of u */
#endif /* UADDR */
#ifndef USIZE
#define USIZE (UPAGES * NBPG) 
#endif /* USIZE */
/*
 * The memory looks like:
 *   0x00000 - 0x01000   : trap vectors
 *   0x01000 - 0x10000	 : first 64k used by BUG
 *   0x10000 == start    : Boot loader jumps here. (for now, this can
 *					handle only NMAGIC - screwy linker)
 *
 ***********************************************************************/
	text

LABEL(_kernel_text)
LABEL(_kernelstart)
LABEL(_start)
LABEL(start)
	br	_start_text
#if 0
	.align 4096		  ; VBR points to page aligned list
    _LABEL(vector_list)	  /* references memory BELOW this line */
	#include "machine/exception_vectors.h"
	word	END_OF_VECTOR_LIST

    _LABEL(_msgsw)
	word 0 /* Bits here turn on/off debugging somewhere. */
#endif
/*
 * Do a dump. Called by auto-restart.
 */

	global	_dumpsys
LABEL(_doadump)
	bsr	_dumpsys
	bsr	_doboot	
	/*NOTREACHED*/

/**************************************************************************/
LABEL(_start_text)	/* This is the *real* start upon poweron or reset */
	/*
	 * Args passed by boot loader
	 * 	r2 howto
	 *	r3 first_addr (first available address)
	 *	r4 ((Clun << 8) | Dlun & FF) -> bootdev
	 *	r5 esym
	 *	r6 miniroot
	 */
	or.u	r13, r0,  hi16(_boothowto)
	st	r2,  r13, lo16(_boothowto)
	or.u	r13, r0,  hi16(_first_addr)
	st	r3,  r13, lo16(_first_addr)
#if 0
	or.u	r13, r0,  hi16(_bootdev)
	st	r4,  r13, lo16(_bootdev)
#endif
	or.u	r13, r0,  hi16(_esym)
	st	r5,  r13, lo16(_esym)
	or.u	r13, r0,  hi16(_miniroot)
	st	r6,  r13, lo16(_miniroot)

	/*
	 * CPU Initialization
	 *
	 * Every CPU starts from here..
	 * (well, from 'start' above, which just jumps here).
	 *
	 * I use r11 and r22 here 'cause they're easy to not
	 * get mixed up -- r10, for example, looks too similar
	 * to r0 when not being careful....
	 *
	 * Ensure that the PSR is as we like:
	 *	supervisor mode
	 *	big-endian byte ordering
	 *	concurrent operation allowed
	 *	carry bit clear (I don't think we really care about this)
	 *	FPU enabled
	 *	misaligned access raises an exception
	 *	interrupts disabled
	 *	shadow registers frozen
	 *
	 * The manual says not to disable interrupts and freeze shadowing
	 * at the same time because interupts are not actually disabled
	 * until after the next instruction. Well, if an interrupt
	 * occurs now, we're in deep   anyway, so I'm going to do
	 * the two together.
	 *
	 * Upon a reset (or poweron, I guess), the PSR indicates:
	 *   supervisor mode
	 *   interrupts, shadowing, FPU, missaligned exception: all disabled
	 *
	 * We'll just construct our own turning on what we want.
	 *
	 *	jfriedl@omron.co.jp
	 */
	stcr	r0, SSBR	/* clear this for later */

	/* XXX We can use SR0-SR3 for any purpose */
	set	r11, r0,  1<PSR_SUPERVISOR_MODE_BIT>
	set	r11, r11, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r11, PSR
	/* shadowing, FPU, misalgined access exception: all enabled now.*/
#if 0
	or.u	r11, r0,  hi16(_vector_list)
	or	r11, r11, lo16(_vector_list)
	stcr	r11, VBR
#endif /* 0 */
	stcr	r0, VBR

/************************************************************************/

#if defined(RAW_PRINTF) && RAW_PRINTF
	bsr  replace_mayput_with_rawputchar
#endif

	/*
	 * switch to interrupt stack
	 */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

	/*
	 * Want to make the call:
	 * 	vector_init(VBR, vector_list)
	 */
	or.u	r3, r0,  hi16(_vector_list)
	or	r3, r3, lo16(_vector_list)
	bsr.n	_vector_init
	ldcr	r2, VBR		
	
#if 0
	/* clear BSS. Boot loader might have already done this... */
	or.u	r2, r0, hi16(_edata)
	or	r2, r2, lo16(_edata)
	or.u	r4, r0, hi16(_end)
	or	r4, r4, lo16(_end)
	bsr.n	_bzero		/* bzero(edata, end-edata) */
	subu	r3, r4, r2
#endif

	/* still on int stack */
	bsr.n	_m187_bootstrap
	subu	r31, r31, 40
	addu	r31, r31, 40
	
	/* switch to proc0 uarea */
	
	or.u	r10, r0, hi16(UADDR)
	or	r31, r10,lo16(UADDR)
	addu	r31, r31, USIZE

	/* make the call: main() */
	bsr.n	_main
	subu	r31, r31, 40
	addu	r31, r31, 40
	br	_return_from_main

/*****************************************************************************/

	data
	.align 4096		  ; VBR points to page aligned list
	global _vector_list
_vector_list:			  ; references memory BELOW this line
	#include "machine/exception_vectors.h"
	word	END_OF_VECTOR_LIST

	global _msgsw
_msgsw:
	word 0 			 ;Bits here turn on/off debugging somewhere.
	.align 4096
	global	_intstack
	global	_intstack_end
_intstack:
	space	4 * NBPG	/* 16K */
_intstack_end:

/*
 * When a process exits and its u. area goes away, we set curpcb to point
 * to this `u.', leaving us with something to use for an interrupt stack,
 * and letting all the register save code have a pcb_uw to examine.
 * This is also carefully arranged (to come just before u0, so that
 * process 0's kernel stack can quietly overrun into it during bootup, if
 * we feel like doing that).
 * Should be page aligned.
 */
	global	_idle_u
_idle_u:
	space	UPAGES * NBPG

/*
 * Process 0's u.
 *
 * This must be page aligned
 */
	global	_u0
	align	4096
_u0:	space	UPAGES * NBPG
estack0:

/*
 * UPAGES get mapped to kstack
 */

	global	_kstack
_kstack:
	word	UADDR

#ifdef DDB
	global	_esym
_esym:
	word 	0
#endif /* DDB */

	global	_proc0paddr	/* move to C code */
_proc0paddr:
	word	_u0		/*  KVA of proc0 uarea */

/*
 * _curpcb points to the current pcb (and hence u. area).
 * Initially this is the special one.
 */
/*
 * pcb is composed of kernel state + user state
 * I may have to change curpcb to u0 + PCB_USER based on what
 * other parts expect XXX
 */
	global	_curpcb	/* move to C code */
_curpcb:	word	_u0	/* curpcb = &u0 */

/*
 * Trampoline code. Gets copied to the top of
 * user stack in exec.
 */
	global	_sigcode
_sigcode:
				/* r31 points to sigframe */
	ld	r2, r31, 0	/* signo */
	ld	r3, r31, 4	/* code */
	ld	r4, r31, 8	/* sigcontext* */
	or	r5, r0, 0	/* addr = 0 for now */
	ld	r6, r31, 12	/* handler */
	jsr.n	r6
	addu	r31, r31, 40
	subu	r31, r31, 40
	ld	r2,  r31, 8	/* sigcontext* */
	or	r9,  r0, SYS_sigreturn
	tb0	0, r0, 128	/* syscall trap, calling sigreturn */
	or	r0, r0, 0
	or	r0, r0, 0
	/* sigreturn will not return unless it fails */
	or	r9, r0, SYS_exit
	tb0	0, r0, 128	/* syscall trap, exit */
	or	r0, r0, 0
	or	r0, r0, 0
	global	_esigcode
_esigcode:

#if 0
/*
 *	thread_bootstrap:
 *
 *	Bootstrap a new thread using the thread state that has been
 *	placed on the stack.  Our fp has been set up for us, we only need
 *	to fix up a few things in the saved frame, then get into
 *	usermode.
 */
ENTRY(thread_bootstrap)
        /* 
	 * Here r31 should point to the place on our stack which
	 * contains a pointer to our exception frame.
	 */	
#if DDB
        ENTRY_ASM
#endif
	br      return_from_exception_handler

/*
 *	save_context
 */
ENTRY(save_context)
	subu	r31,r31,40		/* allocate stack for r1 and args */
	st	r1,r31,36		/* save return address */
	bsr	_spl			/* get the current interrupt mask */
	ld	r1,r31,36		/* recover return address */
	addu	r31,r31,40		/* put stack pointer back */
	ldcr	r10,SR0			/* r10 <- current_thread() */
	ld	r10,r10,THREAD_PCB	/* r10 <- pcb */
#if (PCB_KERNEL!=0)
     	addu	r10, r10, PCB_KERNEL    /* point to kernel save region */
#endif
	st	r1,r10,0	/* do setjmp */ /* save return address */
	st	r14,r10,4
	st	r15,r10,2*4
	st	r16,r10,3*4
	st	r17,r10,4*4
	st	r18,r10,5*4
	st	r19,r10,6*4
	st	r20,r10,7*4
	st	r21,r10,8*4
	st	r22,r10,9*4
	st	r23,r10,10*4
	st	r24,r10,11*4
	st	r25,r10,12*4
        /* In principle, registers 26-29 are never manipulated in the
           kernel. Maybe we can skip saving them? */
	st	r26,r10,13*4
	st	r27,r10,14*4
	st	r28,r10,15*4
	st	r29,r10,16*4
	st	r30,r10,17*4		/* save frame pointer */
	st	r31,r10,18*4		/* save stack pointer */
	st	r2,r10,19*4		/* save interrupt mask */
       /* we need to switch to the interrupt stack here */
	or.u	r31, r0, hi16(_intstack)
	or	r31, r31, lo16(_intstack)
        addu    r31, r31, INTSTACK_SIZE             /* end of stack */
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */
	jmp.n	r1
	or	r2,r0,r0
#endif /* 0 */

/* ------------------------------------------------------------------------ */
/*
 * unsigned measure_pause(volatile int *flag)
 *
 * Count cycles executed until *flag becomes nonzero.
 * Return the number of cycles counted.
 */
ENTRY(measure_pause)
	/* R2 is pointer to flag */
	def GRANULAIRTY, 10000

	or	r3, r0, 1	/* r3 is my counter, this is the first */

    measure_pause_outer_loop:
	or	r4, r0, GRANULAIRTY

    measure_pause_inner_loop:
	/*
	 * Execute a tight loop of a known number of cycles.
	 * This assumes, of course, that the instruction cache is on.
	 * This loop takes two cycles per iteration.
	 */
	bcnd.n	ne0, r4, measure_pause_inner_loop
	subu	r4, r4, 1


	/*
	 * Now add the number of cycles done above (plus the overhead
	 * of the outer loop) to the total count.
	 * Also, check the *flag and exit the outer loop if it's non-zero.
	 *
	 * The overhead is really unknown because it's not known how
	 * the memory system will tread the access to *flag, so we just
	 * take a guess.
	 */
	ld	r4, r2, r0			/* get the flag */
	addu	r3, r3, (GRANULAIRTY * 2 + 10)	/* account for the cost */
	bcnd	eq0, r4, measure_pause_outer_loop /* continue or exit the loop*/

	jmp.n	r1
	or	r2, r3, r0	/* pass count back */

/*
 * void delay_in_microseconds(int count)
 *
 * The processor loops (busy waits) for the given number of microseconds:
 * Thus, delay_in_microseconds(1000000) will delay for one second.
 *
 * REGISTER USAGE:
 *  IN  r1   - return address
 *  IN  r2   - (signed int) number of microseconds
 *      r3   - (float) number of microseconds
 *      r4/5 - (double) number of cycles per microsecond
 *      r6   - (float) number of cycles to delay
 *	r7   - (signed) number of cycles to delay
 */
ENTRY(delay_in_microseconds)
ENTRY(delay)
	flt.ss	 r3, r2	    /* convert microseconds from signed int to float */
	or.u	 r4, r0, hi16(_cycles_per_microsecond)
	ld.d	 r4, r4, lo16(_cycles_per_microsecond)
	fmul.ssd r6, r3, r4 /* convert microseconds to cycles */
	int.ss   r7, r6	    /* convert cycles from float to signed int */
	subu	 r7, r7, 25 /* subtract for overhead of above instruction */

	/* now loop for the given number of cycles */
    pause_loop:
	bcnd.n	gt0, r7, pause_loop
	subu	r7, r7, 2	/* two cycles per iteration */

	jmp	r1 /* return */

#if 0
/*
 * void switch_to_shutdown_context(thread_t thread,
 *				   void (*routine)(processor_t),
 *				   processor_t processor)
 *
 * saves the kernel context of the thread,
 * switches to the interrupt stack,
 * continues the thread (with thread_dispatch),
 * then runs routine on the interrupt stack.
 *
 */
  
ENTRY(switch_to_shutdown_context)
/* call save_context to save the thread state */
     	subu 	r31, r31, 40
	or      r25, r3, r0    /* save arguments */
	or	r24, r4, r0     
     	bsr.n 	_save_context
	st	r1, r31, 36
     	addu 	r31, r31, 40
	ldcr	r10, SR0			/* r10 <- current_thread() */
	st      r31, r10, THREAD_KERNEL_STACK   /* save stack pointer */      
	st	r0,  r10, THREAD_SWAP_FUNC      /* null continuation */
        ldcr	r11, SR1
	mak	r11, r11, FLAG_CPU_FIELD_WIDTH<0> /* r1 = cpu # */
	or	r12, r12, lo16(_interrupt_stack)
	ld	r31, r12 [r11]
        addu    r31, r31, INTSTACK_SIZE             /* end of stack */
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */
        /* save the thread; switched to the interrupt stack; now call thread
	   dispatch to get rid of this thread */
	or	r2, r10, r0      
	bsr.n	_thread_dispatch
     	subu 	r31, r31, 40
  	/* call the continuation routine */
  	jsr.n	r25
	or	r2, r24, r0
        /* panic if here */
        or.u	r2, r0, hi16(1f)
	bsr.n	_panic
	or	r2, r2, lo16(1f)
1:
  	string "switch_to_shutdown_context"
#endif /* 0 */
