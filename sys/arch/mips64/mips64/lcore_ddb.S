/*	$OpenBSD: lcore_ddb.S,v 1.4 2004/09/10 08:58:27 pefo Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>
#include <machine/trap.h>

#include "assym.h"

	.set	noreorder		# Noreorder is default style!

#define	ITLBNOPFIX	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;

LEAF(kdbpeek, 0)
	PTR_L	t0, curprocpaddr
	li	v0, KT_DDBERR
	and	v1, a0, 3			# unaligned ?
	bne	v1, zero, 1f
	sw	v0, U_PCB_ONFAULT(t0)

	lw	v0, (a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)

1:
	LWHI	v0, 0(a0)
	LWLO	v0, 3(a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)
END(kdbpeek)

LEAF(kdbpeekw, 0)
	PTR_L	t0, curprocpaddr
	li	v0, KT_DDBERR
	and	v1, a0, 1			# unaligned ?
	bne	v1, zero, 1f
	sw	v0, U_PCB_ONFAULT(t0)

	lh	v0, (a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)

1:
	li	v0, -1				# error!
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)
END(kdbpeekw)

LEAF(kdbpeekb, 0)
	PTR_L	t0, curprocpaddr
	li	v0, KT_DDBERR
	sw	v0, U_PCB_ONFAULT(t0)
	lb	v0, 0(a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)
END(kdbpeekb)

	.globl	kt_ddberr
kt_ddberr:
	jr	ra
	li	v0, -1

LEAF(kdbpoke, 0)
	PTR_L	t0, curprocpaddr
	li	v0, KT_DDBERR
	and	v1, a0, 3			# unaligned ?
	bne	v1, zero, 1f
	sw	v0, U_PCB_ONFAULT(t0)

	sw	a1, (a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)

1:
	SWHI	a1, 0(a0)
	SWLO	a1, 3(a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)
END(kdbpoke)

LEAF(kdbpokew, 0)
	PTR_L	t0, curprocpaddr
	li	v0, KT_DDBERR
	and	v1, a0, 1			# unaligned ?
	bne	v1, zero, 1f
	sw	v0, U_PCB_ONFAULT(t0)

	sh	a1, (a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)

1:
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)
END(kdbpokew)

LEAF(kdbpokeb, 0)
	PTR_L	t0, curprocpaddr
	li	v0, KT_DDBERR
	sw	v0, U_PCB_ONFAULT(t0)
	sb	a1, 0(a0)
	jr	ra
	sw	zero, U_PCB_ONFAULT(t0)
END(kdbpokeb)

LEAF(Debugger, 0)
	break   BREAK_SOVER_VAL
	jr	ra
	nop
END(Debugger)

LEAF(setjmp, 0)
	mfc0	v0, COP_0_STATUS_REG	# Later the "real" spl value!
	REG_S	s0, REGSZ * 0(a0)
	REG_S	s1, REGSZ * 1(a0)
	REG_S	s2, REGSZ * 2(a0)
	REG_S	s3, REGSZ * 3(a0)
	REG_S	s4, REGSZ * 4(a0)
	REG_S	s5, REGSZ * 5(a0)
	REG_S	s6, REGSZ * 6(a0)
	REG_S	s7, REGSZ * 7(a0)
	REG_S	s8, REGSZ * 8(a0)
	REG_S	sp, REGSZ * 9(a0)
	REG_S	ra, REGSZ * 10(a0)
	REG_S	v0, REGSZ * 11(a0)
	jr	ra
	li	v0, 0			# setjmp return
END(setjmp)

LEAF(longjmp, 0)
	REG_L	v0, REGSZ * 11(a0)
	REG_L	ra, REGSZ * 10(a0)
	REG_L	s0, REGSZ * 0(a0)
	REG_L	s1, REGSZ * 1(a0)
	REG_L	s2, REGSZ * 2(a0)
	REG_L	s3, REGSZ * 3(a0)
	REG_L	s4, REGSZ * 4(a0)
	REG_L	s5, REGSZ * 5(a0)
	REG_L	s6, REGSZ * 6(a0)
	REG_L	s7, REGSZ * 7(a0)
	REG_L	s8, REGSZ * 8(a0)
	REG_L	sp, REGSZ * 9(a0)
	mtc0	v0, COP_0_STATUS_REG	# Later the "real" spl value!
	ITLBNOPFIX
	jr	ra
	li	v0, 1			# longjmp return
END(longjmp)

