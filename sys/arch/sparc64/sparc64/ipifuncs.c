/*	$OpenBSD: ipifuncs.c,v 1.4 2007/10/20 16:54:52 miod Exp $	*/
/*	$NetBSD: ipifuncs.c,v 1.8 2006/10/07 18:11:36 rjs Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/pmap.h>
#include <machine/sparc64.h>

extern int db_active;

#define SPARC64_IPI_RETRIES	10000

#define	sparc64_ipi_sleep()	delay(1000)

/*
 * These are the "function" entry points in locore.s to handle IPI's.
 */
void	ipi_tlb_page_demap(void);
void	ipi_tlb_context_demap(void);
void	ipi_softint(void);

/*
 * Send an interprocessor interrupt.
 */
void
sparc64_send_ipi(int upaid, void (*func)(void), u_int64_t arg0, u_int64_t arg1)
{
	int i, j;

	KASSERT((u_int64_t)func > MAXINTNUM);

	if (ldxa(0, ASR_IDSR) & IDSR_BUSY) {
		__asm __volatile("ta 1; nop");
	}

	/* Schedule an interrupt. */
	for (i = 0; i < SPARC64_IPI_RETRIES; i++) {
		int s = intr_disable();

		stxa(IDDR_0H, ASI_INTERRUPT_DISPATCH, (u_int64_t)func);
		stxa(IDDR_1H, ASI_INTERRUPT_DISPATCH, arg0);
		stxa(IDDR_2H, ASI_INTERRUPT_DISPATCH, arg1);
		stxa(IDCR(upaid), ASI_INTERRUPT_DISPATCH, 0);
		membar(Sync);

		for (j = 0; j < 1000000; j++) {
			if (ldxa(0, ASR_IDSR) & IDSR_BUSY)
				continue;
			else
				break;
		}
		intr_restore(s);

		if (j == 1000000)
			break;

		if ((ldxa(0, ASR_IDSR) & IDSR_NACK) == 0)
			return;
	}

#if 1
	if (db_active || panicstr != NULL)
		printf("ipi_send: couldn't send ipi to module %u\n", upaid);
	else
		panic("ipi_send: couldn't send ipi");
#else
	__asm __volatile("ta 1; nop" : :);
#endif
}

/*
 * Broadcast an IPI to all but ourselves.
 */
void
sparc64_broadcast_ipi(void (*func)(void), u_int64_t arg0, u_int64_t arg1)
{
	struct cpu_info *ci;

	for (ci = cpus; ci != NULL; ci = ci->ci_next) {
		if (ci->ci_number == cpu_number())
			continue;
		if ((ci->ci_flags & CPUF_RUNNING) == 0)
			continue;
		sparc64_send_ipi(ci->ci_upaid, func, arg0, arg1);
	}
}

void
smp_tlb_flush_pte(vaddr_t va, int ctx)
{
	sp_tlb_flush_pte(va, ctx);

	if (db_active)
		return;

	sparc64_broadcast_ipi(ipi_tlb_page_demap, va, ctx);
}

void
smp_tlb_flush_ctx(int ctx)
{
	sp_tlb_flush_ctx(ctx);

	if (db_active)
		return;

	sparc64_broadcast_ipi(ipi_tlb_context_demap, ctx, 0);
}

void
smp_signotify(struct proc *p)
{
	if (db_active)
		return;

	sparc64_send_ipi(p->p_cpu->ci_upaid, ipi_softint, 1 << IPL_NONE, 0UL);
}
