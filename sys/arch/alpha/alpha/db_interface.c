/*	$OpenBSD: db_interface.c,v 1.9 2000/01/03 19:19:41 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserverd.
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
 *	This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_run.h>
#include <ddb/db_sym.h>
#include <ddb/db_var.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>

#include <dev/cons.h>

extern label_t *db_recover;
extern char    *trap_type[];
extern int	trap_types;

void kdbprinttrap __P((int, int));

/*
 * These entries must be in the same order as the CPU registers.
 * You can add things at the end.
 */
struct db_variable db_regs[] = {
	{ "v0", (long *)&ddb_regs.tf_regs[FRAME_V0], FCN_NULL, },	/*0*/
	{ "t0", (long *)&ddb_regs.tf_regs[FRAME_T0], FCN_NULL, },	/*1*/
	{ "t1", (long *)&ddb_regs.tf_regs[FRAME_T1], FCN_NULL, },	/*2*/
	{ "t2", (long *)&ddb_regs.tf_regs[FRAME_T2], FCN_NULL, },	/*3*/
	{ "t3", (long *)&ddb_regs.tf_regs[FRAME_T3], FCN_NULL, },	/*4*/
	{ "t4", (long *)&ddb_regs.tf_regs[FRAME_T4], FCN_NULL, },	/*5*/
	{ "t5", (long *)&ddb_regs.tf_regs[FRAME_T5], FCN_NULL, },	/*6*/
	{ "t6", (long *)&ddb_regs.tf_regs[FRAME_T6], FCN_NULL, },	/*7*/
	{ "t7", (long *)&ddb_regs.tf_regs[FRAME_T7], FCN_NULL, },	/*8*/
	{ "s0", (long *)&ddb_regs.tf_regs[FRAME_S0], FCN_NULL, },	/*9*/
	{ "s1", (long *)&ddb_regs.tf_regs[FRAME_S1], FCN_NULL, },	/*10*/
	{ "s2", (long *)&ddb_regs.tf_regs[FRAME_S2], FCN_NULL, },	/*11*/
	{ "s3", (long *)&ddb_regs.tf_regs[FRAME_S3], FCN_NULL, },	/*12*/
	{ "s4", (long *)&ddb_regs.tf_regs[FRAME_S4], FCN_NULL, },	/*13*/
	{ "s5", (long *)&ddb_regs.tf_regs[FRAME_S5], FCN_NULL, },	/*14*/
	{ "s6", (long *)&ddb_regs.tf_regs[FRAME_S6], FCN_NULL, },	/*15*/
	{ "a0", (long *)&ddb_regs.tf_regs[FRAME_A0], FCN_NULL, },	/*16*/
	{ "a1", (long *)&ddb_regs.tf_regs[FRAME_A1], FCN_NULL, },	/*17*/
	{ "a2", (long *)&ddb_regs.tf_regs[FRAME_A2], FCN_NULL, },	/*18*/
	{ "a3", (long *)&ddb_regs.tf_regs[FRAME_A3], FCN_NULL, },	/*19*/
	{ "a4", (long *)&ddb_regs.tf_regs[FRAME_A4], FCN_NULL, },	/*20*/
	{ "a5", (long *)&ddb_regs.tf_regs[FRAME_A5], FCN_NULL, },	/*21*/
	{ "t8", (long *)&ddb_regs.tf_regs[FRAME_T8], FCN_NULL, },	/*22*/
	{ "t9", (long *)&ddb_regs.tf_regs[FRAME_T9], FCN_NULL, },	/*23*/
	{ "t10", (long *)&ddb_regs.tf_regs[FRAME_T10], FCN_NULL, },	/*24*/
	{ "t11", (long *)&ddb_regs.tf_regs[FRAME_T11], FCN_NULL, },	/*25*/
	{ "ra", (long *)&ddb_regs.tf_regs[FRAME_RA], FCN_NULL, },	/*26*/
	{ "t12", (long *)&ddb_regs.tf_regs[FRAME_T12], FCN_NULL, },	/*27*/
	{ "at", (long *)&ddb_regs.tf_regs[FRAME_AT], FCN_NULL, },	/*28*/
	{ "gp", (long *)&ddb_regs.tf_regs[FRAME_GP], FCN_NULL, },	/*29*/
	{ "sp", (long *)&ddb_regs.tf_regs[FRAME_SP], FCN_NULL, },	/*30*/
	{ "pc", (long *)&ddb_regs.tf_regs[FRAME_PC], FCN_NULL, },	/*not*/
	{ "ps", (long *)&ddb_regs.tf_regs[FRAME_PS], FCN_NULL, },	/*not*/
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);
int	db_active = 0;

void
Debugger()
{
  	__asm__ ("bpt");
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t addr;
	size_t size;
	char *data;
{
	char *src = (char*)addr;

	while (size > 0) {
		--size;
		*data++ = *src++;
	}
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t addr;
	size_t size;
	char *data;
{
	char *dst = (char *)addr;

	while (size > 0) {
		--size;
		*dst++ = *data++;
	}
	alpha_pal_imb();
}

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	db_printf("kernel: ");
	if (type >= trap_types || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 *  kdb_trap - field a BPT trap
 */
int
kdb_trap(type, code, regs)
	int type, code;
	db_regs_t *regs;
{
	int s;

	switch (type) {
	case -1:			/* keyboard interrupt */
		break;
	case ALPHA_KENTRY_IF:		/* breakpoint */
		if (code == ALPHA_IF_CODE_BPT)
			break;
	default:
		if (!db_panic)
			return (0);

		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* XXX Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	*regs = ddb_regs;
	return (1);
}

register_t
getreg_val(regs, reg)
	db_regs_t *regs;
	int reg;
{
	return ((register_t)*db_regs[reg].valuep);
}

/* XXX Where do jsr_coroutine fit in?  We do not use it anyhow so... */
int
inst_call(ins)
	u_int ins;
{
	return ((ins & 0xfc000000) == 0xd0000000 ||	/* bsr */
	    (ins & 0xfc00c000) == 0x68004000);		/* jsr */
}

int
inst_branch(ins)
	u_int ins;
{
	return ((ins & 0xc0000000) == 0xc0000000 &&	/* 30 - 3F */
	    !((ins & 0xfc000000) == 0xd0000000 ||	/* but !34 (bsr) */
	    (ins & 0xfc00c000) == 0x68000000));		/* nor jmp */
}

int
inst_load(ins)
	u_int ins;
{
	char *nm = opcode[ins >> 26].opc_name;

	return (nm[0] == 'l' && nm[1] == 'd');
}

int
inst_store(ins)
	u_int ins;
{
	char *nm = opcode[ins >> 26].opc_name;

	return (nm[0] == 's' && nm[1] == 't');
}

db_addr_t
branch_taken(ins, pc, getreg, regs)
	u_int ins;
	db_addr_t pc;
	register_t (*getreg) __P((db_regs_t *, int));
	db_regs_t *regs;
{
	int offset;

	if (opcode[ins >> 26].opc_fmt == OPC_BR) {
		offset = ins & 0xfffff;
		if (offset & 0x80000)
			offset = offset - 0x100000;
		return (pc + sizeof(int) + offset * sizeof(int));
	} else
		return (db_addr_t)(*getreg)(regs, (ins >> 16) & 0x1f);
}

db_addr_t
next_instr_address(pc, branch)
	db_addr_t pc;
	int branch;
{
	if (!branch)
		return (pc + sizeof(int));
	return (branch_taken(*(u_int *)pc, pc, getreg_val, DDB_REGS));
}

/*
 * Validate an address for use as a breakpoint.  We cannot let some
 * addresses have breakpoints as the ddb code itself uses that codepath.
 * Recursion and kernel stack space exhaustion will follow.
 */
int
db_valid_breakpoint(addr)
	db_addr_t addr;
{
	char *name;
	db_expr_t offset;

	db_find_sym_and_offset(addr, &name, &offset);
	if (name && strcmp(name, "alpha_pal_swpipl") == 0)
		return (0);
	return (1);
}
