/*	$OpenBSD: psl.h,v 1.2 1998/08/29 01:29:59 mickey Exp $	*/

/*
 *  (c) Copyright 1987 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */

/*
 * Copyright (c) 1990,1991,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: psl.h 1.3 94/12/14$
 */

#ifndef _HPPA_PSL_H_
#define _PHPPA_SL_H_

/*
 * Processor Status Word (PSW) Masks
 */
#define	PSW_T	0x01000000	/* Taken Branch Trap Enable */
#define	PSW_H	0x00800000	/* Higher-Privilege Transfer Trap Enable */
#define	PSW_L	0x00400000	/* Lower-Privilege Transfer Trap Enable */
#define	PSW_N	0x00200000	/* PC Queue Front Instruction Nullified */
#define	PSW_X	0x00100000	/* Data Memory Break Disable */
#define	PSW_B	0x00080000	/* Taken Branch in Previous Cycle */
#define	PSW_C	0x00040000	/* Code Address Translation Enable */
#define	PSW_V	0x00020000	/* Divide Step Correction */
#define	PSW_M	0x00010000	/* High-Priority Machine Check Disable */
#define	PSW_CB	0x0000ff00	/* Carry/Borrow Bits */
#define	PSW_R	0x00000010	/* Recovery Counter Enable */
#define	PSW_Q	0x00000008	/* Interruption State Collection Enable */
#define	PSW_P	0x00000004	/* Protection ID Validation Enable */
#define	PSW_D	0x00000002	/* Data Address Translation Enable */
#define	PSW_I	0x00000001	/* External, Power Failure, Low-Priority */
				/* Machine Check Interruption Enable */
/*
 * Software defined PSW masks.
 */
#define PSW_SS	0x10000000	/* Kernel managed single step */

/*
 * PSW Bit Positions
 */
#define	PSW_T_P		7
#define	PSW_H_P		8
#define	PSW_L_P		9
#define	PSW_N_P		10
#define	PSW_X_P		11
#define	PSW_B_P		12
#define	PSW_C_P		13
#define	PSW_V_P		14
#define	PSW_M_P		15
#define	PSW_CB_P	23
#define	PSW_R_P		27
#define	PSW_Q_P		28
#define	PSW_P_P		29
#define	PSW_D_P		30
#define	PSW_I_P		31

/*
 * Kernel PSW Masks
 */
#define	RESET_PSW	(PSW_R | PSW_Q | PSW_P | PSW_D | PSW_I)
#define	KERNEL_PSW	(PSW_C | PSW_Q | PSW_P | PSW_D)
#define	SYSTEM_MASK	(PSW_R | PSW_Q | PSW_P | PSW_D | PSW_I)
#define	GLOBAL_VAR_MASK	(PSW_H | PSW_L | PSW_C | PSW_M)
#define	PER_INST_MASK	(PSW_X | PSW_T | PSW_N | PSW_B | PSW_V | PSW_CB | SYSTEM_MASK)
#define	PTRACE_MASK	(PSW_N | PSW_V | PSW_CB)

/*
 * Macros to decode processor status word.
 */
#define PC_PRIV_MASK    3
#define PC_PRIV_KERN    0
#define PC_PRIV_USER    3
#define USERMODE(pc)    (((pc) & PC_PRIV_MASK) != PC_PRIV_KERN)

#endif  /* _HPPA_PSL_H_ */

