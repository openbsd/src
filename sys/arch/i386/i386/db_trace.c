/*	$OpenBSD: db_trace.c,v 1.8 2003/01/16 04:15:17 art Exp $	*/
/*	$NetBSD: db_trace.c,v 1.18 1996/05/03 19:42:01 christos Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "es",		(long *)&ddb_regs.tf_es,     FCN_NULL },
	{ "ds",		(long *)&ddb_regs.tf_ds,     FCN_NULL },
	{ "edi",	(long *)&ddb_regs.tf_edi,    FCN_NULL },
	{ "esi",	(long *)&ddb_regs.tf_esi,    FCN_NULL },
	{ "ebp",	(long *)&ddb_regs.tf_ebp,    FCN_NULL },
	{ "ebx",	(long *)&ddb_regs.tf_ebx,    FCN_NULL },
	{ "edx",	(long *)&ddb_regs.tf_edx,    FCN_NULL },
	{ "ecx",	(long *)&ddb_regs.tf_ecx,    FCN_NULL },
	{ "eax",	(long *)&ddb_regs.tf_eax,    FCN_NULL },
	{ "eip",	(long *)&ddb_regs.tf_eip,    FCN_NULL },
	{ "cs",		(long *)&ddb_regs.tf_cs,     FCN_NULL },
	{ "eflags",	(long *)&ddb_regs.tf_eflags, FCN_NULL },
	{ "esp",	(long *)&ddb_regs.tf_esp,    FCN_NULL },
	{ "ss",		(long *)&ddb_regs.tf_ss,     FCN_NULL },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

struct i386_frame {
	struct i386_frame	*f_frame;
	int			f_retaddr;
	int			f_arg0;
};

#define	NONE		0
#define	TRAP		1
#define	SYSCALL		2
#define	INTERRUPT	3

db_addr_t	db_trap_symbol_value = 0;
db_addr_t	db_syscall_symbol_value = 0;
db_addr_t	db_kdintr_symbol_value = 0;
boolean_t	db_trace_symbols_found = FALSE;

void db_find_trace_symbols(void);
int db_numargs(struct i386_frame *);
void db_nextframe(struct i386_frame **, db_addr_t *, int *, int,
    int (*pr)(const char *, ...));

void
db_find_trace_symbols()
{
	db_expr_t	value;

	if (db_value_of_name("_trap", &value))
		db_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_kdintr", &value))
		db_kdintr_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_syscall", &value))
		db_syscall_symbol_value = (db_addr_t) value;
	db_trace_symbols_found = TRUE;
}

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
int
db_numargs(fp)
	struct i386_frame *fp;
{
	int	*argp;
	int	inst;
	int	args;
	extern char	etext[];

	argp = (int *)db_get_value((int)&fp->f_retaddr, 4, FALSE);
	if (argp < (int *)VM_MIN_KERNEL_ADDRESS || argp > (int *)etext) {
		args = 5;
	} else {
		inst = db_get_value((int)argp, 4, FALSE);
		if ((inst & 0xff) == 0x59)	/* popl %ecx */
			args = 1;
		else if ((inst & 0xffff) == 0xc483)	/* addl %n, %esp */
			args = ((inst >> 16) & 0xff) / 4;
		else
			args = 5;
	}
	return (args);
}

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
db_nextframe(fp, ip, argp, is_trap, pr)
	struct i386_frame **fp;		/* in/out */
	db_addr_t	*ip;		/* out */
	int *argp;			/* in */
	int is_trap;			/* in */
	int (*pr)(const char *, ...);
{

	switch (is_trap) {
	    case NONE:
		*ip = (db_addr_t)
			db_get_value((int) &(*fp)->f_retaddr, 4, FALSE);
		*fp = (struct i386_frame *)
			db_get_value((int) &(*fp)->f_frame, 4, FALSE);
		break;

	    default: {
		struct trapframe *tf;

		/* The only argument to trap() or syscall() is the trapframe. */
		tf = (struct trapframe *)argp;
		switch (is_trap) {
		case TRAP:
			(*pr)("--- trap (number %d) ---\n", tf->tf_trapno);
			break;
		case SYSCALL:
			(*pr)("--- syscall (number %d) ---\n", tf->tf_eax);
			break;
		case INTERRUPT:
			(*pr)("--- interrupt ---\n");
			break;
		}
		*fp = (struct i386_frame *)tf->tf_ebp;
		*ip = (db_addr_t)tf->tf_eip;
		break;
	    }
	}
}

void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char		*modif;
	int		(*pr)(const char *, ...);
{
	struct i386_frame *frame, *lastframe;
	int		*argp;
	db_addr_t	callpc;
	int		is_trap = 0;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;

#if 0
	if (!db_trace_symbols_found)
		db_find_trace_symbols();
#endif

	{
		register char *cp = modif;
		register char c;

		while ((c = *cp++) != 0) {
			if (c == 't')
				trace_thread = TRUE;
			if (c == 'u')
				kernel_only = FALSE;
		}
	}

	if (count == -1)
		count = 65535;

	if (!have_addr) {
		frame = (struct i386_frame *)ddb_regs.tf_ebp;
		callpc = (db_addr_t)ddb_regs.tf_eip;
	} else if (trace_thread) {
		(*pr) ("db_trace.c: can't trace thread\n");
	} else {
		frame = (struct i386_frame *)addr;
		callpc = (db_addr_t)
			 db_get_value((int)&frame->f_retaddr, 4, FALSE);
	}

	lastframe = 0;
	while (count && frame != 0) {
		int		narg;
		char *	name;
		db_expr_t	offset;
		db_sym_t	sym;
#define MAXNARG	16
		char	*argnames[MAXNARG], **argnp = NULL;

		sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		if (lastframe == 0 && sym == NULL) {
			/* Symbol not found, peek at code */
			int	instr = db_get_value(callpc, 4, FALSE);

			offset = 1;
			if ((instr & 0x00ffffff) == 0x00e58955 ||
					/* enter: pushl %ebp, movl %esp, %ebp */
			    (instr & 0x0000ffff) == 0x0000e589
					/* enter+1: movl %esp, %ebp */) {
				offset = 0;
			}
		}
		if (INKERNEL((int)frame) && name) {
			if (!strcmp(name, "_trap")) {
				is_trap = TRAP;
			} else if (!strcmp(name, "_syscall")) {
				is_trap = SYSCALL;
			} else if (name[0] == '_' && name[1] == 'X') {
				if (!strncmp(name, "_Xintr", 6) ||
				    !strncmp(name, "_Xresume", 8) ||
				    !strncmp(name, "_Xstray", 7) ||
				    !strncmp(name, "_Xhold", 6) ||
				    !strncmp(name, "_Xrecurse", 9) ||
				    !strcmp(name, "_Xdoreti") ||
				    !strncmp(name, "_Xsoft", 6)) {
					is_trap = INTERRUPT;
				} else
					goto normal;
			} else
				goto normal;
			narg = 0;
		} else {
		normal:
			is_trap = NONE;
			narg = MAXNARG;
			if (db_sym_numargs(sym, &narg, argnames))
				argnp = argnames;
			else
				narg = db_numargs(frame);
		}

		(*pr)("%s(", name);

		if (lastframe == 0 && offset == 0 && !have_addr) {
			/*
			 * We have a breakpoint before the frame is set up
			 * Use %esp instead
			 */
			argp = &((struct i386_frame *)(ddb_regs.tf_esp-4))->f_arg0;
		} else {
			argp = &frame->f_arg0;
		}

		while (narg) {
			if (argnp)
				(*pr)("%s=", *argnp++);
			(*pr)("%x", db_get_value((int)argp, 4, FALSE));
			argp++;
			if (--narg != 0)
				(*pr)(",");
		}
		(*pr)(") at ");
		db_printsym(callpc, DB_STGY_PROC, pr);
		(*pr)("\n");

		if (lastframe == 0 && offset == 0 && !have_addr) {
			/* Frame really belongs to next callpc */
			lastframe = (struct i386_frame *)(ddb_regs.tf_esp-4);
			callpc = (db_addr_t)
				 db_get_value((int)&lastframe->f_retaddr, 4, FALSE);
			continue;
		}

		lastframe = frame;
		db_nextframe(&frame, &callpc, &frame->f_arg0, is_trap, pr);

		if (frame == 0) {
			/* end of chain */
			break;
		}
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
		--count;
	}

	if (count && is_trap != NONE) {
		db_printsym(callpc, DB_STGY_XTRN, pr);
		(*pr)(":\n");
	}
}
