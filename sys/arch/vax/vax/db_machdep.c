/*	$OpenBSD: db_machdep.c,v 1.15 2006/01/02 21:44:52 miod Exp $	*/
/*	$NetBSD: db_machdep.c,v 1.17 1999/06/20 00:58:23 ragge Exp $	*/

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
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 *
 * VAX enhancements by cmcmanis@mcmanis.com no rights reserved :-)
 *
 */

/*
 * Interface to new debugger.
 * Taken from i386 port and modified for vax.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>
#include <machine/trap.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/cpu.h>
#include <machine/../vax/gencons.h>

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_var.h>
#include <ddb/db_variables.h>

extern	label_t	*db_recover;

void	kdbprinttrap(int, int);

int	db_active = 0;

extern int qdpolling;
static	int splsave; /* IPL before entering debugger */

/*
 * VAX Call frame on the stack, this from
 * "Computer Programming and Architecture, The VAX-11"
 *		Henry Levy & Richard Eckhouse Jr.
 *			ISBN 0-932376-07-X
 */
typedef struct __vax_frame {
	u_int	vax_cond;				/* condition handler               */
	u_int	vax_psw:16;				/* 16 bit processor status word    */
	u_int	vax_regs:12;			/* Register save mask.             */
	u_int	vax_zero:1;				/* Always zero                     */
	u_int	vax_calls:1;			/* True if CALLS, false if CALLG   */
	u_int	vax_spa:2;				/* Stack pointer alignment         */
	u_int	*vax_ap;				/* argument pointer                */
	struct __vax_frame	*vax_fp;	/* frame pointer of previous frame */
	u_int	vax_pc;					/* program counter                 */
	u_int	vax_args[1];			/* 0 or more arguments             */
} VAX_CALLFRAME;

/*
 * DDB is called by either <ESC> - D on keyboard, via a TRACE or
 * BPT trap or from kernel, normally as a result of a panic.
 * If it is the result of a panic, set the ddb register frame to
 * contain the registers when panic was called. (easy to debug).
 */
void
kdb_trap(frame)
	struct trapframe *frame;
{
	int s;

	switch (frame->trap) {
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP:	/* single_step */
		break;

	/* XXX todo: should be migrated to use VAX_CALLFRAME at some point */
	case T_KDBTRAP:
		if (panicstr) {
			struct	callsframe *pf, *df;

			df = (void *)frame->fp; /* start of debug's calls */
			pf = (void *)df->ca_fp;	/* start of panic's calls */
			bcopy(&pf->ca_argno, &ddb_regs.r0, sizeof(int) * 12);
			ddb_regs.fp = pf->ca_fp;
			ddb_regs.pc = pf->ca_pc;
			ddb_regs.ap = pf->ca_ap;
			ddb_regs.sp = (unsigned)pf;
			ddb_regs.psl = frame->psl & ~0x1fffe0;
			ddb_regs.psl |= pf->ca_maskpsw & 0xffe0;
			ddb_regs.psl |= (splsave << 16);
		}
		break;

	default:
		if ((boothowto & RB_KDB) == 0)
			return;

		kdbprinttrap(frame->trap, frame->code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	if (!panicstr)
		bcopy(frame, &ddb_regs, sizeof(struct trapframe));

	/* XXX Should switch to interrupt stack here, if needed. */

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(frame->trap, frame->code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	if (!panicstr)
		bcopy(&ddb_regs, frame, sizeof(struct trapframe));
	frame->sp = mfpr(PR_USP);

	return;
}

extern char *traptypes[];
extern int no_traps;

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	db_printf("kernel: ");
	if (type >= no_traps || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", traptypes[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	db_addr_t addr;
	size_t	size;
	char	*data;
{
	bcopy((caddr_t)addr, data, size);
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	db_addr_t addr;
	size_t	size;
	char	*data;
{
	memcpy((caddr_t)addr, data, size);
}

void
Debugger()
{
	splsave = splx(0xe);
	mtpr(0xf, PR_SIRR); /* beg for debugger */
	splx(splsave);
}

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{"r0",	(long *)&ddb_regs.r0,	FCN_NULL},
	{"r1",	(long *)&ddb_regs.r1,	FCN_NULL},
	{"r2",	(long *)&ddb_regs.r2,	FCN_NULL},
	{"r3",	(long *)&ddb_regs.r3,	FCN_NULL},
	{"r4",	(long *)&ddb_regs.r4,	FCN_NULL},
	{"r5",	(long *)&ddb_regs.r5,	FCN_NULL},
	{"r6",	(long *)&ddb_regs.r6,	FCN_NULL},
	{"r7",	(long *)&ddb_regs.r7,	FCN_NULL},
	{"r8",	(long *)&ddb_regs.r8,	FCN_NULL},
	{"r9",	(long *)&ddb_regs.r9,	FCN_NULL},
	{"r10",	(long *)&ddb_regs.r10,	FCN_NULL},
	{"r11",	(long *)&ddb_regs.r11,	FCN_NULL},
	{"ap",	(long *)&ddb_regs.ap,	FCN_NULL},
	{"fp",	(long *)&ddb_regs.fp,	FCN_NULL},
	{"sp",	(long *)&ddb_regs.sp,	FCN_NULL},
	{"pc",	(long *)&ddb_regs.pc,	FCN_NULL},
	{"psl",	(long *)&ddb_regs.psl,	FCN_NULL},
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

#define IN_USERLAND(x)	(((u_int)(x) & 0x80000000) == 0)

/*
 * Dump a stack traceback. Takes two arguments:
 *	fp - CALL FRAME pointer
 *	stackbase - Lowest stack value
 */
static void
db_dump_stack(VAX_CALLFRAME *fp, u_int stackbase, int (*pr)(const char *, ...)) {
	u_int nargs, arg_base, regs;
	VAX_CALLFRAME *tmp_frame;
	db_expr_t	diff;
	db_sym_t	sym;
	char		*symname;

	(*pr)("Stack traceback : \n");
	if (IN_USERLAND(fp)) {
		(*pr)("  Process is executing in user space.\n");
		return;
	}

	while (((u_int)(fp->vax_fp) > stackbase) && 
			((u_int)(fp->vax_fp) < (stackbase + USPACE))) {

		diff = INT_MAX;
		symname = NULL;
		sym = db_search_symbol(fp->vax_pc, DB_STGY_ANY, &diff);
		db_symbol_values(sym, &symname, 0);
		(*pr)("%s+0x%lx(", symname, diff);

		/*
		 * Figure out the arguments by using a bit of subtlety.
		 * As the argument pointer may have been used as a temporary
		 * by the callee ... recreate what it would have pointed to
		 * as follows:
		 *  The vax_regs value has a 12 bit bitmask of the registers
		 *    that were saved on the stack.
		 *	Store that in 'regs' and then for every bit that is
		 *    on (indicates the register contents are on the stack)
		 *    increment the argument base (arg_base) by one.
		 *  When that is done, args[arg_base] points to the longword
		 *    that identifies the number of arguments.
		 *	arg_base+1 - arg_base+n are the argument pointers/contents.
		 */

		/* First get the frame that called this function ... */
		tmp_frame = fp->vax_fp;

		/* Isolate the saved register bits, and count them */
		regs = tmp_frame->vax_regs;
		for (arg_base = 0; regs != 0; regs >>= 1) {
			if (regs & 1)
				arg_base++;
		}

		/* number of arguments is then pointed to by vax_args[arg_base] */
		nargs = tmp_frame->vax_args[arg_base];
		if (nargs) {
			nargs--; /* reduce by one for formatting niceties */
			arg_base++; /* skip past the actual number of arguments */
			while (nargs--)
				(*pr)("0x%x,", tmp_frame->vax_args[arg_base++]);

			/* now print out the last arg with closing brace and \n */
			(*pr)("0x%x)\n", tmp_frame->vax_args[arg_base]);
		} else
			(*pr)("void)\n");
		/* move to the next frame */
		fp = fp->vax_fp;
	}
}

/*
 * Implement the trace command which has the form:
 *
 *	trace				<-- Trace panic (same as before)
 *	trace	0x88888 	<-- Trace process whose address is 888888
 *	trace/t				<-- Trace current process (0 if no current proc)
 *	trace/t	0tnn		<-- Trace process nn (0t for decimal)
 */
void
db_stack_trace_print(addr, have_addr, count, modif, pr)
        db_expr_t       addr;		/* Address parameter */
        boolean_t       have_addr;	/* True if addr is valid */
        db_expr_t       count;		/* Optional count */
        char            *modif;		/* pointer to flag modifier 't' */
	int		(*pr)(const char *, ...);
{
	extern vaddr_t 	proc0paddr;
	struct proc	*p = curproc;
	struct user	*uarea;
	int		trace_proc;
	pid_t	curpid;
	char	*s;
 
	/* Check to see if we're tracing a process */
	trace_proc = 0;
	s = modif;
	while (!trace_proc && *s) {
		if (*s++ == 't')
			trace_proc++;	/* why yes we are */
	}

	/* Trace a panic */
	if (! trace_proc) {
		if (! panicstr) {
			(*pr)("Not a panic, use trace/t to trace a process.\n");
			return;
		}
		(*pr)("panic: %s\n", panicstr);
		/* xxx ? where did we panic and whose stack are we using? */
		db_dump_stack((VAX_CALLFRAME *)(ddb_regs.fp), ddb_regs.ap, pr);
		return;
	}

	/* 
	 * If user typed an address its either a PID, or a Frame 
	 * if no address then either current proc or panic
	 */
	if (have_addr) {
		if (trace_proc) {
			p = pfind((int)addr);
			/* Try to be helpful by looking at it as if it were decimal */
			if (p == NULL) {
				u_int	tpid = 0;
				u_int	foo = addr;

				while (foo != 0) {
					int digit = (foo >> 28) & 0xf;
					if (digit > 9) {
						(*pr)("  No such process.\n");
						return;
					}
					tpid = tpid * 10 + digit;
					foo = foo << 4;
				}
				p = pfind(tpid);
				if (p == NULL) {
					(*pr)("  No such process.\n");
					return;
				}
			}
		} else {
			p = (struct proc *)(addr);
			if (pfind(p->p_pid) != p) {
				(*pr)("  This address does not point to a valid process.\n");
				return;
			}
		}
	} else {
		if (trace_proc) {
			p = curproc;
			if (p == NULL) {
				(*pr)("trace: no current process! (ignored)\n");
				return;
			}
		} else {
			if (! panicstr) {
				(*pr)("Not a panic, no active process, ignored.\n");
				return;
			}
		}
	}
	if (p == NULL) {
		uarea = (struct user *)proc0paddr;
		curpid = 0;
	} else {
		uarea = p->p_addr;
		curpid = p->p_pid;
	}
	(*pr)("Process %d\n", curpid);
	(*pr)("  PCB contents:\n");
	(*pr)("	KSP = 0x%x\n", (unsigned int)(uarea->u_pcb.KSP));
	(*pr)("	ESP = 0x%x\n", (unsigned int)(uarea->u_pcb.ESP));
	(*pr)("	SSP = 0x%x\n", (unsigned int)(uarea->u_pcb.SSP));
	(*pr)("	USP = 0x%x\n", (unsigned int)(uarea->u_pcb.USP));
	(*pr)("	R[00] = 0x%08x    R[06] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[0]), (unsigned int)(uarea->u_pcb.R[6]));
	(*pr)("	R[01] = 0x%08x    R[07] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[1]), (unsigned int)(uarea->u_pcb.R[7]));
	(*pr)("	R[02] = 0x%08x    R[08] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[2]), (unsigned int)(uarea->u_pcb.R[8]));
	(*pr)("	R[03] = 0x%08x    R[09] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[3]), (unsigned int)(uarea->u_pcb.R[9]));
	(*pr)("	R[04] = 0x%08x    R[10] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[4]), (unsigned int)(uarea->u_pcb.R[10]));
	(*pr)("	R[05] = 0x%08x    R[11] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[5]), (unsigned int)(uarea->u_pcb.R[11]));
	(*pr)("	AP = 0x%x\n", (unsigned int)(uarea->u_pcb.AP));
	(*pr)("	FP = 0x%x\n", (unsigned int)(uarea->u_pcb.FP));
	(*pr)("	PC = 0x%x\n", (unsigned int)(uarea->u_pcb.PC));
	(*pr)("	PSL = 0x%x\n", (unsigned int)(uarea->u_pcb.PSL));
	(*pr)("	Trap frame pointer: 0x%x\n", 
							(unsigned int)(uarea->u_pcb.framep));
	db_dump_stack((VAX_CALLFRAME *)(uarea->u_pcb.FP), (u_int) uarea->u_pcb.KSP, pr);
	return;
#if 0
	while (((u_int)(cur_frame->vax_fp) > stackbase) && 
			((u_int)(cur_frame->vax_fp) < (stackbase + USPACE))) {
		u_int nargs;
		VAX_CALLFRAME *tmp_frame;

		diff = INT_MAX;
		symname = NULL;
		sym = db_search_symbol(cur_frame->vax_pc, DB_STGY_ANY, &diff);
		db_symbol_values(sym, &symname, 0);
		(*pr)("%s+0x%lx(", symname, diff);

		/*
		 * Figure out the arguments by using a bit of subterfuge
		 * since the argument pointer may have been used as a temporary
		 * by the callee ... recreate what it would have pointed to
		 * as follows:
		 *  The vax_regs value has a 12 bit bitmask of the registers
		 *    that were saved on the stack.
		 *	Store that in 'regs' and then for every bit that is
		 *    on (indicates the register contents are on the stack)
		 *    increment the argument base (arg_base) by one.
		 *  When that is done, args[arg_base] points to the longword
		 *    that identifies the number of arguments.
		 *	arg_base+1 - arg_base+n are the argument pointers/contents.
		 */

		/* First get the frame that called this function ... */
		tmp_frame = cur_frame->vax_fp;

		/* Isolate the saved register bits, and count them */
		regs = tmp_frame->vax_regs;
		for (arg_base = 0; regs != 0; regs >>= 1) {
			if (regs & 1)
				arg_base++;
		}

		/* number of arguments is then pointed to by vax_args[arg_base] */
		nargs = tmp_frame->vax_args[arg_base];
		if (nargs) {
			nargs--; /* reduce by one for formatting niceties */
			arg_base++; /* skip past the actual number of arguments */
			while (nargs--)
				(*pr)("0x%x,", tmp_frame->vax_args[arg_base++]);

			/* now print out the last arg with closing brace and \n */
			(*pr)("0x%x)\n", tmp_frame->vax_args[++arg_base]);
		} else
			(*pr)("void)\n");
		/* move to the next frame */
		cur_frame = cur_frame->vax_fp;
	}

	/*
	 * DEAD CODE, previous panic tracing code.
	 */
	if (! have_addr) {
		printf("Trace default\n");
		if (panicstr) {
			cf = (int *)ddb_regs.sp;
		} else {
			printf("Don't know what to do without panic\n");
			return;
		}
		if (p)
			paddr = (u_int)p->p_addr;
		else
			paddr = proc0paddr;

		stackbase = (ddb_regs.psl & PSL_IS ? istack : paddr);
 	}
#endif
}

static int ddbescape = 0;

int
kdbrint(tkn)
	int tkn;
{

	if (ddbescape && ((tkn & 0x7f) == 'D')) {
		if (db_console)
			mtpr(0xf, PR_SIRR);
		ddbescape = 0;
		return 1;
	}

	if ((ddbescape == 0) && ((tkn & 0x7f) == 27)) {
		ddbescape = 1;
		return 1;
	}

	if (ddbescape) {
		ddbescape = 0;
		return 2;
	}
	
	ddbescape = 0;
	return 0;
}

