/*	$OpenBSD: trap.h,v 1.4 1999/02/09 06:36:27 smurph Exp $ */
/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 * Trap codes 
 */

#ifndef _M88K_TRAP_H 
#define _M88K_TRAP_H  1

/*
 * Trap type values
 */

#define T_RESADFLT	0		/* reserved addressing fault */
#define T_PRIVINFLT	1		/* privileged instruction fault */
#define T_RESOPFLT	2		/* reserved operand fault */

/* End of known constants */

#define T_INSTFLT	3		/* instruction access exception */
#define T_DATAFLT	4		/* data access exception */
#define T_MISALGNFLT	5		/* misaligned access exception */
#define T_ILLFLT	6		/* unimplemented opcode exception */
#define T_BNDFLT	7		/* bounds check violation exception */
#define T_ZERODIV	8		/* illegal divide exception */
#define T_OVFFLT	9		/* integer overflow exception */
#define T_ERRORFLT	10		/* error exception */
#define T_FPEPFLT	11		/* floating point precise exception */
#define T_FPEIFLT	12		/* floating point imprecise exception */
#define	T_ASTFLT	13		/* software trap */
#if	DDB
#define	T_KDB_ENTRY	14		/* force entry to kernel debugger */
#define T_KDB_BREAK	15		/* break point hit */
#define T_KDB_TRACE	16		/* trace */
#endif /* DDB */
#define T_UNKNOWNFLT	17		/* unknown exception */
#define T_SIGTRAP	18		/* generate SIGTRAP */
#define T_SIGSYS	19		/* generate SIGSYS */
#define	T_STEPBPT	20		/* special breakpoint for single step */
#define	T_USERBPT	21		/* user set breakpoint (for debugger) */
#define	T_SYSCALL	22		/* Syscall */
#define T_USER		23		/* user mode fault */
#if	DDB
#define	T_KDB_WATCH	24		/* watchpoint hit */
#endif /* DDB */

#endif  _M88K_TRAP_H 

