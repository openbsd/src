/*	$OpenBSD: syscall.c,v 1.1 2004/02/01 05:09:48 drahn Exp $	*/
/*	$NetBSD: syscall.c,v 1.24 2003/11/14 19:03:17 scw Exp $	*/

/*-
 * Copyright (c) 2000, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * syscall entry handling
 *
 * Created      : 09/11/94
 */

#include <sys/param.h>

#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#ifdef SYSTRACE
#include <sys/systrace.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <arm/swi.h>

#ifdef acorn26
#include <machine/machdep.h>
#endif

#define MAXARGS 8

void syscall_intern(struct proc *);
void syscall_plain(struct trapframe *, struct proc *, u_int32_t);
void syscall_fancy(struct trapframe *, struct proc *, u_int32_t);

void
swi_handler(trapframe_t *frame)
{
	struct proc *p = curproc;
	u_int32_t insn;
	union sigval sv;

	/*
	 * Enable interrupts if they were enabled before the exception.
	 * Since all syscalls *should* come from user mode it will always
	 * be safe to enable them, but check anyway. 
	 */
#ifdef acorn26
	if ((frame->tf_r15 & R15_IRQ_DISABLE) == 0)
		int_on();
#else
	if (!(frame->tf_spsr & I32_bit))
		enable_interrupts(I32_bit);
#endif

#ifdef acorn26
	frame->tf_pc += INSN_SIZE;
#endif

	/*
	 * Make sure the program counter is correctly aligned so we
	 * don't take an alignment fault trying to read the opcode.
	 */
	if (__predict_false(((frame->tf_pc - INSN_SIZE) & 3) != 0)) {
		/* Give the user an illegal instruction signal. */
		sv.sival_ptr = (u_int32_t *)(u_int32_t)(frame->tf_pc-INSN_SIZE);
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);
		userret(p);
		return;
	}

	/* XXX fuword? */
#ifdef __PROG32
	insn = *(u_int32_t *)(frame->tf_pc - INSN_SIZE);
#else
	insn = *(u_int32_t *)((frame->tf_r15 & R15_PC) - INSN_SIZE);
#endif

	p->p_addr->u_pcb.pcb_tf = frame;

#ifdef CPU_ARM7
	/*
	 * This code is only needed if we are including support for the ARM7
	 * core. Other CPUs do not need it but it does not hurt.
	 */

	/*
	 * ARM700/ARM710 match sticks and sellotape job ...
	 *
	 * I know this affects GPS/VLSI ARM700/ARM710 + various ARM7500.
	 *
	 * On occasion data aborts are mishandled and end up calling
	 * the swi vector.
	 *
	 * If the instruction that caused the exception is not a SWI
	 * then we hit the bug.
	 */
	if ((insn & 0x0f000000) != 0x0f000000) {
		frame->tf_pc -= INSN_SIZE;
		curcpu()->ci_arm700bugcount.ev_count++;
		userret(l);
		return;
	}
#endif	/* CPU_ARM7 */

	uvmexp.syscalls++;

#if 0 
	(*(void(*)(struct trapframe *, struct proc *, u_int32_t))
	    (p->p_md.md_syscall))(frame, p, insn);
#else
	syscall_fancy(frame, p, insn);
#endif
}

void
syscall_intern(struct proc *p)
{
#ifdef KTRACE
	if (p->p_traceflag & (KTRFAC_SYSCALL | KTRFAC_SYSRET)) {
		p->p_md.md_syscall = syscall_fancy;
		return;
	}
#endif
#ifdef SYSTRACE
	if (p->p_flag & P_SYSTRACE) {
		p->p_md.md_syscall = syscall_fancy;
		return;
	}
#endif
	p->p_md.md_syscall = syscall_plain;
}

void
syscall_plain(struct trapframe *frame, struct proc *p, u_int32_t insn)
{
	const struct sysent *callp;
	int code, error;
	u_int nap, nargs;
	register_t *ap, *args, copyargs[MAXARGS], rval[2];
	union sigval sv;

	switch (insn & SWI_OS_MASK) { /* Which OS is the SWI from? */
	case SWI_OS_ARM: /* ARM-defined SWIs */
		code = insn & 0x00ffffff;
		switch (code) {
		case SWI_IMB:
		case SWI_IMBrange:
			/*
			 * Do nothing as there is no prefetch unit that needs
			 * flushing
			 */
			break;
		default:
			/* Undefined so illegal instruction */
			sv.sival_ptr = (u_int32_t *)(frame->tf_pc - INSN_SIZE);
			trapsignal(p, SIGILL, 0, ILL_ILLOPN, sv);
			break;
		}

		userret(p);
		return;
	case 0x000000: /* Old unofficial NetBSD range. */
	case SWI_OS_NETBSD: /* New official NetBSD range. */
		nap = 4;
		break;
	default:
		/* Undefined so illegal instruction */
		sv.sival_ptr = (u_int32_t *)(frame->tf_pc - INSN_SIZE);
		trapsignal(p, SIGILL, 0, ILL_ILLOPN, sv);
		userret(p);
		return;
	}

	code = insn & 0x000fffff;

	ap = &frame->tf_r0;
	callp = p->p_emul->e_sysent;

	switch (code) {	
	case SYS_syscall:
		code = *ap++;
		nap--;
		break;
        case SYS___syscall:
		code = ap[_QUAD_LOWWORD];
		ap += 2;
		nap -= 2;
		break;
	}

	if (code < 0 || code >= p->p_emul->e_nsysent) {
		callp += p->p_emul->e_nosys;
	} else {
		callp += code;
	}
	nargs = callp->sy_argsize / sizeof(register_t);
	if (nargs <= nap)
		args = ap;
	else {
		KASSERT(nargs <= MAXARGS);
		memcpy(copyargs, ap, nap * sizeof(register_t));
		error = copyin((void *)frame->tf_usr_sp, copyargs + nap,
		    (nargs - nap) * sizeof(register_t));
		if (error)
			goto bad;
		args = copyargs;
	}

#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
	rval[0] = 0;
	rval[1] = 0;
	error = (*callp->sy_call)(p, args, rval);

	switch (error) {
	case 0:
		frame->tf_r0 = rval[0];
		frame->tf_r1 = rval[1];

#ifdef __PROG32
		frame->tf_spsr &= ~PSR_C_bit;	/* carry bit */
#else
		frame->tf_r15 &= ~R15_FLAG_C;	/* carry bit */
#endif
		break;

	case ERESTART:
		/*
		 * Reconstruct the pc to point at the swi.
		 */
		frame->tf_pc -= INSN_SIZE;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		frame->tf_r0 = error;
#ifdef __PROG32
		frame->tf_spsr |= PSR_C_bit;	/* carry bit */
#else
		frame->tf_r15 |= R15_FLAG_C;	/* carry bit */
#endif
		break;
	}
#ifdef SYSCALL_DEBUG
        scdebug_ret(p, code, error, rval); 
#endif  

	userret(p);
}

void
syscall_fancy(struct trapframe *frame, struct proc *p, u_int32_t insn)
{
	const struct sysent *callp;
	int code, error, orig_error;
	u_int nap, nargs;
	register_t *ap, *args, copyargs[MAXARGS], rval[2];
	union sigval sv;

	switch (insn & SWI_OS_MASK) { /* Which OS is the SWI from? */
	case SWI_OS_ARM: /* ARM-defined SWIs */
		code = insn & 0x00ffffff;
		switch (code) {
		case SWI_IMB:
		case SWI_IMBrange:
			/*
			 * Do nothing as there is no prefetch unit that needs
			 * flushing
			 */
			break;
		default:
			/* Undefined so illegal instruction */
			sv.sival_ptr = (u_int32_t *)(frame->tf_pc - INSN_SIZE);
			trapsignal(p, SIGILL, 0, ILL_ILLOPN, sv);
			break;
		}

		userret(p);
		return;
	case 0x000000: /* Old unofficial NetBSD range. */
	case SWI_OS_NETBSD: /* New official NetBSD range. */
		nap = 4;
		break;
	default:
		/* Undefined so illegal instruction */
		sv.sival_ptr = (u_int32_t *)(frame->tf_pc - INSN_SIZE);
		trapsignal(p, SIGILL, 0, ILL_ILLOPN, sv);
		userret(p);
		return;
	}

	code = insn & 0x000fffff;

	ap = &frame->tf_r0;
	callp = p->p_emul->e_sysent;

	switch (code) {	
	case SYS_syscall:
		code = *ap++;
		nap--;
		break;
        case SYS___syscall:
		code = ap[_QUAD_LOWWORD];
		ap += 2;
		nap -= 2;
		break;
	}

	if (code < 0 || code >= p->p_emul->e_nsysent) {
		callp += p->p_emul->e_nosys;
	} else {
		callp += code;
	}
	nargs = callp->sy_argsize / sizeof(register_t);
	if (nargs <= nap) {
		args = ap;
		error = 0;
	} else {
		KASSERT(nargs <= MAXARGS);
		memcpy(copyargs, ap, nap * sizeof(register_t));
		error = copyin((void *)frame->tf_usr_sp, copyargs + nap,
		    (nargs - nap) * sizeof(register_t));
		args = copyargs;
	}
	orig_error = error;
#ifdef SYSCALL_DEBUG
        scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p, code, callp->sy_argsize, args);
#endif
	if (error)
		goto bad;

	rval[0] = 0;
	rval[1] = 0;
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		orig_error = error = systrace_redirect(code, p, args, rval);
	else 
#endif
		orig_error = error = (*callp->sy_call)(p, args, rval);

	switch (error) {
	case 0:
		frame->tf_r0 = rval[0];
		frame->tf_r1 = rval[1];

#ifdef __PROG32
		frame->tf_spsr &= ~PSR_C_bit;	/* carry bit */
#else
		frame->tf_r15 &= ~R15_FLAG_C;	/* carry bit */
#endif
		break;

	case ERESTART:
		/*
		 * Reconstruct the pc to point at the swi.
		 */
		frame->tf_pc -= INSN_SIZE;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		frame->tf_r0 = error;
#ifdef __PROG32
		frame->tf_spsr |= PSR_C_bit;	/* carry bit */
#else
		frame->tf_r15 |= R15_FLAG_C;	/* carry bit */
#endif
		break;
	}
#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, orig_error, rval);
#endif
	userret(p);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, orig_error, rval[0]);
#endif
}

void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;
	struct trapframe *frame = p->p_addr->u_pcb.pcb_tf;

	frame->tf_r0 = 0;
#ifdef __PROG32
	frame->tf_spsr &= ~PSR_C_bit;	/* carry bit */
#else
	frame->tf_r15 &= ~R15_FLAG_C;	/* carry bit */
#endif

	userret(p);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		ktrsysret(p, SYS_fork, 0, 0);
	}
#endif
}
