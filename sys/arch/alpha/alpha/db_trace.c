/* $OpenBSD: db_trace.c,v 1.6 2000/11/08 19:16:58 ericj Exp $ */
/* $NetBSD: db_trace.c,v 1.6 2000/05/26 03:34:24 jhawk Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ross Harvey.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <machine/db_machdep.h>

#include <alpha/alpha/db_instruction.h>

#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>

/*
 * Information about the `standard' Alpha function prologue.
 */
struct prologue_info {
	int	pi_reg_offset[32]; /* offset of registers in stack frame */
	u_int32_t pi_regmask;	   /* which registers are in frame */
	int	pi_frame_size;	   /* frame size */
};

/*
 * We use several symbols to take special action:
 *
 *	Trap vectors, which use a different (fixed-size) stack frame:
 *
 *		XentArith
 *		XentIF
 *		XentInt
 *		XentMM
 *		XentSys
 *		XentUna
 */

static struct special_symbol {
	void (*ss_val) __P((void));
	const char *ss_note;
} special_symbols[] = {
	{ (void (*)(void))&XentArith,	"arithmetic trap" },
	{ (void (*)(void))&XentIF,	"instruction fault" },
	{ (void (*)(void))&XentInt,	"interrupt" },
	{ (void (*)(void))&XentMM,	"memory management fault" },
	{ (void (*)(void))&XentSys,	"syscall" },
	{ (void (*)(void))&XentUna,	"unaligned access fault" },
	{ (void (*)(void))&XentRestart,	"console restart" },
	{ NULL }
};

static void decode_prologue __P((db_addr_t, db_addr_t, struct prologue_info *,
    int (*)(const char *, ...)));
static void decode_syscall __P((int, struct proc *,
    int (*)(const char *, ...)));
static int sym_is_trapsymbol __P((void *));

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	boolean_t have_addr;
	db_expr_t count;
	char *modif;
{
	db_addr_t callpc, frame, symval;
	struct prologue_info pi;
	void *symval_f;
	db_expr_t diff;
	db_sym_t sym;
	int i;
	u_long tfps;
	char *symname;
	struct pcb *pcbp;
	char c, *cp = modif;
	struct trapframe *tf;
	boolean_t ra_from_tf;
	boolean_t ra_from_pcb;
	u_long last_ipl = ~0L;
	struct proc *p = NULL;
	boolean_t trace_thread = FALSE;
	boolean_t have_trapframe = FALSE;

	while ((c = *cp++) != 0)
		trace_thread |= c == 't';

	if (!have_addr) {
		p = curproc;
		addr = DDB_REGS->tf_regs[FRAME_SP] - FRAME_SIZE * 8;
		tf = (struct trapframe *)addr;
		have_trapframe = 1;
	} else {
		if (trace_thread) {
			db_printf("trace: pid %d ", (int)addr);
			p = pfind(addr);
			if (p == NULL) {
				db_printf("not found\n");
				return;
			}	
			if ((p->p_flag & P_INMEM) == 0) {
				db_printf("swapped out\n");
				return;
			}
			pcbp = &p->p_addr->u_pcb;
			addr = (db_expr_t)pcbp->pcb_hw.apcb_ksp;
			callpc = pcbp->pcb_context[7];
			db_printf("at 0x%lx\n", addr);
		} else {
			db_printf("alpha trace requires known PC =eject=\n");
			return;
		}
		frame = addr;
	}

	while (count--) {
		if (have_trapframe) {
			frame = (db_addr_t)tf + FRAME_SIZE * 8;
			callpc = tf->tf_regs[FRAME_PC];
			ra_from_tf = TRUE;
			have_trapframe = 0;
		}
		sym = db_search_symbol(callpc, DB_STGY_ANY, &diff);
		if (sym == DB_SYM_NULL)
			break;

		db_symbol_values(sym, &symname, (db_expr_t *)&symval);
		symval_f = (void *)symval;

		if (callpc < symval) {
			db_printf("symbol botch: callpc 0x%lx < "
			    "func 0x%lx (%s)\n", callpc, symval, symname);
			return;
		}

		/*
		 * XXX Printing out arguments is Hard.  We'd have to
		 * keep lots of state as we traverse the frame, figuring
		 * out where the arguments to the function are stored
		 * on the stack.
		 *
		 * Even worse, they may be stored to the stack _after_
		 * being modified in place; arguments are passed in
		 * registers.
		 *
		 * So, in order for this to work reliably, we pretty much
		 * have to have a kernel built with `cc -g':
		 *
		 *	- The debugging symbols would tell us where the
		 *	  arguments are, how many there are, if there were
		 *	  any passed on the stack, etc.
		 *
		 *	- Presumably, the compiler would be careful to
		 *	  store the argument registers on the stack before
		 *	  modifying the registers, so that a debugger could
		 *	  know what those values were upon procedure entry.
		 *
		 * Because of this, we don't bother.  We've got most of the
		 * benefit of back tracking without the arguments, and we
		 * could get the arguments if we use a remote source-level
		 * debugger (for serious debugging).
		 */
		db_printf("%s() at ", symname);
		db_printsym(callpc, DB_STGY_PROC);
		db_printf("\n");

		/*
		 * If we are in a trap vector, frame points to a
		 * trapframe.
		 */
		if (sym_is_trapsymbol(symval_f)) {
			tf = (struct trapframe *)frame;

			for (i = 0; special_symbols[i].ss_val != NULL; ++i)
				if (symval_f == special_symbols[i].ss_val)
					db_printf("--- %s",
					    special_symbols[i].ss_note);

			tfps = tf->tf_regs[FRAME_PS];
			if (symval_f == &XentSys)
				decode_syscall(tf->tf_regs[FRAME_V0], p, db_printf);
			if ((tfps & ALPHA_PSL_IPL_MASK) != last_ipl) {
				last_ipl = tfps & ALPHA_PSL_IPL_MASK;
				if (symval_f != &XentSys)
					db_printf(" (from ipl %ld)", last_ipl);
			}
			db_printf(" ---\n");
			if (tfps & ALPHA_PSL_USERMODE) {
				db_printf("--- user mode ---\n");
				break;	/* Terminate search.  */
			}
			have_trapframe = 1;
			continue;
		}

		/*
		 * This is a bit trickier; we must decode the function
		 * prologue to find the saved RA.
		 *
		 * XXX How does this interact w/ alloca()?!
		 */
		decode_prologue(callpc, symval, &pi, db_printf);
		if ((pi.pi_regmask & (1 << 26)) == 0) {
			/*
			 * No saved RA found.  We might have RA from
			 * the trap frame, however (e.g trap occurred
			 * in a leaf call).  If not, we've found the
			 * root of the call graph.
			 */
			if (ra_from_tf)
				callpc = tf->tf_regs[FRAME_RA];
			else {
				db_printf("--- root of call graph ---\n");
				break;
			}
		} else
			callpc = *(u_long *)(frame + pi.pi_reg_offset[26]);
		ra_from_tf = ra_from_pcb = FALSE;
#if 0
		/*
		 * The call was actually made at RA - 4; the PC is
		 * updated before being stored in RA.
		 */
		callpc -= 4;
#endif
		frame += pi.pi_frame_size;
	}
}

/*
 * Decode the function prologue for the function we're in, and note
 * which registers are stored where, and how large the stack frame is.
 */
static void
decode_prologue(callpc, func, pi, pr)
	db_addr_t callpc, func;
	struct prologue_info *pi;
	int (*pr) __P((const char *, ...));
{
	long signed_immediate;
	alpha_instruction ins;
	db_expr_t pc;

	pi->pi_regmask = 0;
	pi->pi_frame_size = 0;

#define	CHECK_FRAMESIZE							\
do {									\
	if (pi->pi_frame_size != 0) {					\
		(*pr)("frame size botch: adjust register offsets?\n"); \
	}								\
} while (0)

	for (pc = func; pc < callpc; pc += sizeof(alpha_instruction)) {
		ins.bits = *(unsigned int *)pc;

		if (ins.mem_format.opcode == op_lda &&
		    ins.mem_format.ra == 30 &&
		    ins.mem_format.rb == 30) {
			/*
			 * GCC 2.7-style stack adjust:
			 *
			 *	lda	sp, -64(sp)
			 */
			signed_immediate = (long)ins.mem_format.displacement;
#if 1
			if (signed_immediate > 0)
				(*pr)("prologue botch: displacement %ld\n",
				    signed_immediate);
#endif
			CHECK_FRAMESIZE;
			pi->pi_frame_size += -signed_immediate;
		} else if (ins.operate_lit_format.opcode == op_arit &&
			   ins.operate_lit_format.function == op_subq &&
			   ins.operate_lit_format.ra == 30 &&
			   ins.operate_lit_format.rc == 30) {
			/*
			 * EGCS-style stack adjust:
			 *
			 *	subq	sp, 64, sp
			 */
			CHECK_FRAMESIZE;
			pi->pi_frame_size += ins.operate_lit_format.literal;
		} else if (ins.mem_format.opcode == op_stq &&
			   ins.mem_format.rb == 30 &&
			   ins.mem_format.ra != 31) {
			/* Store of (non-zero) register onto the stack. */
			signed_immediate = (long)ins.mem_format.displacement;
			pi->pi_regmask |= 1 << ins.mem_format.ra;
			pi->pi_reg_offset[ins.mem_format.ra] = signed_immediate;
		}
	}
}

static int
sym_is_trapsymbol(v)
	void *v;
{
	int i;

	for (i = 0; special_symbols[i].ss_val != NULL; ++i)
		if (v == special_symbols[i].ss_val)
			return 1;
	return 0;
}

static void
decode_syscall(number, p, pr)
	int number;
	struct proc *p;
	int (*pr) __P((const char *, ...));
{
	db_sym_t sym;
	db_expr_t diff;
	char *symname, *ename;
	int (*f) __P((struct proc *, void *, register_t *));

	(*pr)(" (%d", number); /* ) */
	if (!p)
		goto out;
	if (0 <= number && number < p->p_emul->e_nsysent) {
		ename = p->p_emul->e_name;
		f = p->p_emul->e_sysent[number].sy_call;
		sym = db_search_symbol((db_addr_t)f, DB_STGY_ANY, &diff);
		if (sym == DB_SYM_NULL || diff != 0)
			goto out;
		db_symbol_values(sym, &symname, NULL);
		(*pr)(", %s.%s", ename, symname);
	}
out:
	(*pr)(")");
	return;
}
