/*	$OpenBSD: db_machdep.c,v 1.1 1998/03/16 09:03:25 pefo Exp $ */

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

u_int MipsEmulateBranch __P((int *, int, int, u_int));
void  stacktrace_subr __P((int *, int (*)(const char*, ...)));

int   kdbpeek __P((int));
void  kdbpoke __P((int, int));
int   kdb_trap __P((int, struct trap_frame *));

int   db_active = 0;
db_regs_t ddb_regs;

struct db_variable db_regs[] = {
    { "at",  (long *)&ddb_regs.reg[AST],     FCN_NULL },
    { "v0",  (long *)&ddb_regs.reg[V0],      FCN_NULL },
    { "v1",  (long *)&ddb_regs.reg[V1],      FCN_NULL },
    { "a0",  (long *)&ddb_regs.reg[A0],      FCN_NULL },
    { "a1",  (long *)&ddb_regs.reg[A1],      FCN_NULL },
    { "a2",  (long *)&ddb_regs.reg[A2],      FCN_NULL },
    { "a3",  (long *)&ddb_regs.reg[A3],      FCN_NULL },
    { "t0",  (long *)&ddb_regs.reg[T0],      FCN_NULL },
    { "t1",  (long *)&ddb_regs.reg[T1],      FCN_NULL },
    { "t2",  (long *)&ddb_regs.reg[T2],      FCN_NULL },
    { "t3",  (long *)&ddb_regs.reg[T3],      FCN_NULL },
    { "t4",  (long *)&ddb_regs.reg[T4],      FCN_NULL },
    { "t5",  (long *)&ddb_regs.reg[T5],      FCN_NULL },
    { "t6",  (long *)&ddb_regs.reg[T6],      FCN_NULL },
    { "t7",  (long *)&ddb_regs.reg[T7],      FCN_NULL },
    { "s0",  (long *)&ddb_regs.reg[S0],      FCN_NULL },
    { "s1",  (long *)&ddb_regs.reg[S1],      FCN_NULL },
    { "s2",  (long *)&ddb_regs.reg[S2],      FCN_NULL },
    { "s3",  (long *)&ddb_regs.reg[S3],      FCN_NULL },
    { "s4",  (long *)&ddb_regs.reg[S4],      FCN_NULL },
    { "s5",  (long *)&ddb_regs.reg[S5],      FCN_NULL },
    { "s6",  (long *)&ddb_regs.reg[S6],      FCN_NULL },
    { "s7",  (long *)&ddb_regs.reg[S7],      FCN_NULL },
    { "t8",  (long *)&ddb_regs.reg[T8],      FCN_NULL },
    { "t9",  (long *)&ddb_regs.reg[T9],      FCN_NULL },
    { "k0",  (long *)&ddb_regs.reg[K0],      FCN_NULL },
    { "k1",  (long *)&ddb_regs.reg[K1],      FCN_NULL },
    { "gp",  (long *)&ddb_regs.reg[GP],      FCN_NULL },
    { "sp",  (long *)&ddb_regs.reg[SP],      FCN_NULL },
    { "s8",  (long *)&ddb_regs.reg[S8],      FCN_NULL },
    { "ra",  (long *)&ddb_regs.reg[RA],      FCN_NULL },
    { "sr",  (long *)&ddb_regs.reg[SR],      FCN_NULL },
    { "lo",  (long *)&ddb_regs.reg[MULLO],   FCN_NULL },
    { "hi",  (long *)&ddb_regs.reg[MULHI],   FCN_NULL },
    { "bad", (long *)&ddb_regs.reg[BADVADDR],FCN_NULL },
    { "cs",  (long *)&ddb_regs.reg[CAUSE],   FCN_NULL },
    { "pc",  (long *)&ddb_regs.reg[PC],      FCN_NULL },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);


int
kdb_trap(type, t_frame)
	int type;
	struct trap_frame *t_frame;
{
	switch(type) {
	case T_BREAK:		/* breakpoint */
		if(db_get_value((t_frame)->reg[PC], sizeof(int), FALSE) == BREAK_SOVER) {
                	(t_frame)->reg[PC] += BKPT_SIZE;        
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
	stacktrace_subr(ddb_regs.reg, db_printf);
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

	next = (db_addr_t)MipsEmulateBranch(ddb_regs.reg, pc, 0, 0);
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
