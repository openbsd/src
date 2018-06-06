/*	$OpenBSD: locore0.S,v 1.2 2018/06/06 12:58:05 aoyama Exp $	*/
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
#include <machine/board.h>
#include <machine/m88100.h>
#include <machine/param.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#define	BOOT_MAGIC	0xf1abde3f

/*
 * The memory looks like:
 *   0x00000 - 0x01000	trap vectors
 *   0x01000 - 0x20000	ROM monitor work area
 *   0x20000 == start	Boot loader jumps here.
 */
	.text

GLOBAL(kernelstart)
GLOBAL(kernel_text)

	/*
	 * Room for the kernel (post-autoconf) VBR page.
	 * Note this page is in kernel text, in order to be write-protected
	 * by pmap_bootstrap().
	 */

	.balign	PAGE_SIZE
	.space	PAGE_SIZE

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
	 * Startup code common to all processors.
	 */
ASLOCAL(main_start)
#if defined(DDB) || NKSYMS > 0
	or.u	%r10, %r0,  %hi16(BOOT_MAGIC)
	or	%r10, %r10, %lo16(BOOT_MAGIC)
	cmp	%r11, %r2,  %r10
	bb1	ne,   %r11, 1f

	or.u	%r4,  %r0,  %hi16(_C_LABEL(esym))
	st	%r3,  %r4,  %lo16(_C_LABEL(esym))
1:
#endif

	bsr	_ASM_LABEL(setup_psr)

	stcr	%r0,  VBR		/* start with VBR set to zero */
	FLUSH_PIPELINE

	/*
	 * Now we will compete with the other processors to see which one
	 * will be elected as the main one.
	 */
	or.u	%r11, %r0,  %hi16(_C_LABEL(cpu_hatch_mutex))
	or	%r11, %r11, %lo16(_C_LABEL(cpu_hatch_mutex))
1:
	FLUSH_PIPELINE
	or	%r22, %r0,  1
	xmem	%r22, %r11, %r0		/* If %r22 gets 0, we have the lock.. */
	bcnd	eq0,  %r22, 4f		/* ..but if not, we must wait */
2:
	/* just watch the lock until it looks clear */
	ld	%r22, %r11, %r0
	bcnd	ne0,  %r22, 2b
	/* since we can be here with caches off, add a few nops to
	   keep the bus from getting overloaded */
	or	%r2,  %r0, %lo16(1000)
3:
	subu	%r2,  %r2, 1
	bcnd	eq0,  %r2, 3b
	br	1b			/* looks clear -- try to grab */
4:
	/* now try to grab the master_mpu prize */
	FLUSH_PIPELINE
	or.u	%r11, %r0,  %hi16(_ASM_LABEL(master_mpu))
	or	%r11, %r11, %lo16(_ASM_LABEL(master_mpu))
	or	%r22, %r0,  1
	xmem	%r22, %r11, %r0

	/*
	 * If %r22 is not clear we're a secondary,
	 * otherwise we're first and the main.
	 *
	 * Note that we haven't released the interprocessor lock....
	 * We'll do that when we're ready for another CPU to go.
	 */
	bcnd	ne0,  %r22, _ASM_LABEL(secondary_init)

	/*
	 * Main processor specific initialization (with
	 * cpu_hatch_mutex held).
	 */
ASLOCAL(main_init)
	/* clear BSS. PROM might have already done this... */
	or.u	%r2, %r0, %hi16(_C_LABEL(edata))
	or	%r2, %r2, %lo16(_C_LABEL(edata))
	or.u	%r4, %r0, %hi16(_C_LABEL(end))
	or	%r4, %r4, %lo16(_C_LABEL(end))
	bsr.n	_C_LABEL(bzero)		/* bzero(edata, end-edata) */
	 subu	%r3, %r4, %r2

	/* figure out the end of the kernel image. */
#if defined(DDB) || NKSYMS > 0
	or.u	%r4, %r0, %hi16(_C_LABEL(esym))
	ld	%r4, %r4, %lo16(_C_LABEL(esym))
	bcnd	ne0, %r4, 1f
#endif
	or.u	%r4, %r0, %hi16(_C_LABEL(end))
	or	%r4, %r4, %lo16(_C_LABEL(end))		/* if no symbols */
1:
	or.u	%r5, %r0, %hi16(_C_LABEL(first_addr))
	st	%r4, %r5, %lo16(_C_LABEL(first_addr))

	/*
	 * Have curcpu() point to a valid cpuinfo structure,
	 * and initialize cr17.
	 * This is necessary for early spl*() usage, as well as
	 * mutex diagnostic code.
	 */
#ifdef MULTIPROCESSOR
	or.u	%r11, %r0,  %hi16(_ASM_LABEL(dummy_cpu))
	or	%r11, %r11, %lo16(_ASM_LABEL(dummy_cpu))
#else
	/* Need to make sure this matches non-MP curcpu() */
	or.u	%r11, %r0,  %hi16(_C_LABEL(m88k_cpus))
	or	%r11, %r11, %lo16(_C_LABEL(m88k_cpus))
#endif
	stcr	%r11, CPU

	/* Switch to startup stack */
	or.u	%r31, %r0,  %hi16(_ASM_LABEL(initstack_end))
	or	%r31, %r31, %lo16(_ASM_LABEL(initstack_end))

	or.u	%r3,  %r0,  %hi16(_C_LABEL(vector_list))
	or	%r3,  %r3,  %lo16(_C_LABEL(vector_list))

	bsr.n	_C_LABEL(luna88k_vector_init)
	 ldcr	%r2, VBR

	/* PIO stuff */
	or	%r10, %r0,  0xb6		/* initialize pio 0 */
	or.u	%r11, %r0,  %hi16(OBIO_PIO0)	/* PIO0 ctrl */
	st.b	%r10, %r11, %lo16(OBIO_PIO0)

	/* read dispswitch setting */
	ld.bu	%r10, %r11, %lo16(OBIO_PIO0A)	/* dipsw-1 (from portA) */
	mak	%r10, %r10, 0<8>			/* shift left 8 bit */
	ld.bu	%r12, %r11, %lo16(OBIO_PIO0B)	/* dipsw-2 (from portB) */
	or	%r10, %r10, %r12

	or.u	%r11, %r0,  %hi16(_C_LABEL(dipswitch))
	st.h	%r10, %r11, %lo16(_C_LABEL(dipswitch))

	bb1	14,   %r10, 1f			/* XXX: if dipsw-1:2 is on, */
	or	%r10, %r0,  %r0			/* XXX: console is ttya */
	or.u	%r11, %r0,  %hi16(_C_LABEL(sysconsole))
	st	%r10, %r11, %lo16(_C_LABEL(sysconsole))

1:
	/* read frame buffer depth from ROM work area */
	ld	%r10, %r0,  %lo16(0x00001114)	/* frame buffer depth */
	or.u	%r11, %r0,  %hi16(_C_LABEL(hwplanebits))
	st	%r10, %r11, %lo16(_C_LABEL(hwplanebits))

	or	%r10,  %r0,  0x84		/* initialize pio1 */
	or.u	%r11,  %r0,  %hi16(OBIO_PIO1)
	st.b	%r10,  %r11, %lo16(OBIO_PIO1)
	or	%r10,  %r0,  0x9		/* port c bit 1 on */
	st.b	%r10,  %r11, %lo16(OBIO_PIO1)

	or.u	%r10,  %r0,  %hi16(SCSI_ADDR + 4 * 0x04)/* clear scsi INTS */
	ld.b	%r11, %r10,  %lo16(SCSI_ADDR + 4 * 0x04)
	st.b	%r11, %r10,  %lo16(SCSI_ADDR + 4 * 0x04)

	/* write to the PROM area to escape rom */
	or.u	%r2,  %r0,   %hi16(PROM_ADDR)
	st	%r0,  %r2,   %lo16(PROM_ADDR)

	/*
	 * luna88k_bootstrap(), among other things, clears proc0's u area.
	 * We are still using the interrupt stack here, thus we are not
	 * affected...
	 */
	bsr	_C_LABEL(luna88k_bootstrap)

	/*
	 * ...and we can switch to the u area stack now.
	 */
	ldcr	%r10, CPU
	ld	%r31, %r10, CI_CURPCB
	addu	%r31, %r31, USPACE

	/* call main() - no arguments although main() still defines one */
	bsr	_C_LABEL(main)

	or.u	%r2,  %r0,  %hi16(_ASM_LABEL(main_panic))
	bsr.n	_C_LABEL(panic)
	 or	%r2,  %r2,  %lo16(_ASM_LABEL(main_panic))

	.data
	.balign	4
ASLOCAL(main_panic)
	.string	"main() returned\0"
	.text
	.balign	8

	/*
	 * Secondary processor specific initialization (with
	 * cpu_hatch_mutex held).
	 */
ASLOCAL(secondary_init)
#ifdef MULTIPROCESSOR
	/*
	 * Have curcpu() point to the dummy cpuinfo structure,
	 * and initialize cr17.
	 * This is necessary for early spl*() usage, as well as
	 * mutex diagnostic code.
	 */
	or.u	%r11, %r0,  %hi16(_ASM_LABEL(dummy_cpu))
	or	%r11, %r11, %lo16(_ASM_LABEL(dummy_cpu))
	st	%r0,  %r11, CI_FLAGS		/* reset CIF_PRIMARY */
	stcr	%r11, CPU

	/*
	 * While holding the cpu_hatch_mutex, the secondary cpu can use the
	 * slavestack to call secondary_pre_main() to determine its cpu
	 * number. That function will also return the proper stack to
	 * use and we'll switch to it.
	 */

	or.u	%r31, %r0,  %hi16(_ASM_LABEL(slavestack_end))
	bsr.n	_C_LABEL(secondary_pre_main)	/* set cpu number */
	 or	%r31, %r31, %lo16(_ASM_LABEL(slavestack_end))

	bsr.n	_C_LABEL(secondary_main)
	 addu	%r31, %r2, USPACE		/* switch to idle stack */

#else

	/*
	 * Just keep the processor chewing in silence.
	 */
1:	br	1b

#endif	/* MULTIPROCESSOR */

#ifdef MULTIPROCESSOR
	/*
	 * Dummy mp_atomic_begin() and mp_atomic_end() routine, so that
	 * we can interact with ddb if things go wrong very early during
	 * bootstrap. Of course this should never happen (-:
	 */
ASLOCAL(dummy_mplock)
	jmp	%r1
#endif	/* MULTIPROCESSOR */


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

/* The first processor to XMEM this becomes the master */
ASLOCAL(master_mpu)
	.word 0

#ifdef MULTIPROCESSOR
/* Dummy cpuinfo structure, for early bootstrap */
ASLOCAL(dummy_cpu)
	.word	3 /* CIF_ALIVE | CIF_PRIMARY */	/* ci_flags */
	.word	0				/* ci_curproc */
	.word	0				/* ci_curpcb */
	.word	0				/* ci_curpmap */
	.word	0				/* ci_cpuid */
	.word	_ASM_LABEL(dummy_mplock)	/* ci_mp_atomic_begin */
	.word	_ASM_LABEL(dummy_mplock)	/* ci_mp_atomic_end */
	.space	CPU_INFO_SIZEOF - 7 * 4
#endif

#if defined(DDB) || NKSYMS > 0
GLOBAL(esym)
	.word 	0
#endif /* DDB || NKSYMS > 0 */
