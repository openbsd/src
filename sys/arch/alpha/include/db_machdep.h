/*	$OpenBSD: db_machdep.h,v 1.5 1997/07/06 16:20:23 niklas Exp $	*/
/*	$NetBSD: db_machdep.h,v 1.2 1996/07/11 05:31:31 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef	_ALPHA_DB_MACHDEP_H_
#define	_ALPHA_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */

#include <sys/param.h>
#include <vm/vm.h>
#include <machine/frame.h>

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef struct trapframe db_regs_t;
db_regs_t		ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_regs[FRAME_PC])

#define	BKPT_INST	0x00000080	/* breakpoint instruction */
#define	BKPT_SIZE	(4)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	FIXUP_PC_AFTER_BREAK(regs) ((regs)->tf_regs[FRAME_PC] -= BKPT_SIZE)

#define	IS_BREAKPOINT_TRAP(type, code)	0
#define	IS_WATCHPOINT_TRAP(type, code)	0

#define SOFTWARE_SSTEP

/* Hack to skip GCC "unused" warnings. */
#ifdef __GNUC__
#define	inst_trap_return(ins)	({(ins); 0;})
#define	inst_return(ins)	({(ins); 0;})
#define	inst_call(ins)		({(ins); 0;})
#define	inst_branch(ins)	({(ins); 0;})
#define inst_load(ins)		({(ins); 0;})
#define inst_store(ins)		({(ins); 0;})
#else
#define	inst_trap_return(ins)	0
#define	inst_return(ins)	0
#define	inst_call(ins)		0
#define	inst_branch(ins)	0
#define inst_load(ins)		0
#define inst_store(ins)		0
#endif

#define next_instr_address(pc, bd)	(pc + 4)

/* XXX temporary hack until we implement singlestepping */
#define branch_taken(a, b, c, d)	0

int kdb_trap __P((int, int, db_regs_t *));

#endif	/* _ALPHA_DB_MACHDEP_H_ */
