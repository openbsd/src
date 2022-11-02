/* $OpenBSD: syscall.c,v 1.11 2022/11/02 07:20:08 guenther Exp $ */
/*
 * Copyright (c) 2015 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#define MAXARGS 8

void
svc_handler(trapframe_t *frame)
{
	struct proc *p = curproc;
	const struct sysent *callp;
	int code, error;
	u_int nap = 8, nargs;
	register_t *ap, *args, copyargs[MAXARGS], rval[2];

	uvmexp.syscalls++;

	/* Re-enable interrupts if they were enabled previously */
	if (__predict_true((frame->tf_spsr & I_bit) == 0))
		intr_enable();

	/* Skip over speculation-blocking barrier. */
	frame->tf_elr += 8;

	code = frame->tf_x[8];

	ap = &frame->tf_x[0];

	switch (code) {	
	case SYS_syscall:
		code = *ap++;
		nap--;
		break;
        case SYS___syscall:
		code = *ap++;
		nap--;
		break;
	}

	callp = sysent;
	if (code < 0 || code >= SYS_MAXSYSCALL)
		callp += SYS_syscall;
	else
		callp += code;

	nargs = callp->sy_argsize / sizeof(register_t);
	if (nargs <= nap) {
		args = ap;
	} else {
		KASSERT(nargs <= MAXARGS);
		memcpy(copyargs, ap, nap * sizeof(register_t));
		if ((error = copyin((void *)frame->tf_sp, copyargs + nap,
		    (nargs - nap) * sizeof(register_t))))
			goto bad;
		args = copyargs;
	}

	rval[0] = 0;
	rval[1] = 0;

	error = mi_syscall(p, code, callp, args, rval);

	switch (error) {
	case 0:
		frame->tf_x[0] = rval[0];
		frame->tf_spsr &= ~PSR_C;	/* carry bit */
		break;

	case ERESTART:
		/*
		 * Reconstruct the pc to point at the svc.
		 */
		frame->tf_elr -= 12;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		frame->tf_x[0] = error;
		frame->tf_spsr |= PSR_C;	/* carry bit */
		break;
	}

	mi_syscall_return(p, code, error, rval);
}

void
child_return(void *arg)
{
	struct proc *p = arg;
	struct trapframe *frame = p->p_addr->u_pcb.pcb_tf;

	frame->tf_x[0] = 0;
	frame->tf_spsr &= ~PSR_C;	/* carry bit */

	KERNEL_UNLOCK();

	mi_child_return(p);
}
