/*	$OpenBSD: trap.h,v 1.3 2006/05/08 14:03:34 miod Exp $ */
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
#ifndef __MACHINE_TRAP_H__
#define __MACHINE_TRAP_H__

/*
 * Trap type values
 */

#define T_PRIVINFLT	0	/* privileged instruction fault */
#define T_INSTFLT	1	/* instruction access exception */
#define T_DATAFLT	2	/* data access exception */
#define T_MISALGNFLT	3	/* misaligned access exception */
#define T_ILLFLT	4	/* unimplemented opcode exception */
#define T_BNDFLT	5	/* bounds check violation exception */
#define T_ZERODIV	6	/* illegal divide exception */
#define T_OVFFLT	7	/* integer overflow exception */
#define T_FPEPFLT	8	/* floating point precise exception */
#define T_ASTFLT	9	/* software trap */
#define	T_KDB_ENTRY	10	/* force entry to kernel debugger */
#define T_KDB_BREAK	11	/* break point hit */
#define T_KDB_TRACE	12	/* trace */
#define T_UNKNOWNFLT	13	/* unknown exception */
#define T_SIGSYS	14	/* generate SIGSYS */
#define T_STEPBPT	15	/* special breakpoint for single step */
#define T_USERBPT	16	/* user set breakpoint (for debugger) */
#define T_INT		17	/* interrupt exception */
#define T_NON_MASK	18	/* MVME197 Non-Maskable Interrupt */
#define	T_110_DRM	19	/* 88110 data read miss (sw table walk) */
#define	T_110_DWM	20	/* 88110 data write miss (sw table walk) */
#define	T_110_IAM	21	/* 88110 inst ATC miss (sw table walk) */
#define T_USER		22	/* user mode fault */

#ifndef _LOCORE
void cache_flush(struct trapframe *);
void m88100_syscall(register_t, struct trapframe *);
void m88100_trap(unsigned, struct trapframe *);
void m88110_syscall(register_t, struct trapframe *);
void m88110_trap(unsigned, struct trapframe *);
#endif /* _LOCORE */

#endif /* __MACHINE_TRAP_H__ */
