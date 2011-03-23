/*	$OpenBSD: svr4_machdep.h,v 1.8 2011/03/23 16:54:37 pirofti Exp $	*/
/*	$NetBSD: svr4_machdep.h,v 1.4 1996/03/31 22:21:45 pk Exp $	 */

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
 * Machine dependent portions [SPARC]
 */

#define SVR4_SPARC_PSR		0
#define SVR4_SPARC_PC		1
#define SVR4_SPARC_nPC		2
#define SVR4_SPARC_Y		3
#define SVR4_SPARC_G1		4
#define SVR4_SPARC_G2		5
#define SVR4_SPARC_G3		6
#define SVR4_SPARC_G4		7
#define SVR4_SPARC_G5		8
#define SVR4_SPARC_G6		9
#define SVR4_SPARC_G7		10
#define SVR4_SPARC_O0		11
#define SVR4_SPARC_O1		12
#define SVR4_SPARC_O2		13
#define SVR4_SPARC_O3		14
#define SVR4_SPARC_O4		15
#define SVR4_SPARC_O5		16
#define SVR4_SPARC_O6		17
#define SVR4_SPARC_O7		18
#define SVR4_SPARC_MAXREG	19

#define SVR4_SPARC_SP		SVR4_SPARC_O6
#define SVR4_SPARC_PS		SVR4_SPARC_PSR

#define SVR4_SPARC_MAXWIN	31

typedef int svr4_greg_t;

typedef struct {
	svr4_greg_t	rwin_lo[8];
	svr4_greg_t	rwin_in[8];
} svr4_rwindow_t;

typedef struct {
	int		 cnt;
	int		*sp[SVR4_SPARC_MAXWIN];
	svr4_rwindow_t   win[SVR4_SPARC_MAXWIN];
} svr4_gwindow_t;

typedef svr4_greg_t svr4_gregset_t[SVR4_SPARC_MAXREG];

typedef struct {
	union {
		u_int	 fp_ri[32];
		double	 fp_rd[16];
	} fpu_regs;
	void		*fp_q;
	unsigned	 fp_fsr;
	u_char		 fp_nqel;
	u_char		 fp_nqsize;
	u_char		 fp_busy;
} svr4_fregset_t;

typedef struct {
	svr4_gregset_t	 greg;
	svr4_gwindow_t  *gwin;
	svr4_fregset_t	 freg;
	long		 pad[21];
} svr4_mcontext_t;

struct svr4_ucontext;

void svr4_getcontext(struct proc *, struct svr4_ucontext *,
			  int, int);
int svr4_setcontext(struct proc *p, struct svr4_ucontext *);
void svr4_sendsig(sig_t, int, int, u_long, int, union sigval);
int svr4_trap(int, struct proc *);

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
