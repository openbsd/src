/*	$OpenBSD: m8820x_machdep.c,v 1.9 2005/10/13 19:48:33 miod Exp $	*/
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
#include <sys/simplelock.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/cmmu.h>
#include <machine/locore.h>
#include <machine/m8820x.h>

#ifdef DDB
#include <ddb/db_output.h>		/* db_printf()		*/
#endif

/*
 * On some versions of the 88200, page size flushes don't work. I am using
 * sledge hammer approach till I find for sure which ones are bad XXX nivas
 *
 * Looks like 88204 are affected as well... So better keep this -- miod
 */
#define BROKEN_MMU_MASK

void m8820x_cmmu_init(void);
void m8820x_cpu_configuration_print(int);
void m8820x_cmmu_shutdown_now(void);
void m8820x_cmmu_parity_enable(void);
void m8820x_cmmu_set_sapr(unsigned, unsigned);
void m8820x_cmmu_set_uapr(unsigned);
void m8820x_cmmu_flush_tlb(unsigned, unsigned, vaddr_t, u_int);
void m8820x_cmmu_flush_cache(int, paddr_t, psize_t);
void m8820x_cmmu_flush_inst_cache(int, paddr_t, psize_t);
void m8820x_cmmu_flush_data_cache(int, paddr_t, psize_t);
void m8820x_dma_cachectl(pmap_t, vaddr_t, vsize_t, int);
void m8820x_dma_cachectl_pa(paddr_t, psize_t, int);
void m8820x_cmmu_dump_config(void);
void m8820x_cmmu_show_translation(unsigned, unsigned, unsigned, int);
void m8820x_show_apr(unsigned);

/* This is the function table for the mc8820x CMMUs */
struct cmmu_p cmmu8820x = {
	m8820x_cmmu_init,
	m8820x_setup_board_config,
	m8820x_cpu_configuration_print,
	m8820x_cmmu_shutdown_now,
	m8820x_cmmu_parity_enable,
	m8820x_cmmu_cpu_number,
	m8820x_cmmu_set_sapr,
	m8820x_cmmu_set_uapr,
	m8820x_cmmu_flush_tlb,
	m8820x_cmmu_flush_cache,
	m8820x_cmmu_flush_inst_cache,
	m8820x_cmmu_flush_data_cache,
	m8820x_dma_cachectl,
	m8820x_dma_cachectl_pa,
#ifdef DDB
	m8820x_cmmu_dump_config,
	m8820x_cmmu_show_translation,
#else
	NULL,
	NULL,
#endif
#ifdef DEBUG
	m8820x_show_apr,
#else
	NULL,
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
void m8820x_cmmu_set(int, unsigned, int, int, int, vaddr_t);
void m8820x_cmmu_wait(int);
void m8820x_cmmu_sync_cache(paddr_t, psize_t);
void m8820x_cmmu_sync_inval_cache(paddr_t, psize_t);
void m8820x_cmmu_inval_cache(paddr_t, psize_t);

/* Flags passed to m8820x_cmmu_set() */
#define MODE_VAL		0x01
#define ADDR_VAL		0x02

/*
 * This function is called by the MMU module and pokes values
 * into the CMMU's registers.
 */
void
m8820x_cmmu_set(int reg, unsigned val, int flags, int cpu, int mode,
    vaddr_t addr)
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
	"Unknown (0)",
	"Unknown (1)",
	"Unknown (2)",
	"Unknown (3)",
	"Unknown (4)",
	"M88200 (16K)",
	"M88204 (64K)",
	"Unknown (7)"
};

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
*/
void
m8820x_cpu_configuration_print(int master)
{
	struct m8820x_cmmu *cmmu;
	int pid = read_processor_identification_register();
	int proctype = (pid & PID_ARN) >> ARN_SHIFT;
	int procvers = (pid & PID_VN) >> VN_SHIFT;
	int mmu, cnt, cpu = cpu_number();
	struct simplelock print_lock;
#ifdef M88200_HAS_SPLIT_ADDRESS
	int aline, abit, amask;
#endif

	if (master)
		simple_lock_init(&print_lock);

	simple_lock(&print_lock);

	printf("cpu%d: ", cpu);
	if (proctype != ARN_88100) {
		printf("unknown model arch 0x%x rev 0x%x\n",
		    proctype, procvers);
		simple_unlock(&print_lock);
		return;
	}

	printf("M88100 rev 0x%x", procvers);
#if 0	/* not useful yet */
	if (max_cpus > 1)
		printf(", %s", master ? "master" : "slave");
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

		if (mmutypes[mmuid][0] == 'U')
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
				if ((cmmu->cmmu_addr & (1 << abit)) != 0)
					printf("%cA%02d",
					    aline != 0 ? '/' : ' ', abit);
				else
					printf("%cA%02d*",
					    aline != 0 ? '/' : ' ', abit);
				amask ^= 1 << abit;
			}
		} else
#endif
			printf(" full");
		printf(" %ccache", CMMU_MODE(mmu) == INST_CMMU ? 'I' : 'D');
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

	simple_unlock(&print_lock);
}

/*
 * CMMU initialization routine
 */
void
m8820x_cmmu_init()
{
	struct m8820x_cmmu *cmmu;
	unsigned int line, cmmu_num;
	int cssp, cpu, type;
	u_int32_t apr;

	cmmu = m8820x_cmmu;
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++, cmmu++) {
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
		 * Set the SCTR, SAPR, and UAPR to some known state
		 */
		cmmu->cmmu_regs[CMMU_SCTR] &=
		    ~(CMMU_SCTR_PE | CMMU_SCTR_SE | CMMU_SCTR_PR);
		cmmu->cmmu_regs[CMMU_SAPR] = cmmu->cmmu_regs[CMMU_UAPR] =
		    ((0x00000 << PG_BITS) | CACHE_WT | CACHE_GLOBAL |
		    CACHE_INH) & ~APR_V;

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
	 * Enable snooping on multiprocessor systems.
	 * Snooping is enabled for instruction cmmus as well so that
	 * we can share breakpoints.
	 */
	if (max_cpus > 1) {
		for (cpu = 0; cpu < max_cpus; cpu++) {
			m8820x_cmmu_set(CMMU_SCTR, CMMU_SCTR_SE, MODE_VAL, cpu,
			    DATA_CMMU, 0);
			m8820x_cmmu_set(CMMU_SCTR, CMMU_SCTR_SE, MODE_VAL, cpu,
			    INST_CMMU, 0);

			m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL,
			    0, cpu, 0, 0);
			m8820x_cmmu_wait(cpu);
			/* Icache gets flushed just below */
		}
	}

	/*
	 * Enable instruction cache.
	 * Data cache can not be enabled at this point, because some device
	 * addresses can never be cached, and the no-caching zones are not
	 * set up yet.
	 */
	for (cpu = 0; cpu < max_cpus; cpu++) {
		apr = ((0x00000 << PG_BITS) | CACHE_WT | CACHE_GLOBAL)
		    & ~(CACHE_INH | APR_V);

		m8820x_cmmu_set(CMMU_SAPR, apr, MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL,
		    0, cpu, 0, 0);
		m8820x_cmmu_wait(cpu);
	}
}

/*
 * Just before poweroff or reset....
 */
void
m8820x_cmmu_shutdown_now()
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

/*
 * enable parity
 */
void
m8820x_cmmu_parity_enable()
{
	unsigned cmmu_num;
	struct m8820x_cmmu *cmmu;

	cmmu = m8820x_cmmu;
	CMMU_LOCK;
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++, cmmu++)
		cmmu->cmmu_regs[CMMU_SCTR] |= CMMU_SCTR_PE;
	CMMU_UNLOCK;
}

void
m8820x_cmmu_set_sapr(unsigned cpu, unsigned ap)
{
	CMMU_LOCK;
	m8820x_cmmu_set(CMMU_SAPR, ap, 0, cpu, 0, 0);
	CMMU_UNLOCK;
}

void
m8820x_cmmu_set_uapr(unsigned ap)
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
m8820x_cmmu_flush_tlb(unsigned cpu, unsigned kernel, vaddr_t vaddr, u_int count)
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
m8820x_cmmu_flush_cache(int cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
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
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0, cpu, 0, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

/*
 *	flush Instruction caches
 */
void
m8820x_cmmu_flush_inst_cache(int cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
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
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_cmmu_flush_data_cache(int cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
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
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

/*
 * sync dcache - icache is never dirty but needs to be invalidated as well.
 */
void
m8820x_cmmu_sync_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_SEGMENT,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_cmmu_sync_inval_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_LINE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, physaddr);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_cmmu_inval_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, 0,
		    cpu, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_LINE,
		    ADDR_VAL, cpu, 0, physaddr);
	} else if (size <= PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_PAGE,
		    ADDR_VAL, cpu, 0, physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, physaddr,
		    ADDR_VAL, cpu, 0, physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    ADDR_VAL, cpu, 0, physaddr);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, 0,
	    cpu, 0, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_dma_cachectl(pmap_t pmap, vaddr_t va, vsize_t size, int op)
{
	paddr_t pa;
#if !defined(BROKEN_MMU_MASK)
	psize_t count;
#endif

	size = round_page(va + size) - trunc_page(va);
	va = trunc_page(va);

#if !defined(BROKEN_MMU_MASK)
	while (size != 0) {
		count = min(size, PAGE_SIZE);

		if (pmap_extract(pmap, va, &pa) != FALSE) {
			switch (op) {
			case DMA_CACHE_SYNC:
				m8820x_cmmu_sync_cache(pa, count);
				break;
			case DMA_CACHE_SYNC_INVAL:
				m8820x_cmmu_sync_inval_cache(pa, count);
				break;
			default:
				m8820x_cmmu_inval_cache(pa, count);
				break;
			}
		}

		va += count;
		size -= count;
	}
#else
	/*
	 * This assumes the space is also physically contiguous... but this
	 * really doesn't matter as we flush/sync/invalidate the whole cache
	 * anyway...
	 */
	if (pmap_extract(pmap, va, &pa) != FALSE) {
		switch (op) {
		case DMA_CACHE_SYNC:
			m8820x_cmmu_sync_cache(pa, size);
			break;
		case DMA_CACHE_SYNC_INVAL:
			m8820x_cmmu_sync_inval_cache(pa, size);
			break;
		default:
			m8820x_cmmu_inval_cache(pa, size);
			break;
		}
	}
#endif /* !BROKEN_MMU_MASK */
}

void
m8820x_dma_cachectl_pa(paddr_t pa, psize_t size, int op)
{
#if !defined(BROKEN_MMU_MASK)
	psize_t count;
#endif

	size = round_page(pa + size) - trunc_page(pa);
	pa = trunc_page(pa);

#if !defined(BROKEN_MMU_MASK)
	while (size != 0) {
		count = min(size, PAGE_SIZE);

		switch (op) {
		case DMA_CACHE_SYNC:
			m8820x_cmmu_sync_cache(pa, count);
			break;
		case DMA_CACHE_SYNC_INVAL:
			m8820x_cmmu_sync_inval_cache(pa, count);
			break;
		default:
			m8820x_cmmu_inval_cache(pa, count);
			break;
		}

		pa += count;
		size -= count;
	}
#else
	switch (op) {
	case DMA_CACHE_SYNC:
		m8820x_cmmu_sync_cache(pa, size);
		break;
	case DMA_CACHE_SYNC_INVAL:
		m8820x_cmmu_sync_inval_cache(pa, size);
		break;
	default:
		m8820x_cmmu_inval_cache(pa, size);
		break;
	}
#endif /* !BROKEN_MMU_MASK */
}

#ifdef DDB
void
m8820x_cmmu_dump_config()
{
	struct m8820x_cmmu *cmmu;
	int cmmu_num;

	db_printf("Current CPU/CMMU configuration:\n");
	cmmu = m8820x_cmmu;
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++, cmmu++) {
#ifdef M88200_HAS_SPLIT_ADDRESS
		db_printf("CMMU #%d: %s CMMU for CPU %d, addr 0x%08lx mask 0x%08lx\n",
		    cmmu_num,
		    CMMU_MODE(cmmu_num) == INST_CMMU ? "inst" : "data",
		    cmmu_num >> cmmu_shift,
		    cmmu->cmmu_addr, cmmu->cmmu_addr_mask);
#else
		db_printf("CMMU #%d: %s CMMU for CPU %d",
		    cmmu_num,
		    CMMU_MODE(cmmu_num) == INST_CMMU ? "inst" : "data",
		    cmmu_num >> cmmu_shift);
#endif
	}
}

/*
 * Show (for debugging) how the current CPU translates the given ADDRESS
 * (as DATA).
 */
void
m8820x_cmmu_show_translation(unsigned address, unsigned supervisor_flag,
    unsigned verbose_flag, int unused __attribute__ ((unused)))
{
	struct m8820x_cmmu *cmmu;
	int cpu = cpu_number();
	vaddr_t va = address;
	int cmmu_num, cnt;
	u_int32_t value;

	/*
	 * Find the correct data CMMU.
	 */
	cmmu_num = cpu << cmmu_shift;
	cmmu = m8820x_cmmu + cmmu_num;
	for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, cmmu_num++, cmmu++) {
		if (CMMU_MODE(cmmu_num) == INST_CMMU)
			continue;
#ifdef M88200_HAS_SPLIT_ADDRESS
		if (cmmu->cmmu_addr_mask == 0 ||
		    (va & cmmu->cmmu_addr_mask) ==
		     cmmu->cmmu_addr)
#endif
			break;
	}
	if (cnt == 0) {
		db_printf("No matching cmmu for VA %08x\n", address);
		return;
	}

	if (verbose_flag != 0)
		db_printf("VA %08x is managed by CMMU#%d.\n",
		    address, cmmu_num);

	/*
	 * Perform some sanity checks.
	 */
	if (verbose_flag == 0) {
		if ((cmmu->cmmu_regs[CMMU_SCTR] &
		    CMMU_SCTR_SE) == 0)
			db_printf("WARNING: snooping not enabled for CMMU#%d.\n",
			    cmmu_num);
	} else {
		int i;

		cmmu = m8820x_cmmu;
		for (i = 0; i < max_cmmus; i++, cmmu++)
			if (verbose_flag > 1 ||
			    (cmmu->cmmu_regs[CMMU_SCTR] & CMMU_SCTR_SE) == 0) {
				db_printf("CMMU#%d (cpu %d %s) snooping %s\n",
				    i, i >> cmmu_shift,
				    CMMU_MODE(i) == INST_CMMU ? "inst" : "data",
				    (cmmu->cmmu_regs[CMMU_SCTR] &
				     CMMU_SCTR_SE) ? "on" : "OFF");
			}
		cmmu = m8820x_cmmu + cmmu_num;
	}

	/*
	 * Ask for a CMMU probe and report its result.
	 */
	{
		u_int32_t ssr;

		cmmu->cmmu_regs[CMMU_SAR] = address;
		cmmu->cmmu_regs[CMMU_SCR] =
		    supervisor_flag ? CMMU_PROBE_SUPER : CMMU_PROBE_USER;
		ssr = cmmu->cmmu_regs[CMMU_SSR];

		switch (verbose_flag) {
		case 2:
			db_printf("probe of 0x%08x returns ssr=0x%08x\n",
			    address, ssr);
			/* FALLTHROUGH */
		case 1:
			if (ssr & CMMU_SSR_V)
				db_printf("PROBE of 0x%08x returns phys=0x%x",
				    address, cmmu->cmmu_regs[CMMU_SAR]);
			else
				db_printf("PROBE fault at 0x%x",
				    cmmu->cmmu_regs[CMMU_PFAR]);
			if (ssr & CMMU_SSR_CE)
				db_printf(", copyback err");
			if (ssr & CMMU_SSR_BE)
				db_printf(", bus err");
			if (ssr & CACHE_WT)
				db_printf(", writethrough");
			if (ssr & CMMU_SSR_SO)
				db_printf(", sup prot");
			if (ssr & CACHE_GLOBAL)
				db_printf(", global");
			if (ssr & CACHE_INH)
				db_printf(", cache inhibit");
			if (ssr & CMMU_SSR_M)
				db_printf(", modified");
			if (ssr & CMMU_SSR_U)
				db_printf(", used");
			if (ssr & CMMU_SSR_PROT)
				db_printf(", write prot");
			if (ssr & CMMU_SSR_BH)
				db_printf(", BATC");
			db_printf(".\n");
			break;
		}
	}

	/*
	 * Interpret area descriptor.
	 */

	if (supervisor_flag)
		value = cmmu->cmmu_regs[CMMU_SAPR];
	else
		value = cmmu->cmmu_regs[CMMU_UAPR];

	switch (verbose_flag) {
	case 2:
		db_printf("CMMU#%d", cmmu_num);
		db_printf(" %cAPR is 0x%08x\n",
		    supervisor_flag ? 'S' : 'U', value);
		/* FALLTHROUGH */
	case 1:
		db_printf("CMMU#%d", cmmu_num);
		db_printf(" %cAPR: SegTbl: 0x%x000p",
		    supervisor_flag ? 'S' : 'U', PG_PFNUM(value));
		if (value & CACHE_WT)
			db_printf(", WTHRU");
		if (value & CACHE_GLOBAL)
			db_printf(", GLOBAL");
		if (value & CACHE_INH)
			db_printf(", INHIBIT");
		if (value & APR_V)
			db_printf(", VALID");
		db_printf("\n");
		break;
	}

	if ((value & APR_V) == 0) {
		db_printf("VA 0x%08x -> apr 0x%08x not valid\n", va, value);
		return;
	}

	value &= PG_FRAME;	/* now point to seg page */

	/*
	 * Walk segment and page tables to find our page.
	 */
	{
		sdt_entry_t sdt;

		if (verbose_flag)
			db_printf("will follow to entry %d of page at 0x%x...\n",
			    SDTIDX(va), value);
		value |= SDTIDX(va) * sizeof(sdt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			db_printf("VA 0x%08x -> segment table @0x%08x not accessible\n",
			    va, value);
			return;
		}

		sdt = *(sdt_entry_t *)value;
		switch (verbose_flag) {
		case 2:
			db_printf("SEG DESC @0x%x is 0x%08x\n", value, sdt);
			/* FALLTHROUGH */
		case 1:
			db_printf("SEG DESC @0x%x: PgTbl: 0x%x000",
			    value, PG_PFNUM(sdt));
			if (sdt & CACHE_WT)
				db_printf(", WTHRU");
			if (sdt & SG_SO)
				db_printf(", S-PROT");
			if (sdt & CACHE_GLOBAL)
				db_printf(", GLOBAL");
			if (sdt & CACHE_INH)
				db_printf(", $INHIBIT");
			if (sdt & SG_PROT)
				db_printf(", W-PROT");
			if (sdt & SG_V)
				db_printf(", VALID");
			db_printf(".\n");
			break;
		}

		if ((sdt & SG_V) == 0) {
			db_printf("VA 0x%08x -> segment entry 0x%8x @0x%08x not valid\n",
			    va, sdt, value);
			return;
		}

		value = ptoa(PG_PFNUM(sdt));
	}

	{
		pt_entry_t pte;

		if (verbose_flag)
			db_printf("will follow to entry %d of page at 0x%x...\n",
			    PDTIDX(va), value);
		value |= PDTIDX(va) * sizeof(pt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			db_printf("VA 0x%08x -> page table entry @0x%08x not accessible\n",
			    va, value);
			return;
		}

		pte = *(pt_entry_t *)value;
		switch (verbose_flag) {
		case 2:
			db_printf("PAGE DESC @0x%x is 0x%08x.\n", value, pte);
			/* FALLTHROUGH */
		case 1:
			db_printf("PAGE DESC @0x%x: page @%x000",
			    value, PG_PFNUM(pte));
			if (pte & PG_W)
				db_printf(", WIRE");
			if (pte & CACHE_WT)
				db_printf(", WTHRU");
			if (pte & PG_SO)
				db_printf(", S-PROT");
			if (pte & CACHE_GLOBAL)
				db_printf(", GLOBAL");
			if (pte & CACHE_INH)
				db_printf(", $INHIBIT");
			if (pte & PG_M)
				db_printf(", MOD");
			if (pte & PG_U)
				db_printf(", USED");
			if (pte & PG_PROT)
				db_printf(", W-PROT");
			if (pte & PG_V)
				db_printf(", VALID");
			db_printf(".\n");
			break;
		}

		if ((pte & PG_V) == 0) {
			db_printf("VA 0x%08x -> page table entry 0x%08x @0x%08x not valid\n",
			    va, pte, value);
			return;
		}

		value = ptoa(PG_PFNUM(pte)) | (va & PAGE_MASK);
	}

	db_printf("VA 0x%08x -> PA 0x%08x\n", va, value);
}
#endif /* DDB */

#ifdef DEBUG
void
m8820x_show_apr(unsigned value)
{
	printf("table @ 0x%x000", PG_PFNUM(value));
	if (value & CACHE_WT)
		printf(", writethrough");
	if (value & CACHE_GLOBAL)
		printf(", global");
	if (value & CACHE_INH)
		printf(", cache inhibit");
	if (value & APR_V)
		printf(", valid");
	printf("\n");
}
#endif
