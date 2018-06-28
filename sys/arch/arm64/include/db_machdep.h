/*	$OpenBSD: db_machdep.h,v 1.3 2018/06/28 22:47:20 kettenis Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.5 2001/11/22 18:00:00 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Scott K Stevens
 *
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */

#include <sys/param.h>
#include <uvm/uvm_extern.h>
#include <machine/armreg.h>
#include <machine/frame.h>
#include <machine/trap.h>

/* end of mangling */

typedef	vaddr_t		db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef trapframe_t db_regs_t;

extern db_regs_t		ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_lr)
#define	SET_PC_REGS(regs, value)	(regs)->tf_lr = (register_t)(value)

#define	BKPT_INST	(KERNEL_BREAKPOINT)	/* breakpoint instruction */
#define	BKPT_SIZE	(INSN_SIZE)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

/*#define FIXUP_PC_AFTER_BREAK(regs)	((regs)->tf_lr -= BKPT_SIZE)*/

#define T_BREAKPOINT			(1)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BREAKPOINT)
#define IS_WATCHPOINT_TRAP(type, code)	(0)

// ALL BROKEN!!!
#define	inst_trap_return(ins)	((ins) == 0 && (ins) == 1)
#define	inst_return(ins)	((ins) == 0 && (ins) == 1)
				
#define	inst_call(ins)		((ins) == 0 && (ins) == 1)
#define	inst_branch(ins)	((ins) == 0 && (ins) == 1)
#define inst_unconditional_flow_transfer(ins)	(0)

#define getreg_val			(0)
#define next_instr_address(pc, bd)	((bd) ? (pc) : ((pc) + INSN_SIZE))

#define DB_MACHINE_COMMANDS

#define SOFTWARE_SSTEP

db_addr_t	db_branch_taken(u_int inst, db_addr_t pc, db_regs_t *regs);
int kdb_trap (int, db_regs_t *);
void db_machine_init (void);

#define branch_taken(ins, pc, fun, regs) \
	db_branch_taken((ins), (pc), (regs))

void db_show_frame_cmd(db_expr_t, int, db_expr_t, char *);

#define DDB_STATE_NOT_RUNNING	0  
#define DDB_STATE_RUNNING	1
#define DDB_STATE_EXITING	2

#endif	/* _MACHINE_DB_MACHDEP_H_ */
