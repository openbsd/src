/*	$NetBSD: db_machdep.h,v 1.3 1994/10/26 08:24:24 cgd Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND HELSINKI UNIVERSITY OF TECHNOLOGY ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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
 * HISTORY
 * 11-May-92  Tero Kivinen (kivinen) at Helsinki University of Technology
 *	Created.
 *
 */
/*
 * 	File: ns532/db_machdep.h
 *	Author: Tero Kivinen, Helsinki University of Technology 1992.
 *
 *	Machine-dependent defines for kernel debugger.
 *
 *   modified by Phil Nelson for inclusion in 532bsd.
 *
 */

#ifndef	_MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

/* #include <mach/ns532/vm_types.h> */
/* #include <mach/ns532/vm_param.h> */
#include <vm/vm_prot.h>
#include <vm/vm_param.h>
#include <vm/vm_inherit.h>
#include <vm/lock.h>

/* #include <ns532/thread.h>		/* for thread_status */
#include <machine/frame.h>	/* For struct trapframe */

#include <machine/psl.h>
#include <machine/trap.h>

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	int		db_expr_t;	/* expression - signed */

typedef struct ns532_saved_state	db_regs_t;
db_regs_t  	ddb_regs;		/* register state */
#define DDB_REGS	(&ddb_regs)

#define PC_REGS(regs)	((db_addr_t)(regs)->pc)

#define	BKPT_INST	0xf2		/* breakpoint instruction */
#define	BKPT_SIZE	(1)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	db_clear_single_step(regs)	((regs)->psr &= ~PSR_T)
#define	db_set_single_step(regs)	((regs)->psr |=  PSR_T)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BPT)
#define IS_WATCHPOINT_TRAP(type, code)	((type) == T_WATCHPOINT)

#define I_BSR		0x02
#define I_JSR		0x7f /* and low 3 bits of next byte are 0x6 */
#define I_RET		0x12
#define I_RETT		0x42
#define I_RETI		0x52

#define	inst_trap_return(ins)	(((ins)&0xff) == I_RETT || \
				 ((ins)&0xff) == I_RETI)
#define	inst_return(ins)	(((ins)&0xff) == I_RET)
#define	inst_call(ins)		(((ins)&0xff) == I_BSR || \
				 (((ins)&0xff) == I_JSR && \
				  ((ins)&0x0700) == 0x0600))

#define inst_load(ins)	0
#define inst_store(ins)	0

extern int db_active_ipl;

/* access capability and access macros */

#define DB_ACCESS_LEVEL		2	/* access any space */
#define DB_CHECK_ACCESS(addr,size,task)				\
	db_check_access(addr,size,task)
#define DB_PHYS_EQ(task1,addr1,task2,addr2)			\
	db_phys_eq(task1,addr1,task2,addr2)
#define DB_VALID_KERN_ADDR(addr)				\
	((addr) >= VM_MIN_KERNEL_ADDRESS && 			\
	 (addr) < VM_MAX_KERNEL_ADDRESS)
#define DB_VALID_ADDRESS(addr,user)				\
	((!(user) && DB_VALID_KERN_ADDR(addr)) ||		\
	 ((user) && (addr) < VM_MIN_KERNEL_ADDRESS))

boolean_t 	db_check_access(/* vm_offset_t, int, task_t */);
boolean_t	db_phys_eq(/* task_t, vm_offset_t, task_t, vm_offset_t */);

/* macros for printing OS server dependent task name */

#define DB_TASK_NAME(task)	db_task_name(task)
#define DB_TASK_NAME_TITLE	"COMMAND                "
#define DB_TASK_NAME_LEN	23
#define DB_NULL_TASK_NAME	"?                      "

void		db_task_name(/* task_t */);

/* macro for checking if a thread has used floating point */

#define db_thread_fp_used(thread)	((thread)->pcb->fps && (thread)->pcb->fps->valid)

#endif
