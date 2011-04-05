/*	$OpenBSD: sys_machdep.c,v 1.10 2011/04/05 21:14:00 guenther Exp $	*/
/*	$NetBSD: sys_machdep.c,v 1.1 2003/04/26 18:39:32 fvdl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/signal.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/sysarch.h>

#if defined(PERFCTRS) && 0
#include <machine/pmc.h>
#endif

extern struct vm_map *kernel_map;

#if 0
int amd64_get_ioperm(struct proc *, void *, register_t *);
int amd64_set_ioperm(struct proc *, void *, register_t *);
#endif
int amd64_iopl(struct proc *, void *, register_t *);

#ifdef APERTURE
extern int allowaperture;
#endif

int
amd64_iopl(struct proc *p, void *args, register_t *retval)
{
	int error;
	struct trapframe *tf = p->p_md.md_regs;
	struct amd64_iopl_args ua;

	if ((error = suser(p, 0)) != 0)
		return error;

#ifdef APERTURE
	if (!allowaperture && securelevel > 0)
		return EPERM;
#else
	if (securelevel > 0)
		return EPERM;
#endif

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return error;

	if (ua.iopl)
		tf->tf_rflags |= PSL_IOPL;
	else
		tf->tf_rflags &= ~PSL_IOPL;

	return 0;
}

#if 0

int
amd64_get_ioperm(struct proc *p, void *args, register_t *retval)
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct amd64_get_ioperm_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyout(pcb->pcb_iomap, ua.iomap, sizeof(pcb->pcb_iomap));
}

int
amd64_set_ioperm(struct proc *p, void *args, register_t *retval)
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct amd64_set_ioperm_args ua;

	if (securelevel > 1)
		return EPERM;

	if ((error = suser(p, 0)) != 0)
		return error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyin(ua.iomap, pcb->pcb_iomap, sizeof(pcb->pcb_iomap));
}

#endif

int
amd64_get_fsbase(struct proc *p, void *args)
{
	return copyout(&p->p_addr->u_pcb.pcb_fsbase, args,
	    sizeof(p->p_addr->u_pcb.pcb_fsbase));
}

int
amd64_set_fsbase(struct proc *p, void *args)
{
	int error;
	uint64_t base;

	if ((error = copyin(args, &base, sizeof(base))) != 0)
		return (error);

	if (base >= VM_MAXUSER_ADDRESS)
		return (EINVAL);

	p->p_addr->u_pcb.pcb_fsbase = base;
	return 0;
}

int
sys_sysarch(struct proc *p, void *v, register_t *retval)
{
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(void *) parms;
	} */ *uap = v;
	int error = 0;

	switch(SCARG(uap, op)) {
	case AMD64_IOPL: 
		error = amd64_iopl(p, SCARG(uap, parms), retval);
		break;

#if 0
	case AMD64_GET_IOPERM: 
		error = amd64_get_ioperm(p, SCARG(uap, parms), retval);
		break;

	case AMD64_SET_IOPERM: 
		error = amd64_set_ioperm(p, SCARG(uap, parms), retval);
		break;
#endif

#if defined(PERFCTRS) && 0
	case AMD64_PMC_INFO:
		error = pmc_info(p, SCARG(uap, parms), retval);
		break;

	case AMD64_PMC_STARTSTOP:
		error = pmc_startstop(p, SCARG(uap, parms), retval);
		break;

	case AMD64_PMC_READ:
		error = pmc_read(p, SCARG(uap, parms), retval);
		break;
#endif

	case AMD64_GET_FSBASE: 
		error = amd64_get_fsbase(p, SCARG(uap, parms));
		break;

	case AMD64_SET_FSBASE: 
		error = amd64_set_fsbase(p, SCARG(uap, parms));
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}
