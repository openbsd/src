/*	$NetBSD: trap.c,v 1.9.2.3 1995/10/23 07:50:11 leo Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: trap.c 1.32 91/04/06$
 *
 *	@(#)trap.c	7.15 (Berkeley) 8/2/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/syslog.h>
#include <sys/syscall.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <sys/user.h>
#include <vm/pmap.h>

#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/mtpr.h>
#include <machine/pte.h>

#ifdef COMPAT_SUNOS
#include <compat/sunos/sunos_syscall.h>
extern struct emul emul_sunos;
#endif

/*
 * XXX Hack until I can figure out what to do about this code's removal
 * from m68k/include/frame.h
 */

/* 68040 fault frame */
#define SSW_CP		0x8000		/* Continuation - Floating-Point Post*/
#define SSW_CU		0x4000		/* Continuation - Unimpl. FP */
#define SSW_CT		0x2000		/* Continuation - Trace */
#define SSW_CM		0x1000		/* Continuation - MOVEM */
#define SSW_MA		0x0800		/* Misaligned access */
#define SSW_ATC		0x0400		/* ATC fault */
#define SSW_LK		0x0200		/* Locked transfer */
#define SSW_RW040	0x0100		/* Read/Write */
#define SSW_SZMASK	0x0060		/* Transfer size */
#define SSW_TTMASK	0x0018		/* Transfer type */
#define SSW_TMMASK	0x0007		/* Transfer modifier */

#define WBS_TMMASK	0x0007
#define WBS_TTMASK	0x0018
#define WBS_SZMASK	0x0060
#define WBS_VALID	0x0080

#define WBS_SIZE_BYTE	0x0020
#define WBS_SIZE_WORD	0x0040
#define WBS_SIZE_LONG	0x0000
#define WBS_SIZE_LINE	0x0060

#define WBS_TT_NORMAL	0x0000
#define WBS_TT_MOVE16	0x0008
#define WBS_TT_ALTFC	0x0010
#define WBS_TT_ACK	0x0018

#define WBS_TM_PUSH	0x0000
#define WBS_TM_UDATA	0x0001
#define WBS_TM_UCODE	0x0002
#define WBS_TM_MMUTD	0x0003
#define WBS_TM_MMUTC	0x0004
#define WBS_TM_SDATA	0x0005
#define WBS_TM_SCODE	0x0006
#define WBS_TM_RESV	0x0007

#define	MMUSR_PA_MASK	0xfffff000
#define MMUSR_B		0x00000800
#define MMUSR_G		0x00000400
#define MMUSR_U1	0x00000200
#define MMUSR_U0	0x00000100
#define MMUSR_S		0x00000080
#define MMUSR_CM	0x00000060
#define MMUSR_M		0x00000010
#define MMUSR_0		0x00000008
#define MMUSR_W		0x00000004
#define MMUSR_T		0x00000002
#define MMUSR_R		0x00000001
/*
 * XXX End hack
 */
char	*trap_type[] = {
	"Bus error",
	"Address error",
	"Illegal instruction",
	"Zero divide",
	"CHK instruction",
	"TRAPV instruction",
	"Privilege violation",
	"Trace trap",
	"MMU fault",
	"SSIR trap",
	"Format error",
	"68881 exception",
	"Coprocessor violation",
	"Async system trap"
};
int	trap_types = sizeof trap_type / sizeof trap_type[0];

/*
 * Size of various exception stack frames (minus the standard 8 bytes)
 */
short	exframesize[] = {
	FMT0SIZE,	/* type 0 - normal (68020/030/040) */
	FMT1SIZE,	/* type 1 - throwaway (68020/030/040) */
	FMT2SIZE,	/* type 2 - normal 6-word (68020/030/040) */
	FMT3SIZE,	/* type 3 - FP post-instruction (68040) */
	-1, -1, -1,	/* type 4-6 - undefined */
	FMT7SIZE,	/* type 7 - access error (68040) */
	58,		/* type 8 - bus fault (68010) */
	FMT9SIZE,	/* type 9 - coprocessor mid-instruction (68020/030) */
	FMTASIZE,	/* type A - short bus fault (68020/030) */
	FMTBSIZE,	/* type B - long bus fault (68020/030) */
	-1, -1, -1, -1	/* type C-F - undefined */
};

#ifdef DEBUG
int mmudebug = 0;
#endif

extern struct pcb *curpcb;
int fubail();
int subail();

static void
userret(p, pc, oticks)
	struct proc *p;
	int pc;
	u_quad_t oticks;
{
	int sig, s;

	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	p->p_priority = p->p_usrpri;

	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but before
		 * we switch'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}
	/*
	 * If profiling, charge recent system time.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;
		
		addupc_task(p, pc, (int)(p->p_sticks - oticks) * psratio);
	}
	curpriority = p->p_priority;
}

void
panictrap(type, code, v, fp)
	int type;
	u_int code, v;
	struct frame *fp;
{
	static int panicing = 0;
	if (panicing++ == 0) {
		printf("trap type %d, code = %x, v = %x\n", type, code, v);
		regdump(fp, 128);
	}
	type &= ~T_USER;
	DCIS(); /* XXX? push cache */
	if ((u_int)type < trap_types)
		panic(trap_type[type]);
	panic("trap");
	/*NOTREACHED*/
}

/*
 * return to fault handler
 */
void
trapcpfault(p, fp)
	struct proc *p;
	struct frame *fp;
{
	/*
	 * We have arranged to catch this fault in one of the
	 * copy to/from user space routines, set PC to return to
	 * indicated location and set flag informing buserror code
	 * that it may need to clean up stack frame.
	 */
	fp->f_stackadj = exframesize[fp->f_format];
	fp->f_format = fp->f_vector = 0;
	fp->f_pc = (int) p->p_addr->u_pcb.pcb_onfault;
}

void 
trapmmufault(type, code, v, fp, p, sticks)
	int type;
	u_int code, v;
	struct frame *fp;
	struct proc *p;
	u_quad_t sticks;
{
	extern vm_map_t kernel_map;
	struct vmspace *vm;
	vm_prot_t ftype;
	vm_offset_t va;
	vm_map_t map;
	u_int nss;
	int rv;
	
	vm = p->p_vmspace;

	/*
	 * It is only a kernel address space fault iff:
	 * 	1. (type & T_USER) == 0  and
	 * 	2. pcb_onfault not set or
	 *	3. pcb_onfault set but supervisor space data fault
	 * The last can occur during an exec() copyin where the
	 * argument space is lazy-allocated.
	 */
#ifdef DEBUG
	/*
	 * Print out some data about the fault
	 */
	if (mmudebug && cpu040) {
		printf ("68040 access error: pc %x, code %x,"
		    " ea %x, fa %x\n", fp->f_pc, code, fp->f_fmt7.f_ea, v);
		if (curpcb)
			printf (" curpcb %x ->pcb_ustp %x / %x\n",
			    curpcb, curpcb->pcb_ustp, 
			    curpcb->pcb_ustp << PG_SHIFT);
	}
#endif
	if (type == T_MMUFLT && 
	    (p->p_addr->u_pcb.pcb_onfault == 0 ||
	    (cpu040 && (code & SSW_TMMASK) == FC_SUPERD) ||
	    (!cpu040 && (code & (SSW_DF|FC_SUPERD)) == (SSW_DF|FC_SUPERD))))
		map = kernel_map;
	else
		map = &vm->vm_map;
	if (
	    (cpu040 && (code & SSW_RW040) == 0) ||
	    (!cpu040 && (code & (SSW_DF|SSW_RW)) ==
	    SSW_DF))	/* what about RMW? */
		ftype = VM_PROT_READ | VM_PROT_WRITE;
	else
		ftype = VM_PROT_READ;
	va = trunc_page((vm_offset_t)v);
#ifdef DEBUG
	if (map == kernel_map && va == 0) {
		printf("trap: bad kernel access at %x\n", v);
		panictrap(type, code, v, fp);
	}
#endif
#ifndef no_386bsd_code
	/*
	 * XXX: rude hack to make stack limits "work"
	 */
	nss = 0;
	if ((caddr_t)va >= vm->vm_maxsaddr && map != kernel_map) {
		nss = clrnd(btoc(USRSTACK - (unsigned)va));
		if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
			rv = KERN_FAILURE;
			goto nogo;
		}
	}
#endif

#ifdef DEBUG
	if (mmudebug)
		printf("vm_fault(%x,%x,%d,0)\n", map, va, ftype);
#endif

	rv = vm_fault(map, va, ftype, FALSE);

#ifdef DEBUG
	if (mmudebug)
		printf("vmfault %s %x returned %d\n",
		    map == kernel_map ? "kernel" : "user", va, rv);
#endif
	if (cpu040) {
		if(rv != KERN_SUCCESS) {
			goto nogo;
		}

		/*	
		 * The 68040 doesn't re-run instructions that cause
		 * write page faults (unless due to a move16 isntruction).
		 * So once the page is repaired, we have to write the
		 * value of WB2D out to memory ourselves.  Because
		 * the writeback could possibly span two pages in
		 * memory, so we need to check both "ends" of the
		 * address to see if they are in the same page or not.
		 * If not, then we need to make sure the second page
		 * is valid, and bring it into memory if it's not.
		 * 	
		 * This whole process needs to be repeated for WB3 as well.
		 * <sigh>
		 */	

		/* Check WB1 */
		if (fp->f_fmt7.f_wb1s & WBS_VALID) {
			printf ("trap: wb1 was valid, not handled yet\n");
			panictrap(type, code, v, fp);
		}

		/*
		 * Check WB2
		 * skip if it's for a move16 instruction 
		 */
		if(fp->f_fmt7.f_wb2s & WBS_VALID &&
		   ((fp->f_fmt7.f_wb2s & WBS_TTMASK)==WBS_TT_MOVE16) == 0) {
			if (_write_back(2, fp->f_fmt7.f_wb2s, 
			    fp->f_fmt7.f_wb2d, fp->f_fmt7.f_wb2a, map)
			    != KERN_SUCCESS)
				goto nogo;
			if ((fp->f_fmt7.f_wb2s & WBS_TMMASK) 
			    != (code & SSW_TMMASK))
				panictrap(type, code, v, fp);
		}

		/* Check WB3 */
		if(fp->f_fmt7.f_wb3s & WBS_VALID) {
			vm_map_t wb3_map;

			if ((fp->f_fmt7.f_wb3s & WBS_TMMASK) == WBS_TM_SDATA)
				wb3_map = kernel_map;
			else
				wb3_map = &vm->vm_map;
			if (_write_back(3, fp->f_fmt7.f_wb3s, 
			    fp->f_fmt7.f_wb3d, fp->f_fmt7.f_wb3a, wb3_map)
			    != KERN_SUCCESS)
				goto nogo;
		}
	}

#ifdef no_386bsd_code
	/*
	 * If this was a stack access we keep track of the maximum
	 * accessed stack size.  Also, if vm_fault gets a protection
	 * failure it is due to accessing the stack region outside
	 * the current limit and we need to reflect that as an access
	 * error.
	 */
	if ((caddr_t)va >= vm->vm_maxsaddr && map != kernel_map) {
		if (rv == KERN_SUCCESS) {
			nss = clrnd(btoc(USRSTACK-(unsigned)va));
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == KERN_PROTECTION_FAILURE)
			rv = KERN_INVALID_ADDRESS;
	}

	if (rv == KERN_SUCCESS) {
		if (type == T_MMUFLT)
			return;
		userret(p, fp->f_pc, sticks); 
		reutrn;
	}
#else /* use hacky 386bsd_code */
	if (rv == KERN_SUCCESS) {
		/*
		 * XXX: continuation of rude stack hack
		 */
		if (nss > vm->vm_ssize)
			vm->vm_ssize = nss;
		if (type == T_MMUFLT)
			return;
		userret(p, fp->f_pc, sticks); 
		return;
	}
nogo:
#endif
	if (type == T_MMUFLT) {
		if (p->p_addr->u_pcb.pcb_onfault) {
			trapcpfault(p, fp);
			return;
		}
		printf("vm_fault(%x, %x, %x, 0) -> %x\n",
		       map, va, ftype, rv);
		printf("  type %x, code [mmu,,ssw]: %x\n",
		       type, code);
		panictrap(type, code, v, fp);
	}
	trapsignal(p, (rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV, v);
	if ((type & T_USER) == 0)
		return;
	userret(p, fp->f_pc, sticks); 
}
/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
trap(type, code, v, frame)
	int type;
	u_int code, v;
	struct frame frame;
{
	struct proc *p;
	u_int ncode, ucode;
	u_quad_t sticks;
	int i, s;
#ifdef COMPAT_SUNOS
	extern struct emul emul_sunos;
#endif

	p = curproc;
	ucode = 0;
	cnt.v_trap++;

	if (USERMODE(frame.f_sr)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = frame.f_regs;
	}

#ifdef DDB
	if (type == T_TRACE || type == T_BREAKPOINT) {
		if (kdb_trap(type, &frame))
			return;
	}
#endif

	switch (type) {
	default:
		panictrap(type, code, v, &frame);
	/*
	 * Kernel Bus error
	 */
	case T_BUSERR:
		if (!p->p_addr->u_pcb.pcb_onfault)
			panictrap(type, code, v, &frame);
		trapcpfault(p, &frame);
		return;
	/*
	 * User Bus/Addr error.
	 */
	case T_BUSERR|T_USER:
	case T_ADDRERR|T_USER:
		i = SIGBUS;
		break;
	/*
	 * User illegal/privleged inst fault
	 */
	case T_ILLINST|T_USER:
	case T_PRIVINST|T_USER:
		ucode = frame.f_format;	/* XXX was ILL_PRIVIN_FAULT */
		i = SIGILL;
		break;
	/*
	 * divde by zero, CHK/TRAPV inst 
	 */
	case T_ZERODIV|T_USER:
	case T_CHKINST|T_USER:
	case T_TRAPVINST|T_USER:
		ucode = frame.f_format;
		i = SIGFPE;
		break;
	/* 
	 * User coprocessor violation
	 */
	case T_COPERR|T_USER:
		ucode = 0;
		i = SIGFPE;	/* XXX What is a proper response here? */
		break;
	/* 
	 * 6888x exceptions 
	 */
	case T_FPERR|T_USER:
		/*
		 * We pass along the 68881 status register which locore
		 * stashed in code for us.  Note that there is a
		 * possibility that the bit pattern of this register
		 * will conflict with one of the FPE_* codes defined
		 * in signal.h.  Fortunately for us, the only such
		 * codes we use are all in the range 1-7 and the low
		 * 3 bits of the status register are defined as 0 so
		 * there is no clash.
		 */
		ucode = code;
		i = SIGFPE;
		break;
	/*
	 * FPU faults in supervisor mode.
	 */
	case T_FPEMULI:
	case T_FPEMULD: {
		extern int	*nofault;

		if (nofault)	/* If we're probing. */
			longjmp((label_t *) nofault);
		panictrap(type, code, v, &frame);
	}
	/*
	 * Unimplemented FPU instructions/datatypes.
	 */
	case T_FPEMULI|T_USER:
	case T_FPEMULD|T_USER:
#ifdef FPU_EMULATE
		i = fpu_emulate(&frame, &p->p_addr->u_pcb.pcb_fpregs);
		/* XXX -- deal with tracing? (frame.f_sr & PSL_T) */
		if (i == 0) {
			userret(p, frame.f_pc, sticks); 
			return;
		}
#else
		uprintf("pid %d killed: no floating point support.\n",
			p->p_pid);
		i = SIGILL;
#endif
		break;
	/* 
	 * Kernel coprocessor violation
	 */
	case T_COPERR:
		/*FALLTHROUGH*/
	/*
	 * Kernel format error
	 */
	case T_FMTERR:
		/*
		 * The user has most likely trashed the RTE or FP state info
		 * in the stack frame of a signal handler.
		 */
		type |= T_USER;
#ifdef DEBUG
		printf("pid %d: kernel %s exception\n", p->p_pid,
		    type==T_COPERR ? "coprocessor" : "format");
#endif
		p->p_sigacts->ps_sigact[SIGILL] = SIG_DFL;
		i = sigmask(SIGILL);
		p->p_sigignore &= ~i;
		p->p_sigcatch &= ~i;
		p->p_sigmask &= ~i;
		i = SIGILL;
		ucode = frame.f_format;	/* XXX was ILL_RESAD_FAULT */
		break;
	/*
	 * Trace traps.
	 *
	 * M68k NetBSD uses trap #2,
	 * SUN 3.x uses trap #15,
	 * KGDB uses trap #15 (for kernel breakpoints; handled elsewhere).
	 *
	 * Atari traps get mapped by locore.s into T_TRACE.
	 * SUN 3.x traps get passed through as T_TRAP15 and are not really
	 * supported yet.
	 */
	case T_TRACE:
	case T_TRAP15:
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;
	case T_TRACE|T_USER:
	case T_TRAP15|T_USER:
#ifdef COMPAT_SUNOS
		/*
		 * XXX This comment/code is not consistent XXX
		 * SunOS seems to use Trap #2 for some obscure 
		 * fpu operations.  So far, just ignore it, but
		 * DONT trap on it.. 
		 */
		if (p->p_emul == &emul_sunos) {
			userret(p, frame.f_pc, sticks); 
			return;
		}
#endif
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;
	/* 
	 * Kernel AST (should not happen)
	 */
	case T_ASTFLT:
		panictrap(type, code, v, &frame);
	/*
	 * User AST
	 */
	case T_ASTFLT|T_USER:
		astpending = 0;
		/*
		 * We check for software interrupts first.  This is because
		 * they are at a higher level than ASTs, and on a VAX would
		 * interrupt the AST.  We assume that if we are processing
		 * an AST that we must be at IPL0 so we don't bother to
		 * check.  Note that we ensure that we are at least at SIR
		 * IPL while processing the SIR.
		 */
		spl1();
		/*FALLTHROUGH*/
	/*
	 * Software interrupt
	 */
	case T_SSIR:
	case T_SSIR|T_USER:
		if(ssir)
			softint();
		/*
		 * If this was not an AST trap, we are all done.
		 */
		if (type != (T_ASTFLT|T_USER)) {
			cnt.v_trap--;
			return;
		}
		spl0();
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		userret(p, frame.f_pc, sticks); 
		return;
	/*
	 * Kernel/User page fault
	 */
	case T_MMUFLT:
		if (p->p_addr->u_pcb.pcb_onfault == (caddr_t)fubail ||
		    p->p_addr->u_pcb.pcb_onfault == (caddr_t)subail) {
			trapcpfault(p, &frame);
			return;
		}
		/*FALLTHROUGH*/
	case T_MMUFLT|T_USER:	/* page fault */
		trapmmufault(type, code, v, &frame, p, sticks);
		return;
	}

	trapsignal(p, i, ucode);
	if ((type & T_USER) == 0)
		return;
	userret(p, frame.f_pc, sticks); 
}

/*
 * Process a system call.
 */
syscall(code, frame)
	register_t code;
	struct frame frame;
{
	register caddr_t params;
	register struct sysent *callp;
	register struct proc *p;
	int error, opc, nsys;
	size_t argsize;
	register_t args[8], rval[2];
	u_quad_t sticks;
#ifdef COMPAT_SUNOS
	extern struct emul emul_sunos;
#endif

	cnt.v_syscall++;
	if (!USERMODE(frame.f_sr))
		panic("syscall");
	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_regs = frame.f_regs;
	opc = frame.f_pc;

	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;
#ifdef COMPAT_SUNOS
	if (p->p_emul == &emul_sunos) {

		/*
		 * SunOS passes the syscall-number on the stack, whereas
		 * BSD passes it in D0. So, we have to get the real "code"
		 * from the stack, and clean up the stack, as SunOS glue
		 * code assumes the kernel pops the syscall argument the
		 * glue pushed on the stack. Sigh...
		 */
		code = fuword((caddr_t)frame.f_regs[SP]);

		/*
		 * XXX
		 * Don't do this for sunos_sigreturn, as there's no stored pc
		 * on the stack to skip, the argument follows the syscall
		 * number without a gap.
		 */
		if (code != SUNOS_SYS_sigreturn) {
			frame.f_regs[SP] += sizeof (int);
			/*
			 * remember that we adjusted the SP, 
			 * might have to undo this if the system call
			 * returns ERESTART.
			 */
			p->p_md.md_flags |= MDP_STACKADJ;
		} else
			p->p_md.md_flags &= ~MDP_STACKADJ;
	}
#endif

	params = (caddr_t)frame.f_regs[SP] + sizeof(int);

	switch (code) {
	case SYS_syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
		/*
		 * XXX sigreturn requires special stack manipulation
		 * that is only done if entered via the sigreturn
		 * trap.  Cannot allow it here so make sure we fail.
		 */
		if (code == SYS_sigreturn)
			code = nsys;
		break;
	case SYS___syscall:
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
		if (callp != sysent)
			break;
		code = fuword(params + _QUAD_LOWWORD * sizeof(int));
		params += sizeof(quad_t);
		break;
	default:
		break;
	}
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;		/* illegal */
	else
		callp += code;
	argsize = callp->sy_argsize;
	if (argsize)
		error = copyin(params, (caddr_t)args, argsize);
	else
		error = 0;
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, argsize, args);
#endif
	if (error)
		goto bad;
	rval[0] = 0;
	rval[1] = frame.f_regs[D1];
	error = (*callp->sy_call)(p, args, rval);
	switch (error) {
	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.f_regs[D0] = rval[0];
		frame.f_regs[D1] = rval[1];
		frame.f_sr &= ~PSL_C;	/* carry bit */
		break;
	case ERESTART:
		/*
		 * We always enter through a `trap' instruction, which is 2
		 * bytes, so adjust the pc by that amount.
		 */
		frame.f_pc = opc - 2;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
	bad:
		frame.f_regs[D0] = error;
		frame.f_sr |= PSL_C;	/* carry bit */
		break;	
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
#ifdef COMPAT_SUNOS
	/* need new p-value for this */
	if (error == ERESTART && (p->p_md.md_flags & MDP_STACKADJ))
		frame.f_regs[SP] -= sizeof (int);
#endif
	userret(p, frame.f_pc, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}
/*
 * Process the tail end of a fork() for the child
 */
void
child_return(p, frame)
	struct proc *p;
	struct frame frame;
{
	frame.f_regs[D0] = 0;
	frame.f_sr &= ~PSL_C;	/* carry bit */

	userret(p, frame.f_pc, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}

/*
 * Process a pending write back
 */
_write_back (wb, wb_sts, wb_data, wb_addr, wb_map)
	u_int wb;	/* writeback type: 1, 2, or 3 */
	u_int wb_sts;	/* writeback status information */
	u_int wb_data;	/* data to writeback */
	u_int wb_addr;	/* address to writeback to */
	vm_map_t wb_map;
{
	u_int wb_extra_page = 0;
	u_int wb_rc, mmusr;
	void _wb_fault ();	/* fault handler for write back */

#ifdef DEBUG
	if (mmudebug)
		printf("wb%d valid: %x %x %x\n",wb,wb_sts,wb_addr,wb_data);
#endif

	/* See if we're going to span two pages (for word or long transfers) */

	if((wb_sts & WBS_SZMASK) == WBS_SIZE_WORD)
		if(trunc_page((vm_offset_t)wb_addr) !=
		    trunc_page((vm_offset_t)wb_addr+1))
			wb_extra_page = 1;

	if((wb_sts & WBS_SZMASK) == WBS_SIZE_LONG)
		if(trunc_page((vm_offset_t)wb_addr) !=
		    trunc_page((vm_offset_t)wb_addr+3))
			wb_extra_page = 3;

	/*
	 * if it's writeback 3, we need to check the first page
	 */
	if (wb == 3) {
		mmusr = probeva(wb_addr, wb_sts & WBS_TMMASK);
#ifdef DEBUG
	if (mmudebug)
		printf("wb3: probeva(%x,%x) = %x\n",
		    wb_addr + wb_extra_page, wb_sts & WBS_TMMASK, mmusr);
#endif

		if((mmusr & (MMUSR_R | MMUSR_W)) != MMUSR_R) {
#ifdef DEBUG
			if (mmudebug)
				printf("wb3: need to bring in first page\n");
#endif
			wb_rc = vm_fault(wb_map, 
			    trunc_page((vm_offset_t)wb_addr), 
			    VM_PROT_READ | VM_PROT_WRITE, FALSE);

			if(wb_rc != KERN_SUCCESS)
				return (wb_rc);
#ifdef DEBUG
			if (mmudebug)
				printf("wb3: first page brought in.\n");
#endif
		}
	}

	/*
	 * now check to see if a second page is required
	 */
	if(wb_extra_page) {

		mmusr = probeva(wb_addr+wb_extra_page, wb_sts & WBS_TMMASK);
#ifdef DEBUG
		if (mmudebug)
			printf("wb%d: probeva %x %x = %x\n",
			    wb, wb_addr + wb_extra_page, 
			    wb_sts & WBS_TMMASK,mmusr);
#endif

		if((mmusr & (MMUSR_R | MMUSR_W)) != MMUSR_R) {
#ifdef DEBUG
			if (mmudebug)
				printf("wb%d: page boundary crossed."
				    "  Bringing in extra page.\n",wb);
#endif

			wb_rc = vm_fault(wb_map, 
			    trunc_page((vm_offset_t)wb_addr + wb_extra_page),
			    VM_PROT_READ | VM_PROT_WRITE,FALSE);

			if(wb_rc != KERN_SUCCESS)
				return (wb_rc);
		}
#ifdef DEBUG
		if (mmudebug)
			printf("wb%d: extra page brought in okay.\n", wb);
#endif
	}

	/* Actually do the write now */

	if ((wb_sts & WBS_TMMASK) == FC_USERD &&
	    !curpcb->pcb_onfault) {
	    	curpcb->pcb_onfault = (caddr_t) _wb_fault;
	}

	switch(wb_sts & WBS_SZMASK) {

	case WBS_SIZE_BYTE :
		asm volatile ("movec %0,dfc ; movesb %1,%2@" : : "d" (wb_sts & WBS_TMMASK),
								 "d" (wb_data),
								 "a" (wb_addr));
		break;

	case WBS_SIZE_WORD :
		asm volatile ("movec %0,dfc ; movesw %1,%2@" : : "d" (wb_sts & WBS_TMMASK),
								 "d" (wb_data),
								 "a" (wb_addr));
		break;

	case WBS_SIZE_LONG :
		asm volatile ("movec %0,dfc ; movesl %1,%2@" : : "d" (wb_sts & WBS_TMMASK),
								 "d" (wb_data),
								 "a" (wb_addr));
		break;

	}
	if (curpcb->pcb_onfault == (caddr_t) _wb_fault)
		curpcb->pcb_onfault = NULL;
	if ((wb_sts & WBS_TMMASK) != FC_USERD)
		asm volatile ("movec %0,dfc\n" : : "d" (FC_USERD));
	return (KERN_SUCCESS);
}

/*
 * fault handler for write back
 */
void _wb_fault()
{
#ifdef DEBUG
	printf ("trap: writeback fault\n");
#endif
	return;
}
