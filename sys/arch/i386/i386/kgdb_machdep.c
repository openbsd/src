/*	$OpenBSD: kgdb_machdep.c,v 1.13 2015/06/28 01:11:27 guenther Exp $	*/
/*	$NetBSD: kgdb_machdep.c,v 1.6 1998/08/13 21:36:03 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 Matthias Pfaller.
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

#if defined(DDB)
#error "Can't build DDB and KGDB together."
#endif

/*
 * Machine-dependent functions for remote KGDB.  Originally written
 * for NetBSD/pc532 by Matthias Pfaller.  Modified for NetBSD/i386
 * by Jason R. Thorpe.
 */

#include <sys/param.h>
#include <sys/kgdb.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <machine/reg.h>
#include <machine/trap.h>

/*
 * Determine if the memory at va..(va+len) is valid.
 */
int
kgdb_acc(vaddr_t va, size_t len)
{
	vaddr_t last_va;

	last_va = va + len;
	va  &= ~PGOFSET;
	last_va &= ~PGOFSET;

	do {
		if ((pmap_pte_bits(va) & PG_V) == 0)
			return (0);
		va  += NBPG;
	} while (va < last_va);

	return (1);
}

/*
 * Translate a trap number into a unix compatible signal value.
 * (gdb only understands unix signal numbers).
 */
int
kgdb_signal(int type)
{
	switch (type) {
	case T_NMI:
		return (SIGINT);

	case T_ALIGNFLT:
		return (SIGILL);

	case T_BPTFLT:
	case T_TRCTRAP:
		return (SIGTRAP);

	case T_DOUBLEFLT:
		return (SIGEMT);

	case T_ARITHTRAP:
	case T_DIVIDE:
	case T_OFLOW:
	case T_DNA:
	case T_FPOPFLT:
		return (SIGFPE);

	case T_PRIVINFLT:
	case T_PROTFLT:
	case T_PAGEFLT:
	case T_TSSFLT:
	case T_SEGNPFLT:
	case T_STKFLT:
		return (SIGSEGV);

	case T_BOUND:
		return (SIGURG);

	default:
		return (SIGEMT);
	}
}

/*
 * Translate the values stored in the kernel regs struct to the format
 * understood by gdb.
 */
void
kgdb_getregs(db_regs_t *regs, kgdb_reg_t *gdb_regs)
{

	gdb_regs[ 0] = regs->tf_eax;
	gdb_regs[ 1] = regs->tf_ecx;
	gdb_regs[ 2] = regs->tf_edx;
	gdb_regs[ 3] = regs->tf_ebx;
	gdb_regs[ 5] = regs->tf_ebp;
	gdb_regs[ 6] = regs->tf_esi;
	gdb_regs[ 7] = regs->tf_edi;
	gdb_regs[ 8] = regs->tf_eip;
	gdb_regs[ 9] = regs->tf_eflags;
	gdb_regs[10] = regs->tf_cs;
	gdb_regs[12] = regs->tf_ds;
	gdb_regs[13] = regs->tf_es;
	gdb_regs[14] = regs->tf_fs;
	gdb_regs[15] = regs->tf_gs;

	if (KERNELMODE(regs->tf_cs, regs->tf_eflags)) {
		/*
		 * Kernel mode - esp and ss not saved.
		 */
		gdb_regs[ 4] = (kgdb_reg_t)&regs->tf_esp; /* kernel stack
							     pointer */
		__asm volatile("movw %%ss,%w0" : "=r" (gdb_regs[11]));
	}
}

/*
 * Reverse the above.
 */
void
kgdb_setregs(db_regs_t *regs, kgdb_reg_t *gdb_regs)
{

	regs->tf_eax    = gdb_regs[ 0];
	regs->tf_ecx    = gdb_regs[ 1];
	regs->tf_edx    = gdb_regs[ 2];
	regs->tf_ebx    = gdb_regs[ 3];
	regs->tf_ebp    = gdb_regs[ 5];
	regs->tf_esi    = gdb_regs[ 6];
	regs->tf_edi    = gdb_regs[ 7];
	regs->tf_eip    = gdb_regs[ 8];
	regs->tf_eflags = gdb_regs[ 9];
	regs->tf_cs     = gdb_regs[10];
	regs->tf_ds     = gdb_regs[12];
	regs->tf_es     = gdb_regs[13];

	if (KERNELMODE(regs->tf_cs, regs->tf_eflags) == 0) {
		/*
		 * Trapped in user mode - restore esp and ss.
		 */
		regs->tf_esp = gdb_regs[ 4];
		regs->tf_ss  = gdb_regs[11];
	}
}	
