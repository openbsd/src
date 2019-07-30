/*	$OpenBSD: db_trace.c,v 1.38 2018/02/10 10:25:44 mpi Exp $	*/
/*	$NetBSD: db_trace.c,v 1.1 2003/04/26 18:39:27 fvdl Exp $	*/

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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/trap.h>

#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "rdi",	(long *)&ddb_regs.tf_rdi,    FCN_NULL },
	{ "rsi",	(long *)&ddb_regs.tf_rsi,    FCN_NULL },
	{ "rbp",	(long *)&ddb_regs.tf_rbp,    FCN_NULL },
	{ "rbx",	(long *)&ddb_regs.tf_rbx,    FCN_NULL },
	{ "rdx",	(long *)&ddb_regs.tf_rdx,    FCN_NULL },
	{ "rcx",	(long *)&ddb_regs.tf_rcx,    FCN_NULL },
	{ "rax",	(long *)&ddb_regs.tf_rax,    FCN_NULL },
	{ "r8",		(long *)&ddb_regs.tf_r8,     FCN_NULL },
	{ "r9",		(long *)&ddb_regs.tf_r9,     FCN_NULL },
	{ "r10",	(long *)&ddb_regs.tf_r10,    FCN_NULL },
	{ "r11",	(long *)&ddb_regs.tf_r11,    FCN_NULL },
	{ "r12",	(long *)&ddb_regs.tf_r12,    FCN_NULL },
	{ "r13",	(long *)&ddb_regs.tf_r13,    FCN_NULL },
	{ "r14",	(long *)&ddb_regs.tf_r14,    FCN_NULL },
	{ "r15",	(long *)&ddb_regs.tf_r15,    FCN_NULL },
	{ "rip",	(long *)&ddb_regs.tf_rip,    FCN_NULL },
	{ "cs",		(long *)&ddb_regs.tf_cs,     FCN_NULL },
	{ "rflags",	(long *)&ddb_regs.tf_rflags, FCN_NULL },
	{ "rsp",	(long *)&ddb_regs.tf_rsp,    FCN_NULL },
	{ "ss",		(long *)&ddb_regs.tf_ss,     FCN_NULL },
};
struct db_variable * db_eregs = db_regs + nitems(db_regs);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

#define	NONE		0
#define	TRAP		1
#define	SYSCALL		2
#define	INTERRUPT	3
#define	AST		4

void db_nextframe(struct callframe **, db_addr_t *, long *, int,
    int (*) (const char *, ...));

/*
 * Figure out the next frame up in the call stack.
 * For trap(), we print the address of the faulting instruction and
 *   proceed with the calling frame.  We return the ip that faulted.
 *   If the trap was caused by jumping through a bogus pointer, then
 *   the next line in the backtrace will list some random function as
 *   being called.  It should get the argument list correct, though.
 *   It might be possible to dig out from the next frame up the name
 *   of the function that faulted, but that could get hairy.
 */
void
db_nextframe(struct callframe **fp, db_addr_t *ip, long *argp, int is_trap,
    int (*pr)(const char *, ...))
{

	switch (is_trap) {
	    case NONE:
		*ip = (db_addr_t)
			db_get_value((db_addr_t)&(*fp)->f_retaddr, 8, FALSE);
		*fp = (struct callframe *)
			db_get_value((db_addr_t)&(*fp)->f_frame, 8, FALSE);
		break;

	    default: {
		struct trapframe *tf;

		/* The only argument to trap() or syscall() is the trapframe. */
		tf = (struct trapframe *)argp;
		switch (is_trap) {
		case TRAP:
			(*pr)("--- trap (number %d) ---\n", tf->tf_trapno);
			break;
		case AST:
			(*pr)("--- ast ---\n");
			break;
		case SYSCALL:
			(*pr)("--- syscall (number %ld) ---\n", tf->tf_rax);
			break;
		case INTERRUPT:
			(*pr)("--- interrupt ---\n");
			break;
		}
		*fp = (struct callframe *)tf->tf_rbp;
		*ip = (db_addr_t)tf->tf_rip;
		break;
	    }
	}
}

static inline int
db_is_trap(const char *name)
{
	if (name != NULL) {
		if (!strcmp(name, "trap"))
			return TRAP;
		if (!strcmp(name, "ast"))
			return AST;
		if (!strcmp(name, "syscall"))
			return SYSCALL;
		if (name[0] == 'X') {
			if (!strncmp(name, "Xintr", 5) ||
			    !strncmp(name, "Xresume", 7) ||
			    !strncmp(name, "Xrecurse", 8) ||
			    !strcmp(name, "Xdoreti") ||
			    !strncmp(name, "Xsoft", 5))
				return INTERRUPT;
		}
	}
	return NONE;
}

const unsigned long *db_reg_args[6] = {
	(unsigned long *)&ddb_regs.tf_rdi,
	(unsigned long *)&ddb_regs.tf_rsi,
	(unsigned long *)&ddb_regs.tf_rdx,
	(unsigned long *)&ddb_regs.tf_rcx,
	(unsigned long *)&ddb_regs.tf_r8,
	(unsigned long *)&ddb_regs.tf_r9,
};

void
db_stack_trace_print(db_expr_t addr, boolean_t have_addr, db_expr_t count,
    char *modif, int (*pr)(const char *, ...))
{
	struct callframe *frame, *lastframe;
	unsigned long	*argp, *arg0;
	db_addr_t	callpc;
	unsigned int	cr4save = CR4_SMEP|CR4_SMAP;
	int		is_trap = 0;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_proc = FALSE;
	struct proc	*p;

	{
		char *cp = modif;
		char c;

		while ((c = *cp++) != 0) {
			if (c == 'p')
				trace_proc = TRUE;
			if (c == 'u')
				kernel_only = FALSE;
		}
	}

	if (trace_proc) {
		p = tfind((pid_t)addr);
		if (p == NULL) {
			(*pr) ("not found\n");
			return;
		}
	}

	cr4save = rcr4();
	if (cr4save & CR4_SMAP)
		lcr4(cr4save & ~CR4_SMAP);

	if (!have_addr) {
		frame = (struct callframe *)ddb_regs.tf_rbp;
		callpc = (db_addr_t)ddb_regs.tf_rip;
	} else if (trace_proc) {
		frame = (struct callframe *)p->p_addr->u_pcb.pcb_rbp;
		callpc = (db_addr_t)
		    db_get_value((db_addr_t)&frame->f_retaddr, 8, FALSE);
		frame = (struct callframe *)frame->f_frame;
	} else {
		frame = (struct callframe *)addr;
		callpc = (db_addr_t)
		    db_get_value((db_addr_t)&frame->f_retaddr, 8, FALSE);
		frame = (struct callframe *)frame->f_frame;
	}

	lastframe = 0;
	while (count && frame != 0) {
		int		narg;
		unsigned int	i;
		char *		name;
		db_expr_t	offset;
		Elf_Sym *	sym;

		if (INKERNEL(frame)) {
			sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
			db_symbol_values(sym, &name, NULL);
		} else {
			sym = NULL;
			name = NULL;
		}

		if (lastframe == 0 && sym == NULL) {
			/* Symbol not found, peek at code */
			unsigned long instr = db_get_value(callpc, 8, FALSE);

			offset = 1;
			if ((instr & 0x00ffffff) == 0x00e58955 ||
					/* enter: pushl %ebp, movl %esp, %ebp */
			    (instr & 0x0000ffff) == 0x0000e589
					/* enter+1: movl %esp, %ebp */) {
				offset = 0;
			}
		}
		if (INKERNEL(callpc) && (is_trap = db_is_trap(name)) != NONE)
			narg = 0;
		else {
			is_trap = NONE;
			narg = db_ctf_func_numargs(sym);
			if (narg < 0 || narg > 6)
				narg = 6;
		}

		if (name == NULL)
			(*pr)("%lx(", callpc);
		else
			(*pr)("%s(", name);

		if (lastframe == 0 && offset == 0 && !have_addr) {
			/* We have a breakpoint before the frame is set up */
			for (i = 0; i < narg; i++) {
				(*pr)("%lx", *db_reg_args[i]);
				if (--narg != 0)
					(*pr)(",");
			}

			/* Use %rsp instead */
			arg0 =
			    &((struct callframe *)(ddb_regs.tf_rsp-8))->f_arg0;
		} else {
			argp = (unsigned long *)frame;
			for (i = narg; i > 0; i--) {
				argp--;
				(*pr)("%lx", db_get_value((db_addr_t)argp,
				    sizeof(*argp), FALSE));
				if (--narg != 0)
					(*pr)(",");
			}

			arg0 = &frame->f_arg0;
		}

		for (argp = arg0; narg > 0; ) {
			(*pr)("%lx", db_get_value((db_addr_t)argp,
			    sizeof(*argp), FALSE));
			argp++;
			if (--narg != 0)
				(*pr)(",");
		}
		(*pr)(") at ");
		db_printsym(callpc, DB_STGY_PROC, pr);
		(*pr)("\n");

		if (lastframe == 0 && offset == 0 && !have_addr && !is_trap) {
			/* Frame really belongs to next callpc */
			lastframe = (struct callframe *)(ddb_regs.tf_rsp-8);
			callpc = (db_addr_t)
				 db_get_value((db_addr_t)&lastframe->f_retaddr,
				    8, FALSE);
			continue;
		}

		if (is_trap == INTERRUPT && lastframe != NULL) {
			/*
			 * Interrupt routines don't update %rbp, so it still
			 * points to the frame that was interrupted.  Pull
			 * back to just above lastframe so we can find the
			 * trapframe as with syscalls and traps.
			 */
			frame = (struct callframe *)&lastframe->f_retaddr;
			arg0 = &frame->f_arg0;
		}

		lastframe = frame;
		db_nextframe(&frame, &callpc, arg0, is_trap, pr);

		if (frame == 0) {
			/* end of chain */
			break;
		}
		if (INKERNEL(frame)) {
			/* staying in kernel */
			if (frame <= lastframe) {
				(*pr)("Bad frame pointer: %p\n", frame);
				break;
			}
		} else if (INKERNEL(lastframe)) {
			/* switch from user to kernel */
			if (kernel_only) {
				(*pr)("end of kernel\n");
				break;	/* kernel stack only */
			}
		} else {
			/* in user */
			if (frame <= lastframe) {
				(*pr)("Bad user frame pointer: %p\n",
					  frame);
				break;
			}
		}
		--count;
	}
	(*pr)("end trace frame: 0x%lx, count: %d\n", frame, count);

	if (count && is_trap != NONE) {
		db_printsym(callpc, DB_STGY_XTRN, pr);
		(*pr)(":\n");
	}

	if (cr4save & CR4_SMAP)
		lcr4(cr4save);
}

void
db_save_stack_trace(struct db_stack_trace *st)
{
	struct callframe *frame, *lastframe;
	db_addr_t callpc;
	unsigned int i;

	frame = __builtin_frame_address(0);

	callpc = db_get_value((db_addr_t)&frame->f_retaddr, 8, FALSE);
	frame = frame->f_frame;

	lastframe = NULL;
	for (i = 0; i < DB_STACK_TRACE_MAX && frame != NULL; i++) {
		struct trapframe *tf;
		char		*name;
		db_expr_t	offset;
		Elf_Sym *	sym;
		int		is_trap;

		st->st_pc[st->st_count++] = callpc;
		sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		if (INKERNEL(callpc))
			is_trap = db_is_trap(name);
		else
			is_trap = NONE;

		if (is_trap == NONE) {
			lastframe = frame;
			callpc = frame->f_retaddr;
			frame = frame->f_frame;
		} else {
			if (is_trap == INTERRUPT) {
				/*
				 * Interrupt routines don't update %rbp,
				 * so it still points to the frame that
				 * was interrupted.  Pull back to just
				 * above lastframe so we can find the
				 * trapframe as with syscalls and traps.
				 */
				if (lastframe == NULL)
					break;

				frame =
				    (struct callframe *)&lastframe->f_retaddr;
			}
			lastframe = frame;

			tf = (struct trapframe *)&frame->f_arg0;
			callpc = (db_addr_t)tf->tf_rip;
			frame = (struct callframe *)tf->tf_rbp;
		}

		if (!INKERNEL(frame))
			break;
		if (frame <= lastframe)
			break;
	}
}

vaddr_t
db_get_pc(struct trapframe *tf)
{
	struct callframe *cf = (struct callframe *)(tf->tf_rsp - sizeof(long));

	return db_get_value((db_addr_t)&cf->f_retaddr, sizeof(long), 0);
}

vaddr_t
db_get_probe_addr(struct trapframe *tf)
{
	return tf->tf_rip - BKPT_SIZE;
}
