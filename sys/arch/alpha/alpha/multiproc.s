/* $OpenBSD: multiproc.s,v 1.4 2008/06/26 05:42:08 ray Exp $ */
/* $NetBSD: multiproc.s,v 1.5 1999/12/16 20:17:23 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

.file	4 __FILE__
.loc	4 __LINE__

/*
 * Multiprocessor glue code.
 */

	.text

/*
 * cpu_spinup_trampoline:
 *
 * We come here via the secondary processor's console.  We simply
 * make the function call look right, and call cpu_hatch() to finish
 * starting up the processor.
 *
 * We are provided an argument in $27 (pv) (which will be our cpu_info).
 */
NESTED_NOPROFILE(cpu_spinup_trampoline,0,0,ra,0,0)
	mov	pv, s0			/* squirrel away argument */

	br	pv, 1f			/* compute new GP */
1:	LDGP(pv)

	/* Invalidate TLB and I-stream. */
	ldiq	a0, -2			/* TBIA */
	call_pal PAL_OSF1_tbi
	call_pal PAL_imb

	/* Load KGP with current GP. */
	mov	gp, a0
	call_pal PAL_OSF1_wrkgp		/* clobbers a0, t0, t8-t11 */

	/* Restore argument and write it in SysValue. */
	mov	s0, a0
	call_pal PAL_OSF1_wrval

	/* Restore argument and call cpu_hatch() */
	mov	s0, a0
	CALL(cpu_hatch)

	/* cpu_hatch() returned!  Just halt (forever). */
2:	call_pal PAL_halt
	br	zero, 2b
	END(cpu_spinup_trampoline)
