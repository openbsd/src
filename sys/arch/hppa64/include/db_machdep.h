/*	$OpenBSD: db_machdep.h,v 1.2 2010/11/27 19:57:23 miod Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

#include <uvm/uvm_extern.h>

#define	DB_ELF_SYMBOLS
#define	DB_ELFSIZE	64

/* types the generic ddb module needs */
typedef	vaddr_t db_addr_t;
typedef	long db_expr_t;

typedef struct trapframe db_regs_t;
extern db_regs_t	ddb_regs;
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_iioq[0])
#define	SET_PC_REGS(r,pc) ((r)->tf_iioq[0] = (pc), (r)->tf_iioq[1] = (pc) + 4)

/* Breakpoint related definitions */
#define	BKPT_INST	0x00010000	/* break 0,8 */
#define	BKPT_SIZE	sizeof(int)
#define	BKPT_SET(inst)	BKPT_INST

#define	IS_BREAKPOINT_TRAP(type, code) ((type) == T_IBREAK)
#define	IS_WATCHPOINT_TRAP(type, code) ((type) == T_DBREAK)

#define	FIXUP_PC_AFTER_BREAK(regs) ((regs)->tf_iioq[0] -= sizeof(int))

#define DB_VALID_BREAKPOINT(addr) db_valid_breakpoint(addr)

/* TODO 64bit insns */

static __inline int inst_call(u_int ins) {
	return (ins & 0xfc00e000) == 0xe8000000 ||
	       (ins & 0xfc00e000) == 0xe8004000 ||
	       (ins & 0xfc000000) == 0xe4000000;
}
static __inline int inst_branch(u_int ins) {
	return (ins & 0xf0000000) == 0xe0000000 ||
	       (ins & 0xf0000000) == 0xc0000000 ||
	       (ins & 0xf0000000) == 0xa0000000 ||
	       (ins & 0xf0000000) == 0x80000000;
}
static __inline int inst_return(u_int ins) {
	return (ins & 0xfc00e000) == 0xe800c000 ||
	       (ins & 0xfc000000) == 0xe0000000;
}
static __inline int inst_trap_return(u_int ins)	{
	return (ins & 0xfc001fc0) == 0x00000ca0;
}

#if 0
#define db_clear_single_step(r)	((r)->tf_flags &= ~(PSL_Z))
#define db_set_single_step(r)	((r)->tf_flags |= (PSL_Z))
#else
#define	SOFTWARE_SSTEP		1
#define	SOFTWARE_SSTEP_EMUL	1

static __inline db_addr_t
next_instr_address(db_addr_t addr, int b) {
	return (addr + 4);
}

#define	branch_taken(ins,pc,f,regs)	branch_taken1(ins, pc, regs)
static __inline db_addr_t
branch_taken1(int ins, db_addr_t pc, db_regs_t *regs) {
	return (pc);
}

#endif

int db_valid_breakpoint(db_addr_t);
int kdb_trap(int, int, db_regs_t *);

#endif /* _MACHINE_DB_MACHDEP_H_ */
