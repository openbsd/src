/*	$OpenBSD: psl.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

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

#ifndef _MACHINE_PSL_H_
#define _MACHINE_PSL_H_

/*
 * Rference:
 * 1. PA-RISC 1.1 Architecture and Instruction Set Manual
 *    Hewlett Packard, 3rd Edition, February 1994; Part Number 09740-90039
 */

/*
 * Processor Status Word Bit Positions (in PA-RISC bit order)
 */
#define	PSL_Y_POS	(0)
#define	PSL_Z_POS	(1)
#define	PSL_W_POS	(4)
#define	PSL_E_POS	(5)
#define	PSL_S_POS	(6)
#define	PSL_T_POS	(7)
#define	PSL_H_POS	(8)
#define	PSL_L_POS	(9)
#define	PSL_N_POS	(10)
#define	PSL_X_POS	(11)
#define	PSL_B_POS	(12)
#define	PSL_C_POS	(13)
#define	PSL_V_POS	(14)
#define	PSL_M_POS	(15)
#define	PSL_CB_POS	(16)
#define	PSL_O_POS	(24)
#define	PSL_G_POS	(25)
#define	PSL_F_POS	(26)
#define	PSL_R_POS	(27)
#define	PSL_Q_POS	(28)
#define	PSL_P_POS	(29)
#define	PSL_D_POS	(30)
#define	PSL_I_POS	(31)

#define	PSL_BITS	"\020\001I\002D\003P\004Q\005R\006F\007G\010O"  \
			"\021M\022V\023C\024B\025X\026N\027L\030H" \
			"\031T\032S\033E\034W\037Z\040Y"

/*
 * Processor Status Word Bit Values
 */
#define	PSL_Y	(1 << (31-PSL_Y_POS))	/* Data Debug Trap Disable */
#define	PSL_Z	(1 << (31-PSL_Z_POS))	/* Instruction Debug Trap Disable */
#define	PSL_W	(1 << (31-PSL_W_POS))	/* 64bit address decode enable */
#define	PSL_E	(1 << (31-PSL_E_POS))	/* Little Endian Memory Access Enable */
#define	PSL_S	(1 << (31-PSL_S_POS))	/* Secure Interval Timer */
#define	PSL_T	(1 << (31-PSL_T_POS))	/* Taken Branch Trap Enable */
#define	PSL_H	(1 << (31-PSL_H_POS))	/* Higher-privilege xfer Trap Enable */
#define	PSL_L	(1 << (31-PSL_L_POS))	/* Lower-privilege xfer Trap Enable */
#define	PSL_N	(1 << (31-PSL_N_POS))	/* Nullify */
#define	PSL_X	(1 << (31-PSL_X_POS))	/* Data Memory Break Disable */
#define	PSL_B	(1 << (31-PSL_B_POS))	/* Taken Branch */
#define	PSL_C	(1 << (31-PSL_C_POS))	/* Instruction Address Translation */
#define	PSL_V	(1 << (31-PSL_V_POS))	/* Divide Step Correction */
#define	PSL_M	(1 << (31-PSL_M_POS))	/* High-priority Machine Check Mask */
#define	PSL_CB	(1 << (31-PSL_CB_POS))	/* Carry/Borrow Bits */
#define	PSL_O	(1 << (31-PSL_O_POS))	/* Force strong ordering (2.0) */
#define	PSL_G	(1 << (31-PSL_G_POS))	/* Debug Trap Enable */
#define	PSL_F	(1 << (31-PSL_F_POS))	/* Perfomance Monitor Interrupt */
#define	PSL_R	(1 << (31-PSL_R_POS))	/* Recover Counter Enable */
#define	PSL_Q	(1 << (31-PSL_Q_POS))	/* Interrupt State Collection Enable */
#define	PSL_P	(1 << (31-PSL_P_POS))	/* Protection Identifier Validation */
#define	PSL_D	(1 << (31-PSL_D_POS))	/* Data Address Translation Enable */
#define	PSL_I	(1 << (31-PSL_I_POS))	/* External Interrupt, Power Failure
					   Interrupt, and Low-Priority Machine
					   Check Interrupt unmask */

/*
 * Frequently Used PSW Values
 */
#define	RESET_PSL	(PSL_R | PSL_Q | PSL_P | PSL_D | PSL_I)

#endif  /* _MACHINE_PSL_H_ */
