/*	$OpenBSD: kgdb_machdep.c,v 1.1 2005/11/13 17:51:52 fgsch Exp $	*/
/*
 * Copyright (c) 2005 Federico G. Schwindt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/kgdb.h>

int
kgdb_acc(vaddr_t va, size_t len)
{
	vaddr_t last_va;
	pt_entry_t *pte;

	last_va = va + len;
	va &= ~PGOFSET;
	last_va &= PGOFSET;

	do {
		pte = kvtopte(va);
		if ((*pte & PG_V) == 0)
			return (0);
		va += NBPG;
	} while (va < last_va);

	return (1);
}

int
kgdb_signal(int type)
{
	switch (type) {
	default:
		return (SIGEMT);
	}
}

void
kgdb_getregs(db_regs_t *regs, kgdb_reg_t *gdb_regs)
{
	gdb_regs[ 0] = regs->tf_rax;
	gdb_regs[ 1] = regs->tf_rbx;
	gdb_regs[ 2] = regs->tf_rcx;
	gdb_regs[ 3] = regs->tf_rdx;
	gdb_regs[ 4] = regs->tf_rsi;
	gdb_regs[ 5] = regs->tf_rdi;
	gdb_regs[ 6] = regs->tf_rbp;
	gdb_regs[ 7] = regs->tf_rsp;
	gdb_regs[ 8] = regs->tf_r8;
	gdb_regs[ 9] = regs->tf_r9;
	gdb_regs[10] = regs->tf_r10;
	gdb_regs[11] = regs->tf_r11;
	gdb_regs[12] = regs->tf_r12;
	gdb_regs[13] = regs->tf_r13;
	gdb_regs[14] = regs->tf_r14;
	gdb_regs[15] = regs->tf_r15;
	gdb_regs[16] = regs->tf_rip;
	/* XXX: 32bits but defined as 64 */ 
	gdb_regs[17] = regs->tf_rflags;
	gdb_regs[18] = regs->tf_cs;
	gdb_regs[19] = regs->tf_ss;
}

void
kgdb_setregs(db_regs_t *regs, kgdb_reg_t *gdb_regs)
{
	regs->tf_rax = gdb_regs[ 0];
	regs->tf_rbx = gdb_regs[ 1];
	regs->tf_rcx = gdb_regs[ 2];
	regs->tf_rdx = gdb_regs[ 3];
	regs->tf_rsi = gdb_regs[ 4];
	regs->tf_rdi = gdb_regs[ 5];
	regs->tf_rbp = gdb_regs[ 6];
	regs->tf_rsp = gdb_regs[ 7];
	regs->tf_r8  = gdb_regs[ 8];
	regs->tf_r9  = gdb_regs[ 9];
	regs->tf_r10 = gdb_regs[10];
	regs->tf_r11 = gdb_regs[11];
	regs->tf_r12 = gdb_regs[12];
	regs->tf_r13 = gdb_regs[13];
	regs->tf_r14 = gdb_regs[14];
	regs->tf_r15 = gdb_regs[15];
	regs->tf_rip = gdb_regs[16];
	regs->tf_rflags = gdb_regs[17];
	regs->tf_cs  = gdb_regs[18];
	regs->tf_ss  = gdb_regs[19];
}
