/*	$OpenBSD: vectors.s,v 1.7 2004/11/25 18:32:11 miod Exp $	*/
|	$NetBSD: vectors.s,v 1.10 1998/08/12 06:58:42 scottr Exp $

| Copyright (c) 1988 University of Utah
| Copyright (c) 1990 Regents of the University of California.
| All rights reserved.
|
| Redistribution and use in source and binary forms, with or without
| modification, are permitted provided that the following conditions
| are met:
| 1. Redistributions of source code must retain the above copyright
|    notice, this list of conditions and the following disclaimer.
| 2. Redistributions in binary form must reproduce the above copyright
|    notice, this list of conditions and the following disclaimer in the
|    documentation and/or other materials provided with the distribution.
| 3. Neither the name of the University nor the names of its contributors
|    may be used to endorse or promote products derived from this software
|    without specific prior written permission.
|
| THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
| ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
| IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
| ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
| FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
| DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
| OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
| HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
| LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
| OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
| SUCH DAMAGE.
|
|	@(#)vectors.s	7.2 (Berkeley) 5/7/91
|

#define		BADTRAP16		\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap) ;	\
	VECTOR(badtrap) ; VECTOR(badtrap)

	.data
GLOBAL(vectab)
	VECTOR_UNUSED		/* 0: NOT USED (reset SSP) */
	VECTOR_UNUSED		/* 1: NOT USED (reset PC) */
	VECTOR_UNUSED		/* 2: bus error */
	VECTOR_UNUSED		/* 3: address error */
	VECTOR(illinst)		/* 4: illegal instruction */
	VECTOR(zerodiv)		/* 5: zero divide */
	VECTOR(chkinst)		/* 6: CHK instruction */
	VECTOR(trapvinst)	/* 7: TRAPV instruction */
	VECTOR(privinst)	/* 8: privilege violation */
#if defined(MRG_TRACE)
	VECTOR(mrg_tracetrap)	/* 9: trace */
#else /* MRG_TRACE */
	VECTOR(trace)		/* 9: trace */
#endif /* MRG_TRACE */
	VECTOR(alinetrap)	/* 10: line 1010 emulator ; see macromasm.s */
	VECTOR(fpfline)		/* 11: line 1111 emulator */
	VECTOR(badtrap)		/* 12: unassigned, reserved */
	VECTOR(coperr)		/* 13: coprocessor protocol violation */
	VECTOR(fmterr)		/* 14: format error */
	VECTOR(badtrap)		/* 15: uninitialized interrupt vector */
	VECTOR(badtrap)		/* 16: unassigned, reserved */
	VECTOR(badtrap)		/* 17: unassigned, reserved */
	VECTOR(badtrap)		/* 17: unassigned, reserved */
	VECTOR(badtrap)		/* 19: unassigned, reserved */
	VECTOR(badtrap)		/* 20: unassigned, reserved */
	VECTOR(badtrap)		/* 21: unassigned, reserved */
	VECTOR(badtrap)		/* 22: unassigned, reserved */
	VECTOR(badtrap)		/* 23: unassigned, reserved */
	VECTOR(spurintr)	/* 24: spurious interrupt */
	VECTOR(lev1intr)	/* 25: level 1 interrupt autovector */
	VECTOR(lev2intr)	/* 26: level 2 interrupt autovector */
	VECTOR(intrhand)	/* 27: level 3 interrupt autovector */
	VECTOR(intrhand)	/* 28: level 4 interrupt autovector */
	VECTOR(intrhand)	/* 29: level 5 interrupt autovector */
	VECTOR(intrhand)	/* 30: level 6 interrupt autovector */
	VECTOR(lev7intr)	/* 31: level 7 interrupt autovector */
	VECTOR(trap0)		/* 32: syscalls */
	VECTOR(trap1)		/* 33: sigreturn syscall or breakpoint */
	VECTOR(trap2)		/* 34: breakpoint or sigreturn syscall */
	VECTOR(illinst)		/* 35: TRAP instruction vector */
	VECTOR(illinst)		/* 36: TRAP instruction vector */
	VECTOR(illinst)		/* 37: TRAP instruction vector */
	VECTOR(illinst)		/* 38: TRAP instruction vector */
	VECTOR(illinst)		/* 39: TRAP instruction vector */
	VECTOR(illinst)		/* 40: TRAP instruction vector */
	VECTOR(illinst)		/* 41: TRAP instruction vector */
	VECTOR(illinst)		/* 42: TRAP instruction vector */
	VECTOR(illinst)		/* 43: TRAP instruction vector */
	VECTOR(trap12)		/* 44: TRAP instruction vector */
	VECTOR(illinst)		/* 45: TRAP instruction vector */
	VECTOR(illinst)		/* 46: TRAP instruction vector */
	VECTOR(trap15)		/* 47: TRAP instruction vector */

GLOBAL(fpvect_tab)
	VECTOR(fpfault)		/* 48: FPCP branch/set on unordered cond */
	VECTOR(fpfault)		/* 49: FPCP inexact result */
	VECTOR(fpfault)		/* 50: FPCP divide by zero */
	VECTOR(fpfault)		/* 51: FPCP underflow */
	VECTOR(fpfault)		/* 52: FPCP operand error */
	VECTOR(fpfault)		/* 53: FPCP overflow */
	VECTOR(fpfault)		/* 54: FPCP signalling NAN */
GLOBAL(fpvect_end)

	VECTOR(fpunsupp)	/* 55: FPCP unimplemented data type */
	VECTOR(badtrap)		/* 56: unassigned, reserved */
	VECTOR(badtrap)		/* 57: unassigned, reserved */
	VECTOR(badtrap)		/* 58: unassigned, reserved */
	VECTOR(badtrap)		/* 59: unassigned, reserved */
	VECTOR(badtrap)		/* 60: unassigned, reserved */
	VECTOR(badtrap)		/* 61: unassigned, reserved */
	VECTOR(badtrap)		/* 62: unassigned, reserved */
	VECTOR(badtrap)		/* 63: unassigned, reserved */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */
	BADTRAP16		/* 64-255: user interrupt vectors */

#ifdef FPSP
	/*
	 * 68040: this chunk of vectors is copied into the fpfault zone
	 */
GLOBAL(fpsp_tab)
	ASVECTOR(fpsp_bsun)	/* 48: FPCP branch/set on unordered cond */
	ASVECTOR(inex)		/* 49: FPCP inexact result */
	ASVECTOR(dz)		/* 50: FPCP divide by zero */
	ASVECTOR(fpsp_unfl)	/* 51: FPCP underflow */
	ASVECTOR(fpsp_operr)	/* 52: FPCP operand error */
	ASVECTOR(fpsp_ovfl)	/* 53: FPCP overflow */
	ASVECTOR(fpsp_snan)	/* 54: FPCP signalling NAN */
#endif /* FPSP */
