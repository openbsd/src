/*	$NetBSD: db_trace.c,v 1.2 1994/10/26 08:24:58 cgd Exp $	*/

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
 * 	File: ns532/db_trace.c
 *	Author: Tero Kivinen, Tatu Ylonen
 *	Helsinki University of Technology 1992.
 *
 *	Stack trace and special register support for debugger.
 */


#include <mach/boolean.h>
#include <machine/db_machdep.h>
#include <machine/pic.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

#include <kern/thread.h>

int db_spec_regs();
int db_ns532_reg_value();
int db_ns532_kreg_value();

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "r0",	(long *)&ddb_regs.r0,  db_ns532_reg_value },
	{ "r1",	(long *)&ddb_regs.r1,  db_ns532_reg_value },
	{ "r2",	(long *)&ddb_regs.r2,  db_ns532_reg_value },
	{ "r3",	(long *)&ddb_regs.r3,  db_ns532_reg_value },
	{ "r4",	(long *)&ddb_regs.r4,  db_ns532_reg_value },
	{ "r5",	(long *)&ddb_regs.r5,  db_ns532_reg_value },
	{ "r6",	(long *)&ddb_regs.r6,  db_ns532_reg_value },
	{ "r7",	(long *)&ddb_regs.r7,  db_ns532_reg_value },
	{ "sp",	(long *)&ddb_regs.usp, db_ns532_reg_value },
	{ "fp",	(long *)&ddb_regs.fp,  db_ns532_reg_value },
	{ "sb", (long *)&ddb_regs.sb,  db_ns532_reg_value },
	{ "pc", (long *)&ddb_regs.pc,  db_ns532_reg_value },
	{ "psr",(long *)&ddb_regs.psr, db_ns532_reg_value },
	{ "tear",(long *)&ddb_regs.tear,db_ns532_reg_value },
	{ "msr",(long *)&ddb_regs.msr, db_ns532_reg_value },
	{ "ipl",(long *)&db_active_ipl,db_ns532_reg_value },
#ifdef FLOATS_SAVED
	{ "f0",	(long *)&ddb_regs.l0a, db_ns532_reg_value },
	{ "f1",	(long *)&ddb_regs.l0b, db_ns532_reg_value },
	{ "f2",	(long *)&ddb_regs.l1a, db_ns532_reg_value },
	{ "f3",	(long *)&ddb_regs.l1b, db_ns532_reg_value },
	{ "f4",	(long *)&ddb_regs.l2a, db_ns532_reg_value },
	{ "f5",	(long *)&ddb_regs.l2b, db_ns532_reg_value },
	{ "f6",	(long *)&ddb_regs.l3a, db_ns532_reg_value },
	{ "f7",	(long *)&ddb_regs.l3b, db_ns532_reg_value },
	{ "fsr",(long *)&ddb_regs.fsr, db_ns532_reg_value },
#endif FLOATS_SAVED
	{ "ksp",	(long *) 0,	db_spec_regs },
	{ "intbase",	(long *) 0,	db_spec_regs },
	{ "ptb",	(long *) 0,	db_spec_regs },
	{ "ivar",	(long *) 0,	db_spec_regs },
	{ "rtear", 	(long *) 0,	db_spec_regs }, /* current reg value */
	{ "mcr",	(long *) 0,	db_spec_regs },
	{ "rmsr",	(long *) 0,	db_spec_regs }, /* current reg value */
	{ "dcr",	(long *) 0,	db_spec_regs },
	{ "dsr",	(long *) 0,	db_spec_regs },
	{ "car",	(long *) 0,	db_spec_regs },
	{ "bpc",	(long *) 0,	db_spec_regs },
	{ "cfg",	(long *) 0,	db_spec_regs }
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

struct db_regs_bits_s {
	char *name;
	char *bitfld;
};

struct db_regs_bits_s db_regs_bits[] = {
	"psr", "0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,i,p,s,u,n,z,f,v,0,l,t,c",
	"fsr", "0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,rmb,s5,s4,s3,s2,s1,s0,@roundm,if,ien,uf,uen,@trapt",
	"mcr", "0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,ao,ds,ts,tu",
	"msr", "0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0 :,0,0,0,0: ,@sst,ust,ddt,@tex",
	"rmsr", "0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0 :,0,0,0,0: ,@sst,ust,ddt,@tex",
	"dcr", "0,0,0,0: ,0,0,0,0: ,den,sd,ud,pce,tr,bcp,si,0: ,0,0,0,0: ,0,0,0,bf,cae,crd,cwr,vnp,cbe3,cbe2,cbe1,cbe0",
	"dsr", "rd,bpc,bex,bca,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0",
	"cfg", "0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,0,0: ,0,0,pf,lic,ic,ldc,dc,de,1,1,1,1,c,m,f,i"
    };

struct db_regs_bits_s *db_eregs_bits = db_regs_bits +
    sizeof(db_regs_bits)/sizeof(db_regs_bits[0]);

struct db_regs_fields_s {
	char *name;
	int bits;
	char *values;
};

struct db_regs_fields_s db_regs_fields[] = {
	"trapt", 3, "None,Underflow,Overflow,Div by 0,Ill inst,Invalid oper,Inexact res,Reserved",
	"roundm", 2, "Nearest,Zero,Pos inf,Neg inf",
	"tex", 2, "None,1st PTE inv,2nd PTE inv,Prot",
	"sst", 4, "0000,0001,0010,0011,0100,0101,0110,0111,Seq.ins.fetch,Non.seq.ins.fetch,Data transfer,Read-modify-write,Read eff.addr,1101,1110,1111"
};

struct db_regs_fields_s *db_eregs_fields = db_regs_fields +
    sizeof(db_regs_fields)/sizeof(db_regs_fields[0]);
  
/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vm_offset_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

struct ns532_frame {
	struct ns532_frame	*f_frame;
	int			f_retaddr;
	int			f_arg0;
};

#define	TRAP		1
#define	INTERRUPT	2
#define SYSCALL		3

struct ns532_kregs {
	char	*name;
	int	offset;
} ns532_kregs[] = {
	{ "r3", (int)(&((struct ns532_kernel_state *)0)->k_r3) },
	{ "r4", (int)(&((struct ns532_kernel_state *)0)->k_r4) },
	{ "r5", (int)(&((struct ns532_kernel_state *)0)->k_r5) },
	{ "r6", (int)(&((struct ns532_kernel_state *)0)->k_r6) },
	{ "r7", (int)(&((struct ns532_kernel_state *)0)->k_r7) },
	{ "sp", (int)(&((struct ns532_kernel_state *)0)->k_sp) },
	{ "fp", (int)(&((struct ns532_kernel_state *)0)->k_fp) },
	{ "pc", (int)(&((struct ns532_kernel_state *)0)->k_pc) },
	{ 0 },
};

int *
db_lookup_ns532_kreg(name, kregp)
	char *name;
	int *kregp;
{
	register struct ns532_kregs *kp;
	
	for (kp = ns532_kregs; kp->name; kp++) {
		if (strcmp(name, kp->name) == 0)
		    return((int *)((int)kregp + kp->offset));
	}
	return(0);
}
	
int
db_ns532_reg_value(vp, valuep, flag, ap)
	struct	db_variable	*vp;
	db_expr_t		*valuep;
	int			flag;
	db_var_aux_param_t	ap;
{
	int			*dp = 0;
	db_expr_t		null_reg = 0;
	register thread_t	thread = ap->thread;
	
	if (db_option(ap->modif, 'u')) {
		if (thread == THREAD_NULL) {
			if ((thread = current_thread()) == THREAD_NULL)
			    db_error("no user registers\n");
		}
		if (thread == current_thread()) {
			if ((ddb_regs.psr & PSR_U) == 0)
			    dp = vp->valuep;
		}
	} else {
		if (thread == THREAD_NULL || thread == current_thread()) {
			dp = vp->valuep;
		} else if ((thread->state & TH_SWAPPED) == 0 && 
			   thread->kernel_stack) {
			dp = db_lookup_ns532_kreg(vp->name,
				   (int *)(STACK_IKS(thread->kernel_stack)));
			if (dp == 0)
			    dp = &null_reg;
		} else if ((thread->state & TH_SWAPPED) &&
			   thread->swap_func != thread_exception_return) {
			/* only pc is valid */
			if (vp->valuep == (int *) &ddb_regs.pc) {
				dp = (int *)(&thread->swap_func);
			} else {
				dp = &null_reg;
			}
		}
	}
	if (dp == 0) {
		if (thread->pcb == 0)
		    db_error("no pcb\n");
		dp = (int *)((int)(&thread->pcb->iss) + 
			     ((int)vp->valuep - (int)&ddb_regs));
	}
	if (flag == DB_VAR_SET)
	    *dp = *valuep;
	else
	    *valuep = *dp;
	return(0);
}

db_addr_t	db_trap_symbol_value = 0;
db_addr_t	db_intr_symbol_value = 0;
boolean_t	db_trace_symbols_found = FALSE;

void
db_find_trace_symbols()
{
	db_expr_t	value;
	if (db_value_of_name("_trap", &value))
	    db_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_interrupt", &value))
	    db_intr_symbol_value = (db_addr_t) value;
	db_trace_symbols_found = TRUE;
}

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
int db_numargs_default = 5;

int
db_numargs(fp, task)
	struct ns532_frame *fp;
	task_t	task;
{
	int a;
	char *nextaddr;
	
	nextaddr = (char *) db_get_task_value((int) &fp->f_frame, 4,
					      FALSE, task);
	a = nextaddr-(char *)fp-8;
	a /= 4;
	if (a < 0 || a > 16)
	    a = db_numargs_default;
	return a;
}

extern int (*ivect[])();

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
db_nextframe(fp, ip, frame_type, thread)
	struct ns532_frame	**fp;		/* in/out */
	db_addr_t		*ip;		/* out */
	int			frame_type;	/* in */
	thread_t		thread;		/* in */
{
	extern char *	trap_type[];
	extern int	TRAP_TYPES;
	
	struct ns532_saved_state *saved_regs;
	int vector;
	task_t task = (thread != THREAD_NULL)? thread->task: TASK_NULL;
	
	switch(frame_type) {
	      case TRAP:
		/*
		 * We know that trap() has 1 argument and we know that
		 * it is an (int *).
		 */
		saved_regs = (struct ns532_saved_state *)
		    db_get_value((int) &((*fp)->f_arg0), 4, FALSE);
		if (saved_regs->trapno >= 0 &&
		    saved_regs->trapno < TRAP_TYPES) {
			db_printf(">>>>>> %s trap at ",
				  trap_type[saved_regs->trapno]);
		} else {
			db_printf(">>>>>> trap (number %d) at ",
				  saved_regs->trapno & 0xffff);
		}
		db_task_printsym(saved_regs->pc, DB_STGY_PROC, task);
		db_printf(" <<<<<<\n");
		*fp = (struct ns532_frame *)saved_regs->fp;
		*ip = (db_addr_t)saved_regs->pc;
		break;
	      case INTERRUPT:
		/*
		 * We know that interrupt() has 3 argument.
		 */
		
		vector = db_get_value((int) &((*fp)->f_arg0), 4, FALSE);
		saved_regs = (struct ns532_saved_state *)
		    db_get_value((int) &((*fp)->f_arg0) + 8, 4, FALSE);
		db_printf(">>>>>> ");
		if (vector >=0 && vector < NINTR) {
			db_task_printsym((int) ivect[vector],
					 DB_STGY_PROC, task);
			db_printf(" interrupt at ");
		} else {
			db_printf("interrupt vector %d at ", vector);
		}
		db_task_printsym(saved_regs->pc, DB_STGY_PROC, task);
		db_printf(" <<<<<<\n");
		*fp = (struct ns532_frame *)saved_regs->fp;
		*ip = (db_addr_t)saved_regs->pc;
		break;
	      default:
		*ip = (db_addr_t)
		    db_get_task_value((int) &(*fp)->f_retaddr, 4, FALSE, task);
		*fp = (struct ns532_frame *)
		    db_get_task_value((int) &(*fp)->f_frame, 4, FALSE, task);
		break;
	}
}

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char		*modif;
{
	struct ns532_frame *frame, *lastframe;
	int		*argp;
	db_addr_t	callpc;
	int		frame_type;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;
	char		*filename;
	int		linenum;
	task_t		task;
	thread_t	th;
	int		user_frame = 0;
	extern unsigned	db_maxoff;
	
	
	if (!db_trace_symbols_found)
	    db_find_trace_symbols();
	
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
	
	if (!have_addr && !trace_thread) {
		frame = (struct ns532_frame *)ddb_regs.fp;
		callpc = (db_addr_t)ddb_regs.pc;
		th = current_thread();
		task = (th != THREAD_NULL) ? th->task : TASK_NULL;
	} else if (trace_thread) {
		if (have_addr) {
			th = (thread_t) addr;
			if (!db_check_thread_address_valid(th))
			    return;
		} else {
			th = db_default_thread;
			if (th = THREAD_NULL)
			    current_thread();
			if (th == THREAD_NULL) {
				db_printf("no active thread\n");
				return;
			}
		}
		task = th->task;
		if (th == current_thread()) {
			frame = (struct ns532_frame *)ddb_regs.fp;
			callpc = (db_addr_t)ddb_regs.pc;
		} else {
			if (th->pcb == 0) {
				db_printf("thread has no pcb\n");
				return;
			}
			if ((th->state & TH_SWAPPED) ||
			    th->kernel_stack == 0) {
				register struct ns532_saved_state *iss =
				    &th->pcb->iss;
				
				db_printf("Continuation ");
				db_task_printsym(th->swap_func, DB_STGY_PROC,
						 task);
				db_printf("\n");
				
				frame = (struct ns532_frame *) (iss->fp);
				callpc = (db_addr_t) (iss->pc);
			} else {
				register struct ns532_kernel_state *iks;
				iks = STACK_IKS(th->kernel_stack);
				frame = (struct ns532_frame *) (iks->k_fp);
				callpc = (db_addr_t) (iks->k_pc);
			}
		}
	} else {
		frame = (struct ns532_frame *)addr;
		th = (db_default_thread)? db_default_thread: current_thread();
		task = (th != THREAD_NULL)? th->task: TASK_NULL;
		callpc = (db_addr_t)db_get_task_value((int)&frame->f_retaddr,
						      4, FALSE, task);
	}
	
	if (!INKERNEL(callpc) && !INKERNEL(frame)) {
		db_printf(">>>>>> user space <<<<<<\n");
		user_frame++;
	}
	
	while (count-- && frame != 0) {
		register int narg;
		char *	name;
		db_expr_t	offset;
		
		if (INKERNEL(callpc) && user_frame == 0) {
			db_addr_t call_func = 0;
			
			db_symbol_values(db_search_task_symbol(callpc, 
							       DB_STGY_XTRN,
							       &offset,
							       TASK_NULL),
					 &name, &call_func);
			if (call_func == db_trap_symbol_value) {
				frame_type = TRAP;
				narg = 1;
			} else if (call_func == db_intr_symbol_value) {
				frame_type = INTERRUPT;
				narg = 3;
#ifdef SYSCALL_FRAME_IMPLEMENTED
			} else if (call_func == db_syscall_symbol_value) {
				frame_type = SYSCALL;
				goto next_frame;
#endif
			} else {
				frame_type = 0;
				narg = db_numargs(frame, task);
			}
		} else if ((INKERNEL(callpc) == 0) != (INKERNEL(frame) == 0)) {
			frame_type = 0;
			narg = -1;
		} else {
			frame_type = 0;
			narg = db_numargs(frame, task);
		}
		
		db_find_task_sym_and_offset(callpc, &name, &offset, task);
		if (name == 0 || offset > db_maxoff) {
			db_printf("0x%x(", callpc);
			offset = 0;
		} else
		    db_printf("%s(", name);
		
		argp = &frame->f_arg0;
		while (narg > 0) {
			db_printf("%x",
				  db_get_task_value((int)argp,4,FALSE,task));
			argp++;
			if (--narg != 0)
			    db_printf(",");
		}
		if (narg < 0)
		    db_printf("...");
		db_printf(")");
		if (offset) {
			db_printf("+%x", offset);
		}
		if (db_line_at_pc(0, &filename, &linenum, callpc)) {
			db_printf(" [%s", filename);
			if (linenum > 0)
			    db_printf(":%d", linenum);
			printf("]");
		}
		
		db_printf("\n");
	      next_frame:
		lastframe = frame;
		db_nextframe(&frame, &callpc, frame_type, th);
		
		if (frame == 0) {
			/* end of chain */
			break;
		}
		if (!INKERNEL(lastframe) ||
		    (!INKERNEL(callpc) && !INKERNEL(frame)))
		    user_frame++;
		if (user_frame == 1) {
			db_printf(">>>>>> user space <<<<<<\n");
			if (kernel_only)
			    break;
		}
		if (frame <= lastframe) {
			if (INKERNEL(lastframe) && !INKERNEL(frame))
			    continue;
			db_printf("Bad frame pointer: 0x%x\n", frame);
			break;
		}
	}
}

/**********************************************

  Get/Set value of special registers.

  *********************************************/

int db_spec_regs(vp, valp, what)
	struct db_variable *vp;
	db_expr_t *valp;
	int what;
{
	if (strcmp(vp->name, "intbase") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_intbase();
	    else
		_set_intbase(*valp);
	else if (strcmp(vp->name, "ptb") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_ptb();
	    else
		_set_ptb(*valp);
	else if (strcmp(vp->name, "ivar") == 0)
	    if (what == DB_VAR_GET)
		*valp = 0;
	    else
		_invalidate_page(*valp);
	else if (strcmp(vp->name, "rtear") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_tear();
	    else
		_set_tear(*valp);
	else if (strcmp(vp->name, "mcr") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_mcr();
	    else
		_set_mcr(*valp);
	else if (strcmp(vp->name, "rmsr") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_msr();
	    else
		_set_msr(*valp);
	else if (strcmp(vp->name, "dcr") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_dcr();
	    else
		_set_dcr(*valp);
	else if (strcmp(vp->name, "dsr") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_dsr();
	    else
		_set_dsr(*valp);
	else if (strcmp(vp->name, "car") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_car();
	    else
		_set_car(*valp);
	else if (strcmp(vp->name, "bpc") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_bpc();
	    else
		_set_bpc(*valp);
	else if (strcmp(vp->name, "cfg") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_cfg();
	    else
		_set_cfg(*valp);
	else if (strcmp(vp->name, "ksp") == 0)
	    if (what == DB_VAR_GET)
		*valp = _get_ksp();
	    else
		_set_ksp(*valp);
	else
	    db_printf("Internal error, unknown register in db_spec_regs");
}

/***************************************************

  Print special bit mask registers in bitmask format

  **************************************************/

int db_print_spec_reg(vp, valuep)
	struct db_variable *vp;
	db_expr_t valuep;
{
	struct db_regs_bits_s *dbrb;
	int i;
	char *p, buf[256];
	
	for (dbrb = db_regs_bits; dbrb < db_eregs_bits; dbrb++)
	    if (strcmp(dbrb->name, vp->name) == 0)
		break;
	if (dbrb == db_eregs_bits)
	    return 1;
	db_printf("\t<");
	p = dbrb->bitfld;
	for (i = 31 ; i >= 0 && *p ; i--) {		/* All bits */
		if (*p >= '0' && *p <= '9') {		/* is number */
			for (; *p != ',' && *p; p++)	/* Skip number */
			    ;
			if (*p == ',')
			    p++;
		} else {				/* Is text */
			char *q;
			strcpy(buf, p);
			for(q = buf; *q != ',' && *q; q++, p++) /* Find end */
			    ;
			if (*p == ',')
			    p++;
			*q='\0';
			if (buf[0] == '@') {		/* Is bitfield */
				struct db_regs_fields_s *df;
				q=buf+1;		/* Find field */
				for (df = db_regs_fields;
				     df < db_eregs_fields; df++)
				    if (strcmp(df->name, q) == 0)
					break;
				if (df == db_eregs_fields)
				    db_printf("Internal error in print_spec_regs [%s]\n", buf);
				else {
					unsigned int ff;
					db_printf("%s=", q); /* Print field */
					ff = (~(0xffffffff << df->bits)) &
					    (valuep>>(i-(df->bits-1)));
					i += df->bits;	/* Find value */
					for (q = df->values;
					     ff > 0 && *q; ff--, q++)
					    for (;*q != ',' && *q; q++)
						;
					if (*q != '\0') {
						strcpy(buf, q);
						for(q = buf; *q != ',' && *q;
						    q++)
						    ;
						*q='\0';
						db_printf("%s "); /* Print value */
					} else
					    db_printf(" ");
				}
			} else {			/* Normal bit */
				if ((1<<i) & valuep) {
					db_printf("%s ", buf);
				}
			}
		}
	}
	db_printf(">");
	return 0;
}
