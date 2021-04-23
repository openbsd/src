/*
 * Copyright (c) 2020 Dale Rahn <drahn@openbsd.org>
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
#include "machine/asm.h"

void fpu_clear(struct fpreg *fp)
{
	/* rounding mode set to 0, should be RND_NEAREST */
	bzero(fp, sizeof (*fp));
}

// may look into optimizing this, bit map lookup ?

int fpu_valid_opcode(uint32_t instr)
{
	int opcode = instr & 0x7f;
	int valid = 0;

	if ((opcode & 0x3) == 0x3) {
		/* 32 bit instruction */
		switch(opcode) {
		case 0x07:	// LOAD-FP
		case 0x27:	// STORE-FP
		case 0x53:	// OP-FP
			valid = 1;
			break;
		default:
			;
		}
	} else {
		/* 16 bit instruction */
		int opcode16 = instr & 0xe003;
		switch (opcode16) {
		case 0x1000:	// C.FLD
		case 0xa000:	// C.SLD
			valid = 1;
			break;
		case 0x2002:	// C.FLDSP
			// must verify dest register is float
			valid = opcode16 & (1 << 11);
			break;
		case 0xa002:	// C.FSDSP
			// must verify dest register is float
			valid = opcode16 & (1 << 6);
			break;
		default:
			;
		}
	}
	//printf("FPU check requested %d\n", valid);
	return valid;
}

void
fpu_discard(struct proc *p)
{
	if (p->p_addr->u_pcb.pcb_fpcpu == curcpu())
		curcpu()->ci_fpuproc = NULL;
	p->p_addr->u_pcb.pcb_fpcpu = NULL;
}

void
fpu_disable()
{
	__asm volatile ("csrc sstatus, %0" :: "r"(SSTATUS_FS_MASK));
}

void
fpu_enable_clean()
{
	__asm volatile ("csrc sstatus, %0" :: "r"(SSTATUS_FS_MASK));
	__asm volatile ("csrs sstatus, %0" :: "r"(SSTATUS_FS_CLEAN));
}

void
fpu_save(struct proc *p, struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct pcb *pcb =  &p->p_addr->u_pcb;
	struct fpreg *fp = &p->p_addr->u_pcb.pcb_fpstate;
	register void *ptr = fp->fp_f;
	uint64_t fcsr;

	if (ci->ci_fpuproc != p) {
		return;
	}

	if (pcb->pcb_fpcpu == NULL || ci->ci_fpuproc == NULL ||
	    !(pcb->pcb_fpcpu == ci && ci->ci_fpuproc == p)) {
		/* disable fpu */
		panic("FPU enabled but curproc and curcpu do not agree %p %p",
		    pcb->pcb_fpcpu, ci->ci_fpuproc);
	}


	switch (p->p_addr->u_pcb.pcb_tf->tf_sstatus & SSTATUS_FS_MASK)  {
	case SSTATUS_FS_OFF:
		/* Fallthru */
	case SSTATUS_FS_CLEAN:
		p->p_addr->u_pcb.pcb_tf->tf_sstatus &= ~SSTATUS_FS_MASK;
		return;
	case SSTATUS_FS_DIRTY:
	default:
		;
		/* fallthru */
	}

	__asm volatile("frcsr	%0" : "=r"(fcsr));

	fp->fp_fcsr = fcsr;
	#define STFx(x) \
	__asm volatile ("fsd	f" __STRING(x)  ", %1(%0)": :"r"(ptr), "i"(x * 8))

	STFx(0);
	STFx(1);
	STFx(2);
	STFx(3);
	STFx(4);
	STFx(5);
	STFx(6);
	STFx(7);
	STFx(8);
	STFx(9);
	STFx(10);
	STFx(11);
	STFx(12);
	STFx(13);
	STFx(14);
	STFx(15);
	STFx(16);
	STFx(17);
	STFx(18);
	STFx(19);
	STFx(20);
	STFx(21);
	STFx(22);
	STFx(23);
	STFx(24);
	STFx(25);
	STFx(26);
	STFx(27);
	STFx(28);
	STFx(29);
	STFx(30);
	STFx(31);

	/*
	 * pcb->pcb_fpcpu and ci->ci_fpuproc are still valid
	 * until some other fpu context steals either the cpu 
	 * context or another cpu steals the fpu context.
	 */

	p->p_addr->u_pcb.pcb_tf->tf_sstatus &= ~SSTATUS_FS_MASK;
	void fpu_enable_disable();
}

void
fpu_load(struct proc *p)
{
	struct cpu_info *ci = curcpu();
	struct pcb *pcb =  &p->p_addr->u_pcb;

	struct fpreg *fp = &p->p_addr->u_pcb.pcb_fpstate;
	register void *ptr = fp->fp_f;

	/*
	 * Verify that context is not already loaded
	 */
	if (pcb->pcb_fpcpu == ci && ci->ci_fpuproc == p) {
		return;
	}
	//printf("FPU load requested %p %p \n", ci, p);

	if ((pcb->pcb_flags & PCB_FPU) == 0) {
		fpu_clear(fp);
		pcb->pcb_flags |= PCB_FPU;
	}
	fpu_enable_clean();

	__asm volatile("fscsr	%0" : : "r"(fp->fp_fcsr));
	#define RDFx(x) \
	__asm volatile ("fld	f" __STRING(x)  ", %1(%0)": :"r"(ptr), "i"(x * 8))

	RDFx(0);
	RDFx(1);
	RDFx(2);
	RDFx(3);
	RDFx(4);
	RDFx(5);
	RDFx(6);
	RDFx(7);
	RDFx(8);
	RDFx(9);
	RDFx(10);
	RDFx(11);
	RDFx(12);
	RDFx(13);
	RDFx(14);
	RDFx(15);
	RDFx(16);
	RDFx(17);
	RDFx(18);
	RDFx(19);
	RDFx(20);
	RDFx(21);
	RDFx(22);
	RDFx(23);
	RDFx(24);
	RDFx(25);
	RDFx(26);
	RDFx(27);
	RDFx(28);
	RDFx(29);
	RDFx(30);
	RDFx(31);

	/*
	 * pcb->pcb_fpcpu and ci->ci_fpuproc are activated here
	 * to indicate that the fpu context is correctly loaded on
	 * this cpu. XXX block interupts for these saves ?
	 */
	pcb->pcb_fpcpu = ci;
	ci->ci_fpuproc = p;

	void fpu_enable_disable();
}
