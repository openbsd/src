/*	$OpenBSD: reg.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/* 
 * Copyright (c) 1990,1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: regs.h 1.6 94/12/14$
 *	Author: Bob Wheeler, University of Utah CSL
 */

#ifndef _MACHINE_REG_H_
#define _MACHINE_REG_H_

/*
 * constants for registers for use with the following routines:
 * 
 *     void mtctl(reg, value)	- move to control register
 *     int mfctl(reg)		- move from control register
 *     int mtsp(sreg, value)	- move to space register
 *     int mfsr(sreg)		- move from space register
 */

#define	CR_RCTR		0
#define	CR_PIDR1	8
#define	CR_PIDR2	9
#define	CR_CCR		10
#define	CR_SAR		11
#define	CR_PIDR3	12
#define	CR_PIDR4	13
#define	CR_IVA		14
#define	CR_EIEM		15
#define	CR_ITMR		16
#define	CR_PCSQ		17
#define	CR_PCOQ		18
#define	CR_IIR		19
#define	CR_ISR		20
#define	CR_IOR		21
#define	CR_IPSW		22
#define	CR_EIRR		23
#define	CR_CPUINFO	24
#define	CR_VTOP		25
#define	CR_UPADDR	30	/* paddr of U-area of curproc */
#define	CR_TR7		31

#define	HPPA_NREGS	(32)
#define	HPPA_NFPREGS	(33)	/* 33rd is used for r0 in fpemul */

#ifndef _LOCORE

struct reg {
	u_int64_t r_regs[HPPA_NREGS];	/* r0 is sar */
	u_int64_t r_pc;
	u_int64_t r_npc;
};

struct fpreg {
	u_int64_t fpr_regs[HPPA_NFPREGS];
};
#endif /* !_LOCORE */

#endif /* _MACHINE_REG_H_ */
