/*	$NetBSD: trap.h,v 1.3 1994/10/26 08:24:44 cgd Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND HELSINKI UNIVERSITY OF TECHNOLOGY ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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
 * 	File: ns532/trap.h
 *	Author: Tatu Ylonen, Helsinki University of Technology 1992.
 *      Modified for NetBSD by Phil Nelson
 *	Hardware trap vectors for ns532.
 */

#ifndef _MACHINE_TRAP_H_
#define _MACHINE_TRAP_H_

#define T_NVI		0	/* non-vectored interrupt */
#define T_NMI		1	/* non-maskable interrupt */
#define T_ABT		2	/* abort */
#define T_SLAVE		3	/* coprocessor trap */
#define T_ILL		4       /* illegal operation in user mode */
#define T_SVC		5	/* supervisor call */
#define T_DVZ		6	/* divide by zero */
#define T_FLG		7	/* flag instruction */
#define T_BPT		8	/* breakpoint instruction */
#define T_TRC		9	/* trace trap */
#define T_UND		10	/* undefined instruction */
#define T_RBE		11	/* restartable bus error */
#define T_NBE		12	/* non-restartable bus error */
#define T_OVF		13	/* integer overflow trap */
#define T_DBG		14	/* debug trap */
#define T_RESERVED	15	/* reserved */

/* Not a real trap. */
#define T_WATCHPOINT	17	/* watchpoint */

/* To allow for preemption */
#define T_INTERRUPT	18	/* trap code from interrupt! */

/* To include system/user mode in the trap information. */
#define T_USER		32

#define PARRDU_PHYS		0x28000040	/* Read parity error */
#define PARCLU_PHYS		0x28000050	/* Clear parity error */

#define PARRDU_VM		0xFFC80040	/* Read parity error */
#define PARCLU_VM		0xFFC80050	/* Clear parity error */

/* memory management status register bits and meanings. */
#define MSR_STT		0xf0	/* CPU status. */
#define   STT_SEQ_INS	0x80	   /* Sequential instruction fetch */
#define   STT_NSQ_INS	0x90	   /* Non-sequential instruction fetch */
#define   STT_DATA	0xa0	   /* Data transfer */
#define   STT_RMW	0xb0	   /* Read/modify/write */
#define   STT_REA	0xc0	   /* Read for effective address */

#define MSR_UST		0x08	/* User/supervisor */
#define   UST_USER	0x08	   /* User mode is 1.  Super = 0 */

#define MSR_DDT		0x04	/* Data Direction */
#define   DDT_WRITE	0x04	   /* Write is 1.  Read is 0 */

#define MSR_TEX		0x03	/* Exception kind. */
#define   TEX_PTE1	0x01	   /* First level PTE invalid */
#define   TEX_PTE2	0x02	   /* Second level PTE invalid */
#define   TEX_PROT	0x03	   /* Protection violation */

#endif
