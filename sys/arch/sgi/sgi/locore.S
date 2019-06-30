/*	$OpenBSD: locore.S,v 1.19 2019/06/30 07:37:47 visa Exp $ */

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
 *
 */
#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>

#include "assym.h"

	.set	mips3
	.set	noreorder		# Noreorder is default style!

	.globl	locore_start
	.ent	locore_start, 0
locore_start:
	MFC0	v0, COP_0_STATUS_REG
	LI	v1, ~SR_INT_ENAB
	and	v0, v1
	MTC0	v0, COP_0_STATUS_REG		# disable all interrupts
	MTC0_SR_IE_HAZARD
	MTC0	zero, COP_0_CAUSE_REG		# Clear soft interrupts
	MTC0_HAZARD

	/*
	 * Initialize stack and call machine startup.
	 */
	LA	sp, start - FRAMESZ(CF_SZ)
	LA	gp, _gp
	jal	mips_init			# mips_init(argc, argv, envp)
	PTR_S	zero, CF_RA_OFFS(sp)		# Zero out old ra for debugger

	move	sp, v0				# switch to new stack
	jal	main				# main(regs)
	 move	a0, zero
	PANIC("Startup failed!")
	.end	locore_start

#if defined(MULTIPROCESSOR)
LEAF(hw_cpu_spinup_trampoline, 0)
	MFC0	v0, COP_0_STATUS_REG
	LI	v1, ~SR_INT_ENAB
	and	v0, v1
	ori	v0, SR_KX | SR_UX
	MTC0	v0, COP_0_STATUS_REG		# disable all interrupts
	MTC0	zero, COP_0_CAUSE_REG		# Clear soft interrupts
	LA	gp, _gp
	jal	hw_cpu_hatch
	 NOP
END(hw_cpu_spinup_trampoline)

#if defined(TGT_OCTANE)

LEAF(hw_getcurcpu, 0)
	GET_CPU_INFO(v0, v1)
	j	ra
	 NOP
END(hw_getcurcpu)

/*
 * There does not seem to be any fast way to store the curcpu pointer on
 * Octane. We used to abuse the LLAddr register, but this is a 32-bit register,
 * not suitable for allocation over 2GB physical.
 * Instead, we will use an unused field of the MPCONF structure.
 */
LEAF(hw_setcurcpu, 0)
	HW_GET_CPU_PRID(v0, v1)			# get physical processor ID
	LOAD_XKPHYS(v1, CCA_COHERENT_EXCLWRITE)
	PTR_SLL	v0, MPCONF_SHIFT
	PTR_ADD	v0, MPCONF_BASE
	or	v1, v0
	j	ra
	 PTR_S	a0, (MPCONF_LEN - REGSZ)(v1)
END(hw_setcurcpu)

#elif defined(TGT_ORIGIN)

LEAF(hw_getcurcpu, 0)
	GET_CPU_INFO(v0, v1)
	j	ra
	 NOP
END(hw_getcurcpu)

LEAF(hw_setcurcpu, 0)
	DMTC0	a0, COP_0_ERROR_PC
	j	ra
	 NOP
END(hw_setcurcpu)

LEAF(ip27_cpu_spinup_trampoline, 0)
	MFC0	v0, COP_0_STATUS_REG
	LI	v1, ~SR_INT_ENAB
	and	v0, v1
	MTC0	v0, COP_0_STATUS_REG		# Disable all interrupts.
	MTC0	zero, COP_0_CAUSE_REG		# Clear soft interrupts.
	LA	gp, _gp

	/* Wait the turn of this CPU. */
	LA	v1, ip27_spinup_turn
1:	ll	v0, 0(v1)
	bne	v0, a0, 1b
	 not	v0, zero
	sc	v0, 0(v1)
	beqz	v0, 1b
	 nop

	LA	v0, ip27_spinup_a0
	ld	a0, 0(v0)
	LA	v0, ip27_spinup_sp
	ld	sp, 0(v0)
	jal	hw_cpu_hatch
	 nop
	PANIC("secondary CPU start failed")
END(ip27_cpu_spinup_trampoline)

#endif /* TGT_ORIGIN */

#endif /* MULTIPROCESSOR */
