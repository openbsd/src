/* $OpenBSD: vfp.c,v 1.4 2018/07/02 07:23:37 kettenis Exp $ */
/*
 * Copyright (c) 2011 Dale Rahn <drahn@openbsd.org>
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

#include <arm64/include/vfp.h>

static inline void
set_vfp_enable(int val)
{
	uint64_t v;
	__asm __volatile("mrs %x0, cpacr_el1" : "=r" (v));
	if (val != 0) {
		v |= VFP_UFPEN;
	} else {
		v &= ~(VFP_UFPEN);
	}
	__asm __volatile("msr cpacr_el1, %x0" :: "r" (v));
	__asm __volatile("isb");
}

static inline int
get_vfp_enable(void)
{
	uint64_t v;
	int enabled = 0;
	__asm __volatile("mrs %x0, cpacr_el1" : "=r" (v));
	if ((v & VFP_UFPEN) == VFP_UFPEN )
		enabled = 1;
	return enabled;
}

int vfp_fault(vaddr_t pc, uint32_t insn, trapframe_t *tf, int fault_code);
void vfp_load(struct proc *p);
void vfp_store(struct fpreg *vfpsave);

void
vfp_store(struct fpreg *vfpsave)
{
	uint32_t scratch;

	if (get_vfp_enable()) {
		__asm __volatile(
		    "str	q0, [%x1]\n"
		    "str	q1, [%x1, #0x10]\n"
		    "str	q2, [%x1, #0x20]\n"
		    "str	q3, [%x1, #0x30]\n"
		    "str	q4, [%x1, #0x40]\n"
		    "str	q5, [%x1, #0x50]\n"
		    "str	q6, [%x1, 0x60]\n"
		    "str	q7, [%x1, #0x70]\n"
		    "str	q8, [%x1, #0x80]\n"
		    "str	q9, [%x1, #0x90]\n"
		    "str	q10, [%x1, #0xa0]\n"
		    "str	q11, [%x1, #0xb0]\n"
		    "str	q12, [%x1, #0xc0]\n"
		    "str	q13, [%x1, 0xd0]\n"
		    "str	q14, [%x1, #0xe0]\n"
		    "str	q15, [%x1, #0xf0]\n"
		    "str	q16, [%x1, #0x100]\n"
		    "str	q17, [%x1, #0x110]\n"
		    "str	q18, [%x1, #0x120]\n"
		    "str	q19, [%x1, #0x130]\n"
		    "str	q20, [%x1, #0x140]\n"
		    "str	q21, [%x1, #0x150]\n"
		    "str	q22, [%x1, #0x160]\n"
		    "str	q23, [%x1, #0x170]\n"
		    "str	q24, [%x1, #0x180]\n"
		    "str	q25, [%x1, #0x190]\n"
		    "str	q26, [%x1, #0x1a0]\n"
		    "str	q27, [%x1, #0x1b0]\n"
		    "str	q28, [%x1, #0x1c0]\n"
		    "str	q29, [%x1, #0x1d0]\n"
		    "str	q30, [%x1, #0x1e0]\n"
		    "str	q31, [%x1, #0x1f0]\n"
		    "mrs	%x0, fpsr\n"
		    "str	%w0, [%x1, 0x200]\n"	/* save vfpscr */
		    "mrs	%x0, fpcr\n"
		    "str	%w0, [%x1, 0x204]\n"	/* save vfpscr */
		: "=&r" (scratch) : "r" (vfpsave));
	}

	/* disable FPU */
	set_vfp_enable(0);
}

void
vfp_save(void)
{
	struct cpu_info	*ci = curcpu();
	struct pcb *pcb = curpcb;
	struct proc *p = curproc;
	uint32_t vfp_enabled;

	if (ci->ci_fpuproc == 0)
		return;

	vfp_enabled = get_vfp_enable();

	if (!vfp_enabled)
		return;	/* not enabled, nothing to do */

	if (pcb->pcb_fpcpu == NULL || ci->ci_fpuproc == NULL ||
	    !(pcb->pcb_fpcpu == ci && ci->ci_fpuproc == p)) {
		/* disable fpu before panic, otherwise recurse */
		set_vfp_enable(0);

		panic("FPU unit enabled when curproc and curcpu dont agree %p %p %p %p", pcb->pcb_fpcpu, ci, ci->ci_fpuproc,  p);
	}

	vfp_store(&p->p_addr->u_pcb.pcb_fpstate);

	/*
	 * NOTE: fpu state is saved but remains 'valid', as long as
	 * curpcb()->pcb_fpucpu == ci && ci->ci_fpuproc == curproc()
	 * is true FPU state is valid and can just be enabled without reload.
	 */
	set_vfp_enable(0);
}

void
vfp_enable(void)
{
	struct cpu_info	*ci = curcpu();

	if (curproc->p_addr->u_pcb.pcb_fpcpu == ci &&
	    ci->ci_fpuproc == curproc) {
		disable_interrupts();

		/* FPU state is still valid, just enable and go */
		set_vfp_enable(1);
	}
}

void
vfp_load(struct proc *p)
{
	struct cpu_info *ci = curcpu();
	struct pcb *pcb = &p->p_addr->u_pcb;
	uint32_t scratch = 0;
	int psw;

	/* do not allow a partially synced state here */
	psw = disable_interrupts();

	/*
	 * p->p_pcb->pcb_fpucpu _may_ not be NULL here, but the FPU state
	 * was synced on kernel entry, so we can steal the FPU state
	 * instead of signalling and waiting for it to save
	 */

	/* enable to be able to load ctx */
	set_vfp_enable(1);

	__asm __volatile(
	    "ldr	q0, [%x1]\n"
	    "ldr	q1, [%x1, #0x10]\n"
	    "ldr	q2, [%x1, #0x20]\n"
	    "ldr	q3, [%x1, #0x30]\n"
	    "ldr	q4, [%x1, #0x40]\n"
	    "ldr	q5, [%x1, #0x50]\n"
	    "ldr	q6, [%x1, 0x60]\n"
	    "ldr	q7, [%x1, #0x70]\n"
	    "ldr	q8, [%x1, #0x80]\n"
	    "ldr	q9, [%x1, #0x90]\n"
	    "ldr	q10, [%x1, #0xa0]\n"
	    "ldr	q11, [%x1, #0xb0]\n"
	    "ldr	q12, [%x1, #0xc0]\n"
	    "ldr	q13, [%x1, 0xd0]\n"
	    "ldr	q14, [%x1, #0xe0]\n"
	    "ldr	q15, [%x1, #0xf0]\n"
	    "ldr	q16, [%x1, #0x100]\n"
	    "ldr	q17, [%x1, #0x110]\n"
	    "ldr	q18, [%x1, #0x120]\n"
	    "ldr	q19, [%x1, #0x130]\n"
	    "ldr	q20, [%x1, #0x140]\n"
	    "ldr	q21, [%x1, #0x150]\n"
	    "ldr	q22, [%x1, #0x160]\n"
	    "ldr	q23, [%x1, #0x170]\n"
	    "ldr	q24, [%x1, #0x180]\n"
	    "ldr	q25, [%x1, #0x190]\n"
	    "ldr	q26, [%x1, #0x1a0]\n"
	    "ldr	q27, [%x1, #0x1b0]\n"
	    "ldr	q28, [%x1, #0x1c0]\n"
	    "ldr	q29, [%x1, #0x1d0]\n"
	    "ldr	q30, [%x1, #0x1e0]\n"
	    "ldr	q31, [%x1, #0x1f0]\n"

	    "ldr	%w0, [%x1, #0x200]\n"		/* set old fpsr */
	    "msr	fpsr, %x0\n"
	    "ldr	%w0, [%x1, #0x204]\n"		/* set old fpsr */
	    "msr	fpcr, %x0\n"
	    : "=&r" (scratch) : "r" (&pcb->pcb_fpstate));

	ci->ci_fpuproc = p;
	pcb->pcb_fpcpu = ci;

	/* disable until return to userland */
	set_vfp_enable(0);

	restore_interrupts(psw);
}


int
vfp_fault(vaddr_t pc, uint32_t insn, trapframe_t *tf, int fault_code)
{
	struct proc *p = curproc;
	struct pcb *pcb = &p->p_addr->u_pcb;

	if (get_vfp_enable()) {
		/*
		 * We probably ran into an unsupported instruction,
		 * like NEON on a non-NEON system. Let the process know.
		 */
		return 1;
	}

	/* we should be able to ignore old state of pcb_fpcpu ci_fpuproc */
	if ((pcb->pcb_flags & PCB_FPU) == 0) {
		pcb->pcb_flags |= PCB_FPU;
		memset(&pcb->pcb_fpstate, 0, sizeof (pcb->pcb_fpstate));
	}
	vfp_load(p);

	return 0;
}

void
vfp_discard(struct proc *p)
{
	struct cpu_info	*ci = curcpu();

	if (curpcb->pcb_fpcpu == ci && ci->ci_fpuproc == p) {
		ci->ci_fpuproc = NULL;
		curpcb->pcb_fpcpu  = NULL;
	}
}

void
vfp_kernel_enter(void)
{
	struct cpu_info *ci = curcpu();

	ci->ci_fpuproc = NULL;
	set_vfp_enable(1);
}

void
vfp_kernel_exit(void)
{
	set_vfp_enable(0);
}
