/*      $OpenBSD: trap.h,v 1.4 2004/10/08 07:13:44 grange Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: Utah Hdr: trap.h 1.1 90/07/09
 *	from: @(#)trap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MIPS_TRAP_H_
#define _MIPS_TRAP_H_

/*
 * Trap codes also known in trap.c for name strings.
 * Used for indexing so modify with care.
 */

#define T_INT			0	/* Interrupt pending */
#define T_TLB_MOD		1	/* TLB modified fault */
#define T_TLB_LD_MISS		2	/* TLB miss on load or ifetch */
#define T_TLB_ST_MISS		3	/* TLB miss on a store */
#define T_ADDR_ERR_LD		4	/* Address error on a load or ifetch */
#define T_ADDR_ERR_ST		5	/* Address error on a store */
#define T_BUS_ERR_IFETCH	6	/* Bus error on an ifetch */
#define T_BUS_ERR_LD_ST		7	/* Bus error on a load or store */
#define T_SYSCALL		8	/* System call */
#define T_BREAK			9	/* Breakpoint */
#define T_RES_INST		10	/* Reserved instruction exception */
#define T_COP_UNUSABLE		11	/* Coprocessor unusable */
#define T_OVFLOW		12	/* Arithmetic overflow */
#define	T_TRAP			13	/* Trap instruction */
#define	T_VCEI			14	/* Virtual coherency instruction */
#define	T_FPE			15	/* Floating point exception */
#define	T_IWATCH		16	/* Inst. Watch address reference */
#define	T_DWATCH		23	/* Data Watch address reference */
#define T_VCED			31	/* Virtual coherency data */

#define	T_USER			0x20	/* user-mode flag or'ed with type */

/*
 *  Defines for trap handler catching kernel accessng memory.
 */
#define	KT_BADERR	1		/* Bad address */
#define	KT_COPYERR	2		/* User space copy error */
#define	KT_KCOPYERR	3		/* Kernel space copy error */
#define	KT_DDBERR	4		/* DDB access error */


#ifndef _LOCORE

#if defined(DDB) || defined(DEBUG)

struct trapdebug {              /* trap history buffer for debugging */
	u_int	status;
        u_int	cause;
        u_long	vadr;
        u_long   pc;
        u_long   ra;
        u_long   sp;
        u_int   code;
	u_int	cpl;
};

#define	trapdebug_enter(x, cd) {	\
	u_int32_t __s = disableintr();	\
	trp->status = x->sr;		\
	trp->cause = x->cause;		\
	trp->vadr = x->badvaddr;	\
	trp->pc = x->pc;		\
	trp->sp = x->sp;		\
	trp->ra = x->ra;		\
	trp->cpl = x->cpl;		\
	trp->code = cd;				\
	if (++trp == &trapdebug[TRAPSIZE])	\
		trp = trapdebug;		\
	setsr(__s);				\
 }

#define TRAPSIZE 10		/* Trap log buffer length */
extern struct trapdebug trapdebug[TRAPSIZE], *trp;

void trapDump __P((char *msg));

#else

#define	trapdebug_enter(x, y)

#endif
#endif /* _LOCORE */

#endif /* !_MIPS_TRAP_H_ */
