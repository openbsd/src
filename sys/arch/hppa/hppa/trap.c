/*	$OpenBSD: trap.c,v 1.16 2000/01/12 05:51:02 mickey Exp $	*/

/*
 * Copyright (c) 1998-2000 Michael Shalayeff
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
#include <sys/device.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <uvm/uvm.h>

#include <machine/iomod.h>
#include <machine/cpufunc.h>
#include <machine/reg.h>
#include <machine/autoconf.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

#if defined(INTRDEBUG) || defined(TRAPDEBUG)
#include <ddb/db_output.h>
#endif

#define	FAULT_TYPE(op)	(VM_PROT_READ|(inst_store(op) ? VM_PROT_WRITE : 0))

const char *trap_type[] = {
	"invalid interrupt vector",
	"high priority machine check",
	"power failure",
	"recovery counter",
	"external interrupt",
	"low-priority machine check",
	"instruction TLB miss fault",
	"instruction protection",
	"Illegal instruction",
	"break instruction",
	"privileged operation",
	"privileged register",
	"overflow",
	"conditional",
	"assist exception",
	"data TLB miss",
	"ITLB non-access miss",
	"DTLB non-access miss",
	"data protection/rights/alignment",
	"data break",
	"TLB dirty bit",
	"page reference",
	"assist emulation",
	"higher-privelege transfer",
	"lower-privilege transfer",
	"taken branch",
	"data access rights",
	"data protection ID",
	"unaligned data ref",
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
	struct pcb *pcbp;
	register vaddr_t va;
	register vm_map_t map;
	struct vmspace *vm;
	register vm_prot_t vftype;
	register pa_space_t space;
	u_int opcode;
	int ret;
	union sigval sv;
	int s, si;
	const char *tts;

	opcode = frame->tf_iir;
	if (type == T_ITLBMISS || type == T_ITLBMISSNA) {
		va = frame->tf_iioq_head;
		space = frame->tf_iisq_head;
		vftype = VM_PROT_EXECUTE;
	} else {
		va = frame->tf_ior;
		space = frame->tf_isr;
		vftype = FAULT_TYPE(opcode);
	}

	if (frame->tf_flags & TFF_LAST)
		p->p_md.md_regs = frame;

#ifdef TRAPDEBUG
	if ((type & ~T_USER) > trap_types)
		tts = "reserved";
	else
		tts = trap_type[type & ~T_USER];

	if ((type & ~T_USER) != T_INTERRUPT &&
	    (type & ~T_USER) != T_IBREAK)
		db_printf("trap: %d, %s for %x:%x at %x:%x, fl=%x, fp=%p\n",
		    type, tts, space, va, frame->tf_iisq_head,
		    frame->tf_iioq_head, frame->tf_flags, frame);
	else if ((type & ~T_USER) == T_IBREAK)
		db_printf("trap: break instruction %x:%x at %x:%x, fp=%p\n",
		    break5(opcode), break13(opcode),
		    frame->tf_iisq_head, frame->tf_iioq_head, frame);
#endif
	switch (type) {
	case T_NONEXIST:
	case T_NONEXIST|T_USER:
#ifndef DDB
		/* we've got screwed up by the central scrutinizer */
		panic ("trap: elvis has just left the building!");
		break;
#endif
	case T_RECOVERY:
	case T_RECOVERY|T_USER:
#ifndef DDB
		/* XXX will implement later */
		printf ("trap: handicapped");
		break;
#endif

#ifdef DIAGNOSTIC
	case T_EXCEPTION:
		panic("FPU/SFU emulation botch");

		/* these just can't happen ever */
	case T_PRIV_OP:
	case T_PRIV_REG:
		/* these just can't make it to the trap() ever */
	case T_HPMC:      case T_HPMC | T_USER:
	case T_EMULATION: case T_EMULATION | T_USER:
	case T_TLB_DIRTY: case T_TLB_DIRTY | T_USER:
#endif
	case T_IBREAK:
	case T_DATALIGN:
	case T_DBREAK:
	dead_end:
#ifdef DDB
		if (kdb_trap (type, 0, frame)) {
			if (type == T_IBREAK) {
				/* skip break instruction */
				frame->tf_iioq_head = frame->tf_iioq_tail;
				frame->tf_iioq_tail += 4;
			}
			return;
		}
#else
		if (type == T_DATALIGN)
			panic ("trap: %s at 0x%x", tts, va);
		else
			panic ("trap: no debugger for \"%s\" (%d)", tts, type);
#endif
		break;

	case T_IBREAK | T_USER:
	case T_DBREAK | T_USER:
		/* pass to user debugger */
		break;

	case T_EXCEPTION | T_USER:	/* co-proc assist trap */
		sv.sival_int = va;
		trapsignal(p, SIGFPE, type &~ T_USER, FPE_FLTINV, sv);
		break;

	case T_OVERFLOW | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGFPE, type &~ T_USER, FPE_INTOVF, sv);
		break;
		
	case T_CONDITION | T_USER:
		break;

	case T_ILLEGAL | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_ILLOPC, sv);
		break;

	case T_PRIV_OP | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_PRVOPC, sv);
		break;

	case T_PRIV_REG | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_PRVREG, sv);
		break;

		/* these should never got here */
	case T_HIGHERPL | T_USER:
	case T_LOWERPL | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGSEGV, type &~ T_USER, SEGV_ACCERR, sv);
		break;

	case T_IPROT | T_USER:
	case T_DPROT | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGSEGV, vftype, SEGV_ACCERR, sv);
		break;

	case T_DPROT:
	case T_IPROT:
	case T_DATACC:   	case T_DATACC   | T_USER:
	case T_ITLBMISS:	case T_ITLBMISS | T_USER:
	case T_DTLBMISS:	case T_DTLBMISS | T_USER:
	case T_ITLBMISSNA:	case T_ITLBMISSNA | T_USER:
	case T_DTLBMISSNA:	case T_DTLBMISSNA | T_USER:
		va = trunc_page(va);
		vm = p->p_vmspace;

		if (!vm)
			goto dead_end;

		/*
		 * if could be a kernel map for exec_map faults
		 */
		if (!(type & T_USER) && space == HPPA_SID_KERNEL)
			map = kernel_map;
		else
			map = &vm->vm_map;

		ret = uvm_fault(map, va, 0, vftype);

		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if uvm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if (va >= (vaddr_t)vm->vm_maxsaddr + vm->vm_ssize) {
			if (ret == KERN_SUCCESS) {
				vsize_t nss = clrnd(btoc(va - USRSTACK + NBPG));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (ret == KERN_PROTECTION_FAILURE)
				ret = KERN_INVALID_ADDRESS;
		}

		if (ret != KERN_SUCCESS) {
			if (type & T_USER) {
printf("trapsignal: uvm_fault\n");
				sv.sival_int = frame->tf_ior;
				trapsignal(p, SIGSEGV, vftype, SEGV_MAPERR, sv);
			} else {
				if (p && p->p_addr->u_pcb.pcb_onfault) {
#ifdef PMAPDEBUG
					printf("trap: copyin/out %d\n",ret);
#endif
					pcbp = &p->p_addr->u_pcb;
					frame->tf_iioq_tail = 4 +
					    (frame->tf_iioq_head =
						pcbp->pcb_onfault);
					pcbp->pcb_onfault = 0;
					break;
				}
#if 1
if (kdb_trap (type, 0, frame))
	return;
#else
				panic("trap: uvm_fault(%p, %x, %d, %d): %d",
				    map, va, 0, vftype, ret);
#endif
			}
		}
if (type == (T_DATACC|T_USER) && kdb_trap (type, 0, frame))
	return;
		break;

	case T_DATALIGN | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGBUS, vftype, BUS_ADRALN, sv);
		break;

	case T_INTERRUPT:
	case T_INTERRUPT|T_USER:
		cpu_intr(frame);
#if 0
if (kdb_trap (type, 0, frame))
return;
#endif
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
			DONET(NETISR_IPV6, ip6intr);
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
	case T_HIGHERPL:
	case T_TAKENBR:
	case T_POWERFAIL:
	case T_LPMC:
	case T_PAGEREF:
	case T_DATAPID:  	case T_DATAPID  | T_USER:
		if (0 /* T-chip */) {
			break;
		}
		/* FALLTHROUGH to unimplemented */
	default:
#if 1
if (kdb_trap (type, 0, frame))
	return;
#endif
		panic ("trap: unimplemented \'%s\' (%d)", tts, type);
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
	p->p_md.md_regs = frame;
	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;
	code = frame->tf_t1;
	switch (code) {
	case SYS_syscall:
		code = frame->tf_arg0;
		args += 1;
		break;
	case SYS___syscall:
		if (callp != sysent)
			break;
		code = frame->tf_arg0; /* XXX or arg1? */
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
		/* curproc may change inside the fork() */
		frame->tf_ret0 = rval[0];
		frame->tf_ret1 = rval[1];
		frame->tf_t1 = 0;
		break;
	case ERESTART:
		frame->tf_iioq_head -= 4; /* right? XXX */
		break;
	case EJUSTRETURN:
		break;
	default:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame->tf_t1 = error;
		break;
	}
	p = curproc;
#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, p->p_md.md_regs->tf_iioq_head, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

/* all the interrupts, minus cpu clock, which is the last */
struct cpu_intr_vector {
	struct evcnt evcnt;
	int pri;
	int (*handler) __P((void *));
	void *arg;
} cpu_intr_vectors[CPU_NINTS];

void *
cpu_intr_establish(pri, irq, handler, arg, dv)
	int pri, irq;
	int (*handler) __P((void *));
	void *arg;
	struct device *dv;
{
	register struct cpu_intr_vector *iv;

	if (0 <= irq && irq < CPU_NINTS && cpu_intr_vectors[irq].handler)
		return NULL;

	iv = &cpu_intr_vectors[irq];
	iv->pri = pri;
	iv->handler = handler;
	iv->arg = arg;
	evcnt_attach(dv, dv->dv_xname, &iv->evcnt);

	return iv;
}

void
cpu_intr(frame)
	struct trapframe *frame;
{
	u_int32_t eirr;
	register struct cpu_intr_vector *iv;
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
				db_printf ("cpu_intr: 0x%08x\n", (1 << bit));
#endif
			iv = &cpu_intr_vectors[bit];
			if (iv->handler) {
				register int s, r;

				iv->evcnt.ev_count++;
				s = splx(iv->pri);
				/* no arg means pass the frame */
				r = (iv->handler)(iv->arg? iv->arg:frame);
				splx(s);
#ifdef DEBUG
				if (!r)
					db_printf ("%s: can't handle interrupt\n",
						   iv->evcnt.ev_name);
#endif
			} else
				db_printf ("cpu_intr: stray interrupt %d\n", bit);
		}
	} while (eirr);
}
