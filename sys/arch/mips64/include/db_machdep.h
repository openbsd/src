/*	$OpenBSD: db_machdep.h,v 1.12 2011/03/23 16:54:36 pirofti Exp $ */

/*
 * Copyright (c) 1998-2003 Opsycon AB (www.opsycon.se)
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

#ifndef	_MIPS64_DB_MACHDEP_H_
#define	_MIPS64_DB_MACHDEP_H_

#include <machine/frame.h>
#include <machine/trap.h>
#include <uvm/uvm_param.h>

#define DB_MACHINE_COMMANDS	/* We have machine specific commands */
#define	DB_ELF_SYMBOLS		/* Elf style symbol table support */

typedef struct trap_frame db_regs_t;
extern db_regs_t	ddb_regs;

typedef	long		db_expr_t;
typedef vaddr_t		db_addr_t;

#define	SOFTWARE_SSTEP		/* Need software single step */
#define	SOFTWARE_SSTEP_EMUL	/* next_instr_address() emulates 100% */
db_addr_t	next_instr_address(db_addr_t, boolean_t);
#define	BKPT_SIZE   (4)
#define	BKPT_SET(ins)	(BREAK_DDB)
#define	DB_VALID_BREAKPOINT(addr)	(((addr) & 3) == 0)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BREAK)
#define IS_WATCHPOINT_TRAP(type, code)	(0)	/* XXX mips3 watchpoint */

#define	PC_REGS(regs)	((db_addr_t)(regs)->pc)
#define	SET_PC_REGS(regs, value)	(regs)->pc = (register_t)(value)
#define DDB_REGS	(&ddb_regs)

/*
 *  Test of instructions to see class.
 */
#define	IT_CALL		0x01
#define	IT_BRANCH	0x02

#define	inst_branch(i)	(db_inst_type(i) == IT_BRANCH)
#define	inst_trap_return(i)	((i) & 0)
#define	inst_call(i)	(db_inst_type(i) == IT_CALL)
#define	inst_return(i)	((i) == 0x03e00008)

int db_inst_type(int);
void db_machine_init(void);

int db_enter_ddb(void);
void db_startcpu(int);
void db_stopcpu(int);

int	dbmd_print_insn(uint32_t, db_addr_t, int (*)(const char *, ...));

extern struct mutex ddb_mp_mutex;

#define DDB_STATE_NOT_RUNNING	0
#define DDB_STATE_RUNNING	1
#define DDB_STATE_EXITING	2

#endif	/* !_MIPS64_DB_MACHDEP_H_ */
