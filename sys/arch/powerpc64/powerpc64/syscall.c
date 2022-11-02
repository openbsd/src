/*	$OpenBSD: syscall.c,v 1.9 2022/11/02 07:20:08 guenther Exp $	*/

/*
 * Copyright (c) 2015 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>

#define MAXARGS 8

void
syscall(struct trapframe *frame)
{
	struct proc *p = curproc;
	const struct sysent *callp;
	int code, error;
	int nap = 8, nargs;
	register_t *ap, *args, copyargs[MAXARGS], rval[2];

	code = frame->fixreg[0];
	ap = &frame->fixreg[3];

	switch (code) {
	case SYS_syscall:
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
		if ((error = copyin((register_t *)frame->fixreg[1] + 8,
		    copyargs + nap, (nargs - nap) * sizeof(register_t))))
			goto bad;
		args = copyargs;
	}

	rval[0] = 0;
	rval[1] = 0;

	error = mi_syscall(p, code, callp, args, rval);

	switch (error) {
	case 0:
		frame->fixreg[0] = 0;
		frame->fixreg[3] = rval[0];
		frame->cr &= ~0x10000000;
		break;

	case ERESTART:
		frame->srr0 -= 4;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		frame->fixreg[0] = error;
		frame->cr |= 0x10000000;
		break;
	}

	mi_syscall_return(p, code, error, rval);
}

void
child_return(void *arg)
{
	struct proc *p = (struct proc *)arg;
	struct trapframe *frame = p->p_md.md_regs;

	frame->fixreg[0] = 0;
	frame->fixreg[3] = 0;
	frame->cr &= ~0x10000000;

	KERNEL_UNLOCK();

	mi_child_return(p);
}
