/*	$OpenBSD: intrcnt.h,v 1.3 1996/07/29 22:58:53 niklas Exp $	*/
/*	$NetBSD: intrcnt.h,v 1.4.4.2 1996/06/05 03:42:24 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#define	INTRNAMES_DEFINITION						\
/* 0x00 */	ASCIZ "clock";						\
		ASCIZ "isa irq 0";					\
		ASCIZ "isa irq 1";					\
		ASCIZ "isa irq 2";					\
		ASCIZ "isa irq 3";					\
		ASCIZ "isa irq 4";					\
		ASCIZ "isa irq 5";					\
		ASCIZ "isa irq 6";					\
		ASCIZ "isa irq 7";					\
		ASCIZ "isa irq 8";					\
		ASCIZ "isa irq 9";					\
		ASCIZ "isa irq 10";					\
		ASCIZ "isa irq 11";					\
		ASCIZ "isa irq 12";					\
		ASCIZ "isa irq 13";					\
		ASCIZ "isa irq 14";					\
/* 0x10 */	ASCIZ "isa irq 15";					\
		ASCIZ "kn20aa irq 0";					\
		ASCIZ "kn20aa irq 1";					\
		ASCIZ "kn20aa irq 2";					\
		ASCIZ "kn20aa irq 3";					\
		ASCIZ "kn20aa irq 4";					\
		ASCIZ "kn20aa irq 5";					\
		ASCIZ "kn20aa irq 6";					\
		ASCIZ "kn20aa irq 7";					\
		ASCIZ "kn20aa irq 8";					\
		ASCIZ "kn20aa irq 9";					\
		ASCIZ "kn20aa irq 10";					\
		ASCIZ "kn20aa irq 11";					\
		ASCIZ "kn20aa irq 12";					\
		ASCIZ "kn20aa irq 13";					\
		ASCIZ "kn20aa irq 14";					\
/* 0x20 */	ASCIZ "kn20aa irq 15";					\
		ASCIZ "kn20aa irq 16";					\
		ASCIZ "kn20aa irq 17";					\
		ASCIZ "kn20aa irq 18";					\
		ASCIZ "kn20aa irq 19";					\
		ASCIZ "kn20aa irq 20";					\
		ASCIZ "kn20aa irq 21";					\
		ASCIZ "kn20aa irq 22";					\
		ASCIZ "kn20aa irq 23";					\
		ASCIZ "kn20aa irq 24";					\
		ASCIZ "kn20aa irq 25";					\
		ASCIZ "kn20aa irq 26";					\
		ASCIZ "kn20aa irq 27";					\
		ASCIZ "kn20aa irq 28";					\
		ASCIZ "kn20aa irq 29";					\
		ASCIZ "kn20aa irq 30";					\
/* 0x30 */	ASCIZ "kn20aa irq 31";					\
		ASCIZ "kn15 tc slot 0";					\
		ASCIZ "kn15 tc slot 1";					\
		ASCIZ "kn15 tc slot 2";					\
		ASCIZ "kn15 tc slot 3";					\
		ASCIZ "kn15 tc slot 4";					\
		ASCIZ "kn15 tc slot 5";					\
		ASCIZ "kn15 tcds";					\
		ASCIZ "kn15 ioasic";					\
		ASCIZ "kn15 sfb";					\
		ASCIZ "kn16 tc slot 0";					\
		ASCIZ "kn16 tc slot 1";					\
		ASCIZ "kn16 tcds";					\
		ASCIZ "kn16 ioasic";					\
		ASCIZ "kn16 sfb";					\
		ASCIZ "tcds esp 0";					\
/* 0x40 */	ASCIZ "tcds esp 1";					\
		ASCIZ "ioasic le";					\
		ASCIZ "ioasic scc 0";					\
		ASCIZ "ioasic scc 1";					\
		ASCIZ "ioasic am79c30";

#define INTRCNT_DEFINITION						\
/* 0x00 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x10 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x20 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x30 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x40 */	.quad 0, 0, 0, 0, 0;

#define	INTRCNT_CLOCK		0
#define	INTRCNT_ISA_IRQ		(INTRCNT_CLOCK + 1)
#define	INTRCNT_ISA_IRQ_LEN	16
#define	INTRCNT_KN20AA_IRQ	(INTRCNT_ISA_IRQ + INTRCNT_ISA_IRQ_LEN)
#define	INTRCNT_KN20AA_IRQ_LEN	32
#define	INTRCNT_KN15		(INTRCNT_KN20AA_IRQ + INTRCNT_KN20AA_IRQ_LEN)
#define	INTRCNT_KN15_LEN	9
#define	INTRCNT_KN16		(INTRCNT_KN15 + INTRCNT_KN15_LEN)
#define	INTRCNT_KN16_LEN	5
#define	INTRCNT_TCDS		(INTRCNT_KN16 + INTRCNT_KN16_LEN)
#define	INTRCNT_TCDS_LEN	2
#define	INTRCNT_IOASIC		(INTRCNT_TCDS + INTRCNT_TCDS_LEN)
#define	INTRCNT_IOASIC_LEN	4

#ifndef _LOCORE
extern volatile long intrcnt[];
#endif
