/*	$OpenBSD: ipifuncs.c,v 1.7 2007/05/25 16:22:11 art Exp $	*/
/*	$NetBSD: ipifuncs.c,v 1.1 2003/04/26 18:39:28 fvdl Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/*
 * Interprocessor interrupt handlers.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/intr.h>
#include <machine/atomic.h>
#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#include <machine/mtrr.h>
#include <machine/gdt.h>
#include <machine/fpu.h>

#include <ddb/db_output.h>
#include <machine/db_machdep.h>

void x86_64_ipi_halt(struct cpu_info *);

void x86_64_ipi_synch_fpu(struct cpu_info *);
void x86_64_ipi_flush_fpu(struct cpu_info *);

void x86_64_ipi_nop(struct cpu_info *);

#ifdef MTRR
void x86_64_reload_mtrr(struct cpu_info *);
#else
#define x86_64_reload_mtrr NULL
#endif

void (*ipifunc[X86_NIPI])(struct cpu_info *) =
{
	x86_64_ipi_halt,
	x86_64_ipi_nop,
	x86_64_ipi_flush_fpu,
	x86_64_ipi_synch_fpu,
	NULL,
	x86_64_reload_mtrr,
	gdt_reload_cpu,
#ifdef DDB
	x86_ipi_db,
#else
	NULL,
#endif
	x86_setperf_ipi,
};

void
x86_64_ipi_nop(struct cpu_info *ci)
{
}

void
x86_64_ipi_halt(struct cpu_info *ci)
{
	disable_intr();

	for(;;) {
		__asm __volatile("hlt");
	}
}

void
x86_64_ipi_flush_fpu(struct cpu_info *ci)
{
	fpusave_cpu(ci, 0);
}

void
x86_64_ipi_synch_fpu(struct cpu_info *ci)
{
	fpusave_cpu(ci, 1);
}

#ifdef MTRR

/*
 * mtrr_reload_cpu() is a macro in mtrr.h which picks the appropriate
 * function to use..
 */

void
x86_64_reload_mtrr(struct cpu_info *ci)
{
	if (mtrr_funcs != NULL)
		mtrr_reload_cpu(ci);
}
#endif
