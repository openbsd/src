/* $OpenBSD: trap.c,v 1.42 2022/11/07 09:43:04 mpi Exp $ */
/*-
 * Copyright (c) 2014 Andrew Turner
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/user.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/vmparam.h>

#ifdef DDB
#include <ddb/db_output.h>
#endif

/* Called from exception.S */
void do_el1h_sync(struct trapframe *);
void do_el0_sync(struct trapframe *);
void do_el0_error(struct trapframe *);

void dumpregs(struct trapframe*);

/* Check whether we're executing an unprivileged load/store instruction. */
static inline int
is_unpriv_ldst(uint64_t elr)
{
	uint32_t insn = *(uint32_t *)elr;
	return ((insn & 0x3f200c00) == 0x38000800);
}

static inline int
accesstype(uint64_t esr, int exe)
{
	if (exe)
		return PROT_EXEC;
	return (!(esr & ISS_DATA_CM) && (esr & ISS_DATA_WnR)) ?
	    PROT_WRITE : PROT_READ;
}

static void
udata_abort(struct trapframe *frame, uint64_t esr, uint64_t far, int exe)
{
	struct vm_map *map;
	struct proc *p;
	struct pcb *pcb;
	vm_prot_t access_type = accesstype(esr, exe);
	vaddr_t va;
	union sigval sv;
	int error = 0, sig, code;

	pcb = curcpu()->ci_curpcb;
	p = curcpu()->ci_curproc;

	va = trunc_page(far);
	if (va >= VM_MAXUSER_ADDRESS)
		curcpu()->ci_flush_bp();

	switch (esr & ISS_DATA_DFSC_MASK) {
	case ISS_DATA_DFSC_ALIGN:
		sv.sival_ptr = (void *)far;
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);
		return;
	default:
		break;
	}

	map = &p->p_vmspace->vm_map;

	if (!uvm_map_inentry(p, &p->p_spinentry, PROC_STACK(p),
	    "[%s]%d/%d sp=%lx inside %lx-%lx: not MAP_STACK\n",
	    uvm_map_inentry_sp, p->p_vmspace->vm_map.sserial))
		return;

	/* Handle referenced/modified emulation */
	if (pmap_fault_fixup(map->pmap, va, access_type))
		return;

	error = uvm_fault(map, va, 0, access_type);

	if (error == 0) {
		uvm_grow(p, va);
		return;
	}

	if (error == ENOMEM) {
		sig = SIGKILL;
		code = 0;
	} else if (error == EIO) {
		sig = SIGBUS;
		code = BUS_OBJERR;
	} else if (error == EACCES) {
		sig = SIGSEGV;
		code = SEGV_ACCERR;
	} else {
		sig = SIGSEGV;
		code = SEGV_MAPERR;
	}
	sv.sival_ptr = (void *)far;
	trapsignal(p, sig, 0, code, sv);
}

static void
kdata_abort(struct trapframe *frame, uint64_t esr, uint64_t far, int exe)
{
	struct vm_map *map;
	struct proc *p;
	struct pcb *pcb;
	vm_prot_t access_type = accesstype(esr, exe);
	vaddr_t va;
	int error = 0;

	pcb = curcpu()->ci_curpcb;
	p = curcpu()->ci_curproc;

	va = trunc_page(far);

	/* The top bit tells us which range to use */
	if ((far >> 63) == 1)
		map = kernel_map;
	else {
		/*
		 * Only allow user-space access using
		 * unprivileged load/store instructions.
		 */
		if (is_unpriv_ldst(frame->tf_elr))
			map = &p->p_vmspace->vm_map;
		else if (pcb->pcb_onfault != NULL)
			map = kernel_map;
		else {
			panic("attempt to access user address"
			      " 0x%llx from EL1", far);
		}
	}

	/* Handle referenced/modified emulation */
	if (!pmap_fault_fixup(map->pmap, va, access_type)) {
		error = uvm_fault(map, va, 0, access_type);

		if (error == 0 && map != kernel_map)
			uvm_grow(p, va);
	}

	if (error != 0) {
		if (curcpu()->ci_idepth == 0 &&
		    pcb->pcb_onfault != NULL) {
			frame->tf_elr = (register_t)pcb->pcb_onfault;
			return;
		}
		panic("uvm_fault failed: %lx esr %llx far %llx",
		    frame->tf_elr, esr, far);
	}
}

void
do_el1h_sync(struct trapframe *frame)
{
	uint32_t exception;
	uint64_t esr, far;

	/* Read the esr register to get the exception details */
	esr = READ_SPECIALREG(esr_el1);
	exception = ESR_ELx_EXCEPTION(esr);
	far = READ_SPECIALREG(far_el1);

	intr_enable();

	/*
	 * Sanity check we are in an exception er can handle. The IL bit
	 * is used to indicate the instruction length, except in a few
	 * exceptions described in the ARMv8 ARM.
	 *
	 * It is unclear in some cases if the bit is implementation defined.
	 * The Foundation Model and QEMU disagree on if the IL bit should
	 * be set when we are in a data fault from the same EL and the ISV
	 * bit (bit 24) is also set.
	 */
//	KASSERT((esr & ESR_ELx_IL) == ESR_ELx_IL ||
//	    (exception == EXCP_DATA_ABORT && ((esr & ISS_DATA_ISV) == 0)),
//	    ("Invalid instruction length in exception"));

	switch(exception) {
	case EXCP_FP_SIMD:
	case EXCP_TRAP_FP:
		panic("FP exception in the kernel");
	case EXCP_INSN_ABORT:
		kdata_abort(frame, esr, far, 1);
		break;
	case EXCP_DATA_ABORT:
		kdata_abort(frame, esr, far, 0);
		break;
	case EXCP_BRK:
	case EXCP_WATCHPT_EL1:
	case EXCP_SOFTSTP_EL1:
#ifdef DDB
		{
		/* XXX */
		int db_trapper (u_int, u_int, trapframe_t *, int);
		db_trapper(frame->tf_elr, 0/*XXX*/, frame, exception);
		}
#else
		panic("No debugger in kernel.");
#endif
		break;
	default:
#ifdef DDB
		{
		/* XXX */
		int db_trapper (u_int, u_int, trapframe_t *, int);
		db_trapper(frame->tf_elr, 0/*XXX*/, frame, exception);
		break;
		}
#endif
		panic("Unknown kernel exception %x esr_el1 %llx lr %lxpc %lx",
		    exception,
		    esr, frame->tf_lr, frame->tf_elr);
	}
}

void
do_el0_sync(struct trapframe *frame)
{
	struct proc *p = curproc;
	union sigval sv;
	uint32_t exception;
	uint64_t esr, far;

	esr = READ_SPECIALREG(esr_el1);
	exception = ESR_ELx_EXCEPTION(esr);
	far = READ_SPECIALREG(far_el1);

	intr_enable();

	p->p_addr->u_pcb.pcb_tf = frame;
	refreshcreds(p);

	switch (exception) {
	case EXCP_UNKNOWN:
		curcpu()->ci_flush_bp();
		sv.sival_ptr = (void *)frame->tf_elr;
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);
		break;
	case EXCP_FP_SIMD:
	case EXCP_TRAP_FP:
		fpu_load(p);
		break;
	case EXCP_SVC:
		svc_handler(frame);
		break;
	case EXCP_INSN_ABORT_L:
		udata_abort(frame, esr, far, 1);
		break;
	case EXCP_PC_ALIGN:
		curcpu()->ci_flush_bp();
		sv.sival_ptr = (void *)frame->tf_elr;
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);
		break;
	case EXCP_SP_ALIGN:
		curcpu()->ci_flush_bp();
		sv.sival_ptr = (void *)frame->tf_sp;
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);
		break;
	case EXCP_DATA_ABORT_L:
		udata_abort(frame, esr, far, 0);
		break;
	case EXCP_BRK:
		sv.sival_ptr = (void *)frame->tf_elr;
		trapsignal(p, SIGTRAP, 0, TRAP_BRKPT, sv);
		break;
	case EXCP_SOFTSTP_EL0:
		sv.sival_ptr = (void *)frame->tf_elr;
		trapsignal(p, SIGTRAP, 0, TRAP_TRACE, sv);
		break;
	default:
		// panic("Unknown userland exception %x esr_el1 %lx", exception,
		//    esr);
		// USERLAND MUST NOT PANIC MACHINE
		{
			// only here to debug !?!?
			printf("exception %x esr_el1 %llx\n", exception, esr);
			dumpregs(frame);
		}
		curcpu()->ci_flush_bp();
		KERNEL_LOCK();
		sigexit(p, SIGILL);
		KERNEL_UNLOCK();
	}

	userret(p);
}

void
do_el0_error(struct trapframe *frame)
{
	panic("do_el0_error");
}

void
dumpregs(struct trapframe *frame)
{
	int i;

	for (i = 0; i < 30; i += 2) {
		printf("x%02d: 0x%016lx 0x%016lx\n",
		    i, frame->tf_x[i], frame->tf_x[i+1]);
	}
	printf("sp: 0x%016lx\n", frame->tf_sp);
	printf("lr: 0x%016lx\n", frame->tf_lr);
	printf("pc: 0x%016lx\n", frame->tf_elr);
	printf("spsr: 0x%016lx\n", frame->tf_spsr);
}
