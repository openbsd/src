/*	$OpenBSD: db_machdep.h,v 1.7 1997/07/23 23:32:43 niklas Exp $	*/

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

#ifndef	_ALPHA_DB_MACHDEP_H_
#define	_ALPHA_DB_MACHDEP_H_

struct opcode {
	enum opc_fmt { OPC_PAL, OPC_RES, OPC_MEM, OPC_OP, OPC_BR } opc_fmt;
	char *opc_name;
	int opc_print;
};
extern struct opcode opcode[];

/* types the generic ddb module needs */
typedef	vm_offset_t db_addr_t;
typedef	long db_expr_t;
typedef struct trapframe db_regs_t;

db_regs_t		ddb_regs;
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_regs[FRAME_PC])

/* Breakpoint related definitions */
#define	BKPT_INST	0x00000080	/* call_pal bpt */
#define	BKPT_SIZE	sizeof(int)
#define	BKPT_SET(inst)	BKPT_INST

#define	IS_BREAKPOINT_TRAP(type, code) \
    ((type) == ALPHA_KENTRY_IF && (code) == ALPHA_IF_CODE_BPT)
#ifdef notyet
#define	IS_WATCHPOINT_TRAP(type, code)	((type) == ALPHA_KENTRY_MM)
#else
#define	IS_WATCHPOINT_TRAP(type, code)	0
#endif

#define	FIXUP_PC_AFTER_BREAK(regs) ((regs)->tf_regs[FRAME_PC] -= sizeof(int))

#define SOFTWARE_SSTEP
#define DB_VALID_BREAKPOINT(addr) db_valid_breakpoint(addr)

/* Hack to skip GCC "unused" warnings. */
#define	inst_trap_return(ins)	((ins) & 0)		/* XXX */
#define	inst_return(ins)	(((ins) & 0xfc000000) == 0x68000000)

int inst_call __P((u_int));
int inst_branch __P((u_int));
int inst_load __P((u_int));
int inst_store __P((u_int));
db_addr_t branch_taken __P((u_int, db_addr_t,
    register_t (*) __P((db_regs_t *, int)), db_regs_t *));
db_addr_t next_instr_address __P((db_addr_t, int));
int kdb_trap __P((int, int, db_regs_t *));
int db_valid_breakpoint __P((db_addr_t));

#endif	/* _ALPHA_DB_MACHDEP_H_ */
