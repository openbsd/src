/*	$OpenBSD: vector.c,v 1.6 1997/01/16 04:04:35 kstailey Exp $	*/
/*	$NetBSD: vector.c,v 1.15 1996/11/20 18:57:40 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The interrupt vector table.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include "vector.h"

#define BADTRAP16 badtrap, badtrap, badtrap, badtrap, \
                  badtrap, badtrap, badtrap, badtrap, \
                  badtrap, badtrap, badtrap, badtrap, \
                  badtrap, badtrap, badtrap, badtrap

void addrerr __P((void));
void badtrap __P((void));
void buserr __P((void));
void chkinst __P((void));
void coperr __P((void));
void fmterr __P((void));
void fpfline __P((void));
void fpunsupp __P((void));
void illinst __P((void));
void privinst __P((void));
void trace __P((void));
void trap0 __P((void));
void trap1 __P((void));
void trap12 __P((void));
void trap15 __P((void));
void trap2 __P((void));
void trapvinst __P((void));
void zerodiv __P((void));
void fpfault __P((void));

void _isr_autovec __P((void));

#define fpbsun fpfault
#define fpdz fpfault
#define fpinex fpfault
#define fpoperr fpfault
#define fpovfl fpfault
#define fpsnan fpfault
#define fpunfl fpfault

void (*vector_table[NVECTORS]) __P((void)) = {
	(void*)0xfffe000,		/* 0: NOT USED (reset SP) */
	(void*)0xfef0000,		/* 1: NOT USED (reset PC) */
	buserr,				/* 2: bus error */
	addrerr,			/* 3: address error */
	illinst,			/* 4: illegal instruction */
	zerodiv,			/* 5: zero divide */
	chkinst,			/* 6: CHK instruction */
	trapvinst,			/* 7: TRAPV instruction */
	privinst,			/* 8: privilege violation */
	trace,				/* 9: trace (single-step) */
	illinst,			/* 10: line 1010 emulator */
	fpfline,			/* 11: line 1111 emulator */
	badtrap,			/* 12: unassigned, reserved */
	coperr,				/* 13: coprocessor protocol violatio */
	fmterr,				/* 14: format error */
	badtrap,			/* 15: uninitialized interrupt vecto */
	badtrap,			/* 16: unassigned, reserved */
	badtrap,			/* 17: unassigned, reserved */
	badtrap,			/* 18: unassigned, reserved */
	badtrap,			/* 19: unassigned, reserved */
	badtrap,			/* 20: unassigned, reserved */
	badtrap,			/* 21: unassigned, reserved */
	badtrap,			/* 22: unassigned, reserved */
	badtrap,			/* 23: unassigned, reserved */
	_isr_autovec,			/* 24: spurious interrupt */
	_isr_autovec,			/* 25: level 1 interrupt autovector */
	_isr_autovec,			/* 26: level 2 interrupt autovector */
	_isr_autovec,			/* 27: level 3 interrupt autovector */
	_isr_autovec,			/* 28: level 4 interrupt autovector */
	_isr_autovec,			/* 29: level 5 interrupt autovector */
	_isr_autovec,			/* 30: level 6 interrupt autovector */
	_isr_autovec,			/* 31: level 7 interrupt autovector */
	trap0,				/* 32: syscalls (at least on hp300) */
	trap1,				/* 33: sigreturn syscall */
	trap2,				/* 34: HPUX breakpoint */
	illinst,			/* 35: TRAP instruction vector */
	illinst,			/* 36: TRAP instruction vector */
	illinst,			/* 37: TRAP instruction vector */
	illinst,			/* 38: TRAP instruction vector */
	illinst,			/* 39: TRAP instruction vector */
	illinst,			/* 40: TRAP instruction vector */
	illinst,			/* 41: TRAP instruction vector */
	illinst,			/* 42: TRAP instruction vector */
	illinst,			/* 43: TRAP instruction vector */
	trap12,  			/* 44: TRAP 12: cachectl */
	illinst,			/* 45: TRAP instruction vector */
	illinst,			/* 46: TRAP instruction vector */
	trap15,				/* 47: TRAP 15: breakpoint */
	fpbsun, 			/* 48: FPCP branch/set on unordered */
	fpinex, 			/* 49: FPCP inexact result */
	fpdz,   			/* 50: FPCP divide by zero */
	fpunfl, 			/* 51: FPCP underflow */
	fpoperr,			/* 52: FPCP operand error */
	fpovfl, 			/* 53: FPCP overflow */
	fpsnan, 			/* 54: FPCP signalling NAN */
	fpunsupp,			/* 55: FPCP unimplemented data type */
	badtrap,			/* 56: unassigned, reserved */
	badtrap,			/* 57: unassigned, reserved */
	badtrap,			/* 58: unassigned, reserved */
	badtrap,			/* 59: unassigned, reserved */
	badtrap,			/* 60: unassigned, reserved */
	badtrap,			/* 61: unassigned, reserved */
	badtrap,			/* 62: unassigned, reserved */
	badtrap,			/* 63: unassigned, reserved */

	/* 64-255: set later by isr_add_vectored() */

	BADTRAP16,	BADTRAP16,	BADTRAP16,	BADTRAP16,
	BADTRAP16,	BADTRAP16,	BADTRAP16,	BADTRAP16,
	BADTRAP16,	BADTRAP16,	BADTRAP16,	BADTRAP16,
};
