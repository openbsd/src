/*	$NetBSD: sys_machdep.c,v 1.28 1996/05/03 19:42:29 christos Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)sys_machdep.c	5.5 (Berkeley) 1/19/91
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
#include <sys/mtio.h>
#include <sys/buf.h>
#include <sys/trace.h>
#include <sys/signal.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/sysarch.h>

#ifdef VM86
#include <machine/vm86.h>
#endif

extern vm_map_t kernel_map;

#ifdef USER_LDT
int i386_get_ldt __P((struct proc *, char *, register_t *));
int i386_set_ldt __P((struct proc *, char *, register_t *));
#endif
int i386_iopl __P((struct proc *, char *, register_t *));
int i386_get_ioperm __P((struct proc *, char *, register_t *));
int i386_set_ioperm __P((struct proc *, char *, register_t *));

#ifdef TRACE
int	nvualarm;

void
vdoualarm(arg)
	int arg;
{
	register struct proc *p;

	p = pfind(arg);
	if (p)
		psignal(p, 16);
	nvualarm--;
}

int
sys_vtrace(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_vtrace_args /* {
		syscallarg(int) request;
		syscallarg(int) value;
	} */ *uap = v;

	switch (SCARG(uap, request)) {

	case VTR_DISABLE:		/* disable a trace point */
	case VTR_ENABLE:		/* enable a trace point */
		if (SCARG(uap, value) < 0 || SCARG(uap, value) >= TR_NFLAGS)
			return (EINVAL);
		*retval = traceflags[SCARG(uap, value)];
		traceflags[SCARG(uap, value)] = SCARG(uap, request);
		break;

	case VTR_VALUE:		/* return a trace point setting */
		if (SCARG(uap, value) < 0 || SCARG(uap, value) >= TR_NFLAGS)
			return (EINVAL);
		*retval = traceflags[SCARG(uap, value)];
		break;

	case VTR_UALARM:	/* set a real-time ualarm, less than 1 min */
		if (SCARG(uap, value) <= 0 || SCARG(uap, value) > 60 * hz ||
		    nvualarm > 5)
			return (EINVAL);
		nvualarm++;
		timeout(vdoualarm, (caddr_t)p->p_pid, SCARG(uap, value));
		break;

	case VTR_STAMP:
		trace(TR_STAMP, SCARG(uap, value), p->p_pid);
		break;
	}
	return (0);
}
#endif

#ifdef USER_LDT
/*
 * If the process has a local LDT, deallocate it, and restore the default from
 * proc0.     
 */   
void
i386_user_cleanup(pcb)
	struct pcb *pcb;
{

	ldt_free(pcb);
	pcb->pcb_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
	if (pcb == curpcb)
		lldt(pcb->pcb_ldt_sel);
	kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ldt,
	    (pcb->pcb_ldt_len * sizeof(union descriptor))); 
	pcb->pcb_ldt = 0;
}

int
i386_get_ldt(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	int nldt, num;
	union descriptor *lp;
	struct i386_get_ldt_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

#ifdef	DEBUG
	printf("i386_get_ldt: start=%d num=%d descs=%x\n", ua.start,
	    ua.num, ua.desc);
#endif

	if (ua.start < 0 || ua.num < 0)
		return (EINVAL);

	if (pcb->pcb_flags & PCB_USER_LDT) {
		nldt = pcb->pcb_ldt_len;
		lp = pcb->pcb_ldt;
	} else {
		nldt = NLDT;
		lp = ldt;
	}

	if (ua.start > nldt)
		return (EINVAL);

	lp += ua.start;
	num = min(ua.num, nldt - ua.start);

	error = copyout(lp, ua.desc, num * sizeof(union descriptor));
	if (error)
		return (error);

	*retval = num;
	return (0);
}

int
i386_set_ldt(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	int error, i, n;
	struct pcb *pcb = &p->p_addr->u_pcb;
	int fsslot, gsslot;
	int s;
	struct i386_set_ldt_args ua;
	union descriptor desc;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

#ifdef	DEBUG
	printf("i386_set_ldt: start=%d num=%d descs=%x\n", ua.start,
	    ua.num, ua.desc);
#endif

	if (ua.start < 0 || ua.num < 0)
		return (EINVAL);
	if (ua.start > 8192 || (ua.start + ua.num) > 8192)
		return (EINVAL);

	/* allocate user ldt */
	if (pcb->pcb_ldt == 0 || (ua.start + ua.num) > pcb->pcb_ldt_len) {
		size_t old_len, new_len;
		union descriptor *old_ldt, *new_ldt;

		if (pcb->pcb_flags & PCB_USER_LDT) {
			old_len = pcb->pcb_ldt_len * sizeof(union descriptor);
			old_ldt = pcb->pcb_ldt;
		} else {
			old_len = NLDT * sizeof(union descriptor);
			old_ldt = ldt;
			pcb->pcb_ldt_len = 512;
		}
		while ((ua.start + ua.num) > pcb->pcb_ldt_len)
			pcb->pcb_ldt_len *= 2;
		new_len = pcb->pcb_ldt_len * sizeof(union descriptor);
		new_ldt = (union descriptor *)kmem_alloc(kernel_map, new_len);
		bcopy(old_ldt, new_ldt, old_len);
		bzero((caddr_t)new_ldt + old_len, new_len - old_len);
		pcb->pcb_ldt = new_ldt;

		if (pcb->pcb_flags & PCB_USER_LDT)
			ldt_free(pcb);
		else
			pcb->pcb_flags |= PCB_USER_LDT;
		ldt_alloc(pcb, new_ldt, new_len);
		if (pcb == curpcb)
			lldt(pcb->pcb_ldt_sel);

		if (old_ldt != ldt)
			kmem_free(kernel_map, (vm_offset_t)old_ldt, old_len);
#ifdef DEBUG
		printf("i386_set_ldt(%d): new_ldt=%x\n", p->p_pid, new_ldt);
#endif
	}

	if (pcb == curpcb)
		savectx(curpcb);
	fsslot = IDXSEL(pcb->pcb_fs);
	gsslot = IDXSEL(pcb->pcb_gs);
	error = 0;

	/* Check descriptors for access violations. */
	for (i = 0, n = ua.start; i < ua.num; i++, n++) {
		if ((error = copyin(&ua.desc[i], &desc, sizeof(desc))) != 0)
			return (error);

		switch (desc.sd.sd_type) {
		case SDT_SYSNULL:
			desc.sd.sd_p = 0;
			break;
		case SDT_SYS286CGT:
		case SDT_SYS386CGT:
			/* Can't replace in use descriptor with gate. */
			if (n == fsslot || n == gsslot)
				return (EBUSY);
			break;
		case SDT_MEMEC:
		case SDT_MEMEAC:
		case SDT_MEMERC:
		case SDT_MEMERAC:
			/* Must be "present" if executable and conforming. */
			if (desc.sd.sd_p == 0)
				return (EACCES);
			break;
		case SDT_MEMRO:
		case SDT_MEMROA:
		case SDT_MEMRW:
		case SDT_MEMRWA:
		case SDT_MEMROD:
		case SDT_MEMRODA:
		case SDT_MEME:
		case SDT_MEMEA:
		case SDT_MEMER:
		case SDT_MEMERA:
			break;
		default:
			/* Only care if it's present. */
			if (desc.sd.sd_p != 0)
				return (EACCES);
			break;
		}

		if (desc.sd.sd_p != 0) {
			/* Only user (ring-3) descriptors may be present. */
			if (desc.sd.sd_dpl != SEL_UPL)
				return (EACCES);
		} else {
			/* Must be "present" if in use. */
			if (n == fsslot || n == gsslot)
				return (EBUSY);
		}
	}

	s = splhigh();

	/* Now actually replace the descriptors. */
	for (i = 0, n = ua.start; i < ua.num; i++, n++) {
		if ((error = copyin(&ua.desc[i], &desc, sizeof(desc))) != 0)
			goto out;

		pcb->pcb_ldt[n] = desc;
	}

	*retval = ua.start;

out:
	splx(s);
	return (error);
}
#endif	/* USER_LDT */

#ifdef APERTURE
extern int allowaperture;
#endif

int
i386_iopl(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	int error;
	struct trapframe *tf = p->p_md.md_regs;
	struct i386_iopl_args ua;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
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
		tf->tf_eflags |= PSL_IOPL;
	else
		tf->tf_eflags &= ~PSL_IOPL;

	return 0;
}

int
i386_get_ioperm(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct i386_get_ioperm_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyout(pcb->pcb_iomap, ua.iomap, sizeof(pcb->pcb_iomap));
}

int
i386_set_ioperm(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct i386_set_ioperm_args ua;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyin(ua.iomap, pcb->pcb_iomap, sizeof(pcb->pcb_iomap));
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(char *) parms;
	} */ *uap = v;
	int error = 0;

	switch(SCARG(uap, op)) {
#ifdef	USER_LDT
	case I386_GET_LDT: 
		error = i386_get_ldt(p, SCARG(uap, parms), retval);
		break;

	case I386_SET_LDT: 
		error = i386_set_ldt(p, SCARG(uap, parms), retval);
		break;
#endif

	case I386_IOPL: 
		error = i386_iopl(p, SCARG(uap, parms), retval);
		break;

	case I386_GET_IOPERM: 
		error = i386_get_ioperm(p, SCARG(uap, parms), retval);
		break;

	case I386_SET_IOPERM: 
		error = i386_set_ioperm(p, SCARG(uap, parms), retval);
		break;

#ifdef VM86
	case I386_VM86:
		error = i386_vm86(p, SCARG(uap, parms), retval);
		break;
#endif

	default:
		error = EINVAL;
		break;
	}
	return (error);
}
