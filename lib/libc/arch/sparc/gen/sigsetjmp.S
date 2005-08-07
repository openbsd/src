/* $OpenBSD: sigsetjmp.S,v 1.5 2005/08/07 16:40:15 espie Exp $ */
/*
 * Copyright (c) 1995 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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

#include "SYS.h"

#ifdef PIC

ENTRY(sigsetjmp)
	PIC_PROLOGUE(%g1,%g2)	! %g1 = _GLOBAL_OFFSET_TABLE
	cmp %o1,0
	st %o1,[%o0+40]		! jmpbuf[JBLEN]
	bne,a 1f
	ld [%g1+_C_LABEL(setjmp)], %g1	! if (%o1 != 0) goto setjmp;
	ld [%g1+_C_LABEL(_setjmp)], %g1	! else goto _setjmp;
1:
	jmp %g1
	 nop

ENTRY(siglongjmp)
	PIC_PROLOGUE(%g1,%g2)	! %g1 = _GLOBAL_OFFSET_TABLE
	ld [%o0+40],%g2		! jmpbuf[JBLEN]
	cmp %g2,0
	bne,a 1f
	ld [%g1+_C_LABEL(longjmp)], %g1	! if (%g2 != 0) goto longjmp;
	ld [%g1+_C_LABEL(_longjmp)], %g1 ! else goto _longjmp;
1:
	jmp %g1
	 nop
	unimp 0

#else /* PIC */

ENTRY(sigsetjmp)
	cmp %o1,0
	be _C_LABEL(_setjmp)
	 st %o1,[%o0+40]		! jmpbuf[JBLEN]
	nop
	ba,a _C_LABEL(setjmp)
	unimp 0

ENTRY(siglongjmp)
	ld [%o0+40],%g1
	cmp %g1,0
	be _C_LABEL(_longjmp)
	 nop
	ba,a _C_LABEL(_longjmp)
	unimp 0

#endif /* PIC */
