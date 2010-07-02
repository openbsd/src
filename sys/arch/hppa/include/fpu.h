/*	$OpenBSD: fpu.h,v 1.3 2010/07/02 00:00:45 jsing Exp $	*/

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

#ifndef _MACHINE_FPU_H_
#define _MACHINE_FPU_H_

#include <machine/cpu.h>
#include <machine/reg.h>

struct hppa_fpstate {
	struct fpreg hfp_regs;
	volatile struct cpu_info *hfp_cpu;	/* CPU which FPU state is on. */
};

void	fpu_proc_flush(struct proc *);
void	fpu_proc_save(struct proc *);
void	fpu_cpu_save(int);

#endif /* _MACHINE_FPU_H_ */
