/*	$OpenBSD: db_machdep.h,v 1.9 2005/01/04 21:14:35 espie Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.20 1997/06/26 01:26:58 thorpej Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

/*
 * Machine-dependent defines for new kernel debugger.
 */
#ifndef	_M68K_DB_MACHDEP_H_
#define	_M68K_DB_MACHDEP_H_

#include <sys/types.h>

/*
 * XXX - Would rather not pull in vm headers, but need boolean_t,
 * at least until boolean_t moves to <sys/types.h> or someplace.
 */
#include <uvm/uvm_param.h>

#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/trap.h>

typedef	vaddr_t		db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */
typedef struct trapframe db_regs_t;

extern db_regs_t	ddb_regs;	/* register state */
#define DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_pc)
#define	SET_PC_REGS(regs, value)	(regs)->tf_pc = (unsigned int)(value)

#define	BKPT_INST	0x4e4f		/* breakpoint instruction */
#define	BKPT_SIZE	(2)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	FIXUP_PC_AFTER_BREAK(regs)	((regs)->tf_pc -= BKPT_SIZE)

#define	db_clear_single_step(regs)	((regs)->tf_sr &= ~PSL_T)
#define	db_set_single_step(regs)	((regs)->tf_sr |=  PSL_T)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BREAKPOINT)
#ifdef T_WATCHPOINT
#define	IS_WATCHPOINT_TRAP(type, code)	((type) == T_WATCHPOINT)
#else
#define	IS_WATCHPOINT_TRAP(type, code)	0
#endif

#define	M_RTS		0xffff0000
#define I_RTS		0x4e750000
#define M_JSR		0xffc00000
#define I_JSR		0x4e800000
#define M_BSR		0xff000000
#define I_BSR		0x61000000
#define	M_RTE		0xffff0000
#define	I_RTE		0x4e730000

#define	inst_trap_return(ins)	(((ins)&M_RTE) == I_RTE)
#define	inst_return(ins)	(((ins)&M_RTS) == I_RTS)
#define	inst_call(ins)		(((ins)&M_JSR) == I_JSR || \
				 ((ins)&M_BSR) == I_BSR)
#define inst_load(ins)		0
#define inst_store(ins)		0

/*
 * Things needed by kgdb:
 */
typedef long kgdb_reg_t;
#define KGDB_NUMREGS	(16+2)
#define KGDB_BUFLEN	512


#ifdef _KERNEL

void	Debugger(void);	/* XXX */
void	kdb_kintr(db_regs_t *);
int 	kdb_trap(int, db_regs_t *);

#endif /* _KERNEL */

/*
 * We use a.out symbols in DDB.
 */
#define	DB_AOUT_SYMBOLS

#endif	/* _M68K_DB_MACHDEP_H_ */
