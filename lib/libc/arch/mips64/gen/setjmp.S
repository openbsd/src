/*      $OpenBSD: setjmp.S,v 1.1 2004/08/11 17:30:59 pefo Exp $ */

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

#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/regnum.h>
#include <machine/signal.h>

#if defined(LIBC_SCCS) 
	.text
	.asciz "$OpenBSD: setjmp.S,v 1.1 2004/08/11 17:30:59 pefo Exp $"
#endif /* LIBC_SCCS */

/*
 * setjmp, longjmp implementation for libc. this code depends
 * on the layout of the struct sigcontext in machine/signal.h.
 *
 */

#define SETJMP_FRAMESZ	(FRAMESZ(CF_SZ) + 4 * REGSZ)

NON_LEAF(setjmp, SETJMP_FRAMESZ, ra)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_SUB	sp, sp, SETJMP_FRAMESZ		# allocate stack frame
#ifdef ABICALLS
	.cprestore 16
#endif
	.set	reorder
	REG_S	ra, CF_RA_OFFS(sp)		# save state
	REG_S	a0, SETJMP_FRAMESZ(sp)
	move	a0, zero			# get current signal mask
	jal	sigblock
	REG_L	v1, SETJMP_FRAMESZ(sp)		# v1 = jmpbuf
	REG_S	v0, SC_MASK(v1)			# save sc_mask = sigblock(0)
	move	a0, zero
	PTR_ADD	a1, sp, FRAMESZ(CF_SZ)		# pointer to struct sigaltstack
	jal	_thread_sys_sigaltstack
	REG_L	a0, SETJMP_FRAMESZ(sp)		# restore jmpbuf
	lw	v1, FRAMESZ(CF_SZ)+REGSZ+4(sp)	# get old ss_onstack
	and	v1, v1, 1			# extract onstack flag
	REG_S	v1, SC_ONSTACK(a0)		# save it in sc_onstack
	REG_L	ra, CF_RA_OFFS(sp)
	PTR_ADD	sp, sp, SETJMP_FRAMESZ
	blt	v0, zero, botch			# check for sigstack() error
	REG_S	ra, SC_PC(a0)			# sc_pc = return address
#if defined(__mips64)
	dli	v0, 0xACEDBADE			# sigcontext magic number
#else
	li	v0, 0xACEDBADE			# sigcontext magic number
#endif
	REG_S	v0, SC_REGS+ZERO*REGSZ(a0)
	REG_S	s0, SC_REGS+S0*REGSZ(a0)
	REG_S	s1, SC_REGS+S1*REGSZ(a0)
	REG_S	s2, SC_REGS+S2*REGSZ(a0)
	REG_S	s3, SC_REGS+S3*REGSZ(a0)
	REG_S	s4, SC_REGS+S4*REGSZ(a0)
	REG_S	s5, SC_REGS+S5*REGSZ(a0)
	REG_S	s6, SC_REGS+S6*REGSZ(a0)
	REG_S	s7, SC_REGS+S7*REGSZ(a0)
	REG_S	s8, SC_REGS+S8*REGSZ(a0)
	REG_S	gp, SC_REGS+GP*REGSZ(a0)
	REG_S	sp, SC_REGS+SP*REGSZ(a0)
	li	v0, 1				# be nice if we could tell
	REG_S	v0, SC_FPUSED(a0)		# sc_fpused = 1
	cfc1	v0, $31
#if _MIPS_FPSET == 32
	sdc1	$f20, SC_FPREGS+((F20-F0)*REGSZ)(a0)
	sdc1	$f21, SC_FPREGS+((F21-F0)*REGSZ)(a0)
	sdc1	$f22, SC_FPREGS+((F22-F0)*REGSZ)(a0)
	sdc1	$f23, SC_FPREGS+((F23-F0)*REGSZ)(a0)
	sdc1	$f24, SC_FPREGS+((F24-F0)*REGSZ)(a0)
	sdc1	$f25, SC_FPREGS+((F25-F0)*REGSZ)(a0)
	sdc1	$f26, SC_FPREGS+((F26-F0)*REGSZ)(a0)
	sdc1	$f27, SC_FPREGS+((F27-F0)*REGSZ)(a0)
	sdc1	$f28, SC_FPREGS+((F28-F0)*REGSZ)(a0)
	sdc1	$f29, SC_FPREGS+((F29-F0)*REGSZ)(a0)
	sdc1	$f30, SC_FPREGS+((F30-F0)*REGSZ)(a0)
	sdc1	$f31, SC_FPREGS+((F31-F0)*REGSZ)(a0)
#else
	swc1	$f20, SC_FPREGS+((F20-F0)*REGSZ)(a0)
	swc1	$f21, SC_FPREGS+((F21-F0)*REGSZ)(a0)
	swc1	$f22, SC_FPREGS+((F22-F0)*REGSZ)(a0)
	swc1	$f23, SC_FPREGS+((F23-F0)*REGSZ)(a0)
	swc1	$f24, SC_FPREGS+((F24-F0)*REGSZ)(a0)
	swc1	$f25, SC_FPREGS+((F25-F0)*REGSZ)(a0)
	swc1	$f26, SC_FPREGS+((F26-F0)*REGSZ)(a0)
	swc1	$f27, SC_FPREGS+((F27-F0)*REGSZ)(a0)
	swc1	$f28, SC_FPREGS+((F28-F0)*REGSZ)(a0)
	swc1	$f29, SC_FPREGS+((F29-F0)*REGSZ)(a0)
	swc1	$f30, SC_FPREGS+((F30-F0)*REGSZ)(a0)
	swc1	$f31, SC_FPREGS+((F31-F0)*REGSZ)(a0)
#endif
	REG_S	v0, SC_FPREGS+((FSR-F0)*REGSZ)(a0)
	move	v0, zero
	j	ra
END(setjmp)

LEAF(longjmp)
#ifdef ABICALLS
	PTR_SUB	sp, sp, CF_ARGSZ * 2
	.cprestore 16
#endif
	.set	reorder
	REG_S	a1, SC_REGS+V0*REGSZ(a0)
	li	v0, SYS_sigreturn
	syscall
botch:
	jal	longjmperror
	jal	abort
END(longjmp)
