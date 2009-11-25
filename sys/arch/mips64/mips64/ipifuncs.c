/* $OpenBSD: ipifuncs.c,v 1.1 2009/11/25 17:39:51 syuu Exp $ */
/* $NetBSD: ipifuncs.c,v 1.40 2008/04/28 20:23:10 martin Exp $ */

/*-
 * Copyright (c) 1998, 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <sys/device.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/atomic.h>

static int      mips64_ipi_intr(void *);
static void	mips64_ipi_nop(void);

static unsigned int ipi_mailbox[MAXCPUS];

/*
 * NOTE: This table must be kept in order with the bit definitions
 * in <machine/intr.h>.
 */
typedef void (*ipifunc_t)(void);

ipifunc_t ipifuncs[MIPS64_NIPIS] = {
	mips64_ipi_nop,
};

/*
 * Initialize IPI state for a CPU.
 */
void
mips64_ipi_init(void)
{
	cpuid_t cpuid = cpu_number();
	int error;

	hw_ipi_intr_clear(cpuid);

	error = hw_ipi_intr_establish(mips64_ipi_intr, cpuid);
	if (error)
		panic("hw_ipi_intr_establish failed:%d\n", error);
}

/*
 * Process IPIs for a CPU.
 */
static int
mips64_ipi_intr(void *arg)
{
	unsigned int pending_ipis, bit;
	unsigned int cpuid = (unsigned int)(unsigned long)arg;
	int sr;

	KASSERT (cpuid == cpu_number());

	sr = disableintr();

	/* Load the mailbox register to figure out what we're supposed to do */
	pending_ipis = ipi_mailbox[cpuid];
	if (pending_ipis > 0) {
		for (bit = 0; bit < MIPS64_NIPIS; bit++)
			if (pending_ipis & (1UL << bit))
				(*ipifuncs[bit])();

		/* Clear the mailbox to clear the interrupt */
		atomic_clearbits_int(&ipi_mailbox[cpuid], pending_ipis);
	}
	hw_ipi_intr_clear(cpuid);
	setsr(sr);

	return 1;
}

/*
 * Send an interprocessor interrupt.
 */
void
mips64_send_ipi(unsigned int cpuid, unsigned int ipimask)
{
#ifdef DIAGNOSTIC
	if (cpuid >= CPU_MAXID || cpu_info[cpuid] == NULL)
		panic("mips_send_ipi: bogus cpu_id");
	if (!cpuset_isset(&cpus_running, cpu_info[cpuid]))
	        panic("mips_send_ipi: CPU %ld not running", cpuid);
#endif

	atomic_setbits_int(&ipi_mailbox[cpuid], ipimask);

	hw_ipi_intr_set(cpuid);
}

/*
 * Broadcast an IPI to all but ourselves.
 */
void
mips64_broadcast_ipi(unsigned int ipimask)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
		if (curcpu() == ci || !cpuset_isset(&cpus_running, ci))
			continue;
		mips64_send_ipi(ci->ci_cpuid, ipimask);
	}
}

/*
 * Send an IPI to all in the list but ourselves.
 */
void
mips64_multicast_ipi(unsigned int cpumask, unsigned int ipimask)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	cpumask &= ~(1UL << cpu_number());

	CPU_INFO_FOREACH(cii, ci) {
		if (!(cpumask & (1UL << ci->ci_cpuid)) || 
		    !cpuset_isset(&cpus_running, ci))
			continue;
		mips64_send_ipi(ci->ci_cpuid, ipimask);
	}
}

static void
mips64_ipi_nop(void)
{
#ifdef DEBUG
	printf("mips64_ipi_nop on cpu%d\n", cpu_number());
#endif
}
