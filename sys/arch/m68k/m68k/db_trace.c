/*	$OpenBSD: db_trace.c,v 1.17 2002/05/18 09:49:17 art Exp $	*/
/*	$NetBSD: db_trace.c,v 1.20 1997/02/05 05:10:25 scottr Exp $	*/

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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_output.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

extern label_t	*db_recover;

/*
 * Register list
 */
static int db_var_short(struct db_variable *, db_expr_t *, int);

struct db_variable db_regs[] = {
	/* D0-D7 */
	{ "d0",	(long *)&ddb_regs.tf_regs[0],	FCN_NULL },
	{ "d1",	(long *)&ddb_regs.tf_regs[1],	FCN_NULL },
	{ "d2",	(long *)&ddb_regs.tf_regs[2],	FCN_NULL },
	{ "d3",	(long *)&ddb_regs.tf_regs[3],	FCN_NULL },
	{ "d4",	(long *)&ddb_regs.tf_regs[4],	FCN_NULL },
	{ "d5",	(long *)&ddb_regs.tf_regs[5],	FCN_NULL },
	{ "d6",	(long *)&ddb_regs.tf_regs[6],	FCN_NULL },
	{ "d7",	(long *)&ddb_regs.tf_regs[7],	FCN_NULL },
	/* A0-A7 */
	{ "a0",	(long *)&ddb_regs.tf_regs[8+0],	FCN_NULL },
	{ "a1",	(long *)&ddb_regs.tf_regs[8+1],	FCN_NULL },
	{ "a2",	(long *)&ddb_regs.tf_regs[8+2],	FCN_NULL },
	{ "a3",	(long *)&ddb_regs.tf_regs[8+3],	FCN_NULL },
	{ "a4",	(long *)&ddb_regs.tf_regs[8+4],	FCN_NULL },
	{ "a5",	(long *)&ddb_regs.tf_regs[8+5],	FCN_NULL },
	{ "a6",	(long *)&ddb_regs.tf_regs[8+6],	FCN_NULL },
	{ "sp",	(long *)&ddb_regs.tf_regs[8+7],	FCN_NULL },
	/* misc. */
	{ "pc",	(long *)&ddb_regs.tf_pc, 	FCN_NULL },
	{ "sr",	(long *)&ddb_regs.tf_sr,	db_var_short }
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

static int
db_var_short(varp, valp, op)
    struct db_variable *varp;
    db_expr_t *valp;
    int op;
{
    if (op == DB_VAR_GET)
	*valp = (db_expr_t) *((short *)varp->valuep);
    else
	*((short *)varp->valuep) = (short) *valp;
    return(0);
}

#define	MAXINT	0x7fffffff

#if 0
#define	INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS && \
			 (((vaddr_t)(va)) < (USRSTACK - MAXSSIZ) || \
			  ((vaddr_t)(va)) >= USRSTACK))
#else
/* XXX - Slight hack... */
extern int curpcb;
#define	INKERNEL(va)	(((int)(va) > curpcb) && \
			 ((int)(va) < (curpcb + USPACE)))
#endif

#define	get(addr, space) \
		(db_get_value((db_addr_t)(addr), sizeof(int), FALSE))
#define	get16(addr, space) \
		(db_get_value((db_addr_t)(addr), sizeof(u_short), FALSE))

#define	NREGISTERS	16

struct stackpos {
	 int	k_pc;
	 int	k_fp;
	 int	k_nargs;
	 int	k_entry;
	 int	k_caller;
	 int	k_flags;
	 int	k_regloc[NREGISTERS];
};

static void findentry(struct stackpos *);
static void findregs(struct stackpos *, db_addr_t);
static int  nextframe(struct stackpos *, int);
static void stacktop(db_regs_t *, struct stackpos *);


#define FR_SAVFP	0
#define FR_SAVPC	4
#define K_CALLTRAMP	1	/* for k_flags: caller is __sigtramp */
#define K_SIGTRAMP	2	/* for k_flags: this is   __sigtramp */

static void
stacktop(regs, sp)
	db_regs_t *regs;
	struct stackpos *sp;
{
	int i;

	/* Note: leave out a6, a7 */
	for (i = 0; i < (8+6); i++) {
		sp->k_regloc[i] = (int) &regs->tf_regs[i];
	}

	sp->k_fp = get(&regs->tf_regs[8+6], 0);
	/* skip sp (a7) */
	sp->k_pc = get(&regs->tf_pc, 0);
	sp->k_flags = 0;

	findentry(sp);
}


/*
 * The VAX has a very nice calling convention, and it is quite easy to
 * find saved registers, and the number of parameters. We are not nearly
 * so lucky. We must grub around in code for much of this information
 * (remember the PDP-11?), and the saved register list seems to be
 * especially hard to find.
 */

#define HIWORD	0xffff0000
#define LOWORD	0x0000ffff
#define LINKLA6	0x480e0000	/* linkl a6,#x    */
#define LINKWA6	0x4e560000	/* linkw a6,#x    */
#define ADDLSP	0xdffc0000	/* addl #x,sp    */
#define ADDWSP	0xdefc0000	/* addw #x,sp    */
#define LEASP	0x4fef0000	/* lea	sp@(x),sp*/
#define TSTBSP	0x4a2f0000	/* tstb sp@(x)   */
#define INSMSK	0xfff80000
#define MOVLSP	0x2e800000	/* movl dx,sp@   */
#define MOVLD0	0x20000000	/* movl d0,dx	 */
#define MOVLA0	0x20400000	/* movl d0,ax	 */
#define MVLMSK	0xf1ff0000
#define MOVEML	0x48d70000	/* moveml #x,sp@ */
#define JSR	0x4eb80000	/* jsr x.[WL]    */
#define JSRPC	0x4eba0000	/* jsr PC@( )    */
#define LONGBIT 0x00010000
#define BSR	0x61000000	/* bsr x	 */
#define BSRL	0x61ff0000	/* bsrl x	 */
#define BYTE3	0x0000ff00
#define LOBYTE	0x000000ff
#define ADQMSK	0xf1ff0000
#define ADDQSP	0x508f0000	/* addql #x,sp   */
#define ADDQWSP	0x504f0000	/* addqw #x,sp   */

struct nlist *	trampsym = 0;
struct nlist *	funcsym = 0;

static int
nextframe(sp, kerneltrace)
	struct stackpos *sp;
	int kerneltrace;
{
	int		i;
	db_addr_t	addr;
	db_addr_t	calladdr;
	db_addr_t	oldfp = sp->k_fp;

	/*
	 * Find our entry point. Then find out
	 * which registers we saved, and map them.
	 * Our entry point is the address our caller called.
	 */

	calladdr = sp->k_caller;
	addr     = sp->k_entry;
	if (sp->k_flags & K_CALLTRAMP) {
#if	0
	/* we never set CALLTRAMP */
		/*
		 * Caller was sigtramp.  Therefore:
		 *   - no registers were saved;
		 *   - no new frame-pointer
		 *   - caller found in sigcontext structure.
		 *   - WE become sigtramp
		 *   - we have no parameters
		 * MUCH MAGIC USED IN FINDING CALLER'S PC.
		 */
		sp->k_pc = sp->k_caller;
		sp->k_entry = trampsym->n_value;
		sp->k_flags = 0;
		addr = get(sp->k_fp + sizeof(int) * 11, DSP);
		sp->k_nargs = 0;
#if DEBUG
		db_printf("nextframe: sigcontext at 0x%x, signaled at 0x%x\n",
		    addr, sp->k_caller);
#endif
		errflg = 0;
#endif /* 0 */
	} else {
		if (addr == MAXINT) {
			/* we don't know what registers are involved here--
			   invalidate all */
			for (i = 0; i < NREGISTERS; i++)
				sp->k_regloc[i] = -1;
		} else
			findregs(sp, addr);

		/* find caller's pc and fp */
		sp->k_pc = calladdr;
		sp->k_fp = get(sp->k_fp + FR_SAVFP, DSP);

		/* 
		 * Now that we have assumed the identity of our caller, find
		 * how many longwords of argument WE were called with.
		 */
		sp->k_flags = 0;

		/*
		 * Don't dig around in user stack to find no. of args and
		 * entry point if just tracing the kernel
		 */
		if (kerneltrace && !INKERNEL(sp->k_fp)) {
			sp->k_nargs = 0;
			sp->k_entry = MAXINT;
		} else
			findentry(sp);
	}

	if (sp->k_fp == 0 || oldfp == sp->k_fp)
		return 0;
	return (sp->k_fp);
}

static void
findentry(sp)
	struct stackpos *sp;
{ 
	/* 
	 * Set the k_nargs and k_entry fields in the stackpos structure.  This
	 * is called from stacktop() and from nextframe().  Our caller will do
	 * an addq or addl or addw to sp just after we return to pop off our
	 * arguments.  Find that instruction and extract the value.
	 */
	int		instruc;
	int		val;
	db_addr_t	addr, nextword;
	label_t		db_jmpbuf;
	label_t		*savejmp;

	savejmp = db_recover;
	db_recover = &db_jmpbuf;
	if (setjmp(&db_jmpbuf)) {
		/* oops -- we touched something we ought not to have */
		/* cannot trace caller of "start" */
		sp->k_entry = MAXINT;
		sp->k_nargs = 0;
		db_recover = savejmp;
		return;
	}

	addr = get(sp->k_fp + FR_SAVPC, DSP);
	if (addr == 0) {
		/* oops -- we touched something we ought not to have */
		/* cannot trace caller of "start" */
		sp->k_entry = MAXINT;
		sp->k_nargs = 0;
		db_recover = savejmp;
		return;
	}
	instruc  = get(addr - 6, ISP);
	nextword = get(addr - 4, ISP);

	db_recover = savejmp;

	if ((instruc & HIWORD) == (JSR | LONGBIT)) {
		/* longword offset here */
		sp->k_caller = addr - 6;
		sp->k_entry  = nextword;
	} else if ((instruc & HIWORD) == BSRL) {
		/* longword self-relative offset */
		sp->k_caller = addr - 6;
		sp->k_entry  = nextword + (addr - 4);
	} else {
		instruc = nextword;
		if ((instruc & HIWORD) == JSR) {
			/* short word offset */
			sp->k_caller = addr - 4;
			sp->k_entry  = instruc & LOWORD;
		} else if ((instruc & HIWORD) == BSR) {
			/* short word, self-relative offset */
			sp->k_caller = addr - 4;
			sp->k_entry  = (addr - 2) + (short)(instruc & LOWORD);
		} else if ((instruc & HIWORD) == JSRPC) {
			/* PC-relative, short word offset */
			sp->k_caller = addr - 4;
			sp->k_entry  = (addr - 2) + (instruc & LOWORD);
		} else {
			if ((instruc & BYTE3) == (BSR >> 16)) {
				/* byte, self-relative offset */
				sp->k_caller = addr - 2;
				sp->k_entry  = addr + (char)(instruc & LOBYTE);
			} else {
				/* was a call through a proc parameter */
				sp->k_caller = addr - 2;
				sp->k_entry  = MAXINT;
				/*
				 * We know that sigtramp calls your signal
				 * catcher this way -- see if this is the
				 * tramp: if so then:
				 *   - set the K_CALLTRAMP flag, for use by
				 *     nextframe();
				 *   - take k_entry from __sigfunc array.
				 */
#if	0
	/* not in kernel */
				/*
				 * The number (9) in the below expression is
				 * magic: it is the number of stack items below
				 * callee`s fp and sigtramp`s copy of the
				 * signal number.
				 */
				if (trampsym &&
				    (findsym(sp->k_caller, ISYM), cursym == trampsym)) {
					int signl;
					sp->k_flags |= K_CALLTRAMP;
					if (funcsym) {
						signl = get(sp->k_fp + sizeof(int) * 9, DSP);
						sp->k_entry = get(funcsym->n_value+(sizeof(int(*)()))*signl, DSP);
					} else
						sp->k_entry = -1;

					errflg = 0;
#ifdef DEBUG
					db_printf("Caller is sigtramp: signal is %d: entry is %x\n",
					    signl, sp->k_entry);
#endif
				}
#ifdef DEBUG
				else
				db_printf("Non-tramp jsr a0@\n");
#endif
#endif /* 0 */
			}
		}
	}
	instruc = get(addr, ISP);
	/* on bad days, the compiler dumps a register move here */
	if ((instruc & MVLMSK) == MOVLA0 ||
	    (instruc & MVLMSK) == MOVLD0)
		instruc = get(addr += 2, ISP);
	if ((instruc & ADQMSK) == ADDQSP ||
	    (instruc & ADQMSK) == ADDQWSP) {
		val = 0;
		do {
			int n;
			n = (instruc >> (16+9)) & 07;
			if (n == 0)
				n = 8;
			val += n;
			instruc = get(addr += 2, ISP);
		} while ((instruc & ADQMSK) == ADDQSP ||
			 (instruc & ADQMSK) == ADDQWSP);
	} else if ((instruc & HIWORD) == ADDLSP)
		val = get(addr + 2, ISP);
	else if ((instruc & HIWORD) == ADDWSP ||
		 (instruc & HIWORD) == LEASP)
		val = instruc & LOWORD;
	else
		val = 20;
	sp->k_nargs = val / 4;
}

/*
 * Look at the procedure prolog of the current called procedure.
 * Figure out which registers we saved, and where they are
 */
static void
findregs(sp, addr)
	struct stackpos *sp;
	db_addr_t addr;
{
	long instruc, val, i;
	int  regp;

	regp = 0;
	instruc = get(addr, ISP);
	if ((instruc & HIWORD) == LINKLA6) {
		instruc = get(addr + 2, ISP);
		addr += 6;
		regp = sp->k_fp + instruc;
	} else if ((instruc & HIWORD) == LINKWA6) {
		addr += 4;
		if ((instruc &= LOWORD) == 0) {
			/* look for addl */
			instruc = get(addr, ISP);
			if ((instruc & HIWORD) == ADDLSP) {
				instruc = get(addr + 2, ISP);
				addr += 6;
			}
			/* else frame is really size 0 */
		} else {
			/* link offset was non-zero -- sign extend it */
			instruc <<= 16;
			instruc >>= 16;
		}
		/* we now have the negative frame size */
		regp = sp->k_fp + instruc;
	}

	/* find which registers were saved */
	/* (expecting probe instruction next) */
	instruc = get(addr, ISP);
	if ((instruc & HIWORD) == TSTBSP)
		addr += 4;

	/* now we expect either a moveml or a movl */
	instruc = get(addr, ISP);
	if ((instruc & INSMSK) == MOVLSP) {
		/* only saving one register */
		i = (instruc >> 16) & 07;
		sp->k_regloc[i] = regp;
	} else if ((instruc & HIWORD) == MOVEML) {
		/* saving multiple registers or unoptimized code */
		val = instruc & LOWORD;
		i = 0;
		while (val) {
			if (val & 1) {
				sp->k_regloc[i] = regp;
				regp += sizeof(int);
			}
			val >>= 1;
			i++;
		}
	}
	/* else no registers saved */
}

/*
 *	Frame tracing.
 */
void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char		*modif;
	int		(*pr)(const char *, ...);
{
	int i, nargs;
	long val;
	db_addr_t	regp;
	char *		name;
	struct stackpos pos;
	boolean_t	kernel_only = TRUE;
	int		fault_pc = 0;

	{
		char *cp = modif;
		char c;

		while ((c = *cp++) != 0)
			if (c == 'u')
				kernel_only = FALSE;
	}

	if (count == -1)
		count = 65535;

	if (!have_addr)
		stacktop(&ddb_regs, &pos);
#if 0
	else {

		/*
		 * Only have user register state.
		 */
		pcb_t	t_pcb;
		db_regs_t *user_regs;
		
		t_pcb = (pcb_t) get(&th->pcb, 0);
		user_regs = (db_regs_t *)
			get(&t_pcb->user_regs, 0);
		
		stacktop(user_regs, &pos);

		/* foo*/
	}
#endif
	else {
		pos.k_flags = 0;
		pos.k_fp = addr;

		pos.k_nargs = 0;
		pos.k_pc = MAXINT;
		pos.k_entry = MAXINT;
		/* sorry, we cannot find our registers without knowing our pc */
		for (i = 0; i < NREGISTERS; i++)
			pos.k_regloc[i] = 0;
		findentry(&pos);
	}

	while (count) {
		count--;

		/* HACK */
		if (pos.k_pc == MAXINT) {
			name = "?";
			pos.k_pc = 0;
		} else {
			db_find_sym_and_offset(pos.k_pc, &name, &val);
			if (name == 0) {
				val = MAXINT;
				name = "?";
			}
		}

		/*
		 * Since faultstkadj doesn't set up a valid stack frame,
		 * we would assume it was the source of the fault. To
		 * get around this we peek at the fourth argument of
		 * "trap()" (the stack frame at the time of the fault)
		 * to determine the _real_ value of PC when things wen
		 * wrong.
		 *
		 * NOTE: If the argument list for 'trap()' ever changes,
		 * we lose.
		 */
		if (strcmp("_trap", name) == 0) {
			int tfp;

			/* Point to 'trap()'s 4th argument (frame structure) */
			tfp = pos.k_fp + FR_SAVFP + 4 + (4 * 4);

			/* Determine if fault was from kernel or user mode */
			regp = tfp + offsetof(struct frame, f_sr);
			if (!USERMODE(get16(regp, DSP))) {
				/*
				 * Definitely a kernel mode fault,
				 * so get the PC at the time of the fault.
				 */
				regp = tfp + offsetof(struct frame, f_pc);
				fault_pc = get(regp, DSP);
			}
		} else
		if (fault_pc) {
			if (strcmp("faultstkadj", name) == 0) {
				db_find_sym_and_offset(fault_pc, &name, &val);
				if (name == 0) {
					val = MAXINT;
					name = "?";
				}
			}
			fault_pc = 0;
		}

		(*pr)("%s", name);
		if (pos.k_entry != MAXINT && name) {
			char *	entry_name;
			long	e_val;

			db_find_sym_and_offset(pos.k_entry, &entry_name,
			    &e_val);
			if (entry_name != 0 && entry_name != name &&
			    e_val != val) {
				(*pr)("(?)\n%s", entry_name);
			}
		}
		(*pr)("(");
		regp = pos.k_fp + FR_SAVFP + 4;
		if ((nargs = pos.k_nargs)) {
			while (nargs--) {
				(*pr)("%lx", get(regp += 4, DSP));
				if (nargs)
					(*pr)(",");
			}
		}
		if (val == MAXINT)
			(*pr)(") at %x\n", pos.k_pc);
		else
			(*pr)(") + %lx\n", val);

		/*
		 * Stop tracing if frame ptr no longer points into kernel
		 * stack.
		 */
		if (kernel_only && !INKERNEL(pos.k_fp))
			break;
		if (nextframe(&pos, kernel_only) == 0)
			break;
	}
}

