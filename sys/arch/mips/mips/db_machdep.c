/*	$OpenBSD: db_machdep.c,v 1.2 1998/09/15 10:50:13 pefo Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <dev/cons.h>

#include <mips/db_machdep.h>
#include <machine/mips_opcode.h>

#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/db_variables.h>
#include <ddb/db_interface.h>

u_int MipsEmulateBranch __P((db_regs_t *, int, int, u_int));
void  stacktrace_subr __P((db_regs_t *, int (*)(const char*, ...)));

int   kdbpeek __P((int));
void  kdbpoke __P((int, int));
int   kdb_trap __P((int, struct trap_frame *));

int   db_active = 0;
db_regs_t ddb_regs;

struct db_variable db_regs[] = {
    { "at",  (long *)&ddb_regs.ast,     FCN_NULL },
    { "v0",  (long *)&ddb_regs.v0,      FCN_NULL },
    { "v1",  (long *)&ddb_regs.v1,      FCN_NULL },
    { "a0",  (long *)&ddb_regs.a0,      FCN_NULL },
    { "a1",  (long *)&ddb_regs.a1,      FCN_NULL },
    { "a2",  (long *)&ddb_regs.a2,      FCN_NULL },
    { "a3",  (long *)&ddb_regs.a3,      FCN_NULL },
    { "t0",  (long *)&ddb_regs.t0,      FCN_NULL },
    { "t1",  (long *)&ddb_regs.t1,      FCN_NULL },
    { "t2",  (long *)&ddb_regs.t2,      FCN_NULL },
    { "t3",  (long *)&ddb_regs.t3,      FCN_NULL },
    { "t4",  (long *)&ddb_regs.t4,      FCN_NULL },
    { "t5",  (long *)&ddb_regs.t5,      FCN_NULL },
    { "t6",  (long *)&ddb_regs.t6,      FCN_NULL },
    { "t7",  (long *)&ddb_regs.t7,      FCN_NULL },
    { "s0",  (long *)&ddb_regs.s0,      FCN_NULL },
    { "s1",  (long *)&ddb_regs.s1,      FCN_NULL },
    { "s2",  (long *)&ddb_regs.s2,      FCN_NULL },
    { "s3",  (long *)&ddb_regs.s3,      FCN_NULL },
    { "s4",  (long *)&ddb_regs.s4,      FCN_NULL },
    { "s5",  (long *)&ddb_regs.s5,      FCN_NULL },
    { "s6",  (long *)&ddb_regs.s6,      FCN_NULL },
    { "s7",  (long *)&ddb_regs.s7,      FCN_NULL },
    { "t8",  (long *)&ddb_regs.t8,      FCN_NULL },
    { "t9",  (long *)&ddb_regs.t9,      FCN_NULL },
    { "k0",  (long *)&ddb_regs.k0,      FCN_NULL },
    { "k1",  (long *)&ddb_regs.k1,      FCN_NULL },
    { "gp",  (long *)&ddb_regs.gp,      FCN_NULL },
    { "sp",  (long *)&ddb_regs.sp,      FCN_NULL },
    { "s8",  (long *)&ddb_regs.s8,      FCN_NULL },
    { "ra",  (long *)&ddb_regs.ra,      FCN_NULL },
    { "sr",  (long *)&ddb_regs.sr,      FCN_NULL },
    { "lo",  (long *)&ddb_regs.mullo,   FCN_NULL },
    { "hi",  (long *)&ddb_regs.mulhi,   FCN_NULL },
    { "bad", (long *)&ddb_regs.badvaddr,FCN_NULL },
    { "cs",  (long *)&ddb_regs.cause,   FCN_NULL },
    { "pc",  (long *)&ddb_regs.pc,      FCN_NULL },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);


int
kdb_trap(type, t_frame)
	int type;
	struct trap_frame *t_frame;
{
	switch(type) {
	case T_BREAK:		/* breakpoint */
		if(db_get_value((t_frame)->pc, sizeof(int), FALSE) == BREAK_SOVER) {
                	(t_frame)->pc += BKPT_SIZE;        
		}
		break;
	case -1:
		break;
	default:
#if notyet
		if(db_recover != 0) {
			db_error("Caught exception in ddb.\n");
			/*NOTREACHED*/
		}
		return(FALSE);	/* NOT handled */
#endif
	}

	ddb_regs = *t_frame;

	db_active++;
	cnpollc(TRUE);
	db_trap(type, 0);
	cnpollc(FALSE);
	db_active--;

	*t_frame = ddb_regs;
	return(TRUE);
}
void
db_read_bytes(addr, size, data)
	vm_offset_t addr;
	size_t      size;
	char       *data;
{
	while(size >= sizeof(int)) {
		*((int *)data)++ = kdbpeek(addr);
		addr += sizeof(int);
		size -= sizeof(int);
	}

	if(size) {
		int res;
		char *p = (char *)&res;
		res = kdbpeek(addr);
		while(size--) {
			*data++ = *p++;
		}
	}
}

void
db_write_bytes(addr, size, data)
	vm_offset_t addr;
	size_t      size;
	char       *data;
{
	vm_offset_t ptr = addr;
	size_t len = size;

	while(len >= sizeof(int)) {
		kdbpoke(ptr, *((int *)data)++);
		ptr += sizeof(int);
		len -= sizeof(int);
	}

	if(len) {
		int res;
		char *p = (char *)&res;
		res = kdbpeek(ptr);
		while(len--) {
			*data++ = *p++;
		}
		kdbpoke(ptr, res);
	}
	if(addr < VM_MIN_KERNEL_ADDRESS) {
		R4K_HitFlushDCache(addr, size);
		R4K_FlushICache(PHYS_TO_CACHED(addr & 0xffff), size);
	}
}

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char		*modif;
{
	stacktrace_subr(&ddb_regs, db_printf);
}

/*
 *	To do a single step ddb needs to know the next address
 *	that we will get to. It means that we need to find out
 *	both the address for a branch taken and for not taken, NOT! :-)
 *	MipsEmulateBranch will do the job to find out _exactly_ which
 *	address we will end up at so the 'dual bp' method is not
 *	requiered.
 */
db_addr_t
next_instr_address(db_addr_t pc, boolean_t bd)
{
	db_addr_t next;

	next = (db_addr_t)MipsEmulateBranch(&ddb_regs, pc, 0, 0);
	return(next);
}


/*
 *	Decode instruction and figure out type.
 */
int
db_inst_type(ins)
	int	ins;
{
	InstFmt	inst;
	int	ityp = 0;

	inst.word = ins;
	switch ((int)inst.JType.op) {
	case OP_SPECIAL:
		switch ((int)inst.RType.func) {
		case OP_JR:
			ityp = IT_BRANCH;
			break;
		case OP_JALR:
		case OP_SYSCALL:
			ityp = IT_CALL;
			break;
		}
		break;

	case OP_BCOND:
		switch ((int)inst.IType.rt) {
		case OP_BLTZ:
		case OP_BLTZL:
		case OP_BGEZ:
		case OP_BGEZL:
			ityp = IT_BRANCH;
			break;

		case OP_BLTZAL:
		case OP_BLTZALL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			ityp = IT_CALL;
			break;
		}
		break;

	case OP_JAL:
		ityp = IT_CALL;
		break;

	case OP_J:
	case OP_BEQ:
	case OP_BEQL:
	case OP_BNE:
	case OP_BNEL:
	case OP_BLEZ:
	case OP_BLEZL:
	case OP_BGTZ:
	case OP_BGTZL:
		ityp = IT_BRANCH;
		break;

	case OP_COP1:
		switch (inst.RType.rs) {
		case OP_BCx:
		case OP_BCy:
			ityp = IT_BRANCH;
			break;
		}
		break;

	case OP_LB:
	case OP_LH:
	case OP_LW:
	case OP_LD:
	case OP_LBU:
	case OP_LHU:
	case OP_LWU:
	case OP_LWC1:
		ityp = IT_LOAD;
		break;

	case OP_SB:
	case OP_SH:
	case OP_SW:
	case OP_SD:  
	case OP_SWC1:
		ityp = IT_STORE;
		break;
	}
	return (ityp);
}
