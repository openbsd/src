/*	$OpenBSD: m88100.h,v 1.9 2001/09/28 20:45:49 miod Exp $ */
/* 
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 * HISTORY
 */
/*
 * M88100 flags
 */

#ifndef __MACHINE_M88100_H__
#define __MACHINE_M88100_H__


/*
 *	88100 RISC definitions
 */

/* DMT0, DMT1, DMT2 */
#define DMT_SKIP	0x00010000	/* skip this dmt in data_access_emulation */
#define DMT_BO		0x00008000	/* Byte-Ordering */
#define DMT_DAS		0x00004000	/* Data Access Space */
#define DMT_DOUB1	0x00002000	/* Double Word */
#define DMT_LOCKBAR	0x00001000	/* Bud Lock */
#define DMT_DREG	0x00000F80	/* Destination Registers 5bits */
#define DMT_SIGNED	0x00000040	/* Sign-Extended Bit */
#define DMT_EN		0x0000003C	/* Byte Enable Bit */
#define DMT_WRITE	0x00000002	/* Read/Write Transaction Bit */
#define	DMT_VALID	0x00000001	/* Valid Transaction Bit */

#ifndef	_LOCORE
#include <sys/types.h>

/* dmt_skip is never set by the cpu.  It is used to 
 * mark 'known' transactions so that they don't get 
 * prosessed by data_access_emulation().  XXX smurph 
 */
struct dmt_reg {
	unsigned int :15,
	dmt_skip:1,   
	dmt_bo:1,
	dmt_das:1,
	dmt_doub1:1,
	dmt_lockbar:1,
	dmt_dreg:5,
	dmt_signed:1,
	dmt_en:4,
	dmt_write:1,
	dmt_valid:1;
};
#endif 

#endif /* __MACHINE_M88100_H__ */
