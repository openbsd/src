/*	$OpenBSD: sys_machdep.c,v 1.1 2004/01/28 01:39:39 mickey Exp $	*/
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
 * XXXfvdl check USER_LDT
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
#include <machine/mtrr.h>

#if defined(PERFCTRS) && 0
#include <machine/pmc.h>
#endif

extern struct vm_map *kernel_map;

#if 0
int x86_64_get_ioperm(struct proc *, void *, register_t *);
int x86_64_set_ioperm(struct proc *, void *, register_t *);
#endif
int x86_64_iopl(struct proc *, void *, register_t *);
int x86_64_get_mtrr(struct proc *, void *, register_t *);
int x86_64_set_mtrr(struct proc *, void *, register_t *);

/* XXXfvdl disabled USER_LDT stuff until I check this stuff */

#if defined(USER_LDT) && 0
int
x86_64_get_ldt(p, args, retval)
	struct proc *p;
	void *args;
	register_t *retval;
{
	int error;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	int nldt, num;
	union descriptor *lp;
	struct x86_64_get_ldt_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

#ifdef	LDT_DEBUG
	printf("x86_64_get_ldt: start=%d num=%d descs=%p\n", ua.start,
	    ua.num, ua.desc);
#endif

	if (ua.start < 0 || ua.num < 0)
		return (EINVAL);

	/*
	 * XXX LOCKING.
	 */

	if (pmap->pm_flags & PMF_USER_LDT) {
		nldt = pmap->pm_ldt_len;
		lp = pmap->pm_ldt;
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
x86_64_set_ldt(p, args, retval)
	struct proc *p;
	void *args;
	register_t *retval;
{
	int error, i, n;
	struct pcb *pcb = &p->p_addr->u_pcb;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	struct x86_64_set_ldt_args ua;
	union descriptor desc;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

#ifdef	LDT_DEBUG
	printf("x86_64_set_ldt: start=%d num=%d descs=%p\n", ua.start,
	    ua.num, ua.desc);
#endif

	if (ua.start < 0 || ua.num < 0)
		return (EINVAL);
	if (ua.start > 8192 || (ua.start + ua.num) > 8192)
		return (EINVAL);

	/*
	 * XXX LOCKING
	 */

	/* allocate user ldt */
	if (pmap->pm_ldt == 0 || (ua.start + ua.num) > pmap->pm_ldt_len) {
		size_t old_len, new_len;
		union descriptor *old_ldt, *new_ldt;

		if (pmap->pm_flags & PMF_USER_LDT) {
			old_len = pmap->pm_ldt_len * sizeof(union descriptor);
			old_ldt = pmap->pm_ldt;
		} else {
			old_len = NLDT * sizeof(union descriptor);
			old_ldt = ldt;
			pmap->pm_ldt_len = 512;
		}
		while ((ua.start + ua.num) > pmap->pm_ldt_len)
			pmap->pm_ldt_len *= 2;
		new_len = pmap->pm_ldt_len * sizeof(union descriptor);
		new_ldt = (union descriptor *)uvm_km_alloc(kernel_map, new_len);
		memcpy(new_ldt, old_ldt, old_len);
		memset((caddr_t)new_ldt + old_len, 0, new_len - old_len);
		pmap->pm_ldt = new_ldt;

		if (pmap->pm_flags & PCB_USER_LDT)
			ldt_free(pmap);
		else
			pmap->pm_flags |= PCB_USER_LDT;
		ldt_alloc(pmap, new_ldt, new_len);
		pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
		if (pcb == curpcb)
			lldt(pcb->pcb_ldt_sel);

		/*
		 * XXX Need to notify other processors which may be
		 * XXX currently using this pmap that they need to
		 * XXX re-load the LDT.
		 */

		if (old_ldt != ldt)
			uvm_km_free(kernel_map, (vaddr_t)old_ldt, old_len);
#ifdef LDT_DEBUG
		printf("x86_64_set_ldt(%d): new_ldt=%p\n", p->p_pid, new_ldt);
#endif
	}

	if (pcb == curpcb)
		savectx(curpcb);
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
			/*
			 * Only allow call gates targeting a segment
			 * in the LDT or a user segment in the fixed
			 * part of the gdt.  Segments in the LDT are
			 * constrained (below) to be user segments.
			 */
			if (desc.gd.gd_p != 0 && !ISLDT(desc.gd.gd_selector) &&
			    ((IDXSEL(desc.gd.gd_selector) >= NGDT) ||
			     (gdt[IDXSEL(desc.gd.gd_selector)].sd.sd_dpl !=
				 SEL_UPL)))
				return (EACCES);
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
		case SDT_MEMRWD:
		case SDT_MEMRWDA:
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
		}
	}

	/* Now actually replace the descriptors. */
	for (i = 0, n = ua.start; i < ua.num; i++, n++) {
		if ((error = copyin(&ua.desc[i], &desc, sizeof(desc))) != 0)
			goto out;

		pmap->pm_ldt[n] = desc;
	}

	*retval = ua.start;

out:
	return (error);
}
#endif	/* USER_LDT */

int
x86_64_iopl(struct proc *p, void *args, register_t *retval)
{
	int error;
	struct trapframe *tf = p->p_md.md_regs;
	struct x86_64_iopl_args ua;

	if (securelevel > 1)
		return EPERM;

	if ((error = suser(p, 0)) != 0)
		return error;

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
x86_64_get_ioperm(p, args, retval)
	struct proc *p;
	void *args;
	register_t *retval;
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct x86_64_get_ioperm_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyout(pcb->pcb_iomap, ua.iomap, sizeof(pcb->pcb_iomap));
}

int
x86_64_set_ioperm(p, args, retval)
	struct proc *p;
	void *args;
	register_t *retval;
{
	int error;
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct x86_64_set_ioperm_args ua;

	if (securelevel > 1)
		return EPERM;

	if ((error = suser(p, 0)) != 0)
		return error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyin(ua.iomap, pcb->pcb_iomap, sizeof(pcb->pcb_iomap));
}

#endif

#ifdef MTRR

int
x86_64_get_mtrr(struct proc *p, void *args, register_t *retval)
{
	struct x86_64_get_mtrr_args ua;
	int error, n;

	if (mtrr_funcs == NULL)
		return ENOSYS;

	error = copyin(args, &ua, sizeof ua);
	if (error != 0)
		return error;

	error = copyin(ua.n, &n, sizeof n);
	if (error != 0)
		return error;

	error = mtrr_get(ua.mtrrp, &n, p, MTRR_GETSET_USER);

	copyout(&n, ua.n, sizeof (int));

	return error;
}

int
x86_64_set_mtrr(struct proc *p, void *args, register_t *retval)
{
	int error, n;
	struct x86_64_set_mtrr_args ua;

	if (mtrr_funcs == NULL)
		return ENOSYS;

	error = suser(p, 0);
	if (error != 0)
		return error;

	error = copyin(args, &ua, sizeof ua);
	if (error != 0)
		return error;

	error = copyin(ua.n, &n, sizeof n);
	if (error != 0)
		return error;

	error = mtrr_set(ua.mtrrp, &n, p, MTRR_GETSET_USER);
	if (n != 0)
		mtrr_commit();

	copyout(&n, ua.n, sizeof n);

	return error;
}
#endif

int
sys_sysarch(struct proc *p, void *v, register_t *retval)
{
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(void *) parms;
	} */ *uap = v;
	int error = 0;

	switch(SCARG(uap, op)) {
#if defined(USER_LDT) && 0
	case X86_64_GET_LDT: 
		error = x86_64_get_ldt(p, SCARG(uap, parms), retval);
		break;

	case X86_64_SET_LDT: 
		error = x86_64_set_ldt(p, SCARG(uap, parms), retval);
		break;
#endif
	case X86_64_IOPL: 
		error = x86_64_iopl(p, SCARG(uap, parms), retval);
		break;

#if 0
	case X86_64_GET_IOPERM: 
		error = x86_64_get_ioperm(p, SCARG(uap, parms), retval);
		break;

	case X86_64_SET_IOPERM: 
		error = x86_64_set_ioperm(p, SCARG(uap, parms), retval);
		break;
#endif
#ifdef MTRR
	case X86_64_GET_MTRR:
		error = x86_64_get_mtrr(p, SCARG(uap, parms), retval);
		break;
	case X86_64_SET_MTRR:
		error = x86_64_set_mtrr(p, SCARG(uap, parms), retval);
		break;
#endif

#if defined(PERFCTRS) && 0
	case X86_64_PMC_INFO:
		error = pmc_info(p, SCARG(uap, parms), retval);
		break;

	case X86_64_PMC_STARTSTOP:
		error = pmc_startstop(p, SCARG(uap, parms), retval);
		break;

	case X86_64_PMC_READ:
		error = pmc_read(p, SCARG(uap, parms), retval);
		break;
#endif
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
