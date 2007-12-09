/*	$OpenBSD: db_sstep.c,v 1.6 2007/12/09 19:57:50 miod Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_access.h>	/* db_get_value() */
#include <ddb/db_break.h>	/* db_breakpoint_t */
#include <ddb/db_run.h>

/*
 * Support routines for software single step.
 *
 * Author: Daniel Stodolsky (danner@cs.cmu.edu)
 *
 */

/*
 * inst_load(ins)
 * Returns the number of words the instruction loads. byte,
 * half and word count as 1; double word as 2
 */
int
inst_load(u_int ins)
{
	/* look at the top six bits, for starters */
	switch (ins >> (32 - 6)) {
	case 0x0: /* xmem byte imm */
	case 0x1: /* xmem word imm */
	case 0x2: /* unsigned half-word load imm */
	case 0x3: /* unsigned byte load imm */
	case 0x5: /* signed word load imm */
	case 0x6: /* signed half-word load imm */
	case 0x7: /* signed byte load imm */
		return (1);

	case 0x4: /* signed double word load imm */
		return (2);

	case 0x3d: /* load/store/xmem scaled/unscaled instruction */
		if ((ins & 0x0000c0e0) == 0x00000000)	/* is ld/st/xmem */
			/* look at bits 15-10 */
			switch ((ins & 0x0000fc00) >> 10) {
			case 0x0: /* xmem byte */
			case 0x1: /* xmem word */
			case 0x2: /* unsigned half word */
			case 0x3: /* unsigned byte load */
			case 0x5: /* signed word load */
			case 0x6: /* signed half-word load */
			case 0x7: /* signed byte load */
				return (1);

			case 0x4: /* signed double word load */
				return (2);
			}
		break;
	}

	return (0);
}

/*
 * inst_store
 * Like inst_load, except for store instructions.
 */
int
inst_store(u_int ins)
{
	/* decode top 6 bits again */
	switch (ins >> (32 - 6)) {
	case 0x0: /* xmem byte imm */
	case 0x1: /* xmem word imm */
	case 0x9: /* store word imm */
	case 0xa: /* store half-word imm */
	case 0xb: /* store byte imm */
		return (1);

	case 0x8: /* store double word */
		return (2);

	case 0x3d: /* load/store/xmem scaled/unscaled instruction */
		if ((ins & 0x0000c0e0) == 0x00000000)	/* is ld/st/xmem */
			/* look at bits 15-10 */
			switch ((ins & 0x0000fc00) >> 10) {
			case 0x0: /* xmem byte imm */
			case 0x1: /* xmem word imm */
			case 0x9: /* store word */
			case 0xa: /* store half-word */
			case 0xb: /* store byte */
				return (1);

			case 0x8: /* store double word */
				return (2);
			}
		break;
	}

	return (0);
}

/*
 * We can not use the MI ddb SOFTWARE_SSTEP facility, since the 88110 will use
 * hardware single stepping.
 * Moreover, our software single stepping implementation is tailor-made for the
 * 88100 and faster than the MI code.
 */

#ifdef M88100

boolean_t	inst_branch_or_call(u_int);
db_addr_t	branch_taken(u_int, db_addr_t, db_regs_t *);

db_breakpoint_t db_not_taken_bkpt = 0;
db_breakpoint_t db_taken_bkpt = 0;

/*
 * Returns TRUE is the instruction a branch, jump or call instruction
 * (br, bb0, bb1, bcnd, jmp, bsr, jsr)
 */
boolean_t
inst_branch_or_call(u_int ins)
{
	/* check high five bits */
	switch (ins >> (32 - 5)) {
	case 0x18: /* br */
	case 0x19: /* bsr */
	case 0x1a: /* bb0 */
	case 0x1b: /* bb1 */
	case 0x1d: /* bcnd */
		return (TRUE);
	case 0x1e: /* could be jmp or jsr */
		if ((ins & 0xfffff3e0) == 0xf400c000)
			return (TRUE);
	}

	return (FALSE);
}

/*
 * branch_taken(instruction, program counter, regs)
 *
 * instruction will be a control flow instruction location at address pc.
 * Branch taken is supposed to return the address to which the instruction
 * would jump if the branch is taken.
 */
db_addr_t
branch_taken(u_int inst, db_addr_t pc, db_regs_t *regs)
{
	u_int regno;

	/*
	 * Quick check of the instruction. Note that we know we are only
	 * invoked if inst_branch_or_call() returns TRUE, so we do not
	 * need to repeat the jmp and jsr stricter checks here.
	 */
	switch (inst >> (32 - 5)) {
	case 0x18: /* br */
	case 0x19: /* bsr */
		/* signed 26 bit pc relative displacement, shift left two bits */
		inst = (inst & 0x03ffffff) << 2;
		/* check if sign extension is needed */
		if (inst & 0x08000000)
			inst |= 0xf0000000;
		return (pc + inst);

	case 0x1a: /* bb0 */
	case 0x1b: /* bb1 */
	case 0x1d: /* bcnd */
		/* signed 16 bit pc relative displacement, shift left two bits */
		inst = (inst & 0x0000ffff) << 2;
		/* check if sign extension is needed */
		if (inst & 0x00020000)
			inst |= 0xfffc0000;
		return (pc + inst);

	default: /* jmp or jsr */
		regno = inst & 0x1f;
		return (regno == 0 ? 0 : regs->r[regno]);
	}
}

#endif	/* M88100 */

void
db_set_single_step(db_regs_t *regs)
{
#ifdef M88110
	if (CPU_IS88110) {
		/*
		 * On the 88110, we can use the hardware tracing facility...
		 */
		regs->epsr |= PSR_TRACE | PSR_SER;
	}
#endif
#ifdef M88100
	if (CPU_IS88100) {
		/*
		 * ... while the 88100 will use two breakpoints.
		 */
		db_addr_t pc = PC_REGS(regs);
		db_addr_t brpc;
		u_int inst;

		/*
		 * User was stopped at pc, e.g. the instruction
		 * at pc was not executed.
		 */
		db_read_bytes(pc, sizeof(inst), (caddr_t)&inst);

		/*
		 * Find if this instruction may cause a branch, and set up a
		 * breakpoint at the branch location.
		 */
		if (inst_branch_or_call(inst)) {
			brpc = branch_taken(inst, pc, regs);

			/* self-branches are hopeless */
			if (brpc != pc && brpc != 0)
				db_taken_bkpt = db_set_temp_breakpoint(brpc);
		}

		db_not_taken_bkpt = db_set_temp_breakpoint(pc + 4);
	}
#endif
}

void
db_clear_single_step(regs)
	db_regs_t *regs;
{
#ifdef M88110
	if (CPU_IS88110) {
		regs->epsr &= ~(PSR_TRACE | PSR_SER);
	}
#endif
#ifdef M88100
	if (CPU_IS88100) {
		if (db_taken_bkpt != 0) {
			db_delete_temp_breakpoint(db_taken_bkpt);
			db_taken_bkpt = 0;
		}
		if (db_not_taken_bkpt != 0) {
			db_delete_temp_breakpoint(db_not_taken_bkpt);
			db_not_taken_bkpt = 0;
		}
	}
#endif
}
