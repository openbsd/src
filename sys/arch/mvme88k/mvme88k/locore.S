/*	$OpenBSD: locore.S,v 1.7 1999/02/09 06:36:28 smurph Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
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

#ifndef ASSEMBLER
#define ASSEMBLER
#endif

#include "assym.s"
#include "machine/asm.h"
#include "machine/trap.h"
#include "machine/m88100.h"
#include "machine/psl.h"
#include "machine/vmparam.h"      /* INTSTACK_SIZE */

/***********************************************************************/

#ifndef UADDR
#define	UADDR		0xEEE00000	/* address of u */
#endif /* UADDR */

/*
 * The memory looks like:
 *   0x00000 - 0x01000   : trap vectors
 *   0x01000 - 0x10000	 : first 64k used by BUG
 *   0x10000 == start    : Boot loader jumps here. (for now, this can
 *					handle only NMAGIC - screwy linker)
 *
 ***********************************************************************/
	text

LABEL(_kernelstart)
LABEL(_kernel_text)
LABEL(_start)
LABEL(start)
	br	_start_text
#if 0
	.align 4096		  /* VBR points to page aligned list */
    LABEL(_vector_list)	  /* references memory BELOW this line */
	#include "machine/exception_vectors.h"
	word	END_OF_VECTOR_LIST

#endif

ENTRY(doboot)
	/*
	 *	Try hitting the SRST bit in VMEchip2 to reset the system.
	 */
	or.u	r3,r0, 0xfff4
	ld	r4,r3, 0x0060		/* read offset (LCSR +0x60) */
	set	r4,r4,1<23>		/* set SYSRST bit - bit 23 */	
	st	r4,r3, 0x0060		/* and store it back  */

	/*
	 *	We will be here if the reset above failed. In this case,
	 *	we will try to return to bug.
	 *
	 *	Switch to interrupt stack and call __doboot to take care
	 *	going to BUG. Need to do this since __doboot turns off the
	 *	the MMU and we need to be on a 1-to-1 mapped stack so that
	 *	further calls don't get data access exceptions.
	 */

	/* Should we use idle_u instead? XXX nivas */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

	bsr	__doboot
	/*NOTREACHED*/

/**************************************************************************/
LABEL(_start_text)	/* This is the *real* start upon poweron or reset */
#ifdef OLD_BOOT_LOADER
	/*
	 * Args passed by boot loader
	 * 	r2 howto
	 *	r3 first_addr (first available address)
	 *	r4 ((Clun << 8) ; Dlun & FF) -> bootdev
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
#else /* OLD_BOOT_LOADER */
	/*
	 * Args passed by boot loader
	 * 	r2 howto
	 *	r3 boot controler address
	 *	r4 esym
	 *	r5 start of mini
	 *	r6 end miniroot
	 */
	or.u	r13, r0,  hi16(_boothowto)
	st	r2,  r13, lo16(_boothowto)
	or.u	r13, r0,  hi16(_bootaddr)
	st	r3,  r13, lo16(_bootaddr)
	or.u	r13, r0,  hi16(_first_addr)
	st	r4,  r13, lo16(_first_addr)
	or.u	r13, r0,  hi16(_esym)
	st	r4,  r13, lo16(_esym)
	or.u	r13, r0,  hi16(_miniroot)
	st	r5,  r13, lo16(_miniroot)
#endif /* OLD_BOOT_LOADER */
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
#if 0
	stcr	r0, SR0		/* clear "current thread" */
	stcr	r0, SR1		/* clear the CPU flags */
#define PSR_SHADOW_FREEZE_BIT		 0
#define PSR_INTERRUPT_DISABLE_BIT	 1
#define PSR_FPU_DISABLE_BIT		 3
#define PSR_BIG_ENDIAN_MODE		30
#define PSR_SUPERVISOR_MODE_BIT		31
	set     r11, r11, 1<PSR_SHADOW_FREEZE_BIT>
	set     r11, r11, 1<PSR_FPU_DISABLE_BIT>
#endif
	set	r11, r0,  1<PSR_SUPERVISOR_MODE_BIT>
	set	r11, r11, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r11, PSR
        FLUSH_PIPELINE

#if 0
	or.u	r11, r0,  hi16(_vector_table)
	or	r11, r11, lo16(_vector_table)
	stcr	r11, VBR
#else
	stcr	r0, VBR
#endif /* 0 */

	/*
	 * Switch to interrupt stack
	 * Use idle_u's stack instead?
	 */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

	/*
	 * Want to make the call:
	 * 	vector_init(VBR, vector_list)
	 */
	or.u	r3, r0, hi16(_vector_list)
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
	addu	r31, r31, USIZE - 8

	/*
	 * Block clock interrupts for now. There is a problem with
	 * clock interrupts when the first clock interrupt is received.
	 * Hardclock() sees the base priority to be 0 and drops IPL to
	 * splsofclock() before calling softclock(). This opens up other
	 * clock interrupts to be received before the first one is ever
	 * finished. Also, the first entry on calltodo list is stuck for
	 * ever. As a work around, I will set the IPL to softclock so
	 * that the CLKF_BASEPRI() check in hardclock() will return false.
	 * XXX nivas
	 */
	 
#if XXX
	bsr.n	_setipl
	or	r2, r0, IPL_SOFTCLOCK
	bsr	_enable_interrupt
	bsr.n	_setipl
	or	r2, r0, IPL_HIGH
#endif
	/* make the call: main() */
	or.u	r2, r0, hi16(UADDR)
	or	r2, r2,lo16(UADDR)
	addu	r2, r2, USIZE - 8
	subu	r31, r31, 40
	bsr	_main
	addu	r31, r31, 40
	bsr	_panic

/*****************************************************************************/

	data
	.align 4096		  /* VBR points to page aligned list */
LABEL(_vector_list)		  /* references memory BELOW this line */
#include "machine/exception_vectors.h"
	word	END_OF_VECTOR_LIST

	.align 4096		  /* Vector table is a page aligned list */
LABEL(_vector_table)		  /* new vector table location, was addr 0 */
	space	(0x1000)	/* 16K */
	global _ret_addr
_ret_addr:
	word 0
	global _msgsw
_msgsw:
	word 0 			 /* Bits here turn on/off debugging somewhere */
	.align 4096
	global	_intstack
	global	_intstack_end
_intstack:
	space	(4 * NBPG)	/* 16K */
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
	.align 4096
	global	_idle_u
_idle_u:
	space	UPAGES * NBPG

/*
 * Process 0's u.
 *
 * This must be page aligned
 */
	.align	4096
	global	_u0
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

	global	_intiobase, _intiolimit, _extiobase
_intiobase:
	word	0		| KVA of base of internal IO space
_intiolimit:
	word	0		| KVA of end of internal IO space
_extiobase:
	word	0		| KVA of base of external IO space

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
 * other parts expect XXX nivas
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
	ld	r3, r31, 4	/* siginfo_t* */
	ld	r4, r31, 8	/* sigcontext* */
	ld	r5, r31, 12	/* handler */
	jsr.n	r5
	subu	r31, r31, 40	/* give some stack space */
	addu	r31, r31, 40	/* restore old sp value  */
	ld	r2,  r31, 8	/* sigcontext* */
	or	r13,  r0, SYS_sigreturn
	tb0	0, r0, 128	/* syscall trap, calling sigreturn */
	or	r0, r0, 0
	or	r0, r0, 0
	/* sigreturn will not return unless it fails */
	or	r13, r0, SYS_exit
	tb0	0, r0, 128	/* syscall trap, exit */
	or	r0, r0, 0
	or	r0, r0, 0
	global	_esigcode
_esigcode:
