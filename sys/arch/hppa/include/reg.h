/*	$OpenBSD: reg.h,v 1.1 1998/07/07 21:32:45 mickey Exp $	*/

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

/*
 * constants for registers for use with the following routines:
 * 
 *     void mtctl(reg, value)	- move to control register
 *     int mfctl(reg)		- move from control register
 *     int mtsp(sreg, value)	- move to space register
 *     int mfsr(sreg)		- move from space register
 */

#define CR_RCTR	 0
#define CR_PIDR1 8
#define CR_PIDR2 9
#define CR_CCR   10
#define CR_SAR	 11
#define CR_PIDR3 12
#define CR_PIDR4 13
#define CR_IVA	 14
#define CR_EIEM  15
#define CR_ITMR  16
#define CR_PCSQ  17
#define CR_PCOQ  18
#define CR_IIR   19
#define CR_ISR   20
#define CR_IOR   21
#define CR_IPSW  22
#define CR_EIRR  23
#define CR_PTOV  24
#define CR_VTOP  25
#define CR_TR2   26
#define CR_TR3   27
#define CR_TR4   28
#define CR_TR5   29
#define CR_TR6   30
#define CR_TR7   31

#define CCR_MASK 0xff

