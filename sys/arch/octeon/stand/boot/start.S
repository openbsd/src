/*	$OpenBSD: start.S,v 1.3 2014/07/12 20:36:52 jasper Exp $	*/

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>

	.set	noreorder
	.set	mips64r2

	.globl	__start
	.globl	kernel_text
kernel_text = __start
LEAF(cp0_get_count, 0)
        MFC0    v0, COP_0_COUNT
        MFC0_HAZARD
        j       ra
        NOP
END(cp0_get_count)

__start:
/* initialize ebase */
	dla	t0, 0xffffffff80000000
	mtc0	t0, COP_0_EBASE

/* initialize cvmctl */
	dli	t0, 0x900043F0
	dmtc0	t0, COP_0_CVMCTL

/* initialize cvmmemctl */
	dli	t0, 0x1846104 # If you want to skip write buffer, use this
	dmtc0	t0, COP_0_CVMMEMCTL

/* initialize pagegrain */
	dli	t0, 0x20000000
	dmtc0	t0, COP_0_TLB_PG_GRAIN

	mfc0    v0, COP_0_STATUS_REG
	li	v1, ~(SR_INT_ENAB | SR_ERL | SR_EXL)
	and	v0, v1
	mtc0    v0, COP_0_STATUS_REG    # disable all interrupts

	mtc0	zero, COP_0_CAUSE_REG	# Clear soft interrupts

	LA	gp, _gp

	/*
	 * Initialize stack and call bootloader setup.
	 */
	LA	t0, __start - FRAMESZ(CF_SZ)
	PTR_S	ra, CF_RA_OFFS(t0)		# save uboot return address
	PTR_S	sp, 0(t0)			# and stack
	move	sp, t0
	jal	mips_init			# mips_init(argc, argv, envp,
	nop					#    callvec, esym)

	beqz	v0, 1f				# upon failure, return to uboot
	nop

	move	a0, zero
	PANIC("Startup failed!")

1:	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	sp, 0(sp)
	jr	ra
	nop
