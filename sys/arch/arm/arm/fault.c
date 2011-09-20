/*	$OpenBSD: fault.c,v 1.11 2011/09/20 22:02:11 miod Exp $	*/
/*	$NetBSD: fault.c,v 1.46 2004/01/21 15:39:21 skrll Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994-1997 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * fault.c
 *
 * Fault handlers
 *
 * Created      : 28/11/94
 */

#include <sys/types.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>

#include <uvm/uvm_extern.h>

#include <arm/cpuconf.h>

#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#if defined(DDB) || defined(KGDB)
#include <machine/db_machdep.h>
#ifdef KGDB
#include <sys/kgdb.h>
#endif
#if !defined(DDB)
#define kdb_trap	kgdb_trap
#endif
#endif

#include <arm/db_machdep.h>
#include <arch/arm/arm/disassem.h>
#include <arm/machdep.h>
 
#ifdef DEBUG
int last_fault_code;	/* For the benefit of pmap_fault_fixup() */
#endif

struct sigdata {
	int signo;
	int code;
	vaddr_t addr;
	int trap;
};

struct data_abort {
	int (*func)(trapframe_t *, u_int, u_int, struct proc *,
	    struct sigdata *);
	const char *desc;
};

static int dab_fatal(trapframe_t *, u_int, u_int, struct proc *,
    struct sigdata *sd);
static int dab_align(trapframe_t *, u_int, u_int, struct proc *,
    struct sigdata *sd);
static int dab_buserr(trapframe_t *, u_int, u_int, struct proc *,
    struct sigdata *sd);

static const struct data_abort data_aborts[] = {
	{dab_fatal,	"Vector Exception"},
	{dab_align,	"Alignment Fault 1"},
	{dab_fatal,	"Terminal Exception"},
	{dab_align,	"Alignment Fault 3"},
	{dab_buserr,	"External Linefetch Abort (S)"},
	{NULL,		"Translation Fault (S)"},
	{dab_buserr,	"External Linefetch Abort (P)"},
	{NULL,		"Translation Fault (P)"},
	{dab_buserr,	"External Non-Linefetch Abort (S)"},
	{NULL,		"Domain Fault (S)"},
	{dab_buserr,	"External Non-Linefetch Abort (P)"},
	{NULL,		"Domain Fault (P)"},
	{dab_buserr,	"External Translation Abort (L1)"},
	{NULL,		"Permission Fault (S)"},
	{dab_buserr,	"External Translation Abort (L2)"},
	{NULL,		"Permission Fault (P)"}
};

/* Determine if a fault came from user mode */
#define	TRAP_USERMODE(tf)	((tf->tf_spsr & PSR_MODE) == PSR_USR32_MODE)

/* Determine if 'x' is a permission fault */
#define	IS_PERMISSION_FAULT(x)					\
	(((1 << ((x) & FAULT_TYPE_MASK)) &			\
	  ((1 << FAULT_PERM_P) | (1 << FAULT_PERM_S))) != 0)

void
data_abort_handler(trapframe_t *tf)
{
	struct vm_map *map;
	struct pcb *pcb;
	struct proc *p;
	u_int user, far, fsr;
	vm_prot_t ftype;
	void *onfault;
	vaddr_t va;
	int error;
	union sigval sv;
	struct sigdata sd;

	/* Grab FAR/FSR before enabling interrupts */
	far = cpu_faultaddress();
	fsr = cpu_faultstatus();

	/* Update vmmeter statistics */
	uvmexp.traps++;

	/* Re-enable interrupts if they were enabled previously */
	if (__predict_true((tf->tf_spsr & I32_bit) == 0))
		enable_interrupts(I32_bit);

	/* Get the current proc structure or proc0 if there is none */
	p = (curproc != NULL) ? curproc : &proc0;

	/* Data abort came from user mode? */
	user = TRAP_USERMODE(tf);

	/* Grab the current pcb */
	pcb = &p->p_addr->u_pcb;

	/* Invoke the appropriate handler, if necessary */
	if (__predict_false(data_aborts[fsr & FAULT_TYPE_MASK].func != NULL)) {
		if ((data_aborts[fsr & FAULT_TYPE_MASK].func)(tf, fsr, far, p,
		    &sd)) {
			goto do_trapsignal;
		}
		goto out;
	}

	/*
	 * At this point, we're dealing with one of the following data aborts:
	 *
	 *  FAULT_TRANS_S  - Translation -- Section
	 *  FAULT_TRANS_P  - Translation -- Page
	 *  FAULT_DOMAIN_S - Domain -- Section
	 *  FAULT_DOMAIN_P - Domain -- Page
	 *  FAULT_PERM_S   - Permission -- Section
	 *  FAULT_PERM_P   - Permission -- Page
	 *
	 * These are the main virtual memory-related faults signalled by
	 * the MMU.
	 */

	if (user)
		p->p_addr->u_pcb.pcb_tf = tf;

	/*
	 * Make sure the Program Counter is sane. We could fall foul of
	 * someone executing Thumb code, in which case the PC might not
	 * be word-aligned. This would cause a kernel alignment fault
	 * further down if we have to decode the current instruction.
	 * XXX: It would be nice to be able to support Thumb at some point.
	 */
	if (__predict_false((tf->tf_pc & 3) != 0)) {
		if (user) {
			/*
			 * Give the user an illegal instruction signal.
			 */
			/* Deliver a SIGILL to the process */
			sd.signo = SIGILL;
			sd.code = ILL_ILLOPC;
			sd.addr = far;
			sd.trap = fsr;
			goto do_trapsignal;
		}

		/*
		 * The kernel never executes Thumb code.
		 */
		printf("\ndata_abort_fault: Misaligned Kernel-mode "
		    "Program Counter\n");
		dab_fatal(tf, fsr, far, p, NULL);
	}

	va = trunc_page((vaddr_t)far);

	/*
	 * It is only a kernel address space fault iff:
	 *	1. user == 0  and
	 *	2. pcb_onfault not set or
	 *	3. pcb_onfault set and not LDRT/LDRBT/STRT/STRBT instruction.
	 */
	if (user == 0 && (va >= VM_MIN_KERNEL_ADDRESS ||
	    (va < VM_MIN_ADDRESS && vector_page == ARM_VECTORS_LOW)) &&
	    __predict_true((pcb->pcb_onfault == NULL ||
	     ((*(u_int *)tf->tf_pc) & 0x05200000) != 0x04200000))) {
		map = kernel_map;

		/* Was the fault due to the FPE/IPKDB ? */
		if (__predict_false((tf->tf_spsr & PSR_MODE)==PSR_UND32_MODE)) {
			sd.signo = SIGSEGV;
			sd.code = SEGV_ACCERR;
			sd.addr = far;
			sd.trap = fsr;

			/*
			 * Force exit via userret()
			 * This is necessary as the FPE is an extension to
			 * userland that actually runs in a priveledged mode
			 * but uses USR mode permissions for its accesses.
			 */
			user = 1;
			goto do_trapsignal;
		}
	} else {
		map = &p->p_vmspace->vm_map;
#if 0
		if (l->l_flag & L_SA) {
			KDASSERT(l->l_proc->p_sa != NULL);
			l->l_proc->p_sa->sa_vp_faultaddr = (vaddr_t)far;
			l->l_flag |= L_SA_PAGEFAULT;
		}
#endif
	}

	/*
	 * We need to know whether the page should be mapped
	 * as R or R/W. The MMU does not give us the info as
	 * to whether the fault was caused by a read or a write.
	 *
	 * However, we know that a permission fault can only be
	 * the result of a write to a read-only location, so
	 * we can deal with those quickly.
	 *
	 * Otherwise we need to disassemble the instruction
	 * responsible to determine if it was a write.
	 */
	if (IS_PERMISSION_FAULT(fsr))
		ftype = VM_PROT_WRITE; 
	else {
		u_int insn = *(u_int *)tf->tf_pc;

		if (((insn & 0x0c100000) == 0x04000000) ||	/* STR/STRB */
		    ((insn & 0x0e1000b0) == 0x000000b0) ||	/* STRH/STRD */
		    ((insn & 0x0a100000) == 0x08000000))	/* STM/CDT */
			ftype = VM_PROT_WRITE; 
		else
		if ((insn & 0x0fb00ff0) == 0x01000090)		/* SWP */
			ftype = VM_PROT_READ | VM_PROT_WRITE; 
		else
			ftype = VM_PROT_READ; 
	}

	/*
	 * See if the fault is as a result of ref/mod emulation,
	 * or domain mismatch.
	 */
#ifdef DEBUG
	last_fault_code = fsr;
#endif
	if (pmap_fault_fixup(map->pmap, va, ftype, user)) {
#if 0
		if (map != kernel_map)
			p->p_flag &= ~L_SA_PAGEFAULT;
#endif
		goto out;
	}

	if (__predict_false(current_intr_depth > 0)) {
		if (pcb->pcb_onfault) {
			tf->tf_r0 = EINVAL;
			tf->tf_pc = (register_t) pcb->pcb_onfault;
			return;
		}
		printf("\nNon-emulated page fault with intr_depth > 0\n");
		dab_fatal(tf, fsr, far, p, NULL);
	}

	onfault = pcb->pcb_onfault;
	pcb->pcb_onfault = NULL;
	error = uvm_fault(map, va, 0, ftype);
	pcb->pcb_onfault = onfault;

#if 0
	if (map != kernel_map)
		p->p_flag &= ~L_SA_PAGEFAULT;
#endif

	if (__predict_true(error == 0)) {
		if (user)
			uvm_grow(p, va); /* Record any stack growth */
		goto out;
	}

	if (user == 0) {
		if (pcb->pcb_onfault) {
			tf->tf_r0 = error;
			tf->tf_pc = (register_t) pcb->pcb_onfault;
			return;
		}

		printf("\nuvm_fault(%p, %lx, %x, 0) -> %x\n", map, va, ftype,
		    error);
		dab_fatal(tf, fsr, far, p, NULL);
	}


	sv.sival_ptr = (u_int32_t *)far;
	if (error == ENOMEM) {
		printf("UVM: pid %d (%s), uid %d killed: "
		    "out of swap\n", p->p_pid, p->p_comm,
		    (p->p_cred && p->p_ucred) ?
		     p->p_ucred->cr_uid : -1);
		sd.signo = SIGKILL;
	} else
		sd.signo = SIGSEGV;

	sd.code = (error == EACCES) ? SEGV_ACCERR : SEGV_MAPERR;
	sd.addr = far;
	sd.trap = fsr;
do_trapsignal:
	sv.sival_int = sd.addr;
	trapsignal(p, sd.signo, sd.trap, sd.code, sv);
out:
	/* If returning to user mode, make sure to invoke userret() */
	if (user)
		userret(p);
}

/*
 * dab_fatal() handles the following data aborts:
 *
 *  FAULT_WRTBUF_0 - Vector Exception
 *  FAULT_WRTBUF_1 - Terminal Exception
 *
 * We should never see these on a properly functioning system.
 *
 * This function is also called by the other handlers if they
 * detect a fatal problem.
 *
 * Note: If 'l' is NULL, we assume we're dealing with a prefetch abort.
 */
static int
dab_fatal(trapframe_t *tf, u_int fsr, u_int far, struct proc *p,
    struct sigdata *sd)
{
	const char *mode;

	mode = TRAP_USERMODE(tf) ? "user" : "kernel";

	if (p != NULL) {
		printf("Fatal %s mode data abort: '%s'\n", mode,
		    data_aborts[fsr & FAULT_TYPE_MASK].desc);
		printf("trapframe: %p\nFSR=%08x, FAR=", tf, fsr);
		if ((fsr & FAULT_IMPRECISE) == 0)
			printf("%08x, ", far);
		else
			printf("Invalid,  ");
		printf("spsr=%08x\n", tf->tf_spsr);
	} else {
		printf("Fatal %s mode prefetch abort at 0x%08x\n",
		    mode, tf->tf_pc);
		printf("trapframe: %p, spsr=%08x\n", tf, tf->tf_spsr);
	}

	printf("r0 =%08x, r1 =%08x, r2 =%08x, r3 =%08x\n",
	    tf->tf_r0, tf->tf_r1, tf->tf_r2, tf->tf_r3);
	printf("r4 =%08x, r5 =%08x, r6 =%08x, r7 =%08x\n",
	    tf->tf_r4, tf->tf_r5, tf->tf_r6, tf->tf_r7);
	printf("r8 =%08x, r9 =%08x, r10=%08x, r11=%08x\n",
	    tf->tf_r8, tf->tf_r9, tf->tf_r10, tf->tf_r11);
	printf("r12=%08x, ", tf->tf_r12);

	if (TRAP_USERMODE(tf))
		printf("usp=%08x, ulr=%08x",
		    tf->tf_usr_sp, tf->tf_usr_lr);
	else
		printf("ssp=%08x, slr=%08x",
		    tf->tf_svc_sp, tf->tf_svc_lr);
	printf(", pc =%08x\n\n", tf->tf_pc);

#if defined(DDB) || defined(KGDB)
	kdb_trap(T_FAULT, tf);
#endif
	panic("Fatal abort");
	/*NOTREACHED*/
}

/*
 * dab_align() handles the following data aborts:
 *
 *  FAULT_ALIGN_0 - Alignment fault
 *  FAULT_ALIGN_0 - Alignment fault
 *
 * These faults are fatal if they happen in kernel mode. Otherwise, we
 * deliver a bus error to the process.
 */
static int
dab_align(trapframe_t *tf, u_int fsr, u_int far, struct proc *p,
    struct sigdata *sd)
{
	/* Alignment faults are always fatal if they occur in kernel mode */
	if (!TRAP_USERMODE(tf))
		dab_fatal(tf, fsr, far, p, NULL);

	/* pcb_onfault *must* be NULL at this point */
	KDASSERT(p->p_addr->u_pcb.pcb_onfault == NULL);

	/* Deliver a bus error signal to the process */
	sd->signo = SIGBUS;
	sd->code = BUS_ADRALN;
	sd->addr = far;
	sd->trap = fsr;

	p->p_addr->u_pcb.pcb_tf = tf;

	return (1);
}

/*
 * dab_buserr() handles the following data aborts:
 *
 *  FAULT_BUSERR_0 - External Abort on Linefetch -- Section
 *  FAULT_BUSERR_1 - External Abort on Linefetch -- Page
 *  FAULT_BUSERR_2 - External Abort on Non-linefetch -- Section
 *  FAULT_BUSERR_3 - External Abort on Non-linefetch -- Page
 *  FAULT_BUSTRNL1 - External abort on Translation -- Level 1
 *  FAULT_BUSTRNL2 - External abort on Translation -- Level 2
 *
 * If pcb_onfault is set, flag the fault and return to the handler.
 * If the fault occurred in user mode, give the process a SIGBUS.
 *
 * Note: On XScale, FAULT_BUSERR_0, FAULT_BUSERR_1, and FAULT_BUSERR_2
 * can be flagged as imprecise in the FSR. This causes a real headache
 * since some of the machine state is lost. In this case, tf->tf_pc
 * may not actually point to the offending instruction. In fact, if
 * we've taken a double abort fault, it generally points somewhere near
 * the top of "data_abort_entry" in exception.S.
 *
 * In all other cases, these data aborts are considered fatal.
 */
static int
dab_buserr(trapframe_t *tf, u_int fsr, u_int far, struct proc *p,
    struct sigdata *sd)
{
	struct pcb *pcb = &p->p_addr->u_pcb;

#ifdef __XSCALE__
	if ((fsr & FAULT_IMPRECISE) != 0 &&
	    (tf->tf_spsr & PSR_MODE) == PSR_ABT32_MODE) {
		/*
		 * Oops, an imprecise, double abort fault. We've lost the
		 * r14_abt/spsr_abt values corresponding to the original
		 * abort, and the spsr saved in the trapframe indicates
		 * ABT mode.
		 */
		tf->tf_spsr &= ~PSR_MODE;

		/*
		 * We use a simple heuristic to determine if the double abort
		 * happened as a result of a kernel or user mode access.
		 * If the current trapframe is at the top of the kernel stack,
		 * the fault _must_ have come from user mode.
		 */
		if (tf != ((trapframe_t *)pcb->pcb_un.un_32.pcb32_sp) - 1) {
			/*
			 * Kernel mode. We're either about to die a
			 * spectacular death, or pcb_onfault will come
			 * to our rescue. Either way, the current value
			 * of tf->tf_pc is irrelevant.
			 */
			tf->tf_spsr |= PSR_SVC32_MODE;
			if (pcb->pcb_onfault == NULL)
				printf("\nKernel mode double abort!\n");
		} else {
			/*
			 * User mode. We've lost the program counter at the
			 * time of the fault (not that it was accurate anyway;
			 * it's not called an imprecise fault for nothing).
			 * About all we can do is copy r14_usr to tf_pc and
			 * hope for the best. The process is about to get a
			 * SIGBUS, so it's probably history anyway.
			 */
			tf->tf_spsr |= PSR_USR32_MODE;
			tf->tf_pc = tf->tf_usr_lr;
		}
	}

	/* FAR is invalid for imprecise exceptions */
	if ((fsr & FAULT_IMPRECISE) != 0)
		far = 0;
#endif /* __XSCALE__ */

	if (pcb->pcb_onfault) {
		KDASSERT(TRAP_USERMODE(tf) == 0);
		tf->tf_r0 = EFAULT;
		tf->tf_pc = (register_t) pcb->pcb_onfault;
		return (0);
	}

	/*
	 * At this point, if the fault happened in kernel mode or user mode,
	 * we're toast
	 */
	dab_fatal(tf, fsr, far, p, NULL);

	return (1);
}

/*
 * void prefetch_abort_handler(trapframe_t *tf)
 *
 * Abort handler called when instruction execution occurs at
 * a non existent or restricted (access permissions) memory page.
 * If the address is invalid and we were in SVC mode then panic as
 * the kernel should never prefetch abort.
 * If the address is invalid and the page is mapped then the user process
 * does no have read permission so send it a signal.
 * Otherwise fault the page in and try again.
 */
void
prefetch_abort_handler(trapframe_t *tf)
{
	struct proc *p;
	struct vm_map *map;
	vaddr_t fault_pc, va;
	int error;
	union sigval sv;

	/* Update vmmeter statistics */
	uvmexp.traps++;

	/*
	 * Enable IRQ's (disabled by the abort) This always comes
	 * from user mode so we know interrupts were not disabled.
	 * But we check anyway.
	 */
	if (__predict_true((tf->tf_spsr & I32_bit) == 0))
		enable_interrupts(I32_bit);

	/* Prefetch aborts cannot happen in kernel mode */
	if (__predict_false(!TRAP_USERMODE(tf)))
		dab_fatal(tf, 0, tf->tf_pc, NULL, NULL);

	/* Get fault address */
	fault_pc = tf->tf_pc;
	p = curproc;
	p->p_addr->u_pcb.pcb_tf = tf;

	/* Ok validate the address, can only execute in USER space */
	if (__predict_false(fault_pc >= VM_MAXUSER_ADDRESS ||
	    (fault_pc < VM_MIN_ADDRESS && vector_page == ARM_VECTORS_LOW))) {
		sv.sival_ptr = (u_int32_t *)fault_pc;
		trapsignal(p, SIGSEGV, 0, SEGV_ACCERR, sv);
		goto out;
	}

	map = &p->p_vmspace->vm_map;
	va = trunc_page(fault_pc);

	/*
	 * See if the pmap can handle this fault on its own...
	 */
#ifdef DEBUG
	last_fault_code = -1;
#endif
	if (pmap_fault_fixup(map->pmap, va, VM_PROT_READ, 1))
		goto out;

#ifdef DIAGNOSTIC
	if (__predict_false(current_intr_depth > 0)) {
		printf("\nNon-emulated prefetch abort with intr_depth > 0\n");
		dab_fatal(tf, 0, tf->tf_pc, NULL, NULL);
	}
#endif

	error = uvm_fault(map, va, 0, VM_PROT_READ);
	if (__predict_true(error == 0))
		goto out;

	sv.sival_ptr = (u_int32_t *) fault_pc;
	if (error == ENOMEM) {
		printf("UVM: pid %d (%s), uid %d killed: "
		    "out of swap\n", p->p_pid, p->p_comm,
		    (p->p_cred && p->p_ucred) ?
		     p->p_ucred->cr_uid : -1);
		trapsignal(p, SIGKILL, 0, SEGV_MAPERR, sv);
	} else
		trapsignal(p, SIGSEGV, 0, SEGV_MAPERR, sv);

out:
	userret(p);
}

/*
 * Tentatively read an 8, 16, or 32-bit value from 'addr'.
 * If the read succeeds, the value is written to 'rptr' and zero is returned.
 * Else, return EFAULT.
 */
int
badaddr_read(void *addr, size_t size, void *rptr)
{
	extern int badaddr_read_1(const uint8_t *, uint8_t *);
	extern int badaddr_read_2(const uint16_t *, uint16_t *);
	extern int badaddr_read_4(const uint32_t *, uint32_t *);
	union {
		uint8_t v1;
		uint16_t v2;
		uint32_t v4;
	} u;
	int rv;

	cpu_drain_writebuf();

	/* Read from the test address. */
	switch (size) {
	case sizeof(uint8_t):
		rv = badaddr_read_1(addr, &u.v1);
		if (rv == 0 && rptr)
			*(uint8_t *) rptr = u.v1;
		break;

	case sizeof(uint16_t):
		rv = badaddr_read_2(addr, &u.v2);
		if (rv == 0 && rptr)
			*(uint16_t *) rptr = u.v2;
		break;

	case sizeof(uint32_t):
		rv = badaddr_read_4(addr, &u.v4);
		if (rv == 0 && rptr)
			*(uint32_t *) rptr = u.v4;
		break;

	default:
		panic("badaddr: invalid size (%lu)", (u_long) size);
	}

	/* Return EFAULT if the address was invalid, else zero */
	return (rv);
}
