/*	$OpenBSD: fpu.c,v 1.1 2010/06/29 04:03:21 jsing Exp $	*/

/*
 * Copyright (c) 2010 Joel Sing <jsing@openbsd.org>
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

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/fpu.h>
#include <machine/pcb.h>
#include <machine/reg.h>

void
fpu_proc_flush(struct proc *p)
{
	struct cpu_info *ci = curcpu();

	/* Flush process FPU state from CPU. */
	if (p->p_md.md_regs->tf_cr30 == ci->ci_fpu_state) {
		fpu_exit();
		ci->ci_fpu_state = 0;
	}
}

void
fpu_proc_save(struct proc *p)
{
	struct cpu_info *ci = curcpu();
	extern u_int fpu_enable;

	/* Save process FPU state. */
	if (p->p_md.md_regs->tf_cr30 == ci->ci_fpu_state) {
		mtctl(fpu_enable, CR_CCR);
		fpu_save(ci->ci_fpu_state);
		mtctl(0, CR_CCR);
	}
}
