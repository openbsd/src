/*	$OpenBSD: m8820x_machdep.c,v 1.19 2005/12/21 22:15:24 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/locore.h>
#include <machine/lock.h>
#include <machine/m8820x.h>
#include <machine/psl.h>

cpuid_t	m8820x_init(void);
void	m8820x_cpu_configuration_print(int);
void	m8820x_shutdown(void);
void	m8820x_set_sapr(cpuid_t, apr_t);
void	m8820x_set_uapr(apr_t);
void	m8820x_flush_tlb(cpuid_t, u_int, vaddr_t, u_int);
void	m8820x_flush_cache(cpuid_t, paddr_t, psize_t);
void	m8820x_flush_inst_cache(cpuid_t, paddr_t, psize_t);
void	m8820x_flush_data_cache(cpuid_t, paddr_t, psize_t);
int	m8820x_dma_cachectl(pmap_t, vaddr_t, vsize_t, int);
int	m8820x_dma_cachectl_pa(paddr_t, psize_t, int);
void	m8820x_initialize_cpu(cpuid_t);

/* This is the function table for the MC8820x CMMUs */
struct cmmu_p cmmu8820x = {
	m8820x_init,
	m8820x_setup_board_config,
	m8820x_cpu_configuration_print,
	m8820x_shutdown,
	m8820x_cpu_number,
	m8820x_set_sapr,
	m8820x_set_uapr,
	m8820x_flush_tlb,
	m8820x_flush_cache,
	m8820x_flush_inst_cache,
	m8820x_flush_data_cache,
	m8820x_dma_cachectl,
	m8820x_dma_cachectl_pa,
#ifdef MULTIPROCESSOR
	m8820x_initialize_cpu,
#endif
};

/*
 * Systems with more than 2 CMMUs per CPU use programmable split schemes.
 *
 * The following schemes are available on MVME188 boards:
 * - split on A12 address bit (A14 for 88204)
 * - split on supervisor/user access
 * - split on SRAM/non-SRAM addresses, with either supervisor-only or all
 *   access to SRAM.
 *
 * MVME188 configuration 6, with 4 CMMUs par CPU, also forces a split on
 * A14 address bit (A16 for 88204).
 *
 * Under OpenBSD, we will only split on A12 and A14 address bits, since we
 * do not want to waste CMMU resources on the SRAM, and user/supervisor
 * splits seem less efficient.
 *
 * The really nasty part of this choice is in the exception handling code,
 * when it needs to get error information from up to 4 CMMUs. See eh.S on
 * mvme88k for the gory details, luna88k is more sane.
 */

struct m8820x_cmmu m8820x_cmmu[MAX_CMMUS];
u_int max_cmmus;
u_int cmmu_shift;

/* local prototypes */
void	m8820x_cmmu_set(int, u_int, int, int, int, vaddr_t);
void	m8820x_cmmu_wait(int);
int	m8820x_cmmu_sync_cache(paddr_t, psize_t);
int	m8820x_cmmu_sync_inval_cache(paddr_t, psize_t);
int	m8820x_cmmu_inval_cache(paddr_t, psize_t);

/* Flags passed to m8820x_cmmu_set() */
#define MODE_VAL		0x01
#define ADDR_VAL		0x02

/*
 * This function is called by the MMU module and pokes values
 * into the CMMU's registers.
 */
void
m8820x_cmmu_set(int reg, u_int val, int flags, int cpu, int mode, vaddr_t addr)
{
	struct m8820x_cmmu *cmmu;
	int mmu, cnt;

	mmu = cpu << cmmu_shift;
	cmmu = m8820x_cmmu + mmu;

	/*
	 * We scan all CMMUs to find the matching ones and store the
	 * values there.
	 */
	for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, mmu++, cmmu++) {
		if ((flags & MODE_VAL) != 0) {
			if (CMMU_MODE(mmu) != mode)
				continue;
		}
#ifdef M88200_HAS_SPLIT_ADDRESS
		if ((flags & ADDR_VAL) != 0) {
			if (cmmu->cmmu_addr_mask != 0 &&
			    (addr & cmmu->cmmu_addr_mask) != cmmu->cmmu_addr)
				continue;
		}
#endif
		cmmu->cmmu_regs[reg] = val;
	}
}

/*
 * Force a read from the CMMU status register, thereby forcing execution to
 * stop until all pending CMMU operations are finished.
 * This is used by the various cache invalidation functions.
 */
void
m8820x_cmmu_wait(int cpu)
{
	struct m8820x_cmmu *cmmu;
	int mmu, cnt;

	mmu = cpu << cmmu_shift;
	cmmu = m8820x_cmmu + mmu;

	/*
	 * We scan all related CMMUs and read their status register.
	 */
	for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, mmu++, cmmu++) {
#ifdef DEBUG
		if (cmmu->cmmu_regs[CMMU_SSR] & CMMU_SSR_BE) {
			panic("cache flush failed!");
		}
#else
		/* force the read access, but do not issue this statement... */
		__asm__ __volatile__ ("|or r0, r0, %0" ::
		    "r" (cmmu->cmmu_regs[CMMU_SSR]));
#endif
	}
}

const char *mmutypes[8] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"M88200 (16K)",
	"M88204 (64K)",
	NULL
};

/*
 * Should only be called after the calling cpus knows its cpu
 * number and main/secondary status. Should be called first
 * by the main processor, before the others are started.
*/
void
m8820x_cpu_configuration_print(int main)
{
	struct m8820x_cmmu *cmmu;
	int pid = get_cpu_pid();
	int proctype = (pid & PID_ARN) >> ARN_SHIFT;
	int procvers = (pid & PID_VN) >> VN_SHIFT;
	int mmu, cnt, cpu = cpu_number();
#ifdef M88200_HAS_SPLIT_ADDRESS
	int aline, abit, amask;
#endif

	printf("cpu%d: ", cpu);
	switch (proctype) {
	default:
		printf("unknown model arch 0x%x rev 0x%x",
		    proctype, procvers);
		break;
	case ARN_88100:
		printf("M88100 rev 0x%x", procvers);
#ifdef MULTIPROCESSOR
		if (main == 0)
			printf(", secondary");
#endif
		printf(", %d CMMU", 1 << cmmu_shift);

		mmu = cpu << cmmu_shift;
		cmmu = m8820x_cmmu + mmu;
		for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, mmu++, cmmu++) {
			int idr = cmmu->cmmu_regs[CMMU_IDR];
			int mmuid = CMMU_TYPE(idr);

			if (mmu % 2 == 0)
				printf("\ncpu%d: ", cpu);
			else
				printf(", ");

			if (mmutypes[mmuid] == NULL)
				printf("unknown model id 0x%x", mmuid);
			else
				printf("%s", mmutypes[mmuid]);
			printf(" rev 0x%x,", CMMU_VERSION(idr));
#ifdef M88200_HAS_SPLIT_ADDRESS
			/*
			 * Print address lines
			 */
			amask = cmmu->cmmu_addr_mask;
			if (amask != 0) {
				aline = 0;
				while (amask != 0) {
					abit = ff1(amask);
					if ((cmmu->cmmu_addr &
					    (1 << abit)) != 0)
						printf("%cA%02d",
						    aline != 0 ? '/' : ' ',
						    abit);
					else
						printf("%cA%02d*",
						    aline != 0 ? '/' : ' ',
						    abit);
					amask ^= 1 << abit;
				}
			} else
#endif
				printf(" full");
			printf(" %ccache",
			    CMMU_MODE(mmu) == INST_CMMU ? 'I' : 'D');
		}
		break;
	}
	printf("\n");

#ifndef ERRATA__XXX_USR
	{
		static int errata_warn = 0;

		if (proctype == ARN_88100 && procvers < 2) {
			if (!errata_warn++)
				printf("WARNING: M88100 bug workaround code "
				    "not enabled.\nPlease recompile the kernel "
				    "with option ERRATA__XXX_USR !\n");
		}
	}
#endif
}

/*
 * CMMU initialization routine
 */
cpuid_t
m8820x_init()
{
	cpuid_t cpu;

	cpu = m8820x_cpu_number();
	m8820x_initialize_cpu(cpu);
	return (cpu);
}

/*
 * Initialize the set of CMMUs tied to a particular CPU.
 */
void
m8820x_initialize_cpu(cpuid_t cpu)
{
	struct m8820x_cmmu *cmmu;
	u_int line, cnt;
	int cssp, sctr, type;
	apr_t apr;

	apr = ((0x00000 << PG_BITS) | CACHE_WT | CACHE_GLOBAL | CACHE_INH) &
	    ~APR_V;

	cmmu = m8820x_cmmu + (cpu << cmmu_shift);
	for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, cmmu++) {
		type = CMMU_TYPE(cmmu->cmmu_regs[CMMU_IDR]);

		/*
		 * Reset cache
		 */
		for (cssp = type == M88204_ID ? 3 : 0; cssp >= 0; cssp--)
			for (line = 0; line <= 255; line++) {
				cmmu->cmmu_regs[CMMU_SAR] =
				    line << MC88200_CACHE_SHIFT;
				cmmu->cmmu_regs[CMMU_CSSP(cssp)] =
				    CMMU_CSSP_L5 | CMMU_CSSP_L4 |
				    CMMU_CSSP_L3 | CMMU_CSSP_L2 |
				    CMMU_CSSP_L1 | CMMU_CSSP_L0 |
				    CMMU_CSSP_VV(3, CMMU_VV_INVALID) |
				    CMMU_CSSP_VV(2, CMMU_VV_INVALID) |
				    CMMU_CSSP_VV(1, CMMU_VV_INVALID) |
				    CMMU_CSSP_VV(0, CMMU_VV_INVALID);
			}

		/*
		 * Set the SCTR, SAPR, and UAPR to some known state.
		 * Snooping is enabled on multiprocessor systems; for
		 * instruction CMMUs as well so that we can share breakpoints.
		 * XXX Investigate why enabling parity at this point
		 * doesn't work.
		 */
		sctr = cmmu->cmmu_regs[CMMU_SCTR] &
		    ~(CMMU_SCTR_PE | CMMU_SCTR_SE | CMMU_SCTR_PR);
#ifdef MULTIPROCESSOR
		if (max_cpus > 1)
			sctr |= CMMU_SCTR_SE;
#endif
		cmmu->cmmu_regs[CMMU_SCTR] = sctr;

		cmmu->cmmu_regs[CMMU_SAPR] = cmmu->cmmu_regs[CMMU_UAPR] = apr;

		cmmu->cmmu_regs[CMMU_BWP0] = cmmu->cmmu_regs[CMMU_BWP1] =
		cmmu->cmmu_regs[CMMU_BWP2] = cmmu->cmmu_regs[CMMU_BWP3] =
		cmmu->cmmu_regs[CMMU_BWP4] = cmmu->cmmu_regs[CMMU_BWP5] =
		cmmu->cmmu_regs[CMMU_BWP6] = cmmu->cmmu_regs[CMMU_BWP7] = 0;
		cmmu->cmmu_regs[CMMU_SCR] = CMMU_FLUSH_CACHE_INV_ALL;
		__asm__ __volatile__ ("|or r0, r0, %0" ::
		    "r" (cmmu->cmmu_regs[CMMU_SSR]));
		cmmu->cmmu_regs[CMMU_SCR] = CMMU_FLUSH_SUPER_ALL;
		cmmu->cmmu_regs[CMMU_SCR] = CMMU_FLUSH_USER_ALL;
	}

	/*
	 * Enable instruction cache.
	 * Data cache will be enabled later.
	 */
	apr &= ~CACHE_INH;
	m8820x_cmmu_set(CMMU_SAPR, apr, MODE_VAL, cpu, INST_CMMU, 0);
}

/*
 * Just before poweroff or reset....
 */
void
m8820x_shutdown()
{
	unsigned cmmu_num;
	struct m8820x_cmmu *cmmu;

	CMMU_LOCK;
	cmmu = m8820x_cmmu;
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++, cmmu++) {
		cmmu->cmmu_regs[CMMU_SCTR] &=
		    ~(CMMU_SCTR_PE | CMMU_SCTR_SE | CMMU_SCTR_PR);
		cmmu->cmmu_regs[CMMU_SAPR] = cmmu->cmmu_regs[CMMU_UAPR] =
		    ((0x00000 << PG_BITS) | CACHE_INH) &
		    ~(CACHE_WT | CACHE_GLOBAL | APR_V);
	}
	CMMU_UNLOCK;
}

void
m8820x_set_sapr(cpuid_t cpu, apr_t ap)
{
	CMMU_LOCK;
	m8820x_cmmu_set(CMMU_SAPR, ap, 0, cpu, 0, 0);
	CMMU_UNLOCK;
}

void
m8820x_set_uapr(apr_t ap)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;
	m8820x_cmmu_set(CMMU_UAPR, ap, 0, cpu, 0, 0);
	CMMU_UNLOCK;
	splx(s);
}

/*
 * Functions that invalidate TLB entries.
 */

/*
 *	flush any tlb
 */
void
m8820x_flush_tlb(cpuid_t cpu, unsigned kernel, vaddr_t vaddr, u_int count)
{
	int s = splhigh();

	CMMU_LOCK;

	/*
	 * Since segment operations are horribly expensive, don't
	 * do any here. Invalidations of up to three pages are performed
	 * as page invalidations, otherwise the entire tlb is flushed.
	 *
	 * Note that this code relies upon vaddr being page-aligned.
	 */
	switch (count) {
	default:
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_ALL : CMMU_FLUSH_USER_ALL,
		    0, cpu, 0, 0);
		break;
	case 3:
		m8820x_cmmu_set(CMMU_SAR, vaddr, ADDR_VAL, cpu, 0, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL, cpu, 0, vaddr);
		vaddr += PAGE_SIZE;
		/* FALLTHROUGH */
	case 2:
		m8820x_cmmu_set(CMMU_SAR, vaddr, ADDR_VAL, cpu, 0, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL, cpu, 0, vaddr);
		vaddr += PAGE_SIZE;
		/* FALLTHROUGH */
	case 1:			/* most frequent situation */
	case 0:
		m8820x_cmmu_set(CMMU_SAR, vaddr, ADDR_VAL, cpu, 0, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL, cpu, 0, vaddr);
		break;
	}

	CMMU_UNLOCK;
	splx(s);
}

/*
 * Functions that invalidate caches.
 *
 * Cache invalidates require physical addresses.  Care must be exercised when
 * using segment invalidates.  This implies that the starting physical address
 * plus the segment length should be invalidated.  A typical mistake is to
 * extract the first physical page of a segment from a virtual address, and
 * then expecting to invalidate when the pages are not physically contiguous.
 *
 * We don't push Instruction Caches prior to invalidate because they are not
 * snooped and never modified (I guess it doesn't matter then which form
 * of the command we use then).
 */

/*
 *	flush both Instruction and Data caches
 */
void
m8820x_flush_cache(cpuid_t cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0,
		    cpu, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr, ADDR_VAL,
		    cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE, ADDR_VAL,
		    cpu, 0, physaddr);
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr, ADDR_VAL,
		    cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE, ADDR_VAL,
		    cpu, 0, physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr, 0,
		    cpu, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT, 0,
		    cpu, 0, 0);
	}

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

/*
 *	flush Instruction caches
 */
void
m8820x_flush_inst_cache(cpuid_t cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0);
	}

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_flush_data_cache(cpuid_t cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL, cpu, DATA_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	}

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

/*
 * sync dcache - icache is never dirty but needs to be invalidated as well.
 */
int
m8820x_cmmu_sync_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();
	int rc;

	CMMU_LOCK;

	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
		rc = 1;
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		rc = 0;
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		rc = 0;
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_SEGMENT,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		rc = 0;
	}

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
	return (rc);
}

int
m8820x_cmmu_sync_inval_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();
	int rc;

	CMMU_LOCK;

	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
		rc = 1;
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_LINE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		rc = 0;
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		rc = 0;
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		rc = 0;
	}

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
	return (rc);
}

int
m8820x_cmmu_inval_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();
	int rc;

	CMMU_LOCK;

	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, 0,
		    cpu, 0, 0);
		rc = 1;
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_LINE,
		    ADDR_VAL, cpu, 0, physaddr);
		rc = 0;
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_PAGE,
		    ADDR_VAL, cpu, 0, physaddr);
		rc = 0;
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    ADDR_VAL, cpu, 0, physaddr);
		rc = 0;
	}

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
	return (rc);
}

int
m8820x_dma_cachectl(pmap_t pmap, vaddr_t va, vsize_t size, int op)
{
	paddr_t pa;
	psize_t count;
	int rc = 0;

	size = round_page(va + size) - trunc_page(va);
	va = trunc_page(va);

	while (size != 0 && rc == 0) {
		count = min(size, PAGE_SIZE);

		if (pmap_extract(pmap, va, &pa) != FALSE) {
			switch (op) {
			case DMA_CACHE_SYNC:
				rc |= m8820x_cmmu_sync_cache(pa, count);
				break;
			case DMA_CACHE_SYNC_INVAL:
				rc |= m8820x_cmmu_sync_inval_cache(pa, count);
				break;
			default:
				rc |= m8820x_cmmu_inval_cache(pa, count);
				break;
			}
		}

		va += count;
		size -= count;
	}
	return (rc);
}

int
m8820x_dma_cachectl_pa(paddr_t pa, psize_t size, int op)
{
	psize_t count;
	int rc = 0;

	size = round_page(pa + size) - trunc_page(pa);
	pa = trunc_page(pa);

	while (size != 0 && rc == 0) {
		count = min(size, PAGE_SIZE);

		switch (op) {
		case DMA_CACHE_SYNC:
			rc |= m8820x_cmmu_sync_cache(pa, count);
			break;
		case DMA_CACHE_SYNC_INVAL:
			rc |= m8820x_cmmu_sync_inval_cache(pa, count);
			break;
		default:
			rc |= m8820x_cmmu_inval_cache(pa, count);
			break;
		}

		pa += count;
		size -= count;
	}
	return (rc);
}
