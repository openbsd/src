/*	$OpenBSD: ipi.c,v 1.7 2007/05/25 16:22:11 art Exp $	*/
/*	$NetBSD: ipi.c,v 1.2 2003/03/01 13:05:37 fvdl Exp $	*/

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

#include <sys/cdefs.h>                  /* RCS ID & Copyright macro defns */

#include <sys/param.h> 
#include <sys/device.h>
#include <sys/systm.h>
 
#include <machine/intr.h>
#include <machine/atomic.h>
#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>

int
x86_send_ipi(struct cpu_info *ci, int ipimask)
{
	int ret;

	x86_atomic_setbits_l(&ci->ci_ipis, ipimask);

	/* Don't send IPI to cpu which isn't (yet) running. */
	if (!(ci->ci_flags & CPUF_RUNNING))
		return ENOENT;

	ret = x86_ipi(LAPIC_IPI_VECTOR, ci->ci_apicid, LAPIC_DLMODE_FIXED);
	if (ret != 0) {
		printf("ipi of %x from %s to %s failed\n",
		    ipimask,
		    curcpu()->ci_dev->dv_xname,
		    ci->ci_dev->dv_xname);
	}

	return ret;
}

int
x86_fast_ipi(struct cpu_info *ci, int ipi)
{
	if (!(ci->ci_flags & CPUF_RUNNING))
		return (ENOENT);

	return (x86_ipi(ipi, ci->ci_apicid, LAPIC_DLMODE_FIXED));
}

void
x86_broadcast_ipi(int ipimask)
{
	struct cpu_info *ci, *self = curcpu();
	int count = 0;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
		if (ci == self)
			continue;
		if ((ci->ci_flags & CPUF_RUNNING) == 0)
			continue;
		x86_atomic_setbits_l(&ci->ci_ipis, ipimask);
		count++;
	}
	if (!count)
		return;

	x86_ipi(LAPIC_IPI_VECTOR, LAPIC_DEST_ALLEXCL, LAPIC_DLMODE_FIXED);
}

void
x86_multicast_ipi(int cpumask, int ipimask)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	cpumask &= ~(1U << cpu_number());
	if (cpumask == 0)
		return;

	CPU_INFO_FOREACH(cii, ci) {
		if ((cpumask & (1U << ci->ci_cpuid)) == 0)
			continue;
		x86_send_ipi(ci, ipimask);
	}
}

void
x86_ipi_handler(void)
{
	extern struct evcount ipi_count;
	struct cpu_info *ci = curcpu();
	u_int32_t pending;
	int bit;

	pending = x86_atomic_testset_ul(&ci->ci_ipis, 0);

	for (bit = 0; bit < X86_NIPI && pending; bit++) {
		if (pending & (1<<bit)) {
			pending &= ~(1<<bit);
			(*ipifunc[bit])(ci);
			ipi_count.ec_count++;
		}
	}
}
