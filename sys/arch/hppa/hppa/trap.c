/*	$OpenBSD: trap.c,v 1.7 1999/08/14 03:06:55 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
 * All rights reserved.
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef INTRDEBUG
#define TRAPDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/signal.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <uvm/uvm.h>

#include <machine/iomod.h>
#include <machine/cpufunc.h>
#include <machine/reg.h>
#include <machine/db_machdep.h>
#include <machine/autoconf.h>

#define	FAULT_TYPE(op)	(VM_PROT_READ|(inst_load(op) ? 0 : VM_PROT_WRITE))

const char *trap_type[] = {
	"invalid interrupt vector",
	"high priority machine check",
	"power failure",
	"recovery counter trap",
	"external interrupt",
	"low-priority machine check",
	"instruction TLB miss fault",
	"instruction protection trap",
	"Illegal instruction trap",
	"break instruction trap",
	"privileged operation trap",
	"privileged register trap",
	"overflow trap",
	"conditional trap",
	"assist exception trap",
	"data TLB miss fault",
	"ITLB non-access miss fault",
	"DTLB non-access miss fault",
	"data protection trap/unalligned data reference trap",
	"data break trap",
	"TLB dirty bit trap",
	"page reference trap",
	"assist emulation trap",
	"higher-privelege transfer trap",
	"lower-privilege transfer trap",
	"taken branch trap",
	"data access rights trap",
	"data protection ID trap",
	"unaligned data ref trap",
	"reserved",
	"reserved 2"
};
int trap_types = sizeof(trap_type)/sizeof(trap_type[0]);

u_int32_t sir;
int want_resched;

void pmap_hptdump __P((void));
void cpu_intr __P((struct trapframe *frame));
void syscall __P((struct trapframe *frame, int *args));

static __inline void
userret (struct proc *p, register_t pc, u_quad_t oticks)
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	p->p_priority = p->p_usrpri;
	if (want_resched) {
		register int s;
		/*
		 * Since we are curproc, a clock interrupt could
		 * change our priority without changing run queues
		 * (the running process is not kept on a run queue).
		 * If this happened after we setrunqueue ourselves but
		 * before we switch()'ed, we might not be on the queue
		 * indicated by our priority.
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
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, pc, (int)(p->p_sticks - oticks) * psratio);
	}

	curpriority = p->p_priority;
}

void
trap(type, frame)
	int type;
	struct trapframe *frame;
{
	struct proc *p = curproc;
	register vm_offset_t va;
	register vm_map_t map;
	register vm_prot_t vftype;
	register pa_space_t space;
	u_int opcode;
	int ret;
	union sigval sv;
	int s, si;

	if (type == T_ITLBMISS || type == T_ITLBMISSNA) {
		va = frame->tf_iioq_head;
		space = frame->tf_iisq_head;
	} else {
		va = frame->tf_ior;
		space = frame->tf_isr;
	}
	opcode = frame->tf_iir;
	vftype = FAULT_TYPE(opcode);

	if (USERMODE(frame->tf_iioq_head)) {
		type |= T_USER;
		p->p_md.md_regs = frame;
	}
#ifdef TRAPDEBUG
	if ((type & ~T_USER) != T_INTERRUPT &&
	    (type & ~T_USER) != T_IBREAK)
		printf ("trap: %d, %s for %x:%x at %x:%x\n",
			type, trap_type[type & ~T_USER], space, va,
			frame->tf_iisq_head, frame->tf_iioq_head);
	else if ((type & ~T_USER) == T_IBREAK)
		printf ("trap: break instruction %x:%x at %x:%x\n",
			break5(opcode), break13(opcode),
			frame->tf_iisq_head, frame->tf_iioq_head);
#endif
	switch (type) {
	case T_NONEXIST:
	case T_NONEXIST|T_USER:
		/* we've got screwed up by the central scrutinizer */
		panic ("trap: zombie's on the bridge!!!");
		break;

	case T_RECOVERY:
	case T_RECOVERY|T_USER:
		/* XXX will implement later */
		printf ("trap: handicapped");
		break;

#ifdef DIAGNOSTIC
	case T_HPMC:
	case T_HPMC | T_USER:
	case T_EXCEPTION:
	case T_EMULATION:
	case T_TLB_DIRTY:
	case T_TLB_DIRTY | T_USER:
		panic ("trap: impossible \'%s\' (%d)",
			trap_type[type & ~T_USER], type);
		break;
#endif

	case T_IBREAK:
	case T_DATALIGN:
	case T_DBREAK:
#ifdef DDB
		if (kdb_trap (type, 0, frame)) {
			if (type == T_IBREAK) {
				/* skip break instruction */
				frame->tf_iioq_head += 4;
				frame->tf_iioq_tail += 4;
			}
			return;
		}
#endif
		/* probably panic otherwise */
		break;

	case T_IBREAK | T_USER:
	case T_DBREAK | T_USER:
		/* pass to user debugger */
		break;

	case T_EMULATION | T_USER:
	case T_EXCEPTION | T_USER:	/* co-proc assist trap */
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGFPE, type &~ T_USER, FPE_FLTINV, sv);
		break;

	case T_OVERFLOW | T_USER:
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGFPE, type &~ T_USER, FPE_INTOVF, sv);
		break;
		
	case T_CONDITION | T_USER:
		break;

	case T_ILLEGAL | T_USER:
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_ILLOPC, sv);
		break;

	case T_PRIV_OP | T_USER:
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_PRVOPC, sv);
		break;

	case T_PRIV_REG | T_USER:
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_PRVREG, sv);
		break;

		/* these should never got here */
	case T_HIGHERPL | T_USER:
	case T_LOWERPL | T_USER:
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGSEGV, type &~ T_USER, SEGV_ACCERR, sv);
		break;

	case T_IPROT | T_USER:
	case T_DPROT | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGSEGV, vftype, SEGV_ACCERR, sv);
		break;

	case T_IPROT:
	case T_DPROT:
	case T_ITLBMISS:
	case T_ITLBMISS | T_USER:
	case T_ITLBMISSNA:
	case T_ITLBMISSNA | T_USER:
	case T_DTLBMISS:
	case T_DTLBMISS | T_USER:
	case T_DTLBMISSNA:
	case T_DTLBMISSNA | T_USER:
#if 0
if (kdb_trap (type, 0, frame))
return;
break;
#endif
		va = trunc_page(va);
		map = &p->p_vmspace->vm_map;

		ret = uvm_fault(map, va, vftype, FALSE);
		if (ret != KERN_SUCCESS) {
			if (type & T_USER) {
				sv.sival_int = frame->tf_ior;
				trapsignal(p, SIGSEGV, vftype, SEGV_MAPERR, sv);
			} else
				panic ("trap: uvm_fault(%p, %x, %d, %d): %d",
				       map, frame->tf_ior, vftype, 0, ret);
		}
		break;

	case T_DATALIGN | T_USER:
		sv.sival_int = frame->tf_ior;
		trapsignal(p, SIGBUS, vftype, BUS_ADRALN, sv);
		break;

	case T_INTERRUPT:
	case T_INTERRUPT|T_USER:
		cpu_intr(frame);
		/* FALLTHROUGH */
	case T_LOWERPL:
		__asm __volatile ("ldcws 0(%1), %0"
				  : "=r" (si) : "r" (&sir));
		s = spl0();
		if (si & SIR_CLOCK) {
			splclock();
			softclock();
			spl0();
		}

		if (si & SIR_NET) {
			register int ni;
			/* use atomic "load & clear" */
			__asm __volatile ("ldcws 0(%1), %0"
					  : "=r" (ni) : "r" (&netisr));
			splnet();
#define	DONET(m,c) if (ni & (1 << (m))) c()
#include "ether.h"
#if NETHER > 0
			DONET(NETISR_ARP, arpintr);
#endif
#ifdef INET
			DONET(NETISR_IP, ipintr);
#endif
#ifdef INET6
			DONET(NETISR_IPV6, ipv6intr);
#endif
#ifdef NETATALK
			DONET(NETISR_ATALK, atintr);
#endif
#ifdef IMP
			DONET(NETISR_IMP, impintr);
#endif
#ifdef IPX
			DONET(NETISR_IPX, ipxintr);
#endif
#ifdef NS
			DONET(NETISR_NS, nsintr);
#endif
#ifdef ISO
			DONET(NETISR_ISO, clnlintr);
#endif
#ifdef CCITT
			DONET(NETISR_CCITT, ccittintr);
#endif
#ifdef NATM
			DONET(NETISR_NATM, natmintr);
#endif
#include "ppp.h"
#if NPPP > 0
			DONET(NETISR_PPP, pppintr);
#endif
#include "bridge.h"
#if NBRIDGE > 0
			DONET(NETISR_BRIDGE, bridgeintr)
#endif
		}
		splx(s);
		break;

	case T_OVERFLOW:
	case T_CONDITION:
	case T_ILLEGAL:
	case T_PRIV_OP:
	case T_PRIV_REG:
	case T_HIGHERPL:
	case T_TAKENBR:
	case T_POWERFAIL:
	case T_LPMC:
	case T_PAGEREF:
	case T_DATACC:   case T_DATACC   | T_USER:
	case T_DATAPID:  case T_DATAPID  | T_USER:
		if (0 /* T-chip */) {
			break;
		}
		/* FALLTHROUGH to unimplemented */
	default:
		panic ("trap: unimplemented \'%s\' (%d)",
			trap_type[type & ~T_USER], type);
	}

	if (type & T_USER)
		userret(p, p->p_md.md_regs->tf_iioq_head, 0);
}

void
child_return(p)
	struct proc *p;
{
	userret(p, p->p_md.md_regs->tf_iioq_head, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}

/*
 * call actual syscall routine
 * from the low-level syscall handler:
 * - all HPPA_FRAME_NARGS syscall's arguments supposed to be copied onto
 *   our stack, this wins compared to copyin just needed amount anyway
 * - register args are copied onto stack too
 */
void
syscall(frame, args)
	struct trapframe *frame;
	int *args;
{
	register struct proc *p;
	register const struct sysent *callp;
	int nsys, code, argsize, error;
	int rval[2];

	uvmexp.syscalls++;

	if (!USERMODE(frame->tf_iioq_head))
		panic("syscall");

	p = curproc;
	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;
	code = frame->tf_arg0;
	switch (code) {
	case SYS_syscall:
		code = frame->tf_arg1;
		args += 1;
		break;
	case SYS___syscall:
		if (callp != sysent)
			break;
		code = frame->tf_arg1; /* XXX or arg2? */
		args += 2;
	}

	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;	/* bad syscall # */
	else
		callp += code;
	argsize = callp->sy_argsize;

#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, argsize, args);
#endif

	rval[0] = 0;
	rval[1] = 0;
	switch (error = (*callp->sy_call)(p, args, rval)) {
	case 0:
		/* curproc may change iside the fork() */
		p = curproc;
		frame->tf_ret0 = rval[0];
		frame->tf_ret1 = rval[1];
		break;
	case ERESTART:
		frame->tf_iioq_head -= 4; /* right? XXX */
		break;
	case EJUSTRETURN:
		break;
	default:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame->tf_ret0 = error;
		break;
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, p->p_md.md_regs->tf_rp, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

/* all the interrupts, minus cpu clock, which is the last */
struct cpu_intr_vector {
	const char *name;
	int pri;
	int (*handler) __P((void *));
	void *arg;
} cpu_intr_vectors[CPU_NINTS];

void *
cpu_intr_establish(pri, irq, handler, arg, name)
	int pri, irq;
	int (*handler) __P((void *));
	void *arg;
	const char *name;
{
	register struct cpu_intr_vector *p;

	if (0 <= irq && irq < CPU_NINTS && cpu_intr_vectors[irq].handler)
		return NULL;

	p = &cpu_intr_vectors[irq];
	p->name = name;
	p->pri = pri;
	p->handler = handler;
	p->arg = arg;

	return p;
}

void
cpu_intr(frame)
	struct trapframe *frame;
{
	u_int32_t eirr;
	register struct cpu_intr_vector *p;
	register int bit;

	do {
		mfctl(CR_EIRR, eirr);
		eirr &= frame->tf_eiem;
		bit = ffs(eirr) - 1;
		if (bit >= 0) {
			mtctl(1 << bit, CR_EIRR);
			eirr &= ~(1 << bit);
			/* ((struct iomod *)cpu_gethpa(0))->io_eir = 0; */
#ifdef INTRDEBUG
			if (bit != 31)
				printf ("cpu_intr: 0x%08x\n", (1 << bit));
#endif
			p = &cpu_intr_vectors[bit];
			if (p->handler) {
				register int s = splx(p->pri);
				/* no arg means pass the frame */
				if (!(p->handler)(p->arg? p->arg:frame))
#ifdef INTRDEBUG1
					panic ("%s: can't handle interrupt",
					       p->name);
#else
					printf ("%s: can't handle interrupt\n",
						p->name);
#endif
				splx(s);
			} else {
#ifdef INTRDEBUG
				panic  ("cpu_intr: stray interrupt %d", bit);
#else
				printf ("cpu_intr: stray interrupt %d\n", bit);
#endif
			}
		}
	} while (eirr);
}

