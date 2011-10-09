/*	$OpenBSD: m8820x_machdep.c,v 1.48 2011/10/09 17:01:34 miod Exp $	*/
/*
 * Copyright (c) 2004, 2007, 2010, 2011, Miodrag Vallat.
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
#include <machine/lock.h>
#include <machine/m8820x.h>
#include <machine/psl.h>

extern	void m8820x_zeropage(vaddr_t);
extern	void m8820x_copypage(vaddr_t, vaddr_t);

cpuid_t	m8820x_init(void);
void	m8820x_cpu_configuration_print(int);
void	m8820x_shutdown(void);
void	m8820x_set_sapr(apr_t);
void	m8820x_set_uapr(apr_t);
void	m8820x_tlb_inv(cpuid_t, u_int, vaddr_t);
void	m8820x_tlb_inv_all(cpuid_t);
void	m8820x_cache_wbinv(cpuid_t, paddr_t, psize_t);
void	m8820x_dcache_wb(cpuid_t, paddr_t, psize_t);
void	m8820x_icache_inv(cpuid_t, paddr_t, psize_t);
void	m8820x_dma_cachectl(paddr_t, psize_t, int);
void	m8820x_dma_cachectl_local(paddr_t, psize_t, int);
void	m8820x_initialize_cpu(cpuid_t);

struct cmmu_p cmmu8820x = {
	m8820x_init,
	m8820x_setup_board_config,
	m8820x_cpu_configuration_print,
	m8820x_shutdown,
	m8820x_cpu_number,
	m8820x_set_sapr,
	m8820x_set_uapr,
	m8820x_tlb_inv,
	m8820x_tlb_inv_all,
	m8820x_cache_wbinv,
	m8820x_dcache_wb,
	m8820x_icache_inv,
	m8820x_dma_cachectl,
#ifdef MULTIPROCESSOR
	m8820x_dma_cachectl_local,
	m8820x_initialize_cpu,
#endif
};

/*
 * Systems with more than 2 CMMUs per CPU use split schemes, which sometimes
 * are programmable (well, no more than having a few hardwired choices).
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
 * when it needs to get error information from up to 4 CMMUs. See eh.S for
 * the gory details.
 */

struct m8820x_cmmu m8820x_cmmu[MAX_CMMUS];
u_int max_cmmus;
u_int cmmu_shift;

/* local prototypes */
void	m8820x_cmmu_set_reg(int, u_int, int, int, int);
void	m8820x_cmmu_set_cmd(u_int, int, int, int, vaddr_t);
void	m8820x_cmmu_wait(int);
void	m8820x_cmmu_wb_locked(int, paddr_t, psize_t);
void	m8820x_cmmu_wbinv_locked(int, paddr_t, psize_t);
void	m8820x_cmmu_inv_locked(int, paddr_t, psize_t);

/* Flags passed to m8820x_cmmu_set_*() */
#define MODE_VAL		0x01
#define ADDR_VAL		0x02

/*
 * Helper functions to poke values into the appropriate CMMU registers.
 */

void
m8820x_cmmu_set_reg(int reg, u_int val, int flags, int cpu, int mode)
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
#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
		if (cmmu->cmmu_regs == NULL)
			continue;
#endif
		if ((flags & MODE_VAL) != 0) {
			if (CMMU_MODE(mmu) != mode)
				continue;
		}
		cmmu->cmmu_regs[reg] = val;
	}
}

void
m8820x_cmmu_set_cmd(u_int cmd, int flags, int cpu, int mode, vaddr_t addr)
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
#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
		if (cmmu->cmmu_regs == NULL)
			continue;
#endif
		if ((flags & MODE_VAL) != 0) {
			if (CMMU_MODE(mmu) != mode)
				continue;
		}
#ifdef M88200_HAS_SPLIT_ADDRESS
		if ((flags & ADDR_VAL) != 0 && cmmu->cmmu_addr_mask != 0) {
			if ((addr & cmmu->cmmu_addr_mask) != cmmu->cmmu_addr)
				continue;
		}
#endif
		cmmu->cmmu_regs[CMMU_SAR] = addr;
		cmmu->cmmu_regs[CMMU_SCR] = cmd;
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
#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
		if (cmmu->cmmu_regs == NULL)
			continue;
#endif
#ifdef DEBUG
		if (cmmu->cmmu_regs[CMMU_SSR] & CMMU_SSR_BE) {
			panic("cache flush failed!");
		}
#else
		(void)cmmu->cmmu_regs[CMMU_SSR];
#endif
	}
}

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
	int reported, nmmu, mmu, cnt, cpu = cpu_number();
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
		nmmu = 1 << cmmu_shift;
#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
		mmu = cpu << cmmu_shift;
		cmmu = m8820x_cmmu + mmu;
		for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, mmu++, cmmu++)
			if (cmmu->cmmu_regs == NULL)
				nmmu--;
#endif
		printf(", %d CMMU", nmmu);

		mmu = cpu << cmmu_shift;
		cmmu = m8820x_cmmu + mmu;
		reported = 0;
		for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, mmu++, cmmu++) {
			int idr, mmuid;

#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
			if (cmmu->cmmu_regs == NULL)
				continue;
#endif

			idr = cmmu->cmmu_regs[CMMU_IDR];
			mmuid = CMMU_TYPE(idr);

			if (reported++ % 2 == 0)
				printf("\ncpu%d: ", cpu);
			else
				printf(", ");

			switch (mmuid) {
			case M88200_ID:
				printf("M88200 (16K)");
				break;
			case M88204_ID:
				printf("M88204 (64K)");
				break;
			default:
				printf("unknown CMMU id 0x%x", mmuid);
				break;
			}
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
			} else if (cmmu_shift != 1) {
				/* unknown split scheme */
				printf(" split");
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

		if (proctype == ARN_88100 && procvers <= 10) {
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
	struct cpu_info *ci;
	struct m8820x_cmmu *cmmu;
	u_int line, cnt;
	int cssp, type;
	apr_t apr;

	apr = ((0x00000 << PG_BITS) | CACHE_GLOBAL | CACHE_INH) & ~APR_V;

	cmmu = m8820x_cmmu + (cpu << cmmu_shift);

	/*
	 * Setup CMMU pointers for faster exception processing.
	 * This relies on the board-dependent code putting instruction
	 * CMMUs and data CMMUs interleaved with instruction CMMUs first.
	 */
	ci = &m88k_cpus[cpu];
	switch (cmmu_shift) {
	default:
		/* exception code may not use ci_pfsr fields, compute anyway */
		/* FALLTHROUGH */
	case 2:
		ci->ci_pfsr_d1 = (u_int)cmmu[3].cmmu_regs + CMMU_PFSR * 4;
		ci->ci_pfsr_i1 = (u_int)cmmu[2].cmmu_regs + CMMU_PFSR * 4;
		/* FALLTHROUGH */
	case 1:
		ci->ci_pfsr_d0 = (u_int)cmmu[1].cmmu_regs + CMMU_PFSR * 4;
		ci->ci_pfsr_i0 = (u_int)cmmu[0].cmmu_regs + CMMU_PFSR * 4;
		break;
	}

	for (cnt = 1 << cmmu_shift; cnt != 0; cnt--, cmmu++) {
#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
		if (cmmu->cmmu_regs == NULL)
			continue;
#endif
		type = CMMU_TYPE(cmmu->cmmu_regs[CMMU_IDR]);

		/*
		 * Reset cache
		 */
		for (cssp = type == M88204_ID ? 3 : 0; cssp >= 0; cssp--)
			for (line = 0; line <= 255; line++) {
				cmmu->cmmu_regs[CMMU_SAR] =
				    line << MC88200_CACHE_SHIFT;
				if (cmmu->cmmu_regs[CMMU_CSSP(cssp)] &
				    (CMMU_CSSP_D3 | CMMU_CSSP_D2 |
				     CMMU_CSSP_D1 | CMMU_CSSP_D0)) {
					printf("cpu%d: CMMU@%p has disabled"
					    " cache lines in set 0x%03x,"
					    " cssp %08x\n",
					    cpu, cmmu->cmmu_regs,
					    (cssp << 8) | line,
					    cmmu->cmmu_regs[CMMU_CSSP(cssp)]);
				}
				cmmu->cmmu_regs[CMMU_CSSP(cssp)] =
				    (cmmu->cmmu_regs[CMMU_CSSP(cssp)] &
				     ~(CMMU_CSSP_D3 | CMMU_CSSP_D2 |
				       CMMU_CSSP_D1 | CMMU_CSSP_D0)) |
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
		 * Snooping is always enabled, so that we do not need to
		 * writeback userland code pages when they first get filled
		 * as data pages.
		 */
		cmmu->cmmu_regs[CMMU_SCTR] = CMMU_SCTR_SE;

		cmmu->cmmu_regs[CMMU_SAPR] = cmmu->cmmu_regs[CMMU_UAPR] = apr;

		cmmu->cmmu_regs[CMMU_BWP0] = cmmu->cmmu_regs[CMMU_BWP1] =
		cmmu->cmmu_regs[CMMU_BWP2] = cmmu->cmmu_regs[CMMU_BWP3] =
		cmmu->cmmu_regs[CMMU_BWP4] = cmmu->cmmu_regs[CMMU_BWP5] =
		cmmu->cmmu_regs[CMMU_BWP6] = cmmu->cmmu_regs[CMMU_BWP7] = 0;
		cmmu->cmmu_regs[CMMU_SCR] = CMMU_FLUSH_CACHE_INV_ALL;
		(void)cmmu->cmmu_regs[CMMU_SSR];
		cmmu->cmmu_regs[CMMU_SCR] = CMMU_FLUSH_SUPER_ALL;
		cmmu->cmmu_regs[CMMU_SCR] = CMMU_FLUSH_USER_ALL;
	}

	/*
	 * Enable instruction cache.
	 */
	apr &= ~CACHE_INH;
	m8820x_cmmu_set_reg(CMMU_SAPR, apr, MODE_VAL, cpu, INST_CMMU);

	/*
	 * Data cache will be enabled at pmap_bootstrap_cpu() time,
	 * because the PROM won't likely expect its work area in memory
	 * to be cached. On at least aviion, starting secondary processors
	 * returns an error code although the processor has correctly spun
	 * up, if the PROM work area is cached.
	 */
#ifdef dont_do_this_at_home
	apr |= CACHE_WT;
	m8820x_cmmu_set_reg(CMMU_SAPR, apr, MODE_VAL, cpu, DATA_CMMU);
#endif

	ci->ci_zeropage = m8820x_zeropage;
	ci->ci_copypage = m8820x_copypage;
}

/*
 * Just before poweroff or reset....
 */
void
m8820x_shutdown()
{
	u_int cmmu_num;
	struct m8820x_cmmu *cmmu;

	CMMU_LOCK;

	cmmu = m8820x_cmmu;
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++, cmmu++) {
#ifdef M88200_HAS_ASYMMETRICAL_ASSOCIATION
		if (cmmu->cmmu_regs == NULL)
			continue;
#endif
		cmmu->cmmu_regs[CMMU_SAPR] = cmmu->cmmu_regs[CMMU_UAPR] =
		    ((0x00000 << PG_BITS) | CACHE_INH) &
		    ~(CACHE_WT | CACHE_GLOBAL | APR_V);
	}

	CMMU_UNLOCK;
}

void
m8820x_set_sapr(apr_t ap)
{
	int cpu = cpu_number();

	CMMU_LOCK;

	m8820x_cmmu_set_reg(CMMU_SAPR, ap, 0, cpu, 0);

	CMMU_UNLOCK;
}

void
m8820x_set_uapr(apr_t ap)
{
	u_int32_t psr;
	int cpu = cpu_number();

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;

	m8820x_cmmu_set_reg(CMMU_UAPR, ap, 0, cpu, 0);

	CMMU_UNLOCK;
	set_psr(psr);
}

/*
 * Functions that invalidate TLB entries.
 */

void
m8820x_tlb_inv(cpuid_t cpu, u_int kernel, vaddr_t vaddr)
{
	u_int32_t psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;
	m8820x_cmmu_set_cmd(kernel ? CMMU_FLUSH_SUPER_PAGE :
	    CMMU_FLUSH_USER_PAGE, ADDR_VAL, cpu, 0, vaddr);
	CMMU_UNLOCK;
	set_psr(psr);
}

void
m8820x_tlb_inv_all(cpuid_t cpu)
{
	u_int32_t psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;
	m8820x_cmmu_set_reg(CMMU_SCR, CMMU_FLUSH_USER_ALL, 0, cpu, 0);
	CMMU_UNLOCK;
	set_psr(psr);
}

/*
 * Functions that invalidate caches.
 *
 * Cache operations require physical addresses. 
 *
 * We don't writeback instruction caches prior to invalidate because they
 * are never modified.
 *
 * Note that on systems with more than two CMMUs per CPU, we can not benefit
 * from the address split - the split is done on virtual (not translated yet)
 * addresses, but caches are physically indexed.
 */

#define	trunc_cache_line(a)	((a) & ~(MC88200_CACHE_LINE - 1))
#define	round_cache_line(a)	trunc_cache_line((a) + MC88200_CACHE_LINE - 1)

/*
 * invalidate I$, writeback and invalidate D$
 */
void
m8820x_cache_wbinv(cpuid_t cpu, paddr_t pa, psize_t size)
{
	u_int32_t psr;
	psize_t count;

	size = round_cache_line(pa + size) - trunc_cache_line(pa);
	pa = trunc_cache_line(pa);

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;

	while (size != 0) {
		if ((pa & PAGE_MASK) == 0 && size >= PAGE_SIZE) {
			m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CBI_PAGE,
			    0, cpu, 0, pa);
			count = PAGE_SIZE;
		} else {
			m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CBI_LINE,
			    0, cpu, 0, pa);
			count = MC88200_CACHE_LINE;
		}
		pa += count;
		size -= count;
		m8820x_cmmu_wait(cpu);
	}

	CMMU_UNLOCK;
	set_psr(psr);
}

/*
 * writeback D$
 */
void
m8820x_dcache_wb(cpuid_t cpu, paddr_t pa, psize_t size)
{
	u_int32_t psr;
	psize_t count;

	size = round_cache_line(pa + size) - trunc_cache_line(pa);
	pa = trunc_cache_line(pa);

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;

	while (size != 0) {
		if ((pa & PAGE_MASK) == 0 && size >= PAGE_SIZE) {
			m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CB_PAGE,
			    0, cpu, 0, pa);
			count = PAGE_SIZE;
		} else {
			m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CB_LINE,
			    0, cpu, 0, pa);
			count = MC88200_CACHE_LINE;
		}
		pa += count;
		size -= count;
		m8820x_cmmu_wait(cpu);
	}

	CMMU_UNLOCK;
	set_psr(psr);
}

/*
 * invalidate I$
 */
void
m8820x_icache_inv(cpuid_t cpu, paddr_t pa, psize_t size)
{
	u_int32_t psr;
	psize_t count;

	size = round_cache_line(pa + size) - trunc_cache_line(pa);
	pa = trunc_cache_line(pa);

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;

	while (size != 0) {
		if ((pa & PAGE_MASK) == 0 && size >= PAGE_SIZE) {
			m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_INV_PAGE,
			    MODE_VAL, cpu, INST_CMMU, pa);
			count = PAGE_SIZE;
		} else {
			m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_INV_LINE,
			    MODE_VAL, cpu, INST_CMMU, pa);
			count = MC88200_CACHE_LINE;
		}
		pa += count;
		size -= count;
		m8820x_cmmu_wait(cpu);
	}

	CMMU_UNLOCK;
	set_psr(psr);
}

/*
 * writeback D$
 */
void
m8820x_cmmu_wb_locked(int cpu, paddr_t pa, psize_t size)
{
	if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL, cpu, DATA_CMMU, pa);
	} else {
		m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, pa);
	}
	m8820x_cmmu_wait(cpu);
}

/*
 * invalidate I$, writeback and invalidate D$
 */
void
m8820x_cmmu_wbinv_locked(int cpu, paddr_t pa, psize_t size)
{
	if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL, cpu, 0, pa);
	} else {
		m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL, cpu, 0, pa);
	}
	m8820x_cmmu_wait(cpu);
}

/*
 * invalidate I$ and D$
 */
void
m8820x_cmmu_inv_locked(int cpu, paddr_t pa, psize_t size)
{
	if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_INV_LINE, 0, cpu, 0, pa);
	} else {
		m8820x_cmmu_set_cmd(CMMU_FLUSH_CACHE_INV_PAGE, 0, cpu, 0, pa);
	}
	m8820x_cmmu_wait(cpu);
}

/*
 * High level cache handling functions (used by bus_dma).
 *
 * On multiprocessor systems, since the CMMUs snoop each other, they
 * all have a coherent view of the data. Thus, we only need to writeback
 * on a single CMMU. However, invalidations need to be done on all CMMUs.
 */

void
m8820x_dma_cachectl(paddr_t _pa, psize_t _size, int op)
{
	u_int32_t psr;
	int cpu;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
#endif
	paddr_t pa;
	psize_t size, count;
	void (*flusher)(int, paddr_t, psize_t);
	uint8_t lines[2 * MC88200_CACHE_LINE];
	paddr_t pa1, pa2;
	psize_t sz1, sz2;

	pa = trunc_cache_line(_pa);
	size = round_cache_line(_pa + _size) - pa;
	sz1 = sz2 = 0;

	switch (op) {
	case DMA_CACHE_SYNC:
		flusher = m8820x_cmmu_wb_locked;
		break;
	case DMA_CACHE_SYNC_INVAL:
		flusher = m8820x_cmmu_wbinv_locked;
		break;
	default:
	case DMA_CACHE_INV:
		pa1 = pa;
		sz1 = _pa - pa1;
		pa2 = _pa + _size;
		sz2 = pa + size - pa2;
		flusher = m8820x_cmmu_inv_locked;
		break;
	}

#ifndef MULTIPROCESSOR
	cpu = cpu_number();
#endif

	psr = get_psr();
	set_psr(psr | PSR_IND);
	CMMU_LOCK;

	/*
	 * Preserve the data from incomplete cache lines about to be
	 * invalidated, if necessary.
	 */
	if (sz1 != 0)
		bcopy((void *)pa1, lines, sz1);
	if (sz2 != 0)
		bcopy((void *)pa2, lines + MC88200_CACHE_LINE, sz2);

	while (size != 0) {
		count = (pa & PAGE_MASK) == 0 && size >= PAGE_SIZE ?
		    PAGE_SIZE : MC88200_CACHE_LINE;

#ifdef MULTIPROCESSOR
		/* writeback on a single cpu... */
		(*flusher)(ci->ci_cpuid, pa, count);

		/* invalidate on all... */
		if (flusher != m8820x_cmmu_wb_locked) {
			for (cpu = 0; cpu < MAX_CPUS; cpu++) {
				if (!ISSET(m88k_cpus[cpu].ci_flags, CIF_ALIVE))
					continue;
				if (cpu == ci->ci_cpuid)
					continue;
				m8820x_cmmu_inv_locked(cpu, pa, count);
			}
		}
#else	/* MULTIPROCESSOR */
		(*flusher)(cpu, pa, count);
#endif	/* MULTIPROCESSOR */

		pa += count;
		size -= count;
	}

	/*
	 * Restore data from incomplete cache lines having been invalidated,
	 * if necessary.
	 */
	if (sz1 != 0)
		bcopy(lines, (void *)pa1, sz1);
	if (sz2 != 0)
		bcopy(lines + MC88200_CACHE_LINE, (void *)pa2, sz2);

	CMMU_UNLOCK;
	set_psr(psr);
}

#ifdef MULTIPROCESSOR
void
m8820x_dma_cachectl_local(paddr_t pa, psize_t size, int op)
{
	/* This function is not used on 88100 systems */
}
#endif
