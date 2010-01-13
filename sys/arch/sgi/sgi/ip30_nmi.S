/*	$OpenBSD: ip30_nmi.S,v 1.2 2010/01/13 23:24:27 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/cpustate.h>
#include <machine/regnum.h>

#include <arch/sgi/sgi/ip30.h>

#ifndef MULTIPROCESSOR
#define	HW_CPU_NUMBER_REG	0x900000000ff50000	/* HEART_PRID */
#endif

#include "assym.h"

	.set	mips3

/*
 * The NMI handler routine is shared accross all processors.
 *
 * When the NMI is triggered, we might be in the middle of an exception
 * handler, and relying upon k0 and k1.
 *
 * Unfortunately, since there is no way to know whether our stack is valid,
 * we will need these registers. Therefore NMI are fatal if they occur in
 * kernel mode... but we don't even try to resume from them, yet.
 */
	.globl	ip30_nmi
ip30_nmi:
	.set	noat
	sync

	LOAD_XKPHYS(k0, CCA_CACHED)
	LA	k1, IP30_MEMORY_BASE + 0x4000
	PTR_ADDU k0, k0, k1

	/*
	 * We use part of the low memory as stack and save area.
	 * This is safe since we reserved this area early.
	 * We give ourselves 8KB, minus the size of the frame, of stack.
	 */

	LA	k1, HW_CPU_NUMBER_REG
	PTR_L	k1, 0(k1)
	beqz	k1, 1f
	nop

	/* CPU #1 */
	PTR_ADDU k0, k0, 0x2000
1:
	/*
	 * Save state.
	 * k0 = frame base, k1 = cpu number
	 */

	SAVE_CPU(k0, 0)
	SAVE_CPU_SREG(k0, 0)
	.set	at

	PTR_ADDU sp, k0, 0x2000
	LA	gp, _gp
	
	/*
	 * Interrupts should be disabled. Just in case they aren't,
	 * enforce this (a1 still contains the value of cop0 SR).
	 */
	
	and	a1, a1, ~SR_INT_ENAB
	mtc0	a1, COP_0_STATUS_REG
	ITLBNOPFIX

	jal	ip30_nmi_handler		/* ip30_machdep.c */
	nop

9:
	sync
	b	9b
	nop
