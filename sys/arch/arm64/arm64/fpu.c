/*	$OpenBSD: fpu.c,v 1.1 2022/01/01 18:52:36 kettenis Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/armreg.h>

void
fpu_save(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct fpreg *fp = &pcb->pcb_fpstate;
	uint64_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	if ((cpacr & CPACR_FPEN_MASK) == CPACR_FPEN_TRAP_ALL1)
		return;
	KASSERT((cpacr & CPACR_FPEN_MASK) == CPACR_FPEN_TRAP_NONE);

#define STRQx(x) \
    __asm volatile ("str q" #x ", [%0, %1]" :: "r"(fp->fp_reg), "i"(x * 16))

	STRQx(0);
	STRQx(1);
	STRQx(2);
	STRQx(3);
	STRQx(4);
	STRQx(5);
	STRQx(6);
	STRQx(7);
	STRQx(8);
	STRQx(9);
	STRQx(10);
	STRQx(11);
	STRQx(12);
	STRQx(13);
	STRQx(14);
	STRQx(15);
	STRQx(16);
	STRQx(17);
	STRQx(18);
	STRQx(19);
	STRQx(20);
	STRQx(21);
	STRQx(22);
	STRQx(23);
	STRQx(24);
	STRQx(25);
	STRQx(26);
	STRQx(27);
	STRQx(28);
	STRQx(29);
	STRQx(30);
	STRQx(31);

	fp->fp_sr = READ_SPECIALREG(fpsr);
	fp->fp_cr = READ_SPECIALREG(fpcr);
}

void
fpu_load(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct fpreg *fp = &pcb->pcb_fpstate;
	uint64_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	KASSERT((cpacr & CPACR_FPEN_MASK) == CPACR_FPEN_TRAP_ALL1);

	if ((pcb->pcb_flags & PCB_FPU) == 0) {
		memset(fp, 0, sizeof(*fp));
		pcb->pcb_flags |= PCB_FPU;
	}

	/* Enable FPU. */
	cpacr &= ~CPACR_FPEN_MASK;
	cpacr |= CPACR_FPEN_TRAP_NONE;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	__asm volatile ("isb");

#define LDRQx(x) \
    __asm volatile ("ldr q" #x ", [%0, %1]" :: "r"(fp->fp_reg), "i"(x * 16))

	LDRQx(0);
	LDRQx(1);
	LDRQx(2);
	LDRQx(3);
	LDRQx(4);
	LDRQx(5);
	LDRQx(6);
	LDRQx(7);
	LDRQx(8);
	LDRQx(9);
	LDRQx(10);
	LDRQx(11);
	LDRQx(12);
	LDRQx(13);
	LDRQx(14);
	LDRQx(15);
	LDRQx(16);
	LDRQx(17);
	LDRQx(18);
	LDRQx(19);
	LDRQx(20);
	LDRQx(21);
	LDRQx(22);
	LDRQx(23);
	LDRQx(24);
	LDRQx(25);
	LDRQx(26);
	LDRQx(27);
	LDRQx(28);
	LDRQx(29);
	LDRQx(30);
	LDRQx(31);

	WRITE_SPECIALREG(fpsr, fp->fp_sr);
	WRITE_SPECIALREG(fpcr, fp->fp_cr);
}

void
fpu_drop(void)
{
	uint64_t cpacr;

	/* Disable FPU. */
	cpacr = READ_SPECIALREG(cpacr_el1);
	cpacr &= ~CPACR_FPEN_MASK;
	cpacr |= CPACR_FPEN_TRAP_ALL1;
	WRITE_SPECIALREG(cpacr_el1, cpacr);

	/*
	 * No ISB instruction needed here, as returning to EL0 is a
	 * context synchronization event.
	 */
}

void
fpu_kernel_enter(void)
{
	struct pcb *pcb = &curproc->p_addr->u_pcb;
	uint64_t cpacr;

	if (pcb->pcb_flags & PCB_FPU)
		fpu_save(curproc);

	/* Enable FPU (kernel only). */
	cpacr = READ_SPECIALREG(cpacr_el1);
	cpacr &= ~CPACR_FPEN_MASK;
	cpacr |= CPACR_FPEN_TRAP_EL0;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	__asm volatile ("isb");
}

void
fpu_kernel_exit(void)
{
	uint64_t cpacr;

	/* Disable FPU. */
	cpacr = READ_SPECIALREG(cpacr_el1);
	cpacr &= ~CPACR_FPEN_MASK;
	cpacr |= CPACR_FPEN_TRAP_ALL1;
	WRITE_SPECIALREG(cpacr_el1, cpacr);

	/*
	 * No ISB instruction needed here, as returning to EL0 is a
	 * context synchronization event.
	 */
}
