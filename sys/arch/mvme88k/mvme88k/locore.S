/*	$OpenBSD: locore.S,v 1.26 2003/01/14 03:20:16 miod Exp $	*/
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

#include "assym.h"
#include "ksyms.h"

#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/m88100.h>
#include <machine/psl.h>
#include <machine/param.h>
#include <machine/vmparam.h>      /* INTSTACK_SIZE */

/***********************************************************************/

/*
 * The memory looks like:
 *   0x00000 - 0x01000   : trap vectors
 *   0x01000 - 0x10000	 : first 64k used by BUG
 *   0x10000 == start    : Boot loader jumps here. (for now, this can
 *					handle only NMAGIC - screwy linker)
 *
 ***********************************************************************/
	text

GLOBAL(kernelstart)
GLOBAL(kernel_text)
GLOBAL(start)
ASGLOBAL(start)
	br	_C_LABEL(start_text)
	br	_C_LABEL(start_text)
	br	_C_LABEL(start_text)
	br	_C_LABEL(start_text)

GLOBAL(doboot)
	/*
	 *	Try hitting the SRST bit in VMEchip2 to reset the system.
	 */
#ifdef MVME188
	/* check if it's a mvme188 */
	or.u	r4, r0, hi16(_C_LABEL(brdtyp))
	ld	r3, r4, lo16(_C_LABEL(brdtyp))
	cmp	r4, r3, BRD_188
	bb1	ne, r4, 1f
	bsr	_C_LABEL(m188_reset)
	br	m188_doboot_fail
#endif	/* MVME188 */
1:	
	or.u	r3,r0, 0xfff4
	ld	r4,r3, 0x0060		/* read offset (LCSR +0x60) */
	set	r4,r4,1<23>		/* set SYSRST bit - bit 23 */	
	st	r4,r3, 0x0060		/* and store it back  */

	/*
	 *	We will be here if the reset above failed. In this case,
	 *	we will try to return to bug.
	 *
	 *	Switch to interrupt stack and call _doboot to take care
	 *	going to BUG. Need to do this since _doboot turns off the
	 *	the MMU and we need to be on a 1-to-1 mapped stack so that
	 *	further calls don't get data access exceptions.
	 */

	/* Should we use idle_u instead? XXX nivas */
m188_doboot_fail:
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

	bsr	_C_LABEL(_doboot)
	/*NOTREACHED*/

/**************************************************************************/
GLOBAL(start_text)	/* This is the *real* start upon poweron or reset */
	/*
	 * Args passed by boot loader
	 * 	r2 howto
	 *	r3 boot controler address
	 *	r4 esym
	 *	r5 start of mini
	 *	r6 end miniroot
	 *	r7 ((Clun << 8) ; Dlun & FF) -> bootdev
	 *	r8 cpu type (0x187, 0x188, 0x197)
	 */
/*
 *	(*entry)(flag, bugargs.ctrl_addr, cp, kernel.smini, 
 *               kernel.emini, bootdev, brdtyp); 
 */ 
	or.u	r13, r0,  hi16(_boothowto)
	st	r2,  r13, lo16(_boothowto)
	or.u	r13, r0,  hi16(_bootaddr)
	st	r3,  r13, lo16(_bootaddr)
	or.u	r13, r0,  hi16(_first_addr)
	st	r4,  r13, lo16(_first_addr)
#if defined(DDB) || NKSYMS > 0
	or.u	r13, r0,  hi16(_esym)
	st	r4,  r13, lo16(_esym)
#endif
	or.u	r13, r0,  hi16(_bootdev)
	st	r7,  r13, lo16(_bootdev)
	or.u	r13, r0,  hi16(_brdtyp)
	st	r8,  r13, lo16(_brdtyp)
	
	/* set _cputyp */
	cmp     r2,  r8, BRD_197	/* r8 contains brdtyp */
        bb1     ne, r2, 1f	/* if it's a '197, CPU is 88110 */ 
	or.u    r13, r0,  hi16(CPU_88110) 
	or	r8,  r13, lo16(CPU_88110)/* r8 contains 0x110 */
	br	2f
1:	
	or.u    r13, r0,  hi16(CPU_88100) 
	or	r8,  r13, lo16(CPU_88100)/* r8 contains 0x100 */
2:	
	or.u	r13, r0,  hi16(_cputyp)
	st	r8,  r13, lo16(_cputyp) /* r8 contains cputyp */
	
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
        
	cmp     r2, r8, CPU_88110 /* r8 contains cputyp */
        bb1     eq, r2, 1f      /* if it's a 'mc88110, skip SSBR */
	stcr	r0, SSBR	/* clear this for later */
1:
	stcr	r0, SR0		/* clear "current thread" */
	stcr	r0, SR1		/* clear the CPU flags */
	
	set	r11, r0,  1<PSR_SUPERVISOR_MODE_BIT>
	set	r11, r11, 1<PSR_INTERRUPT_DISABLE_BIT>
	set	r11, r11, 1<PSR_GRAPHICS_DISABLE_BIT>
/*	set	r11, r11, 1<PSR_SERIAL_MODE_BIT> */
	set	r11, r11, 1<PSR_SERIALIZE_BIT>
	stcr	r11, PSR
	stcr	r0,  VBR     /* set Vector Base Register to 0, ALWAYS! */
	FLUSH_PIPELINE
        cmp     r2, r8, CPU_88110 /* r8 contains cputyp */
	bb1	eq, r2, master_start	/* if it's a '197, skip to master_start */
	
#if 0
	/* clear BSS. Boot loader might have already done this... */
	or.u	r2, r0, hi16(_edata)
	or	r2, r2, lo16(_edata)
	or.u	r4, r0, hi16(_end)
	or	r4, r4, lo16(_end)
	bsr.n	_bzero		/* bzero(edata, end-edata) */
	subu	r3, r4, r2
#endif
	/*
	 * First time to count how many CPUs to attach
	 */
	or.u	r11, r0,  hi16(initialized_cpu_lock)
	or	r11, r11, lo16(initialized_cpu_lock)
ASGLOBAL(check_init_lock)
	FLUSH_PIPELINE
	or	r22, r0,  1
	xmem    r22, r11, r0	  	/* If r22 gets 0, we have the lock.. */
	bcnd	eq0, r22, have_init_lock/* ..but if not, we must wait */

ASGLOBAL(wait_for_init_lock)
	/* just watch the lock until it looks clear */
	ld	r22, r11, r0
	bcnd	eq0, r22, check_init_lock
	br	wait_for_init_lock	/* looks clear -- try to grab */

ASGLOBAL(have_init_lock)
	FLUSH_PIPELINE
	or.u	r11, r0,  hi16(_initialized_cpus)
	ld	r22, r11, lo16(_initialized_cpus)
	add	r23, r22, 1
	st	r23, r11, lo16(_initialized_cpus)

	or.u	r11, r0,  hi16(initialized_cpu_lock)
	st	r0,  r11, lo16(initialized_cpu_lock)
	/*
 	 * Now we vie with any other processors to see who's the master.
 	 * We first try to obtain a lock to see who's allowed
	 * to check/set the master lock.
	 */
	or.u	r11, r0,  hi16(_inter_processor_lock)
	or	r11, r11, lo16(_inter_processor_lock)
ASGLOBAL(check_ip_lock)
	FLUSH_PIPELINE
	or	r22, r0,  1
	xmem	r22, r11, r0            /* If r22 gets 0, we have the lock.. */ 
	bcnd	eq0, r22, have_ip_lock  /* ..but if not, we must wait */
ASGLOBAL(wait_for_ip_lock)
	/* just watch the lock until it looks clear */
	ld	r22, r11, r0
	bcnd	ne0, r22, wait_for_ip_lock
	/* since we can be here with caches off, add a few nops to 
	keep the bus from getting overloaded */
	or	r2, r0, lo16(1000)
ASGLOBAL(ip_loop)
	subu	r2, r2, 1
	bcnd	eq0, r2, ip_loop
	br	check_ip_lock           /* looks clear -- try to grab */

ASGLOBAL(have_ip_lock)
	/* now try to grab the master_processor_chosen prize */
	FLUSH_PIPELINE
	or.u	r11, r0,  hi16(master_processor_chosen)
	or	r11, r11, lo16(master_processor_chosen)
	or	r22, r0,  1
	xmem	r22, r11, r0

	/*
	 * If r22 is not clear we're a slave,
	 * otherwise we're first and the master.
	 *
	 * Note that we haven't released the interprocessor lock....
	 * We'll do that when we're ready for another CPU to go.
	 * (if we're the master, we'll do that in master_start below.
	 *  if we're a slave, we'll do it in slave_start below).
	 */
	bcnd	ne0, r22, slave_start
	/* fall through to master start if that's appropriate */

ASGLOBAL(master_start)
	/*
	 * Switch to interrupt stack
	 * Use idle_u's stack instead?
	 */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

#ifdef M88110
	cmp	r2, r8, CPU_88110 /* r8 contains cputyp */
	bb1	ne, r2, 1f	/* if it's a 'mc88110, use different vectors */ 
	or.u	r3, r0, hi16(_m88110_vector_list)
	or	r3, r3, lo16(_m88110_vector_list)
	bsr.n	_vector_init
	ldcr	r2, VBR
	br	2f
#endif /* M88110 */
1:
#ifdef M88100
	/*
	 * Want to make the call:
	 * 	vector_init(VBR, vector_list)
	 */
	or.u	r3, r0, hi16(_vector_list)
	or	r3, r3, lo16(_vector_list)
	bsr.n	_vector_init
	ldcr	r2, VBR		
#endif /* M88100 */
2:	
	/* still on int stack */
	bsr.n	_mvme_bootstrap
	subu	r31, r31, 40
	addu	r31, r31, 40
	
	/* we now know our cpu number, so we
	 * can set interrupt_stack[cpu_number()] = _intstack
	 */
	ldcr  	r10, SR1
	extu   	r10, r10, FLAG_CPU_FIELD_WIDTH<0>    /* r10 <-- CPU# */
	
	/* figure interrupt_stack[cpu_number()] */
	or.u	r11,  r0, hi16(_interrupt_stack)  
	or	r11, r11, lo16(_interrupt_stack)
	or.u	r12, r0,  hi16(_intstack)
	or	r12, r12,  lo16(_intstack)
	st	r12, r11 [r10]

	/* switch to proc0 uarea */
	or.u	r10, r0, hi16(UADDR)
	or	r31, r10,lo16(UADDR)
	addu	r31, r31, USIZE - 8

	/* make the call: main() */
	or.u	r2, r0, hi16(UADDR)
	or	r2, r2,lo16(UADDR)
	addu	r2, r2, USIZE - 8
	subu	r31, r31, 40
	bsr	_main
	addu	r31, r31, 40
	bsr	_panic

/***********************************************************************
 *	slave CPUs starts here
 */
ASGLOBAL(slave_start)
	/* while holding the inter_processor_lock, the slave cpu
	   can find use the slavestack to call slave_pre_main and
	   determine its cpu number. After that, however, it should
	   switch over to the interrupt stack associated with its
           cpu */

	/* r31 <-- slavestack */
	or.u	r31, r0, hi16(_slavestack_end)
	or	r31, r31, lo16(_slavestack_end)
	clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

	bsr.n	_slave_pre_main /* set cpu number */
	subu	r31, r31, 48    /* allocate frame */

	bsr	_get_slave_stack
	addu	r31, r2, INTSTACK_SIZE + 4096

	/* SR1 now contains our cpu number. We can now release
	   the inter_processor_lock, as we are done with the 
	   slavestack.  We also have an interrupt stack  */

	or.u	r10, r0, hi16(_inter_processor_lock)
	st	r0, r10, lo16(_inter_processor_lock)	

	br.n	_slave_main     /* does not return */
	subu	r31, r31, 40    /* allocate frame */
	
GLOBAL(spin_cpu)
        or.u	r3,   r0,  hi16(_start_text)
	or	r3,   r3,  lo16(_start_text)
	or	r9,   r0,  0x100		/* .FORKMPU */
	tb0	0,    r0,  0x200-16		/* call 188Bug */
        jmp     r1
        
/*****************************************************************************/

	data
	.align 4096		  /* SDT (segment descriptor table */
	global _kernel_sdt
_kernel_sdt:	
	space	(0x2000)	  /* 8K - 4K phys, 4K virt*/
	global _ret_addr
_ret_addr:
	word 0
	global _msgsw
_msgsw:
	word 0 			 /* Bits here turn on/off debugging somewhere */
ASGLOBAL(initialized_cpu_lock)
   /* XMEM spin lock -- to count CPUs */
	word 0
GLOBAL(initialized_cpus)
   /* CPU counter to initialize */
	word 0
ASGLOBAL(master_processor_chosen)
   /* The first processor that XMEMs this becomes the master */
	word 0
GLOBAL(inter_processor_lock)
   /* XMEM spin lock -- controls access to master_processor_chosen */
	word 0
	
	.align 4096
	global	_intstack, _intstack_end, _slavestack, _slavestack_end
_intstack:
	space (4 * NBPG)	/* 16K, just to be safe */
_intstack_end:
_slavestack:
	space (NBPG)      /* 4K, small, interim stack */
_slavestack_end:

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

#if defined(DDB) || NKSYMS > 0
	global	_esym
_esym:
	word 	0
#endif /* DDB || NKSYMS > 0 */

	global	_intiobase, _intiolimit
_intiobase:
	word	0		| KVA of base of internal IO space
_intiolimit:
	word	0		| KVA of end of internal IO space

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

/* interrupt counters */
	global	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	string	"spur\0"
	string	"lev1\0"
	string	"lev2\0"
	string	"lev3\0"
	string	"lev4\0"
	string	"lev5\0"
	string	"lev6\0"
	string	"lev7\0"
	string	"clk\0"
	string	"sclk\0"
	string	"pclk\0"
	string	"nmi\0"
_eintrnames:
	.align	8
_intrcnt:
	word	0,0,0,0,0,0,0,0,0,0,0,0
_eintrcnt:


