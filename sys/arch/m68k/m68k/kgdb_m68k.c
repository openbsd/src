/*	$OpenBSD: kgdb_m68k.c,v 1.2 2003/06/02 23:27:48 millert Exp $	*/
/*	$NetBSD: kgdb_m68k.c,v 1.1 1997/02/12 00:58:01 gwr Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	@(#)kgdb_stub.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Machine-dependent (m68k) part of the KGDB remote "stub"
 */

#include <sys/param.h>
#include <sys/kgdb.h>

#include <machine/frame.h>
#include <machine/trap.h>

/*
 * Translate a trap number into a unix compatible signal value.
 * (gdb only understands unix signal numbers).
 */
int 
kgdb_signal(type)
	int type;
{
	int sigval;

	switch (type) {

	case T_ASTFLT:
	case T_SSIR:
		sigval = SIGINT;
		break;

	case T_ILLINST:
	case T_PRIVINST:
	case T_FMTERR:
		sigval = SIGILL;
		break;

	case T_TRACE:
	case T_TRAP15:
		sigval = SIGTRAP;
		break;

	case T_ZERODIV:
	case T_CHKINST:
	case T_TRAPVINST:
	case T_FPERR:
	case T_COPERR:
		sigval = SIGFPE;
		break;

	case T_BUSERR:
	case T_ADDRERR:
		sigval = SIGBUS;
		break;

	case T_MMUFLT:
		sigval = SIGSEGV;
		break;

	default:
		sigval = SIGEMT;
		break;
	}
	return (sigval);
}

/*
 * Definitions exported from gdb.
 */
/* KGDB_NUMREGS == 18 */

#define GDB_SR 16
#define GDB_PC 17


/*
 * Translate the values stored in the kernel regs struct to/from
 * the format understood by gdb.
 *
 * There is a short pad word between SP (A7) and SR which keeps the
 * kernel stack long word aligned (note that this is in addition to
 * the stack adjust short that we treat as the upper half of the SR
 * (always zero).  We must skip this when copying to/from gdb regs.
 */

void
kgdb_getregs(regs, gdb_regs)
	db_regs_t *regs;
	kgdb_reg_t *gdb_regs;
{
	int i;

	for (i = 0; i < 16; i++)
	    gdb_regs[i]  = regs->tf_regs[i];
	gdb_regs[GDB_SR] = regs->tf_sr;
	gdb_regs[GDB_PC] = regs->tf_pc;
}

void
kgdb_setregs(regs, gdb_regs)
	db_regs_t *regs;
	kgdb_reg_t *gdb_regs;
{
	int i;

	for (i = 0; i < 16; i++)
		regs->tf_regs[i] = gdb_regs[i];
	regs->tf_sr = gdb_regs[GDB_SR] |
		(regs->tf_sr & PSL_T);
	regs->tf_pc = gdb_regs[GDB_PC];
}
