/*	$OpenBSD: db_machdep.h,v 1.14 2011/03/23 16:54:37 pirofti Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.6 1998/08/10 14:33:33 ragge Exp $	*/

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
 * Modified for vax out of i386 code.
 */

#include <sys/param.h>
#include <uvm/uvm_extern.h>
#include <machine/trap.h>
#include <machine/psl.h>

typedef	vaddr_t	db_addr_t;		/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef struct trapframe db_regs_t;
extern db_regs_t	ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->pc)
#define	SET_PC_REGS(regs, value)	(regs)->pc = (long)(value)

#define	BKPT_INST	0x03		/* breakpoint instruction */
#define	BKPT_SIZE	(1)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	FIXUP_PC_AFTER_BREAK(regs)	((regs)->pc -= BKPT_SIZE)

#define	db_clear_single_step(regs)	((regs)->psl &= ~PSL_T)
#define	db_set_single_step(regs)	((regs)->psl |=  PSL_T)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BPTFLT)
#define IS_WATCHPOINT_TRAP(type, code)	((type) == T_TRCTRAP)

#define	I_CALL		0xfb
#define	I_RET		0x04
#define	I_IRET		0x02

#define	inst_trap_return(ins)	(((ins)&0xff) == I_IRET)
#define	inst_return(ins)	(((ins)&0xff) == I_RET)
#define	inst_call(ins)		(((ins)&0xff) == I_CALL)

/* Prototypes */
void	kdb_trap(struct trapframe *);

/*
 * We use a.out symbols in DDB.
 */
#define	DB_AOUT_SYMBOLS

#endif	/* _MACHINE_DB_MACHDEP_H_ */
