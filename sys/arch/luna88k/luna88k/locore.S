/*	$OpenBSD: locore.S,v 1.11 2005/12/03 18:20:48 miod Exp $	*/
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
#include <machine/vmparam.h>

/*
 * The memory looks like:
 *   0x00000 - 0x01000	trap vectors
 *   0x01000 - 0x20000	ROM monitor work area
 *   0x20000 == start	Boot loader jumps here.
 */
	text

GLOBAL(kernelstart)
GLOBAL(kernel_text)
GLOBAL(start)
ASGLOBAL(start)
	br	_C_LABEL(start_text)
	br	_C_LABEL(start_text)
	br	_C_LABEL(start_text)
	br	_C_LABEL(start_text)

	/* This is the *real* start upon poweron or reset */
GLOBAL(start_text)
	/*
	 * XXX: OpenBSD/luna88k does not have its native boot
	 * loader, so no args are passed ...
	 * (below is the comment for mvme88k.)
	 * 
	 * Args passed by boot loader
	 * 	r2 howto
	 *	r3 boot controller address
	 *	r4 esym
	 *	r5 start of mini
	 *	r6 end miniroot
	 *	r7 ((Clun << 8) ; Dlun & FF) -> bootdev
	 *	r8 cpu type (0x187, 0x188, 0x197)
	 */
#if 0 /* not yet */
/*
 * (*entry)(flag, bugargs.ctrl_addr, cp, kernel.smini,kernel.emini,
 *  bootdev, brdtyp);
 */
	or.u	r13, r0,  hi16(_C_LABEL(boothowto))
	st	r2,  r13, lo16(_C_LABEL(boothowto))
	or.u	r13, r0,  hi16(_C_LABEL(bootaddr))
	st	r3,  r13, lo16(_C_LABEL(bootaddr))
	or.u	r13, r0,  hi16(_C_LABEL(first_addr))
	st	r4,  r13, lo16(_C_LABEL(first_addr))
#if defined(DDB) || NKSYMS > 0
	or.u	r13, r0,  hi16(_C_LABEL(esym))
	st	r4,  r13, lo16(_C_LABEL(esym))
#endif
	or.u	r13, r0,  hi16(_C_LABEL(bootdev))
	st	r7,  r13, lo16(_C_LABEL(bootdev))
	or.u	r13, r0,  hi16(_C_LABEL(brdtyp))
	st	r8,  r13, lo16(_C_LABEL(brdtyp))

	/* set cputyp */
	cmp	r2,  r8, BRD_197	/* r8 contains brdtyp */
	bb1	ne, r2, 1f	/* if it's a '197, CPU is 88110 */
	or.u	r13, r0,  hi16(CPU_88110)
	or	r8,  r13, lo16(CPU_88110)
	br	2f
1:
	or.u	r13, r0,  hi16(CPU_88100)
	or	r8,  r13, lo16(CPU_88100)
2:
	or.u	r13, r0,  hi16(_C_LABEL(cputyp))
	st	r8,  r13, lo16(_C_LABEL(cputyp))
#endif /* not yet */

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
	 * at the same time because interrupts are not actually disabled
	 * until after the next instruction. Well, if an interrupt
	 * occurs now, we're in deep trouble anyway, so I'm going to do
	 * the two together.
	 *
	 * Upon a reset (or poweron, I guess), the PSR indicates:
	 *   supervisor mode
	 *   interrupts, shadowing, FPU, misaligned exception: all disabled
	 *
	 * We'll just construct our own turning on what we want.
	 *
	 *	jfriedl@omron.co.jp
	 */

	stcr	r0, SSBR	/* clear this for later */
	stcr	r0, SR1		/* clear the CPU flags */

	set	r11, r0,  1<PSR_SUPERVISOR_MODE_BIT>
	set	r11, r11, 1<PSR_INTERRUPT_DISABLE_BIT>
	set	r11, r11, 1<PSR_GRAPHICS_DISABLE_BIT>

	stcr	r11, PSR
	FLUSH_PIPELINE
	stcr	r0,  VBR	/* set Vector Base Register to 0, ALWAYS! */
	FLUSH_PIPELINE

	/* clear BSS. Boot loader might have already done this... */
	or.u	r2, r0, hi16(_C_LABEL(edata))
	or	r2, r2, lo16(_C_LABEL(edata))
	or.u	r4, r0, hi16(_C_LABEL(end))
	or	r4, r4, lo16(_C_LABEL(end))
	bsr.n	_bzero		/* bzero(edata, end-edata) */
	 subu	r3, r4, r2

	/*
	 * First time to count how many CPUs to attach
	 */
	or.u	r11, r0,  hi16(_ASM_LABEL(initialized_cpu_lock))
	or	r11, r11, lo16(_ASM_LABEL(initialized_cpu_lock))
1:
	FLUSH_PIPELINE
	or	r22, r0,  1
	xmem	r22, r11, r0	/* If r22 gets 0, we have the lock.. */
	bcnd	eq0, r22, 3f	/* ..but if not, we must wait */
2:
	/* just watch the lock until it looks clear */
	ld	r22, r11, r0
	bcnd	eq0, r22, 1b
	br	2b		/* looks clear -- try to grab */
3:
	FLUSH_PIPELINE
	or.u	r11, r0,  hi16(_ASM_LABEL(initialized_cpus))
	ld	r22, r11, lo16(_ASM_LABEL(initialized_cpus))
	add	r23, r22, 1
	st	r23, r11, lo16(_ASM_LABEL(initialized_cpus))

	or.u	r11, r0,  hi16(_ASM_LABEL(initialized_cpu_lock))
	st	r0,  r11, lo16(_ASM_LABEL(initialized_cpu_lock))
	/*
	 * Now we view with any other processors to see who's the master.
	 * We first try to obtain a lock to see who's allowed
	 * to check/set the master lock.
	 */
	or.u	r11, r0,  hi16(_ASM_LABEL(inter_processor_lock))
	or	r11, r11, lo16(_ASM_LABEL(inter_processor_lock))
1:
	FLUSH_PIPELINE
	or	r22, r0,  1
	xmem	r22, r11, r0		/* If r22 gets 0, we have the lock.. */
	bcnd	eq0, r22, 4f		/* ..but if not, we must wait */
2:
	/* just watch the lock until it looks clear */
	ld	r22, r11, r0
	bcnd	ne0, r22, 2b
	/* since we can be here with caches off, add a few nops to
	   keep the bus from getting overloaded */
	or	r2, r0, lo16(1000)
3:
	subu	r2, r2, 1
	bcnd	eq0, r2, 3b
	br	1b			/* looks clear -- try to grab */
4:
	/* now try to grab the master_processor_chosen prize */
	FLUSH_PIPELINE
	or.u	r11, r0,  hi16(_ASM_LABEL(master_processor_chosen))
	or	r11, r11, lo16(_ASM_LABEL(master_processor_chosen))
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
	bcnd	ne0, r22, _ASM_LABEL(slave_start)
	/* fall through to master start if that's appropriate */

ASLOCAL(master_start)
	/*
	 * Switch to interrupt stack
	 */
	or.u	r31, r0,  hi16(_ASM_LABEL(intstack_end))
	or	r31, r31, lo16(_ASM_LABEL(intstack_end))

	or.u	r3, r0, hi16(_C_LABEL(vector_list))
	or	r3, r3, lo16(_C_LABEL(vector_list))

	bsr.n	_C_LABEL(vector_init)
	 ldcr	r2, VBR

	/* PIO stuff */
	or	r10, r0, 0xb6			/* initialize pio 0 */
	or.u	r11, r0,  hi16(0x4900000c)	/* 0x4900000c: PIO0 ctrl */
	st.b	r10, r11, lo16(0x4900000c)

	/* read dispswitch setting */
	ld.bu	r10, r11, lo16(0x49000000)	/* dipsw-1 (from portA) */
	mak	r10, r10, 0<8>			/* shift left 8 bit */
	ld.bu	r12, r11, lo16(0x49000004)	/* dipsw-2 (from portB) */
	or	r10, r10, r12

	or.u	r11, r0,  hi16(_dipswitch)
	st.h	r10, r11, lo16(_dipswitch)

	bb1	14, r10, 1f			/* XXX: if dipsw-1:2 is on, */
	or	r10, r0, r0			/* XXX: console is ttya */
	or.u	r11, r0, hi16(_sysconsole)
	st	r10, r11, lo16(_sysconsole)

1:
	/* read frame buffer depth from ROM work area */
	ld	r10, r0,  lo16(0x00001114)	/* frame buffer depth */
	or.u	r11, r0,  hi16(_hwplanebits)
	st	r10, r11, lo16(_hwplanebits)

	or	r10,  r0, 0x84			/* initialize pio1 */
	or.u	r11,  r0, hi16(0x4d00000c)
	st.b	r10, r11, lo16(0x4d00000c)
	or	r10,  r0, 0x9			/* port c bit 1 on */
	st.b	r10, r11, lo16(0x4d00000c)

	or.u	r10,  r0, hi16(0xe1000010)	/* clear scsi int */
	ld.b	r11, r10, lo16(0xe1000010)
	st.b	r11, r10, lo16(0xe1000010)

	/* write 0x41000000 to escape rom */
	or.u	r2,  r0,  hi16(0x41000000)
	st	r0,  r2,  lo16(0x41000000)

	/*
	 * luna88k_bootstrap(), among other things, clears proc0's u area.
	 * We are still using the interrupt stack here, thus we are not
	 * affected...
	 */
	bsr	_C_LABEL(luna88k_bootstrap)

	/*
	 * ...and we can switch to the u area stack now.
	 */
	ldcr	r10, CPU
	ld	r31, r10, CI_CURPCB
	addu	r31, r31, USIZE

	/* call main() - no arguments although main() still defines one */
	bsr	_C_LABEL(main)
	or.u	r2, r0, hi16(_ASM_LABEL(main_panic))
	bsr.n	_C_LABEL(panic)
	 or	r2, r2, lo16(_ASM_LABEL(main_panic))

	data
	.align	4
ASLOCAL(main_panic)
	string	"main() returned\0"
	text
	.align	8

/*
 *	slave CPUs starts here
 */
ASLOCAL(slave_start)
	/*
	 * While holding the inter_processor_lock, the slave cpu can use
	 * the slavestack to call slave_pre_main and determine its cpu number.
	 * After that, however, it should switch over to the interrupt stack
	 * associated with its cpu.
	 */

	/* r31 <-- slavestack */
	or.u	r31, r0,  hi16(_ASM_LABEL(slavestack_end))
	or	r31, r31, lo16(_ASM_LABEL(slavestack_end))

	bsr.n	_C_LABEL(slave_pre_main)	/* set cpu number */
	 clr	r31, r31, 3<0>	/* round down to 8-byte boundary */

	bsr	_C_LABEL(get_slave_stack)
	addu	r31, r2, INTSTACK_SIZE

	/*
	 * SR1 now contains our cpu number. We can now release the
	 * inter_processor_lock, as we are done with the slavestack.
	 * We also have an interrupt stack
	 */

	or.u	r10, r0, hi16(_ASM_LABEL(inter_processor_lock))
	st	r0, r10, lo16(_ASM_LABEL(inter_processor_lock))

	br	_C_LABEL(slave_main)	/* does not return */

GLOBAL(spin_cpu)
	or.u	r3,   r0,  hi16(_C_LABEL(start_text))
	or	r3,   r3,  lo16(_C_LABEL(start_text))
	or	r9,   r0,  0x100		/* .FORKMPU */
	tb0	0,    r0,  0x200-16		/* call 188Bug */
	jmp	r1

/*
 * void delay(int count)
 *
 * The processor loops (busy waits) for the given number of microseconds:
 * Thus, delay(1000000) will delay for one second.
 * (originally from Mach 2.5)
 */

GLOBAL(delay)
	or.u	r3, r0, hi16(_cpuspeed)
	ld	r3, r3, lo16(_cpuspeed)
	mul	r4, r2, r3
	subu	r4, r4, 4	/* overhead of these instructions */

	/* now loop for the given number of cycles */
1:
	bcnd.n	gt0, r4, 1b
	 subu	r4, r4, 2	/* two cycles per iteration */

	jmp	r1

/*****************************************************************************/

	data
	.align	PAGE_SIZE
GLOBAL(kernel_sdt)		/* SDT (segment descriptor table */
	space	0x2000		/* 8K - 4K phys, 4K virt*/

	.align	PAGE_SIZE
ASGLOBAL(intstack)
	space	USIZE
ASGLOBAL(intstack_end)
ASGLOBAL(slavestack)
	space	PAGE_SIZE	/* 4K, small, interim stack */
ASGLOBAL(slavestack_end)

/*
 * When a process exits and its u. area goes away, we set curpcb to point
 * to this `u.', leaving us with something to use for an interrupt stack,
 * and letting all the register save code have a pcb_uw to examine.
 * This is also carefully arranged (to come just before u0, so that
 * process 0's kernel stack can quietly overrun into it during bootup, if
 * we feel like doing that).
 * Should be page aligned.
 */
	.align	PAGE_SIZE
GLOBAL(idle_u)
	space	USIZE

/*
 * Process 0's u.
 *
 * This must be page aligned
 */
	.align	PAGE_SIZE
ASLOCAL(u0)
	space	USIZE
GLOBAL(proc0paddr)
	word	_ASM_LABEL(u0)	/*  KVA of proc0 uarea */

GLOBAL(ret_addr)
	word 0
/* XMEM spin lock -- to count CPUs */
ASLOCAL(initialized_cpu_lock)
	word 0
/* CPU counter to initialize */
ASLOCAL(initialized_cpus)
	word 0
/* The first processor that XMEMs this becomes the master */
ASLOCAL(master_processor_chosen)
	word 0
/* XMEM spin lock -- controls access to master_processor_chosen */
ASLOCAL(inter_processor_lock)
	word 0

#if defined(DDB) || NKSYMS > 0
GLOBAL(esym)
	word 	0
#endif /* DDB || NKSYMS > 0 */
