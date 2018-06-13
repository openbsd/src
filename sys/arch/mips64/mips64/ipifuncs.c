/* $OpenBSD: ipifuncs.c,v 1.19 2018/06/13 14:38:42 visa Exp $ */
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
#include <sys/proc.h>
#include <sys/atomic.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/intr.h>

int	mips64_ipi_intr(void *);
void	mips64_ipi_nop(void);
void	smp_rendezvous_action(void);
void	mips64_ipi_ddb(void);
void	mips64_multicast_ipi(unsigned int, unsigned int);

struct evcount ipi_count;
unsigned int ipi_irq = 0;
unsigned int ipi_mailbox[MAXCPUS];

/* Variables needed for SMP rendezvous. */
struct mutex smp_ipi_mtx;
volatile unsigned long smp_rv_map;
void (*volatile smp_rv_action_func)(void *arg);
void * volatile smp_rv_func_arg;
volatile unsigned int smp_rv_waiters[2];

/*
 * NOTE: This table must be kept in order with the bit definitions
 * in <machine/intr.h>.
 */
typedef void (*ipifunc_t)(void);

ipifunc_t ipifuncs[MIPS64_NIPIS] = {
	mips64_ipi_nop,
	smp_rendezvous_action,
	mips64_ipi_ddb
};

/*
 * Initialize IPI state for a CPU.
 */
void
mips64_ipi_init(void)
{
	cpuid_t cpuid = cpu_number();
	int error;

	if (!cpuid) {
		mtx_init(&smp_ipi_mtx, IPL_IPI);
		evcount_attach(&ipi_count, "ipi", &ipi_irq);
	}

	hw_ipi_intr_clear(cpuid);

	error = hw_ipi_intr_establish(mips64_ipi_intr, cpuid);
	if (error)
		panic("hw_ipi_intr_establish failed:%d", error);
}

/*
 * Process IPIs for a CPU.
 */
int
mips64_ipi_intr(void *arg)
{
	unsigned int pending_ipis, bit;
	unsigned int cpuid = (unsigned int)(unsigned long)arg;

	KASSERT (cpuid == cpu_number());

	/* clear ipi interrupt */
	hw_ipi_intr_clear(cpuid);
	/* get and clear pending ipis */
	pending_ipis = atomic_swap_uint(&ipi_mailbox[cpuid], 0);
	
	if (pending_ipis > 0) {
		for (bit = 0; bit < MIPS64_NIPIS; bit++) {
			if (pending_ipis & (1UL << bit)) {
				(*ipifuncs[bit])();
				atomic_inc_long(
				    (unsigned long *)&ipi_count.ec_count);
			}
		}
	}

	return 1;
}

/*
 * Send an interprocessor interrupt.
 */
void
mips64_send_ipi(unsigned int cpuid, unsigned int ipimask)
{
#ifdef DEBUG
	if (cpuid >= CPU_MAXID || get_cpu_info(cpuid) == NULL)
		panic("mips_send_ipi: bogus cpu_id");
	if (!cpuset_isset(&cpus_running, get_cpu_info(cpuid)))
	        panic("mips_send_ipi: CPU %u not running", cpuid);
#endif

	atomic_setbits_int(&ipi_mailbox[cpuid], ipimask);

	hw_ipi_intr_set(cpuid);
}

/*
 * Send an IPI to all in the list but ourselves.
 */
void
mips64_multicast_ipi(unsigned int cpumask, unsigned int ipimask)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	cpumask &= ~(1 << cpu_number());

	CPU_INFO_FOREACH(cii, ci) {
		if (!(cpumask & (1UL << ci->ci_cpuid)) || 
		    !cpuset_isset(&cpus_running, ci))
			continue;
		mips64_send_ipi(ci->ci_cpuid, ipimask);
	}
}

void
mips64_ipi_nop(void)
{
#ifdef DEBUG
	printf("mips64_ipi_nop on cpu%lu\n", cpu_number());
#endif
}

/*
 * All-CPU rendezvous.  CPUs are signalled, all execute the setup function 
 * (if specified), rendezvous, execute the action function (if specified),
 * rendezvous again, execute the teardown function (if specified), and then
 * resume.
 *
 * Note that the supplied external functions _must_ be reentrant and aware
 * that they are running in parallel and in an unknown lock context.
 */

void
smp_rendezvous_action(void)
{
	void* local_func_arg = smp_rv_func_arg;
	void (*local_action_func)(void*) = smp_rv_action_func;
	unsigned int cpumask = 1 << cpu_number();

	/* Ensure we have up-to-date values. */
	atomic_setbits_int(&smp_rv_waiters[0], cpumask);
	while (smp_rv_waiters[0] != smp_rv_map)
		;

	/* action function */
	(*local_action_func)(local_func_arg);

	/* spin on exit rendezvous */
	atomic_setbits_int(&smp_rv_waiters[1], cpumask);
}

void
smp_rendezvous_cpus(unsigned long map,
	void (* action_func)(void *),
	void *arg)
{
	unsigned int cpumask = 1 << cpu_number();

	if (cpumask == map) {
		(*action_func)(arg);
		return;
	}

	/* obtain rendezvous lock */
        mtx_enter(&smp_ipi_mtx);

	/* set static function pointers */
	smp_rv_map = map;
	smp_rv_action_func = action_func;
	smp_rv_func_arg = arg;
	smp_rv_waiters[0] = 0;
	smp_rv_waiters[1] = 0;

	/* Ensure the parameters are visible to other CPUs. */
	membar_producer();

	/* signal other processors, which will enter the IPI with interrupts off */
	mips64_multicast_ipi(map, MIPS64_IPI_RENDEZVOUS);

	/* Check if the current CPU is in the map */
	if (map & cpumask)
		smp_rendezvous_action();

	while (smp_rv_waiters[1] != smp_rv_map)
		continue;

	smp_rv_action_func = NULL;

	/* release lock */
	mtx_leave(&smp_ipi_mtx);
}

void
mips64_ipi_ddb(void)
{
#ifdef DDB
	db_enter();
#endif
}
