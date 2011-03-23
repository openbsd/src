/*	$OpenBSD: db_machdep.h,v 1.15 2011/03/23 16:54:37 pirofti Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.10 1997/08/31 21:23:40 pk Exp $ */

/*
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


#include <uvm/uvm_extern.h>
#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/reg.h>

/* end of mangling */

typedef	vaddr_t		db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef struct {
	struct trapframe db_tf;
	struct frame	 db_fr;
} db_regs_t;

extern db_regs_t	ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)
#define	DDB_TF		(&ddb_regs.db_tf)
#define	DDB_FR		(&ddb_regs.db_fr)

#define	PC_REGS(regs)	((db_addr_t)(regs)->db_tf.tf_pc)
#define	SET_PC_REGS(regs, value)	(regs)->db_tf.tf_pc = (int)(value)
#define	PC_ADVANCE(regs) do {				\
	int n = (regs)->db_tf.tf_npc;			\
	(regs)->db_tf.tf_pc = n;			\
	(regs)->db_tf.tf_npc = n + 4;			\
} while(0)

#define	BKPT_INST	0x91d02001	/* breakpoint instruction */
#define	BKPT_SIZE	(4)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	db_clear_single_step(regs)	(void) (0)
#define	db_set_single_step(regs)	(void) (0)

#define	IS_BREAKPOINT_TRAP(type, code)	\
	((type) == T_BREAKPOINT || (type) == T_KGDB_EXEC)
#define IS_WATCHPOINT_TRAP(type, code)	(0)

#define	inst_trap_return(ins)	((ins)&0)
#define	inst_return(ins)	((ins)&0)
#define	inst_call(ins)		((ins)&0)

#define DB_MACHINE_COMMANDS

void db_machine_init(void);
int kdb_trap(int, struct trapframe *);

#define DB_ELF_SYMBOLS
#define DB_ELFSIZE	32


/*
 * KGDB definitions
 */
typedef u_long		kgdb_reg_t;
#define KGDB_NUMREGS	72
#define KGDB_BUFLEN	1024

#define KGDB_PREPARE	fb_unblank()
#define KGDB_ENTER	__asm("ta %0" :: "n" (T_KGDB_EXEC))

#endif	/* _MACHINE_DB_MACHDEP_H_ */
