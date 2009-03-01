/*	$OpenBSD: trap.h,v 1.9 2009/03/01 17:43:23 miod Exp $ */
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
#ifndef __M88K_TRAP_H__
#define __M88K_TRAP_H__

/*
 * Trap type values
 */
#define	T_PRIVINFLT	0	/* privileged instruction fault */
#define	T_INSTFLT	1	/* instruction access exception */
#define	T_DATAFLT	2	/* data access exception */
#define	T_MISALGNFLT	3	/* misaligned access exception */
#define	T_ILLFLT	4	/* unimplemented opcode exception */
#define	T_BNDFLT	5	/* bounds check violation exception */
#define	T_ZERODIV	6	/* illegal divide exception */
#define	T_OVFFLT	7	/* integer overflow exception */
#define	T_FPEPFLT	8	/* floating point precise exception */
#define	T_KDB_ENTRY	9	/* force entry to kernel debugger */
#define	T_KDB_BREAK	10	/* break point hit */
#define	T_KDB_TRACE	11	/* trace */
#define	T_UNKNOWNFLT	12	/* unknown exception */
#define	T_SIGSYS	13	/* generate SIGSYS */
#define	T_STEPBPT	14	/* special breakpoint for single step */
#define	T_USERBPT	15	/* user set breakpoint (for debugger) */
#define	T_110_DRM	16	/* 88110 data read miss (sw table walk) */
#define	T_110_DWM	17	/* 88110 data write miss (sw table walk) */
#define	T_110_IAM	18	/* 88110 inst ATC miss (sw table walk) */

#define	T_USER		19	/* user mode fault */

#ifndef _LOCORE

void	ast(struct trapframe *);
void	cache_flush(struct trapframe *);
void	interrupt(struct trapframe *);
int	nmi(struct trapframe *);
void	nmi_wrapup(struct trapframe *);

void	m88100_syscall(register_t, struct trapframe *);
void	m88100_trap(u_int, struct trapframe *);
void	m88110_syscall(register_t, struct trapframe *);
void	m88110_trap(u_int, struct trapframe *);

void	m88110_fpu_exception(struct trapframe *);

#endif /* _LOCORE */

#endif /* __M88K_TRAP_H__ */
