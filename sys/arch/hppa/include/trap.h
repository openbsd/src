/*	$OpenBSD: trap.h,v 1.3 1999/02/17 20:40:32 mickey Exp $	*/

/* 
 * Copyright (c) 1988-1994, The University of Utah and
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
 * 	Utah $Hdr: trap.h 1.6 94/12/16$
 *
 * 	Utah $Hdr: break.h 1.10 94/12/14$
 *	Author: Bob Wheeler, University of Utah CSL
 */

#ifndef _MACHINE_TRAP_H_
#define _MACHINE_TRAP_H_

/*
 * Trap type values
 * also known in trap.c for name strings
 */

#define	T_NONEXIST	0
#define	T_HPMC		1
#define	T_POWERFAIL	2
#define	T_RECOVERY	3
#define	T_INTERRUPT	4
#define	T_LPMC		5
#define	T_ITLBMISS	6
#define	T_IPROT		7
#define	T_ILLEGAL	8
#define	T_IBREAK	9
#define	T_PRIV_OP	10
#define	T_PRIV_REG	11
#define	T_OVERFLOW	12
#define	T_CONDITION	13
#define	T_EXCEPTION	14
#define	T_DTLBMISS	15
#define	T_ITLBMISSNA	16
#define	T_DTLBMISSNA	17
#define	T_DPROT		18
#define	T_DBREAK       	19
#define	T_TLB_DIRTY	20
#define	T_PAGEREF	21
#define	T_EMULATION	22
#define	T_HIGHERPL	23
#define	T_LOWERPL	24
#define	T_TAKENBR	25

#define	T_DATACC	26	/* 7100 */
#define	T_DATAPID	27	/* 7100 */
#define	T_DATALIGN	28	/* 7100 */

#define	T_ICS_OVFL	30	/* SW: interrupt stack overflow */
#define	T_KS_OVFL	31	/* SW: kernel stack overflow */

#define	T_USER		0x20	/* user-mode flag or'ed with type */

/*  Values for break instructions */

/* values for the im5 field of the break instruction */
#define	HPPA_BREAK_KERNEL	0
#define	HPPA_BREAK_MAYDEBUG	31	/* Reserved for Mayfly debugger. */

/* values for the im13 field of the break instruction */
#define	HPPA_BREAK_PDC_DUMP  		2
#define	HPPA_BREAK_KERNTRACE		3
#define	HPPA_BREAK_MACH_DEBUGGER	4
#define	HPPA_BREAK_KGDB			5
#define	HPPA_BREAK_KERNPRINT		6
#define	HPPA_BREAK_IVA			7
#define	HPPA_BREAK_PDC_IODC_CALL	8
#define	HPPA_BREAK_GDB			8	/* Standard GDB breakpoint.  */

/*
 * Tear apart a break instruction to find its type.
 */
#define	break5(x)	((x) & 0x1F)
#define	break13(x)	(((x) >> 13) & 0x1FFF)

/*
 * Trace debugging.
 */
#define	HPPA_TRACE_OFF	0
#define	HPPA_TRACE_JUMP      -1
#define	HPPA_TRACE_SUSPEND   -2
#define	HPPA_TRACE_RESUME    -3

#endif /* _MACHINE_TRAP_H_ */
