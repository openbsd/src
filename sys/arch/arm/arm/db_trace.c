/*	$OpenBSD: db_trace.c,v 1.2 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: db_trace.c,v 1.8 2003/01/17 22:28:48 thorpej Exp $	*/

/* 
 * Copyright (c) 2000, 2001 Ben Harris
 * Copyright (c) 1996 Scott K. Stevens
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>

#include <sys/proc.h>
#include <sys/user.h>
#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

db_regs_t ddb_regs;

#define INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

/*
 * APCS stack frames are awkward beasts, so I don't think even trying to use
 * a structure to represent them is a good idea.
 *
 * Here's the diagram from the APCS.  Increasing address is _up_ the page.
 * 
 *          save code pointer       [fp]        <- fp points to here
 *          return link value       [fp, #-4]
 *          return sp value         [fp, #-8]
 *          return fp value         [fp, #-12]
 *          [saved v7 value]
 *          [saved v6 value]
 *          [saved v5 value]
 *          [saved v4 value]
 *          [saved v3 value]
 *          [saved v2 value]
 *          [saved v1 value]
 *          [saved a4 value]
 *          [saved a3 value]
 *          [saved a2 value]
 *          [saved a1 value]
 *
 * The save code pointer points twelve bytes beyond the start of the 
 * code sequence (usually a single STM) that created the stack frame.  
 * We have to disassemble it if we want to know which of the optional 
 * fields are actually present.
 */

#define FR_SCP	(0)
#define FR_RLV	(-1)
#define FR_RSP	(-2)
#define FR_RFP	(-3)

void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
	int		(*pr) (const char *, ...);
{
	u_int32_t	*frame, *lastframe;
	char c, *cp = modif;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;
	int	scp_offset;

	while ((c = *cp++) != 0) {
		if (c == 'u')
			kernel_only = FALSE;
		if (c == 't')
			trace_thread = TRUE;
	}

	if (!have_addr)
		frame = (u_int32_t *)(DDB_REGS->tf_r11);
	else {
		if (trace_thread) {
			struct proc *p;
			struct user *u;
			(*pr) ("trace: pid %d ", (int)addr);
			p = pfind(addr);
			if (p == NULL) {
				(*pr)("not found\n");
				return;
			}	
			if (!(p->p_flag & P_INMEM)) {
				(*pr)("swapped out\n");
				return;
			}
			u = p->p_addr;
#ifdef acorn26
			frame = (u_int32_t *)(u->u_pcb.pcb_sf->sf_r11);
#else
			frame = (u_int32_t *)(u->u_pcb.pcb_un.un_32.pcb32_r11);
#endif
			(*pr)("at %p\n", frame);
		} else
			frame = (u_int32_t *)(addr);
	}
	lastframe = NULL;
	scp_offset = -(get_pc_str_offset() >> 2);

	while (count-- && frame != NULL) {
		db_addr_t	scp;
		u_int32_t	savecode;
		int		r;
		u_int32_t	*rp;
		const char	*sep;

		/*
		 * In theory, the SCP isn't guaranteed to be in the function
		 * that generated the stack frame.  We hope for the best.
		 */
#ifdef __PROG26
		scp = frame[FR_SCP] & R15_PC;
#else
		scp = frame[FR_SCP];
#endif

		db_printsym(scp, DB_STGY_PROC, pr);
		(*pr)("\n\t");
#ifdef __PROG26
		(*pr)("scp=0x%08x rlv=0x%08x (", scp, frame[FR_RLV] & R15_PC);
		db_printsym(frame[FR_RLV] & R15_PC, DB_STGY_PROC, pr);
		(*pr)(")\n");
#else
		(*pr)("scp=0x%08x rlv=0x%08x (", scp, frame[FR_RLV]);
		db_printsym(frame[FR_RLV], DB_STGY_PROC, pr);
		(*pr)(")\n");
#endif
		(*pr)("\trsp=0x%08x rfp=0x%08x", frame[FR_RSP], frame[FR_RFP]);

		savecode = ((u_int32_t *)scp)[scp_offset];
		if ((savecode & 0x0e100000) == 0x08000000) {
			/* Looks like an STM */
			rp = frame - 4;
			sep = "\n\t";
			for (r = 10; r >= 0; r--) {
				if (savecode & (1 << r)) {
					(*pr)("%sr%d=0x%08x",
					    sep, r, *rp--);
					sep = (frame - rp) % 4 == 2 ?
					    "\n\t" : " ";
				}
			}
		}

		(*pr)("\n");

		/*
		 * Switch to next frame up
		 */
		if (frame[FR_RFP] == 0)
			break; /* Top of stack */

		lastframe = frame;
		frame = (u_int32_t *)(frame[FR_RFP]);

		if (INKERNEL((int)frame)) {
			/* staying in kernel */
			if (frame <= lastframe) {
				(*pr)("Bad frame pointer: %p\n", frame);
				break;
			}
		} else if (INKERNEL((int)lastframe)) {
			/* switch from user to kernel */
			if (kernel_only)
				break;	/* kernel stack only */
		} else {
			/* in user */
			if (frame <= lastframe) {
				(*pr)("Bad user frame pointer: %p\n",
					  frame);
				break;
			}
		}
	}
}
