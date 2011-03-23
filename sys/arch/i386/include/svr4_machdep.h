/*	$OpenBSD: svr4_machdep.h,v 1.8 2011/03/23 16:54:35 pirofti Exp $	 */
/*	$NetBSD: svr4_machdep.h,v 1.5 1995/03/31 02:51:37 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_SVR4_MACHDEP_H_
#define	_MACHINE_SVR4_MACHDEP_H_

#include <compat/svr4/svr4_types.h>

/*
 * Machine dependent portions [X86]
 */

#define SVR4_X86_GS	0
#define SVR4_X86_FS	1
#define SVR4_X86_ES	2
#define SVR4_X86_DS	3
#define SVR4_X86_EDI	4
#define SVR4_X86_ESI	5
#define SVR4_X86_EBP	6
#define SVR4_X86_ESP	7
#define SVR4_X86_EBX	8
#define SVR4_X86_EDX	9
#define SVR4_X86_ECX	10
#define SVR4_X86_EAX	11
#define SVR4_X86_TRAPNO	12
#define SVR4_X86_ERR	13
#define SVR4_X86_EIP	14
#define SVR4_X86_CS	15
#define SVR4_X86_EFL	16
#define SVR4_X86_UESP	17
#define SVR4_X86_SS	18
#define SVR4_X86_MAXREG	19


typedef int svr4_greg_t;
typedef svr4_greg_t svr4_gregset_t[SVR4_X86_MAXREG];

typedef struct {
    int		f_x87[62];	/* x87 registers */
    long	f_weitek[33]; 	/* weitek */
} svr4_fregset_t;

struct svr4_ucontext;

#ifdef _KERNEL
void svr4_getcontext(struct proc *, struct svr4_ucontext *, int, int);
int svr4_setcontext(struct proc *, struct svr4_ucontext *);
void svr4_sendsig(sig_t, int, int, u_long, int, union sigval);
#endif

typedef struct {
	svr4_gregset_t	greg;
	svr4_fregset_t	freg;
} svr4_mcontext_t;

/*
 * SYSARCH numbers
 */
#define SVR4_SYSARCH_FPHW	40
#define SVR4_SYSARCH_DSCR	75

struct svr4_ssd {
	unsigned int selector;
	unsigned int base;
	unsigned int limit;
	unsigned int access1;
	unsigned int access2;
};

#define SVR4_SYSARCH_GOSF	114	/* get OS features vector */

/*
 * Processor traps
 */
#define	SVR4_T_DIVIDE		0
#define	SVR4_T_TRCTRAP		1
#define	SVR4_T_NMI		2
#define	SVR4_T_BPTFLT		3
#define	SVR4_T_OFLOW		4
#define	SVR4_T_BOUND		5
#define	SVR4_T_PRIVINFLT	6
#define	SVR4_T_DNA		7
#define	SVR4_T_DOUBLEFLT	8
#define	SVR4_T_FPOPFLT		9
#define	SVR4_T_TSSFLT		10
#define	SVR4_T_SEGNPFLT		11
#define	SVR4_T_STKFLT		12
#define	SVR4_T_PROTFLT		13
#define	SVR4_T_PAGEFLT		14
#define	SVR4_T_ALIGNFLT		17

#endif /* !_MACHINE_SVR4_MACHDEP_H_ */
