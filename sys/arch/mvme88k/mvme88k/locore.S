/*	$OpenBSD: locore.S,v 1.72 2013/05/17 22:46:28 miod Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat.
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
#include <machine/m88100.h>
#include <machine/param.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

/*
 * The memory looks like:
 *   0x00000 - 0x01000	trap vectors
 *   0x01000 - 0x10000	first 64k used by BUG
 *   0x10000 == start	Boot loader jumps here. (for now, this can
 *			handle only NMAGIC - screwy linker)
 */
	.text

GLOBAL(kernelstart)
GLOBAL(kernel_text)
ASGLOBAL(__start)
	/*
	 * A few identical jump instructions to make sure the pipeline is
	 * in a good state. Probably overkill, but it's cheap.
	 */
	br	_ASM_LABEL(main_start)
	br	_ASM_LABEL(main_start)
	br	_ASM_LABEL(main_start)
	br	_ASM_LABEL(main_start)

	/*
	 * Room for the kernel (post-autoconf) VBR page.
	 * Note this page is in kernel text, in order to be write-protected
	 * by pmap_bootstrap().
	 */
#ifdef __ELF__
	.space	PAGE_SIZE - 4 * 4
#else
	.space	PAGE_SIZE - 4 * 4 - 0x20
#endif

#ifdef M88100
	/*
	 * The 88100 may execute the first instruction of the next trap
	 * handler, as documented in its Errata. Processing trap #511
	 * would then fall into the next page, unless the address computation
	 * wraps, or software traps are exempt from the issue - the Errata
	 * does not provide more detail.
	 * Although the MVME BUG does not add an extra NOP after its VBR page,
	 * it is cheap to add an extra NOP for safety.
	 */
	NOP
#endif

	/*
	 * Startup code for main processor.
	 */
ASLOCAL(main_start)
	/*
	 * Save the arguments passed by the boot loader
	 * 	r2 boot flags
	 *	r3 boot controller physical address
	 *	r4 esym (if applicable)
	 *	r5 start of miniroot (unused)
	 *	r6 end of miniroot (unused)
	 *	r7 ((Clun << 8) | Dlun): encoded bootdev
	 *	r8 board type (0x187, 0x188, 0x197)
	 */
/*
 * (*entry)(flag, bugargs.ctrl_addr, cp, kernel.smini,kernel.emini,
 *  bootdev, brdtyp);
 */
	or.u	%r13, %r0,  %hi16(_C_LABEL(boothowto))
	st	%r2,  %r13, %lo16(_C_LABEL(boothowto))
	or.u	%r13, %r0,  %hi16(_C_LABEL(bootaddr))
	st	%r3,  %r13, %lo16(_C_LABEL(bootaddr))
	or.u	%r13, %r0,  %hi16(_C_LABEL(first_addr))
	st	%r4,  %r13, %lo16(_C_LABEL(first_addr))
#if defined(DDB) || NKSYMS > 0
	or.u	%r13, %r0,  %hi16(_C_LABEL(esym))
	st	%r4,  %r13, %lo16(_C_LABEL(esym))
#endif
	or.u	%r13, %r0,  %hi16(_C_LABEL(bootdev))
	st	%r7,  %r13, %lo16(_C_LABEL(bootdev))
	or.u	%r13, %r0,  %hi16(_C_LABEL(brdtyp))
	st	%r8,  %r13, %lo16(_C_LABEL(brdtyp))

	/* set cputyp */
	ldcr	%r1,  PID
	extu	%r8,  %r1, 8<8>
	or.u	%r13, %r0,  %hi16(_C_LABEL(cputyp))

	bsr.n	_ASM_LABEL(setup_psr)
	 st	%r8,  %r13, %lo16(_C_LABEL(cputyp))

#ifdef MULTIPROCESSOR
	/*
	 * Have curcpu() point at the dummy cpuinfo structure,
	 * and initialize cr17.
	 * This is necessary for early spl*() usage, as well as
	 * mutex diagnostic code.
	 */
	or.u	%r11, %r0,  %hi16(_C_LABEL(dummy_cpu))
	or	%r11, %r11, %lo16(_C_LABEL(dummy_cpu))
	stcr	%r11, CPU

	/*
	 * MVME BUG idles all secondary MPUs upon startup, so at this point
	 * we do not have to compete with them.
	 */
#endif	/* MULTIPROCESSOR */

	/* Switch to startup stack */
	or.u	%r31, %r0,  %hi16(_ASM_LABEL(initstack_end))
	or	%r31, %r31, %lo16(_ASM_LABEL(initstack_end))

	/* ...and to our exception vectors */
#ifdef M88110
#ifdef M88100
	cmp	%r2, %r8, CPU_88110
	bb1	ne, %r2, 1f	/* if it's a 'mc88110, use different vectors */
#endif
	or.u	%r3, %r0, %hi16(_C_LABEL(m88110_vector_list))
	br.n	2f
	 or	%r3, %r3, %lo16(_C_LABEL(m88110_vector_list))
1:
#endif /* M88110 */
#ifdef M88100
	or.u	%r3, %r0, %hi16(_C_LABEL(vector_list))
	or	%r3, %r3, %lo16(_C_LABEL(vector_list))
#endif /* M88100 */
2:
	bsr.n	_C_LABEL(mvme88k_vector_init)
	 ldcr	%r2, VBR

#ifdef MULTIPROCESSOR
	bsr	_C_LABEL(atomic_init)
#endif

	/*
	 * mvme_bootstrap(), among other things, clears proc0's u area.
	 * We are still using the interrupt stack here, thus we are not
	 * affected...
	 */
	bsr	_C_LABEL(mvme_bootstrap)

	/*
	 * ...and we can switch to the u area stack now.
	 */
	ldcr	%r10, CPU
	ld	%r2,  %r10, CI_CURPCB

	/* call main() - no arguments although main() still defines one */
	bsr.n	_C_LABEL(main)
	 addu	%r31, %r2, USPACE

#ifdef MULTIPROCESSOR

	/*
	 * Startup code for secondary processors.
	 * Some of these initializations are very close to main_start; refer
	 * to the comments there for details.
	 */
GLOBAL(secondary_start)
	or.u	%r31, %r0,  %hi16(_ASM_LABEL(slavestack_end))
	bsr.n	_ASM_LABEL(setup_psr)
	 or	%r31, %r31, %lo16(_ASM_LABEL(slavestack_end))

	or.u	%r3,  %r0,  %hi16(_C_LABEL(kernel_vbr))
	ld	%r2,  %r3,  %lo16(_C_LABEL(kernel_vbr))
	stcr	%r2,  VBR
	FLUSH_PIPELINE

	/*
	 * Have curcpu() point at the dummy cpuinfo structure,
	 * and initialize cr17.
	 * This is necessary for early spl*() usage, as well as
	 * mutex diagnostic code.
	 */
	or.u	%r11, %r0,  %hi16(_C_LABEL(dummy_cpu))
	or	%r11, %r11, %lo16(_C_LABEL(dummy_cpu))
	st	%r0,  %r11, CI_FLAGS		/* reset CIF_PRIMARY */
	stcr	%r11, CPU

	/*
	 * While holding the cpu_mutex, the secondary cpu can use the slavestack
	 * to call secondary_pre_main() to determine its cpu number.
	 * After that, however, it should allocate its own stack and switch
	 * to it.
	 */

	bsr	_C_LABEL(secondary_pre_main)	/* set cpu number */

	ldcr	%r1, CPU
	st	%r2,  %r1, CI_CURPCB	/* save stack as curpcb for traps */

	bsr.n	_C_LABEL(secondary_main)
	 addu	%r31, %r2, USPACE		/* switch to startup stack */

#endif	/* MULTIPROCESSOR */

/*
 * Reset code trampoline.
 */
GLOBAL(doboot)
	/*
	 * Switch to interrupt stack and call _doboot to take care of
	 * going to BUG. Need to do this since _doboot turns off the
	 * the MMU and we need to be on a 1-to-1 mapped stack so that
	 * further calls don't get data access exceptions.
	 */
	or.u	%r31, %r0,  %hi16(_ASM_LABEL(initstack_end))
	bsr.n	_C_LABEL(_doboot)
	 or	%r31, %r31, %lo16(_ASM_LABEL(initstack_end))
	/*NOTREACHED*/

/* 
 * void dumb_delay(int us)
 *
 * The processor loops (busy waits) for the given number of microseconds:
 * Thus, delay(1000000) will delay for one second.
 * (originally from Mach 2.5) 
 */
GLOBAL(dumb_delay)
	bcnd	eq0, %r2, 2f
	or.u	%r3, %r0, %hi16(_C_LABEL(dumb_delay_const))
	ld	%r3, %r3, %lo16(_C_LABEL(dumb_delay_const))
	mul	%r4, %r2, %r3
	subu	%r4, %r4, 4	/* overhead of these instructions */

	/* now loop for the given number of cycles */
1: 
	bcnd.n	gt0, %r4, 1b
	 subu	%r4, %r4, 2	/* two cycles per iteration */
2:
	jmp	%r1

/*****************************************************************************/

	.data
	.balign	PAGE_SIZE
GLOBAL(kernel_sdt)		/* SDT (segment descriptor table */
	.space	0x2000		/* 8K - 4K phys, 4K virt*/

	.balign	PAGE_SIZE
ASGLOBAL(initstack)
	.space	USPACE
ASGLOBAL(initstack_end)

#ifdef MULTIPROCESSOR
	.space	PAGE_SIZE	/* 4K, small, interim stack */
ASLOCAL(slavestack_end)
#endif

/*
 * Process 0's u.
 * Should be page aligned.
 */
	.balign	PAGE_SIZE
ASLOCAL(u0)
	.space	USPACE

GLOBAL(proc0paddr)
	.word	_ASM_LABEL(u0)	/*  KVA of proc0 uarea */

#if defined(DDB) || NKSYMS > 0
GLOBAL(esym)
	.word 	0
#endif /* DDB || NKSYMS > 0 */
