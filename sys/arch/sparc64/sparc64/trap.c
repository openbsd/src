/*	$OpenBSD: trap.c,v 1.23 2002/10/12 01:09:43 krw Exp $	*/
/*	$NetBSD: trap.c,v 1.73 2001/08/09 01:03:01 eeh Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)trap.c	8.4 (Berkeley) 9/23/93
 */

#define NEW_FPSTATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/syslog.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include "systrace.h"
#include <dev/systrace.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/trap.h>
#include <machine/instr.h>
#include <machine/pmap.h>

#ifdef DDB
#include <machine/db_machdep.h>
#else
#include <machine/frame.h>
#endif
#ifdef COMPAT_SVR4
#include <machine/svr4_machdep.h>
#endif
#ifdef COMPAT_SVR4_32
#include <machine/svr4_32_machdep.h>
#endif

#include <sparc64/fpu/fpu_extern.h>
#include <sparc64/sparc64/cache.h>

#ifndef offsetof
#define	offsetof(s, f) ((int)&((s *)0)->f)
#endif

/* trapstats */
int trapstats = 0;
int protfix = 0;
int udmiss = 0;	/* Number of normal/nucleus data/text miss/protection faults */
int udhit = 0;	
int udprot = 0;
int utmiss = 0;
int kdmiss = 0;
int kdhit = 0;	
int kdprot = 0;
int ktmiss = 0;
int iveccnt = 0; /* number if normal/nucleus interrupt/interrupt vector faults */
int uintrcnt = 0;
int kiveccnt = 0;
int kintrcnt = 0;
int intristk = 0; /* interrupts when already on intrstack */
int intrpoll = 0; /* interrupts not using vector lists */
int wfill = 0;
int kwfill = 0;
int wspill = 0;
int wspillskip = 0;
int rftucnt = 0;
int rftuld = 0;
int rftudone = 0;
int rftkcnt[5] = { 0, 0, 0, 0, 0 };

/*
 * Initial FPU state is all registers == all 1s, everything else == all 0s.
 * This makes every floating point register a signalling NaN, with sign bit
 * set, no matter how it is interpreted.  Appendix N of the Sparc V8 document
 * seems to imply that we should do this, and it does make sense.
 */
__asm(".align 64");
struct	fpstate64 initfpstate = {
	{ ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
	  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0 }
};

/*
 * There are more than 100 trap types, but most are unused.
 *
 * Trap type 0 is taken over as an `Asynchronous System Trap'.
 * This is left-over Vax emulation crap that should be fixed.
 *
 * Traps not supported on the spitfire are marked with `*',
 * and additions are marked with `+'
 */
static const char T[] = "*trap";
const char *trap_type[] = {
	/* non-user vectors */
	"ast",			/* 0 */
	"power on reset",	/* 1 */
	"watchdog reset",	/* 2 */
	"externally initiated reset",/*3 */
	"software initiated reset",/* 4 */
	"RED state exception",	/* 5 */
	T, T,			/* 6..7 */
	"instruction access exception",	/* 8 */
	"*instruction MMU miss",/* 9 */
	"instruction access error",/* 0a */
	T, T, T, T, T,		/* 0b..0f */
	"illegal instruction",	/* 10 */
	"privileged opcode",	/* 11 */
	"*unimplemented LDD",	/* 12 */
	"*unimplemented STD",	/* 13 */
	T, T, T, T,		/* 14..17 */
	T, T, T, T, T, T, T, T, /* 18..1f */
	"fp disabled",		/* 20 */
	"fp exception ieee 754",/* 21 */
	"fp exception other",	/* 22 */
	"tag overflow",		/* 23 */
	"clean window",		/* 24 */
	T, T, T,		/* 25..27 -- trap continues */
	"division by zero",	/* 28 */
	"*internal processor error",/* 29 */
	T, T, T, T, T, T,	/* 2a..2f */
	"data access exception",/* 30 */
	"*data access MMU miss",/* 31 */
	"data access error",	/* 32 */
	"*data access protection",/* 33 */
	"mem address not aligned",	/* 34 */
	"LDDF mem address not aligned",/* 35 */
	"STDF mem address not aligned",/* 36 */
	"privileged action",	/* 37 */
	"LDQF mem address not aligned",/* 38 */
	"STQF mem address not aligned",/* 39 */
	T, T, T, T, T, T,	/* 3a..3f */
	"*async data error",	/* 40 */
	"level 1 int",		/* 41 */
	"level 2 int",		/* 42 */
	"level 3 int",		/* 43 */
	"level 4 int",		/* 44 */
	"level 5 int",		/* 45 */
	"level 6 int",		/* 46 */
	"level 7 int",		/* 47 */
	"level 8 int",		/* 48 */
	"level 9 int",		/* 49 */
	"level 10 int",		/* 4a */
	"level 11 int",		/* 4b */
	"level 12 int",		/* 4c */
	"level 13 int",		/* 4d */
	"level 14 int",		/* 4e */
	"level 15 int",		/* 4f */
	T, T, T, T, T, T, T, T, /* 50..57 */
	T, T, T, T, T, T, T, T, /* 58..5f */
	"+interrupt vector",	/* 60 */
	"+PA_watchpoint",	/* 61 */
	"+VA_watchpoint",	/* 62 */
	"+corrected ECC error",	/* 63 */
	"+fast instruction access MMU miss",/* 64 */
	T, T, T,		/* 65..67 -- trap continues */
	"+fast data access MMU miss",/* 68 */
	T, T, T,		/* 69..6b -- trap continues */
	"+fast data access protection",/* 6c */
	T, T, T,		/* 6d..6f -- trap continues */
	T, T, T, T, T, T, T, T, /* 70..77 */
	T, T, T, T, T, T, T, T, /* 78..7f */
	"spill 0 normal",	/* 80 */
	T, T, T,		/* 81..83 -- trap continues */
	"spill 1 normal",	/* 84 */
	T, T, T,		/* 85..87 -- trap continues */
	"spill 2 normal",	/* 88 */
	T, T, T,		/* 89..8b -- trap continues */
	"spill 3 normal",	/* 8c */
	T, T, T,		/* 8d..8f -- trap continues */
	"spill 4 normal",	/* 90 */
	T, T, T,		/* 91..93 -- trap continues */
	"spill 5 normal",	/* 94 */
	T, T, T,		/* 95..97 -- trap continues */
	"spill 6 normal",	/* 98 */
	T, T, T,		/* 99..9b -- trap continues */
	"spill 7 normal",	/* 9c */
	T, T, T,		/* 9c..9f -- trap continues */
	"spill 0 other",	/* a0 */
	T, T, T,		/* a1..a3 -- trap continues */
	"spill 1 other",	/* a4 */
	T, T, T,		/* a5..a7 -- trap continues */
	"spill 2 other",	/* a8 */
	T, T, T,		/* a9..ab -- trap continues */
	"spill 3 other",	/* ac */
	T, T, T,		/* ad..af -- trap continues */
	"spill 4 other",	/* b0 */
	T, T, T,		/* b1..b3 -- trap continues */
	"spill 5 other",	/* b4 */
	T, T, T,		/* b5..b7 -- trap continues */
	"spill 6 other",	/* b8 */
	T, T, T,		/* b9..bb -- trap continues */
	"spill 7 other",	/* bc */
	T, T, T,		/* bc..bf -- trap continues */
	"fill 0 normal",	/* c0 */
	T, T, T,		/* c1..c3 -- trap continues */
	"fill 1 normal",	/* c4 */
	T, T, T,		/* c5..c7 -- trap continues */
	"fill 2 normal",	/* c8 */
	T, T, T,		/* c9..cb -- trap continues */
	"fill 3 normal",	/* cc */
	T, T, T,		/* cd..cf -- trap continues */
	"fill 4 normal",	/* d0 */
	T, T, T,		/* d1..d3 -- trap continues */
	"fill 5 normal",	/* d4 */
	T, T, T,		/* d5..d7 -- trap continues */
	"fill 6 normal",	/* d8 */
	T, T, T,		/* d9..db -- trap continues */
	"fill 7 normal",	/* dc */
	T, T, T,		/* dc..df -- trap continues */
	"fill 0 other",		/* e0 */
	T, T, T,		/* e1..e3 -- trap continues */
	"fill 1 other",		/* e4 */
	T, T, T,		/* e5..e7 -- trap continues */
	"fill 2 other",		/* e8 */
	T, T, T,		/* e9..eb -- trap continues */
	"fill 3 other",		/* ec */
	T, T, T,		/* ed..ef -- trap continues */
	"fill 4 other",		/* f0 */
	T, T, T,		/* f1..f3 -- trap continues */
	"fill 5 other",		/* f4 */
	T, T, T,		/* f5..f7 -- trap continues */
	"fill 6 other",		/* f8 */
	T, T, T,		/* f9..fb -- trap continues */
	"fill 7 other",		/* fc */
	T, T, T,		/* fc..ff -- trap continues */

	/* user (software trap) vectors */
	"syscall",		/* 100 */
	"breakpoint",		/* 101 */
	"zero divide",		/* 102 */
	"flush windows",	/* 103 */
	"clean windows",	/* 104 */
	"range check",		/* 105 */
	"fix align",		/* 106 */
	"integer overflow",	/* 107 */
	"svr4 syscall",		/* 108 */
	"4.4 syscall",		/* 109 */
	"kgdb exec",		/* 10a */
	T, T, T, T, T,		/* 10b..10f */
	T, T, T, T, T, T, T, T,	/* 11a..117 */
	T, T, T, T, T, T, T, T,	/* 118..11f */
	"svr4 getcc",		/* 120 */
	"svr4 setcc",		/* 121 */
	"svr4 getpsr",		/* 122 */
	"svr4 setpsr",		/* 123 */
	"svr4 gethrtime",	/* 124 */
	"svr4 gethrvtime",	/* 125 */
	T,			/* 126 */
	"svr4 gethrestime",	/* 127 */
	T, T, T, T, T, T, T, T, /* 128..12f */
	T, T,			/* 130..131 */
	"get condition codes",	/* 132 */
	"set condision codes",	/* 133 */
	T, T, T, T,		/* 134..137 */
	T, T, T, T, T, T, T, T, /* 138..13f */
	T, T, T, T, T, T, T, T, /* 140..147 */
	T, T, T, T, T, T, T, T, /* 148..14f */
	T, T, T, T, T, T, T, T, /* 150..157 */
	T, T, T, T, T, T, T, T, /* 158..15f */
	T, T, T, T,		/* 160..163 */
	"SVID syscall64",	/* 164 */
	"SPARC Intl syscall64",	/* 165 */
	"OS vedor spec syscall",/* 166 */
	"HW OEM syscall",	/* 167 */
	"ret from deferred trap",	/* 168 */
};

#define	N_TRAP_TYPES	(sizeof trap_type / sizeof *trap_type)

static __inline void userret(struct proc *, int,  u_quad_t);
static __inline void share_fpu(struct proc *, struct trapframe64 *);

void trap(struct trapframe64 *tf, unsigned type, vaddr_t pc, long tstate);
void data_access_fault(struct trapframe64 *tf, unsigned type, vaddr_t pc, 
	vaddr_t va, vaddr_t sfva, u_long sfsr);
void data_access_error(struct trapframe64 *tf, unsigned type, 
	vaddr_t afva, u_long afsr, vaddr_t sfva, u_long sfsr);
void text_access_fault(struct trapframe64 *tf, unsigned type, 
	vaddr_t pc, u_long sfsr);
void text_access_error(struct trapframe64 *tf, unsigned type, 
	vaddr_t pc, u_long sfsr, vaddr_t afva, u_long afsr);
void syscall(struct trapframe64 *, register_t code, register_t pc);

/*
 * Define the code needed before returning to user mode, for
 * trap, mem_access_fault, and syscall.
 */
static __inline void
userret(p, pc, oticks)
	struct proc *p;
	int pc;
	u_quad_t oticks;
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_ast) {
		want_ast = 0;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
	}
	if (want_resched) {
		/*
		 * We are being preempted.
		 */
		preempt(NULL);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, pc, (int)(p->p_sticks - oticks) * psratio);
	}

#ifdef notyet
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
#else
	curpriority = p->p_priority;
#endif
}

/*
 * If someone stole the FPU while we were away, do not enable it
 * on return.  This is not done in userret() above as it must follow
 * the ktrsysret() in syscall().  Actually, it is likely that the
 * ktrsysret should occur before the call to userret.
 *
 * Oh, and don't touch the FPU bit if we're returning to the kernel.
 */
static __inline void share_fpu(p, tf)
	struct proc *p;
	struct trapframe64 *tf;
{
	if (!(tf->tf_tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) &&
	    (tf->tf_tstate & (PSTATE_PEF<<TSTATE_PSTATE_SHIFT)) && fpproc != p)
		tf->tf_tstate &= ~(PSTATE_PEF<<TSTATE_PSTATE_SHIFT);
}

/*
 * Called from locore.s trap handling, for non-MMU-related traps.
 * (MMU-related traps go through mem_access_fault, below.)
 */
void
trap(tf, type, pc, tstate)
	struct trapframe64 *tf;
	unsigned type;
	vaddr_t pc;
	long tstate;
{
	struct proc *p;
	struct pcb *pcb;
	int pstate = (tstate>>TSTATE_PSTATE_SHIFT);
	int64_t n;
	u_quad_t sticks;
	union sigval sv;

	sv.sival_ptr = (void *)pc;

	/* This steps the PC over the trap. */
#define	ADVANCE (n = tf->tf_npc, tf->tf_pc = n, tf->tf_npc = n + 4)

	uvmexp.traps++;
	/*
	 * Generally, kernel traps cause a panic.  Any exceptions are
	 * handled early here.
	 */
	if (pstate & PSTATE_PRIV) {
#ifdef DDB
		if (type == T_BREAKPOINT) {
			write_all_windows();
			if (kdb_trap(type, tf)) {
				/* ADVANCE; */
				return;
			}
		}
		if (type == T_PA_WATCHPT || type == T_VA_WATCHPT) {
			if (kdb_trap(type, tf)) {
				/* DDB must turn off watchpoints or something */
				return;
			}
		}
#endif
		/*
		 * The kernel needs to use FPU registers for block
		 * load/store.  If we trap in priviliged code, save
		 * the FPU state if there is any and enable the FPU.
		 *
		 * We rely on the kernel code properly enabling the FPU
		 * in %fprs, otherwise we'll hang here trying to enable
		 * the FPU.
		 */
		if (type == T_FPDISABLED) {
extern void db_printf(const char * , ...);
#ifndef NEW_FPSTATE
			if (fpproc != NULL) {	/* someone else had it */
				savefpstate(fpproc->p_md.md_fpstate);
				fpproc = NULL;
				/* Enable the FPU */
/*				loadfpstate(initfpstate);*/
			}
			tf->tf_tstate |= (PSTATE_PEF<<TSTATE_PSTATE_SHIFT);
			return;
#else
			struct proc *newfpproc;

			/* New scheme */
			if (CLKF_INTR((struct clockframe *)tf) || !curproc) {
				newfpproc = &proc0;
			} else {
				newfpproc = curproc;
			}
			if (fpproc != newfpproc) {
				if (fpproc != NULL) {
				/* someone else had it, maybe? */
					savefpstate(fpproc->p_md.md_fpstate);
					fpproc = NULL;
				}
				/* If we have an allocated fpstate, load it */
				if (newfpproc->p_md.md_fpstate != 0) {
					fpproc = newfpproc;
					loadfpstate(fpproc->p_md.md_fpstate);
				} else
					fpproc = NULL;
			}
			/* Enable the FPU */
			tf->tf_tstate |= (PSTATE_PEF<<TSTATE_PSTATE_SHIFT);
			return;
#endif
		}
		goto dopanic;
	}
	if ((p = curproc) == NULL)
		p = &proc0;
	sticks = p->p_sticks;
	pcb = &p->p_addr->u_pcb;
	p->p_md.md_tf = tf;	/* for ptrace/signals */

	switch (type) {

	default:
		if (type < 0x100) {
			extern int trap_trace_dis;
dopanic:
			trap_trace_dis = 1;

			printf("trap type 0x%x: pc=%lx", type, pc); 
			printf(" npc=%lx pstate=%b\n",
			    (long)tf->tf_npc, pstate, PSTATE_BITS);
			panic(type < N_TRAP_TYPES ? trap_type[type] : T);
			/* NOTREACHED */
		}
#if defined(COMPAT_SVR4) || defined(COMPAT_SVR4_32)
badtrap:
#endif
		trapsignal(p, SIGILL, type, ILL_ILLOPC, sv);
		break;

#if defined(COMPAT_SVR4) || defined(COMPAT_SVR4_32)
	case T_SVR4_GETCC:
	case T_SVR4_SETCC:
	case T_SVR4_GETPSR:
	case T_SVR4_SETPSR:
	case T_SVR4_GETHRTIME:
	case T_SVR4_GETHRVTIME:
	case T_SVR4_GETHRESTIME:
#if defined(COMPAT_SVR4_32)
		if (svr4_32_trap(type, p))
			break;
#endif
#if defined(COMPAT_SVR4)
		if (svr4_trap(type, p))
			break;
#endif
		goto badtrap;
#endif

	case T_AST:
		break;	/* the work is all in userret() */

	case T_ILLINST:
	case T_INST_EXCEPT:
	case T_TEXTFAULT:
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);	/* XXX code?? */
		break;

	case T_PRIVINST:
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);	/* XXX code?? */
		break;

	case T_FPDISABLED: {
		struct fpstate64 *fs = p->p_md.md_fpstate;

		if (fs == NULL) {
			/* NOTE: fpstate must be 64-bit aligned */
			fs = malloc((sizeof *fs), M_SUBPROC, M_WAITOK);
			*fs = initfpstate;
			fs->fs_qsize = 0;
			p->p_md.md_fpstate = fs;
		}
		/*
		 * If we have not found an FPU, we have to emulate it.
		 *
		 * Since All UltraSPARC CPUs have an FPU how can this happen?
		 */
		if (!foundfpu) {
#ifdef notyet
			fpu_emulate(p, tf, fs);
			break;
#else
			trapsignal(p, SIGFPE, 0, FPE_FLTINV, sv);	/* XXX code?? */
			break;
#endif
		}
		/*
		 * We may have more FPEs stored up and/or ops queued.
		 * If they exist, handle them and get out.  Otherwise,
		 * resolve the FPU state, turn it on, and try again.
		 *
		 * Ultras should never have a FPU queue.
		 */
		if (fs->fs_qsize) {
			printf("trap: Warning fs_qsize is %d\n",fs->fs_qsize);
			fpu_cleanup(p, fs);
			break;
		}
		if (fpproc != p) {		/* we do not have it */
			if (fpproc != NULL)	/* someone else had it */
				savefpstate(fpproc->p_md.md_fpstate);
			loadfpstate(fs);
			fpproc = p;		/* now we do have it */
		}
		tf->tf_tstate |= (PSTATE_PEF<<TSTATE_PSTATE_SHIFT);
		break;
	}

	case T_ALIGN:
	case T_LDDF_ALIGN:
	case T_STDF_ALIGN:
	{
		int64_t dsfsr, dsfar=0, isfsr;

		dsfsr = ldxa(SFSR, ASI_DMMU);
		if (dsfsr & SFSR_FV)
			dsfar = ldxa(SFAR, ASI_DMMU);
		isfsr = ldxa(SFSR, ASI_IMMU);
		/* 
		 * If we're busy doing copyin/copyout continue
		 */
		if (p->p_addr && p->p_addr->u_pcb.pcb_onfault) {
			tf->tf_pc = (vaddr_t)p->p_addr->u_pcb.pcb_onfault;
			tf->tf_npc = tf->tf_pc + 4;
			break;
		}
	}
		
		if ((p->p_md.md_flags & MDP_FIXALIGN) != 0 && 
		    fixalign(p, tf) == 0) {
			ADVANCE;
			break;
		}
		/* XXX sv.sival_ptr should be the fault address! */
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);	/* XXX code?? */
		break;

	case T_FP_IEEE_754:
	case T_FP_OTHER:
		/*
		 * Clean up after a floating point exception.
		 * fpu_cleanup can (and usually does) modify the
		 * state we save here, so we must `give up' the FPU
		 * chip context.  (The software and hardware states
		 * will not match once fpu_cleanup does its job, so
		 * we must not save again later.)
		 */
		if (p != fpproc)
			panic("fpe without being the FP user");
		savefpstate(p->p_md.md_fpstate);
		fpproc = NULL;
		/* tf->tf_psr &= ~PSR_EF; */	/* share_fpu will do this */
		if (p->p_md.md_fpstate->fs_qsize == 0) {
			copyin((caddr_t)pc, &p->p_md.md_fpstate->fs_queue[0].fq_instr, sizeof(int));
			p->p_md.md_fpstate->fs_qsize = 1;
			fpu_cleanup(p, p->p_md.md_fpstate);
			ADVANCE;
		} else
			fpu_cleanup(p, p->p_md.md_fpstate);
		/* fpu_cleanup posts signals if needed */
#if 0		/* ??? really never??? */
		ADVANCE;
#endif
		break;

	case T_TAGOF:
		trapsignal(p, SIGEMT, 0, EMT_TAGOVF, sv);	/* XXX code?? */
		break;

	case T_BREAKPOINT:
		trapsignal(p, SIGTRAP, 0, TRAP_BRKPT, sv);
		break;

	case T_DIV0:
		ADVANCE;
		trapsignal(p, SIGFPE, 0, FPE_INTDIV, sv);
		break;

	case T_CLEANWIN:
		uprintf("T_CLEANWIN\n");	/* XXX Should not get this */
		ADVANCE;
		break;

	case T_FLUSHWIN:
		/* Software window flush for v8 software */
		write_all_windows();
		ADVANCE;
		break;

	case T_RANGECHECK:
		ADVANCE;
		trapsignal(p, SIGILL, 0, ILL_ILLOPN, sv);	/* XXX code?? */
		break;

	case T_FIXALIGN:
#ifdef DEBUG_ALIGN
		uprintf("T_FIXALIGN\n");
#endif
		/* User wants us to fix alignment faults */
		p->p_md.md_flags |= MDP_FIXALIGN;
		ADVANCE;
		break;

	case T_INTOF:
		uprintf("T_INTOF\n");		/* XXX */
		ADVANCE;
		trapsignal(p, SIGFPE, FPE_INTOVF_TRAP, FPE_INTOVF, sv);
		break;
	}
	userret(p, pc, sticks);
	share_fpu(p, tf);
#undef ADVANCE
}

/*
 * Save windows from PCB into user stack, and return 0.  This is used on
 * window overflow pseudo-traps (from locore.s, just before returning to
 * user mode) and when ptrace or sendsig needs a consistent state.
 * As a side effect, rwindow_save() always sets pcb_nsaved to 0.
 *
 * If the windows cannot be saved, pcb_nsaved is restored and we return -1.
 * 
 * XXXXXX This cannot work properly.  I need to re-examine this register
 * window thing entirely.  
 */
int
rwindow_save(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct rwindow64 *rw = &pcb->pcb_rw[0];
	u_int64_t rwdest;
	int i, j;

	i = pcb->pcb_nsaved;
	if (i == 0)
		return (0);
	 while (i > 0) {
		rwdest = rw[i--].rw_in[6];
		if (rwdest & 1) {
			rwdest += BIAS;
			if (copyout((caddr_t)&rw[i], (caddr_t)(u_long)rwdest,
			    sizeof(*rw))) {
				return (-1);
			}
		} else {
			struct rwindow32 rwstack;

			/* 32-bit window */
			for (j = 0; j < 8; j++) { 
				rwstack.rw_local[j] = (int)rw[i].rw_local[j];
				rwstack.rw_in[j] = (int)rw[i].rw_in[j];
			}
			/* Must truncate rwdest */
			if (copyout(&rwstack, (caddr_t)(u_long)(u_int)rwdest,
			    sizeof(rwstack))) {
				return (-1);
			}
		}
	}
	pcb->pcb_nsaved = 0;

	return (0);
}

/*
 * Kill user windows (before exec) by writing back to stack or pcb
 * and then erasing any pcb tracks.  Otherwise we might try to write
 * the registers into the new process after the exec.
 */
void
kill_user_windows(p)
	struct proc *p;
{

	write_user_windows();
	p->p_addr->u_pcb.pcb_nsaved = 0;
}

/*
 * This routine handles MMU generated faults.  About half
 * of them could be recoverable through uvm_fault.
 */
void
data_access_fault(tf, type, pc, addr, sfva, sfsr)
	struct trapframe64 *tf;
	unsigned type;
	vaddr_t pc;
	vaddr_t addr;
	vaddr_t sfva;
	u_long sfsr;
{
	u_int64_t tstate;
	struct proc *p;
	struct vmspace *vm;
	vaddr_t va;
	int rv;
	vm_prot_t access_type;
	vaddr_t onfault;
	u_quad_t sticks;
	union sigval sv;

	sv.sival_ptr = (void *)addr;

	uvmexp.traps++;
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

	tstate = tf->tf_tstate;

	/* Find the faulting va to give to uvm_fault */
	va = trunc_page(addr);

	/* 
	 * Now munch on protections.
	 *
	 * If it was a FAST_DATA_ACCESS_MMU_MISS we have no idea what the
	 * access was since the SFSR is not set.  But we should never get
	 * here from there.
	 */
	if (type == T_FDMMU_MISS || (sfsr & SFSR_FV) == 0) {
		/* Punt */
		access_type = VM_PROT_READ;
	} else {
		access_type = (sfsr & SFSR_W) ? VM_PROT_READ|VM_PROT_WRITE
			: VM_PROT_READ;
	}
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		extern char Lfsbail[];
		/*
		 * If this was an access that we shouldn't try to page in,
		 * resume at the fault handler without any action.
		 */
		if (p->p_addr && p->p_addr->u_pcb.pcb_onfault == Lfsbail)
			goto kfault;

		/*
		 * During autoconfiguration, faults are never OK unless
		 * pcb_onfault is set.  Once running normally we must allow
		 * exec() to cause copy-on-write faults to kernel addresses.
		 */
		if (cold)
			goto kfault;
		if (!(addr & TLB_TAG_ACCESS_CTX)) {
			/* CTXT == NUCLEUS */
			rv = uvm_fault(kernel_map, va, 0, access_type);
			if (rv == 0)
				return;
			goto kfault;
		}
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
	onfault = (vaddr_t)p->p_addr->u_pcb.pcb_onfault;
	p->p_addr->u_pcb.pcb_onfault = NULL;
	rv = uvm_fault(&vm->vm_map, (vaddr_t)va, 0, access_type);
	p->p_addr->u_pcb.pcb_onfault = (void *)onfault;

	/*
	 * If this was a stack access we keep track of the maximum
	 * accessed stack size.  Also, if uvm_fault gets a protection
	 * failure it is due to accessing the stack region outside
	 * the current limit and we need to reflect that as an access
	 * error.
	 */
	if ((caddr_t)va >= vm->vm_maxsaddr) {
		if (rv == 0) {
			segsz_t nss = btoc(USRSTACK - va);
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == EACCES)
			rv = EFAULT;
	}
	if (rv != 0) {
		/*
		 * Pagein failed.  If doing copyin/out, return to onfault
		 * address.  Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
		if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
kfault:
			onfault = p->p_addr ?
			    (long)p->p_addr->u_pcb.pcb_onfault : 0;
			if (!onfault) {
				extern int trap_trace_dis;
				trap_trace_dis = 1; /* Disable traptrace for printf */
				(void) splhigh();
				printf("data fault: pc=%lx addr=%lx\n",
				    pc, addr);
				panic("kernel fault");
				/* NOTREACHED */
			}
			tf->tf_pc = onfault;
			tf->tf_npc = onfault + 4;
			return;
		}
		if (rv == ENOMEM) {
			printf("UVM: pid %d (%s), uid %u killed: out of swap\n",
			       p->p_pid, p->p_comm,
			       p->p_cred && p->p_ucred ?
			       p->p_ucred->cr_uid : -1);
			trapsignal(p, SIGKILL, access_type, SEGV_MAPERR, sv);
		} else {
			trapsignal(p, SIGSEGV, access_type, SEGV_MAPERR, sv);
		}
	}
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
}

/*
 * This routine handles deferred errors caused by the memory
 * or I/O bus subsystems.  Most of these are fatal, and even
 * if they are not, recovery is painful.  Also, the TPC and
 * TNPC values are probably not valid if we're not doing a
 * special PEEK/POKE code sequence.
 */
void
data_access_error(tf, type, afva, afsr, sfva, sfsr)
	struct trapframe64 *tf;
	unsigned type;
	vaddr_t sfva;
	u_long sfsr;
	vaddr_t afva;
	u_long afsr;
{
	u_long pc;
	u_int64_t tstate;
	struct proc *p;
	vaddr_t onfault;
	u_quad_t sticks;
	union sigval sv;

	sv.sival_ptr = (void *)pc;

	uvmexp.traps++;
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

	pc = tf->tf_pc;
	tstate = tf->tf_tstate;

	onfault = p->p_addr ? (long)p->p_addr->u_pcb.pcb_onfault : 0;
	printf("data error type %x sfsr=%lx sfva=%lx afsr=%lx afva=%lx tf=%p\n",
		type, sfsr, sfva, afsr, afva, tf);

	if (afsr == 0) {
		printf("data_access_error: no fault\n");
		goto out;	/* No fault. Why were we called? */
	}

	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {

		if (!onfault) {
			extern int trap_trace_dis;

			trap_trace_dis = 1; /* Disable traptrace for printf */
			(void) splhigh();
			printf("data fault: pc=%lx addr=%lx sfsr=%b\n",
				(u_long)pc, (long)sfva, sfsr, SFSR_BITS);
			panic("kernel fault");
			/* NOTREACHED */
		}

		/*
		 * If this was a priviliged error but not a probe, we
		 * cannot recover, so panic.
		 */
		if (afsr & ASFR_PRIV) {
			panic("Privileged Async Fault: AFAR %p AFSR %lx\n%b",
				(void *)afva, afsr, afsr, AFSR_BITS);
			/* NOTREACHED */
		}
		tf->tf_pc = onfault;
		tf->tf_npc = onfault + 4;
		return;
	}

	trapsignal(p, SIGSEGV, VM_PROT_READ|VM_PROT_WRITE, SEGV_MAPERR, sv);
out:
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
}

/*
 * This routine handles MMU generated faults.  About half
 * of them could be recoverable through uvm_fault.
 */
void
text_access_fault(tf, type, pc, sfsr)
	unsigned type;
	vaddr_t pc;
	struct trapframe64 *tf;
	u_long sfsr;
{
	u_int64_t tstate;
	struct proc *p;
	struct vmspace *vm;
	vaddr_t va;
	int rv;
	vm_prot_t access_type;
	u_quad_t sticks;
	union sigval sv;

	sv.sival_ptr = (void *)pc;

	uvmexp.traps++;
	if ((p = curproc) == NULL)	/* safety check */
		panic("text_access_fault: no curproc");
	sticks = p->p_sticks;

	tstate = tf->tf_tstate;

	va = trunc_page(pc);

	/* Now munch on protections... */

	access_type = VM_PROT_EXECUTE;
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		extern int trap_trace_dis;
		trap_trace_dis = 1; /* Disable traptrace for printf */
		(void) splhigh();
		printf("text_access_fault: pc=%lx va=%lx\n", pc, va);
		panic("kernel fault");
		/* NOTREACHED */
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
	rv = uvm_fault(&vm->vm_map, va, 0, access_type);

	/*
	 * If this was a stack access we keep track of the maximum
	 * accessed stack size.  Also, if uvm_fault gets a protection
	 * failure it is due to accessing the stack region outside
	 * the current limit and we need to reflect that as an access
	 * error.
	 */
	if ((caddr_t)va >= vm->vm_maxsaddr) {
		if (rv == 0) {
			segsz_t nss = btoc(USRSTACK - va);
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == EACCES)
			rv = EFAULT;
	}
	if (rv != 0) {
		/*
		 * Pagein failed. Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
		if (tstate & TSTATE_PRIV) {
			extern int trap_trace_dis;
			trap_trace_dis = 1; /* Disable traptrace for printf */
			(void) splhigh();
			printf("text fault: pc=%llx\n", (unsigned long long)pc);
			panic("kernel fault");
			/* NOTREACHED */
		}
		trapsignal(p, SIGSEGV, access_type, SEGV_MAPERR, sv);
	}
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
}


/*
 * This routine handles deferred errors caused by the memory
 * or I/O bus subsystems.  Most of these are fatal, and even
 * if they are not, recovery is painful.  Also, the TPC and
 * TNPC values are probably not valid if we're not doing a
 * special PEEK/POKE code sequence.
 */
void
text_access_error(tf, type, pc, sfsr, afva, afsr)
	struct trapframe64 *tf;
	unsigned type;
	vaddr_t pc;
	u_long sfsr;
	vaddr_t afva;
	u_long afsr;
{
	int64_t tstate;
	struct proc *p;
	struct vmspace *vm;
	vaddr_t va;
	int rv;
	vm_prot_t access_type;
	u_quad_t sticks;
	union sigval sv;

	sv.sival_ptr = (void *)pc;
	uvmexp.traps++;
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

	tstate = tf->tf_tstate;

	if ((afsr) != 0) {
		extern int trap_trace_dis;

		trap_trace_dis++; /* Disable traptrace for printf */
		printf("text_access_error: memory error...\n");
		printf("text memory error type %d sfsr=%lx sfva=%lx afsr=%lx afva=%lx tf=%p\n",
		       type, sfsr, pc, afsr, afva, tf);
		trap_trace_dis--; /* Reenable traptrace for printf */

		if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT))
			panic("text_access_error: kernel memory error");

		/* User fault -- Berr */
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);
	}

	if ((sfsr & SFSR_FV) == 0 || (sfsr & SFSR_FT) == 0)
		goto out;	/* No fault. Why were we called? */

	va = trunc_page(pc);

	/* Now munch on protections... */
	access_type = VM_PROT_EXECUTE;
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		extern int trap_trace_dis;
		trap_trace_dis = 1; /* Disable traptrace for printf */
		(void) splhigh();
		printf("text error: pc=%lx sfsr=%b\n", pc, sfsr, SFSR_BITS);
		panic("kernel fault");
		/* NOTREACHED */
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
	rv = uvm_fault(&vm->vm_map, va, 0, access_type);

	/*
	 * If this was a stack access we keep track of the maximum
	 * accessed stack size.  Also, if uvm_fault gets a protection
	 * failure it is due to accessing the stack region outside
	 * the current limit and we need to reflect that as an access
	 * error.
	 */
	if ((caddr_t)va >= vm->vm_maxsaddr) {
		if (rv == 0) {
			segsz_t nss = btoc(USRSTACK - va);
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == EACCES)
			rv = EFAULT;
	}
	if (rv != 0) {
		/*
		 * Pagein failed.  If doing copyin/out, return to onfault
		 * address.  Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
		if (tstate & TSTATE_PRIV) {
			extern int trap_trace_dis;
			trap_trace_dis = 1; /* Disable traptrace for printf */
			(void) splhigh();
			printf("text error: pc=%lx sfsr=%b\n", pc,
			    sfsr, SFSR_BITS);
			panic("kernel fault");
			/* NOTREACHED */
		}
		trapsignal(p, SIGSEGV, access_type, SEGV_MAPERR, sv);
	}
out:
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
}

/*
 * System calls.  `pc' is just a copy of tf->tf_pc.
 *
 * Note that the things labelled `out' registers in the trapframe were the
 * `in' registers within the syscall trap code (because of the automatic
 * `save' effect of each trap).  They are, however, the %o registers of the
 * thing that made the system call, and are named that way here.
 *
 * 32-bit system calls on a 64-bit system are a problem.  Each system call
 * argument is stored in the smaller of the argument's true size or a
 * `register_t'.  Now on a 64-bit machine all normal types can be stored in a
 * `register_t'.  (The only exceptions would be 128-bit `quad's or 128-bit
 * extended precision floating point values, which we don't support.)  For
 * 32-bit syscalls, 64-bit integers like `off_t's, double precision floating
 * point values, and several other types cannot fit in a 32-bit `register_t'.
 * These will require reading in two `register_t' values for one argument.
 *
 * In order to calculate the true size of the arguments and therefore whether
 * any argument needs to be split into two slots, the system call args
 * structure needs to be built with the appropriately sized register_t.
 * Otherwise the emul needs to do some magic to split oversized arguments.
 *
 * We can handle most this stuff for normal syscalls by using either a 32-bit
 * or 64-bit array of `register_t' arguments.  Unfortunately ktrace always
 * expects arguments to be `register_t's, so it loses badly.  What's worse,
 * ktrace may need to do size translations to massage the argument array
 * appropriately according to the emulation that is doing the ktrace.
 *  
 */
void
syscall(tf, code, pc)
	register_t code;
	struct trapframe64 *tf;
	register_t pc;
{
	int i, nsys, nap;
	int64_t *ap;
	const struct sysent *callp;
	struct proc *p;
	int error = 0, new;
	union args {
		register32_t i[8];
		register64_t l[8];
	} args;
	register_t rval[2];
	u_quad_t sticks;
#ifdef DIAGNOSTIC
	extern struct pcb *cpcb;
#endif

	uvmexp.syscalls++;
	p = curproc;
#ifdef DIAGNOSTIC
	if (tf->tf_tstate & TSTATE_PRIV)
		panic("syscall from kernel");
	if (cpcb != &p->p_addr->u_pcb)
		panic("syscall: cpcb/ppcb mismatch");
	if (tf != (struct trapframe64 *)((caddr_t)cpcb + USPACE) - 1)
		panic("syscall: trapframe");
#endif
	sticks = p->p_sticks;
	p->p_md.md_tf = tf;
	new = code & (SYSCALL_G7RFLAG | SYSCALL_G2RFLAG);
	code &= ~(SYSCALL_G7RFLAG | SYSCALL_G2RFLAG);

	callp = p->p_emul->e_sysent;
	nsys = p->p_emul->e_nsysent;

	/*
	 * The first six system call arguments are in the six %o registers.
	 * Any arguments beyond that are in the `argument extension' area
	 * of the user's stack frame (see <machine/frame.h>).
	 *
	 * Check for ``special'' codes that alter this, namely syscall and
	 * __syscall.  The latter takes a quad syscall number, so that other
	 * arguments are at their natural alignments.  Adjust the number
	 * of ``easy'' arguments as appropriate; we will copy the hard
	 * ones later as needed.
	 */
	ap = &tf->tf_out[0];
	nap = 6;

	switch (code) {
	case SYS_syscall:
		code = *ap++;
		nap--;
		break;
	case SYS___syscall:
		if (code < nsys &&
		    callp[code].sy_call != callp[p->p_emul->e_nosys].sy_call)
			break; /* valid system call */
		if (tf->tf_out[6] & 1L) {
			/* longs *are* quadwords */
			code = ap[0];
			ap += 1;
			nap -= 1;			
		} else {
			code = ap[_QUAD_LOWWORD];
			ap += 2;
			nap -= 2;
		}
		break;
	}

	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;
	else if (tf->tf_out[6] & 1L) {
		register64_t *argp;

		callp += code;
		i = callp->sy_narg; /* Why divide? */
		if (i > nap) {	/* usually false */
			if (i > 8)
				panic("syscall nargs");
			/* Read the whole block in */
			error = copyin((caddr_t)(u_long)tf->tf_out[6] + BIAS +
				       offsetof(struct frame64, fr_argx),
				       (caddr_t)&args.l[nap], (i - nap) * sizeof(register64_t));
			i = nap;
		}
		/* It should be faster to do <=6 longword copies than call bcopy */
		for (argp = &args.l[0]; i--;) 
			*argp++ = *ap++;
		
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p, code,
				   callp->sy_argsize, (register_t*)args.l);
#endif
		if (error)
			goto bad;
	} else {
#if !defined(COMPAT_NETBSD32)
		error = EFAULT;
		goto bad;
#else
		register32_t *argp;
		int j = 0;

		/* 32-bit stack */
		callp += code;

		i = (long)callp->sy_argsize / sizeof(register32_t);
		if (i > nap) {	/* usually false */
			register32_t temp[6];
			if (i > 8)
				panic("syscall nargs");
			/* Read the whole block in */
			error = copyin((caddr_t)(u_long)(tf->tf_out[6] +
						 offsetof(struct frame32, fr_argx)),
				       (caddr_t)&temp, (i - nap) * sizeof(register32_t));
			/* Copy each to the argument array */
			for (j = 0; nap + j < i; j++)
				args.i[nap+j] = temp[j];
			i = nap;
		}
		/* Need to convert from int64 to int32 or we lose */
		for (argp = &args.i[0]; i--;) 
				*argp++ = *ap++;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL)) {
			register_t temp[8];
			
			/* Need to xlate 32-bit->64-bit */
			i = (long)callp->sy_argsize / 
				sizeof(register32_t);
			for (j=0; j<i; j++) 
				temp[j] = args.i[j];
			ktrsyscall(p, code,
				   i * sizeof(register_t), (register_t *)temp);
		}
#endif
		if (error) {
			goto bad;
		}
#endif	/* !COMPAT_NETBSD32 */
	}
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, (register_t *)&args);
#endif
	rval[0] = 0;
	rval[1] = tf->tf_out[1];
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		error = systrace_redirect(code, p, &args, rval);
	else
#endif
		error = (*callp->sy_call)(p, &args, rval);

	switch (error) {
		vaddr_t dest;
	case 0:
		/* Note: fork() does not return here in the child */
		tf->tf_out[0] = rval[0];
		tf->tf_out[1] = rval[1];
		if (new) {
			/* jmp %g2 (or %g7, deprecated) on success */
			dest = tf->tf_global[new & SYSCALL_G2RFLAG ? 2 : 7];
			if (dest & 3) {
				error = EINVAL;
				goto bad;
			}
		} else {
			/* old system call convention: clear C on success */
			tf->tf_tstate &= ~(((int64_t)(ICC_C|XCC_C))<<TSTATE_CCR_SHIFT);	/* success */
			dest = tf->tf_npc;
		}
		tf->tf_pc = dest;
		tf->tf_npc = dest + 4;
		break;

	case ERESTART:
	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->tf_out[0] = error;
		tf->tf_tstate |= (((int64_t)(ICC_C|XCC_C))<<TSTATE_CCR_SHIFT);	/* fail */
		dest = tf->tf_npc;
		tf->tf_pc = dest;
		tf->tf_npc = dest + 4;
		break;
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, pc, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, error, rval[0]);
#endif
	share_fpu(p, tf);
}

/*
 * Process the tail end of a fork() for the child.
 */
void
child_return(arg)
	void *arg;
{
	struct proc *p = (struct proc *)arg;

	/*
	 * Return values in the frame set by cpu_fork().
	 */
	userret(p, p->p_md.md_tf->tf_pc, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p,
			  (p->p_flag & P_PPWAIT) ? SYS_vfork : SYS_fork, 0, 0);
#endif
}

