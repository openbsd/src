/*      $OpenBSD: trap.h,v 1.12 2003/06/02 23:27:57 millert Exp $     */
/*      $NetBSD: trap.h,v 1.18 2000/06/04 02:19:26 matt Exp $     */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)trap.h	5.4 (Berkeley) 5/9/91
 */

/*
 * Trap type values
 * also known in trap.c for name strings
 */
#ifndef _VAX_TRAP_H_
#define _VAX_TRAP_H_

#define	T_RESADFLT	0	/* reserved addressing */
#define	T_PRIVINFLT	1	/* privileged instruction */
#define	T_RESOPFLT	2	/* reserved operand */
#define	T_BPTFLT	3	/* breakpoint instruction */
#define	T_XFCFLT	4	/* Customer reserved instruction */
#define	T_SYSCALL	5	/* system call (kcall) */
#define	T_ARITHFLT	6	/* arithmetic trap */
#define	T_ASTFLT	7	/* system forced exception */
#define	T_PTELEN	8	/* Page table length exceeded */
#define	T_TRANSFLT	9	/* translation fault */
#define	T_TRCTRAP	10	/* trace trap */
#define	T_COMPAT	11	/* compatibility mode fault on VAX */
#define	T_ACCFLT	12	/* Access violation fault */
#define	T_KSPNOTVAL	15	/* kernel stack pointer not valid */
#define	T_KDBTRAP	17	/* kernel debugger trap */

/* These gets ORed with the word for page handling routines */
#define	T_WRITE		0x80
#define	T_PTEFETCH	0x40

/* Trap's coming from user mode */
#define	T_USER	0x100

#ifndef _LOCORE
struct	trapframe {
	long	fp;	/* Stack frame pointer */
	long	ap;     /* Argument pointer on user stack */
	long	sp;	/* Stack pointer */
	long	r0;     /* General registers saved upon trap/syscall */
	long	r1;
	long	r2;
	long	r3;
	long	r4;
	long	r5;
	long	r6;
	long	r7;
	long	r8;
	long	r9;
	long	r10;
	long	r11;
	long	trap;	/* Type of trap */
        long	code;   /* Trap specific code */
        long	pc;     /* User pc */
        long	psl;    /* User psl */
};

#endif /* _LOCORE */

#endif /* _VAX_TRAP_H_ */
