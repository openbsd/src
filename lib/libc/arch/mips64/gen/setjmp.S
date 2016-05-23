/*	$OpenBSD: setjmp.S,v 1.10 2016/05/23 00:18:57 guenther Exp $ */

/*
 * Copyright (c) 2001-2002 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opsycon AB nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include "SYS.h"
#include <machine/regnum.h>
#include <machine/signal.h>

/*
 * setjmp, longjmp implementation for libc. this code depends
 * on the layout of the struct sigcontext in machine/signal.h.
 */

FRAMESZ= MKFSIZ(0,4)
GPOFF= FRAMESZ-2*REGSZ

LEAF(setjmp, FRAMESZ)
	PTR_SUBU sp, FRAMESZ
	SETUP_GP64(GPOFF, setjmp)
	.set	noreorder

	move	a2, a0				# save jmpbuf
	li	a0, 1				# how = SIG_BLOCK
	move	a1, zero			# get current signal mask
	li	v0, SYS_sigprocmask
	syscall					# mask in v0
	bne	a3, zero, botch
	REG_S	v0, SC_MASK(a2)			# save sc_mask

#if defined(__mips64)
	dli	v0, 0xACEDBADE			# sigcontext magic number
#else
	li	v0, 0xACEDBADE			# sigcontext magic number
#endif
	REG_S	v0, SC_REGS+ZERO*REGSZ(a2)
	REG_S	s0, SC_REGS+S0*REGSZ(a2)
	REG_S	s1, SC_REGS+S1*REGSZ(a2)
	REG_S	s2, SC_REGS+S2*REGSZ(a2)
	REG_S	s3, SC_REGS+S3*REGSZ(a2)
	REG_S	s4, SC_REGS+S4*REGSZ(a2)
	REG_S	s5, SC_REGS+S5*REGSZ(a2)
	REG_S	s6, SC_REGS+S6*REGSZ(a2)
	REG_S	s7, SC_REGS+S7*REGSZ(a2)
	REG_S	s8, SC_REGS+S8*REGSZ(a2)
	REG_L	v0, GPOFF(sp)
	REG_S	v0, SC_REGS+GP*REGSZ(a2)
	PTR_ADDU v0, sp, FRAMESZ
	REG_S	v0, SC_REGS+SP*REGSZ(a2)
	REG_S	ra, SC_PC(a2)
	cfc1	v0, $31
#if _MIPS_FPSET == 32
	sdc1	$f20, SC_FPREGS+((F20-F0)*REGSZ)(a2)
	sdc1	$f21, SC_FPREGS+((F21-F0)*REGSZ)(a2)
	sdc1	$f22, SC_FPREGS+((F22-F0)*REGSZ)(a2)
	sdc1	$f23, SC_FPREGS+((F23-F0)*REGSZ)(a2)
	sdc1	$f24, SC_FPREGS+((F24-F0)*REGSZ)(a2)
	sdc1	$f25, SC_FPREGS+((F25-F0)*REGSZ)(a2)
	sdc1	$f26, SC_FPREGS+((F26-F0)*REGSZ)(a2)
	sdc1	$f27, SC_FPREGS+((F27-F0)*REGSZ)(a2)
	sdc1	$f28, SC_FPREGS+((F28-F0)*REGSZ)(a2)
	sdc1	$f29, SC_FPREGS+((F29-F0)*REGSZ)(a2)
	sdc1	$f30, SC_FPREGS+((F30-F0)*REGSZ)(a2)
	sdc1	$f31, SC_FPREGS+((F31-F0)*REGSZ)(a2)
#else
	swc1	$f20, SC_FPREGS+((F20-F0)*REGSZ)(a2)
	swc1	$f21, SC_FPREGS+((F21-F0)*REGSZ)(a2)
	swc1	$f22, SC_FPREGS+((F22-F0)*REGSZ)(a2)
	swc1	$f23, SC_FPREGS+((F23-F0)*REGSZ)(a2)
	swc1	$f24, SC_FPREGS+((F24-F0)*REGSZ)(a2)
	swc1	$f25, SC_FPREGS+((F25-F0)*REGSZ)(a2)
	swc1	$f26, SC_FPREGS+((F26-F0)*REGSZ)(a2)
	swc1	$f27, SC_FPREGS+((F27-F0)*REGSZ)(a2)
	swc1	$f28, SC_FPREGS+((F28-F0)*REGSZ)(a2)
	swc1	$f29, SC_FPREGS+((F29-F0)*REGSZ)(a2)
	swc1	$f30, SC_FPREGS+((F30-F0)*REGSZ)(a2)
	swc1	$f31, SC_FPREGS+((F31-F0)*REGSZ)(a2)
#endif
	REG_S	v0, SC_FPREGS+((FSR-F0)*REGSZ)(a2)
	RESTORE_GP64
	PTR_ADDU sp, FRAMESZ
	j	ra
	move	v0, zero
END_STRONG(setjmp)

LEAF(longjmp, FRAMESZ)
	PTR_SUBU sp, FRAMESZ
	SETUP_GP64(GPOFF, longjmp)
	.set    noreorder

	move	a2, a0				# save jmpbuf
	move	a4, a1				# save val
	REG_L	a1, SC_MASK(a2)			# load sc_mask
	li	a0, 3				# how = SIG_SETMASK
	li	v0, SYS_sigprocmask
	syscall
	bne	a3, zero, botch

	REG_L	v0, SC_REGS+ZERO*REGSZ(a2)
	bne	v0, 0xACEDBADE, botch		# jump if error
	REG_L	ra, SC_PC(a2)
	REG_L	v0, SC_FPREGS+((FSR-F0)*REGSZ)(a2)
	REG_L	s0, SC_REGS+S0*REGSZ(a2)
	REG_L	s1, SC_REGS+S1*REGSZ(a2)
	REG_L	s2, SC_REGS+S2*REGSZ(a2)
	REG_L	s3, SC_REGS+S3*REGSZ(a2)
	REG_L	s4, SC_REGS+S4*REGSZ(a2)
	REG_L	s5, SC_REGS+S5*REGSZ(a2)
	REG_L	s6, SC_REGS+S6*REGSZ(a2)
	REG_L	s7, SC_REGS+S7*REGSZ(a2)
	REG_L	s8, SC_REGS+S8*REGSZ(a2)
	REG_L	gp, SC_REGS+GP*REGSZ(a2)
	REG_L	sp, SC_REGS+SP*REGSZ(a2)
	ctc1	v0, $31
#if _MIPS_FPSET == 32
	ldc1	$f20, SC_FPREGS+((F20-F0)*REGSZ)(a2)
	ldc1	$f21, SC_FPREGS+((F21-F0)*REGSZ)(a2)
	ldc1	$f22, SC_FPREGS+((F22-F0)*REGSZ)(a2)
	ldc1	$f23, SC_FPREGS+((F23-F0)*REGSZ)(a2)
	ldc1	$f24, SC_FPREGS+((F24-F0)*REGSZ)(a2)
	ldc1	$f25, SC_FPREGS+((F25-F0)*REGSZ)(a2)
	ldc1	$f26, SC_FPREGS+((F26-F0)*REGSZ)(a2)
	ldc1	$f27, SC_FPREGS+((F27-F0)*REGSZ)(a2)
	ldc1	$f28, SC_FPREGS+((F28-F0)*REGSZ)(a2)
	ldc1	$f29, SC_FPREGS+((F29-F0)*REGSZ)(a2)
	ldc1	$f30, SC_FPREGS+((F30-F0)*REGSZ)(a2)
	ldc1	$f31, SC_FPREGS+((F31-F0)*REGSZ)(a2)
#else
	lwc1	$f20, SC_FPREGS+((F20-F0)*REGSZ)(a2)
	lwc1	$f21, SC_FPREGS+((F21-F0)*REGSZ)(a2)
	lwc1	$f22, SC_FPREGS+((F22-F0)*REGSZ)(a2)
	lwc1	$f23, SC_FPREGS+((F23-F0)*REGSZ)(a2)
	lwc1	$f24, SC_FPREGS+((F24-F0)*REGSZ)(a2)
	lwc1	$f25, SC_FPREGS+((F25-F0)*REGSZ)(a2)
	lwc1	$f26, SC_FPREGS+((F26-F0)*REGSZ)(a2)
	lwc1	$f27, SC_FPREGS+((F27-F0)*REGSZ)(a2)
	lwc1	$f28, SC_FPREGS+((F28-F0)*REGSZ)(a2)
	lwc1	$f29, SC_FPREGS+((F29-F0)*REGSZ)(a2)
	lwc1	$f30, SC_FPREGS+((F30-F0)*REGSZ)(a2)
	lwc1	$f31, SC_FPREGS+((F31-F0)*REGSZ)(a2)
#endif
	beql	a4, zero, 1f
	li	a4, 1		# only executed if branch taken.
1:
	j	ra
	move	v0, a4

botch:
	jal	_libc_abort
	nop
	RESTORE_GP64
	PTR_ADDU sp, FRAMESZ
END_STRONG(longjmp)
