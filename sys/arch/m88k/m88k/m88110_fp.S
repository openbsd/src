/* $OpenBSD: m88110_fp.S,v 1.1 2004/04/29 14:33:27 miod Exp $	*/
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 *      This product includes software developed by Christopher G. Demetriou.
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
 */

/* Floating point trouble routines */
/*
 * August 1, 1999
 * smurph@OpenBSD.org
 *
 * Additions to support MVME197 (mc88110) mmu routines.
 */

/*
 * This is cheesy.  I'm using the TCFP features of the mc88110
 * because it was easy.  It is not 100% IEEE.  But it may be
 * close enough.  We shall see...  XXXsmurph
 * Err... TCFP == "Time Critical Floating Point"
 *
 * The only two SFU1 exceptions that can occure in TCFP mode are:
 * 1) Unimplemented Floating Point Instruction
 * 2) Floating Point Privilege Violation
 */

#include "assym.h"
#include <machine/trap.h>
#include <machine/asm.h>

ASENTRY(m88110_Xfp_precise)
	or	r29, r3, r0     /*  r29 is now the E.F. */
	subu	r31, r31, 40
	st	r1,  r31, 32
	st	r29, r31, 36

	ld	r2, r29, EF_FPSR * 4
	ld	r3, r29, EF_FPCR * 4
	ld	r4, r29, EF_FPECR * 4

	/*
	 * Load into r1 the return address for the 0 handlers.  Looking
	 * at FPECR, branch to the appropriate 0 handler.  However,
	 * if none of the 0 bits are enabled, then a floating point
	 * instruction was issued with the floating point unit disabled.  This
	 * will cause an unimplemented opcode 0.
	 */

   	bb0	6,r4, 2f   	/* branch to m88110_FPunimp if bit set */
     	br	_ASM_LABEL(m88110_FPuimp)
2:  	bb0	5,r4, 3f 	/* branch to m88110_FPpriviol if bit set */
	br	_ASM_LABEL(m88110_FPpriviol)
3:
	or.u	r4, r4, 0xffff

ASLOCAL(m88110_FPuimp)
	subu	r31,r31,40		/* allocate stack */
	st	r1,r31,36		/* save return address */
	st	r3,r31,32		/* save exception frame */
	or	r2,r0,T_FPEPFLT		/* load trap type */
	or	r3, r29, r0
	bsr	_C_LABEL(m88110_trap)	/* trap */
	ld	r1,r31,36		/* recover return address */
	addu	r31,r31,40		/* deallocate stack */
 	jmp	r1

ASLOCAL(m88110_FPpriviol)
	subu	r31,r31,40		/* allocate stack */
	st	r1,r31,36		/* save return address */
	st	r3,r31,32		/* save exception frame */
	or	r2,r0,T_PRIVINFLT	/* load trap type */
	or	r3, r29, r0
	bsr	_C_LABEL(m88110_trap)	/* trap */
	ld	r1,r31,36		/* recover return address */
 	jmp.n	r1
	 addu	r31,r31,40		/* deallocate stack */

ENTRY(set_tcfp)
	or.u	r2, r0, hi16(0x200000)
	or	r2, r2, lo16(0x200000)
	jmp.n	r1
	 fstcr	r2, fcr0
