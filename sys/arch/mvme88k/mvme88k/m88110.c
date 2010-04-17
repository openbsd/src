/*	$OpenBSD: m88110.c,v 1.64 2010/04/17 22:10:13 miod Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
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
#include <machine/bugio.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/lock.h>
#include <machine/m88110.h>
#include <machine/m88410.h>
#include <machine/psl.h>

#include <mvme88k/dev/busswreg.h>
#ifdef MULTIPROCESSOR
#include <machine/mvme197.h>
#endif

cpuid_t	m88110_init(void);
cpuid_t	m88410_init(void);
void	m88110_setup_board_config(void);
void	m88410_setup_board_config(void);
void	m88110_cpu_configuration_print(int);
void	m88410_cpu_configuration_print(int);
void	m88110_shutdown(void);
cpuid_t	m88110_cpu_number(void);
void	m88110_set_sapr(apr_t);
void	m88110_set_uapr(apr_t);
void	m88110_flush_tlb(cpuid_t, u_int, vaddr_t, u_int);
void	m88410_flush_tlb(cpuid_t, u_int, vaddr_t, u_int);
void	m88110_flush_cache(cpuid_t, paddr_t, psize_t);
void	m88410_flush_cache(cpuid_t, paddr_t, psize_t);
void	m88110_flush_inst_cache(cpuid_t, paddr_t, psize_t);
void	m88410_flush_inst_cache(cpuid_t, paddr_t, psize_t);
void	m88110_dma_cachectl(paddr_t, psize_t, int);
void	m88110_dma_cachectl_local(paddr_t, psize_t, int);
void	m88410_dma_cachectl(paddr_t, psize_t, int);
void	m88410_dma_cachectl_local(paddr_t, psize_t, int);
void	m88110_initialize_cpu(cpuid_t);
void	m88410_initialize_cpu(cpuid_t);

/*
 * This is the function table for the MC88110 built-in CMMUs without
 * external 88410.
 */
struct cmmu_p cmmu88110 = {
	m88110_init,
	m88110_setup_board_config,
	m88110_cpu_configuration_print,
	m88110_shutdown,
	m88110_cpu_number,
	m88110_set_sapr,
	m88110_set_uapr,
	m88110_flush_tlb,
	m88110_flush_cache,
	m88110_flush_inst_cache,
	m88110_dma_cachectl,
#ifdef MULTIPROCESSOR
	m88110_dma_cachectl_local,
	m88110_initialize_cpu,
#endif
};

/*
 * This is the function table for the MC88110 built-in CMMUs with
 * external 88410.
 */
struct cmmu_p cmmu88410 = {
	m88410_init,
	m88410_setup_board_config,
	m88410_cpu_configuration_print,
	m88110_shutdown,
	m88110_cpu_number,
	m88110_set_sapr,
	m88110_set_uapr,
	m88110_flush_tlb,
	m88410_flush_cache,
	m88410_flush_inst_cache,
	m88410_dma_cachectl,
#ifdef MULTIPROCESSOR
	m88410_dma_cachectl_local,
	m88410_initialize_cpu,
#endif
};

void patc_clear(void);

void m88110_cmmu_sync_cache(paddr_t, psize_t);
void m88110_cmmu_sync_inval_cache(paddr_t, psize_t);
void m88110_cmmu_inval_cache(paddr_t, psize_t);

void
patc_clear(void)
{
	int i;

	for (i = 0; i < 32; i++) {
		set_dir(i << 5);
		set_dppu(0);
		set_dppl(0);

		set_iir(i << 5);
		set_ippu(0);
		set_ippl(0);
	}
}

void
m88110_setup_board_config(void)
{
	ncpusfound = 1;
}

void
m88410_setup_board_config(void)
{
	struct mvmeprom_brdid brdid;

	/*
	 * MVME197SP are 01-W3815B04, while MVME197DP are 01-W3815B03.
	 * If the CNFG memory has been lost and the board is a 197SP,
	 * we'll just fail to spin up the non-existing second processor.
	 */
	bzero(&brdid, sizeof(brdid));
	bugbrdid(&brdid);
	if (bcmp(brdid.pwa, "01-W3815B04", 11) == 0)
		ncpusfound = 1;
	else
		ncpusfound = 2;
}

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status. Should be called first
 * by the master, before the slaves are started.
 */
void
m88110_cpu_configuration_print(int master)
{
	int pid = get_cpu_pid();
	int proctype = (pid & PID_ARN) >> ARN_SHIFT;
	int procvers = (pid & PID_VN) >> VN_SHIFT;
	int cpu = cpu_number();

	printf("cpu%d: ", cpu);
	switch (proctype) {
	default:
		printf("unknown model arch 0x%x version 0x%x",
		    proctype, procvers);
		break;
	case ARN_88110:
		printf("M88110 version 0x%x, 8K I/D caches", procvers);
		break;
	}
	printf("\n");
}

void
m88410_cpu_configuration_print(int master)
{
	m88110_cpu_configuration_print(master);
	/* XXX how to get its size? */
	printf("cpu%d: external M88410 cache controller\n", cpu_number());

#ifdef MULTIPROCESSOR
	/*
	 * Mark us as allowing IPIs now.
	 */
	*(volatile u_int8_t *)(BS_BASE + BS_CPINT) = BS_CPI_ICLR | BS_CPI_IEN;
#endif
}

/*
 * CMMU initialization routine
 */
cpuid_t
m88110_init(void)
{
	cpuid_t cpu;

	cpu = m88110_cpu_number();
	m88110_initialize_cpu(cpu);
	return (cpu);
}

cpuid_t
m88410_init(void)
{
	cpuid_t cpu;

	cpu = m88110_cpu_number();
	m88410_initialize_cpu(cpu);
	return (cpu);
}

cpuid_t
m88110_cpu_number(void)
{
	u_int16_t gcsr;

	gcsr = *(volatile u_int16_t *)(BS_BASE + BS_GCSR);

	return ((gcsr & BS_GCSR_CPUID) != 0 ? 1 : 0);
}

void
m88110_initialize_cpu(cpuid_t cpu)
{
	u_int ictl, dctl;
	int i;
	int procvers = (get_cpu_pid() & PID_VN) >> VN_SHIFT;

	/* clear BATCs */
	for (i = 0; i < 8; i++) {
		set_dir(i);
		set_dbp(0);
		set_iir(i);
		set_ibp(0);
	}

	/* clear PATCs */
	patc_clear();

	ictl = BATC_512K | CMMU_ICTL_DID | CMMU_ICTL_CEN | CMMU_ICTL_BEN;

	/*
	 * 88110 errata #10 (4.2) or #2 (5.1.1):
	 * ``Under some circumstances, the 88110 may incorrectly latch data
	 *   as it comes from the bus.
	 *   [...]
	 *   It is the data matching mechanism that may give corrupt data to
	 *   the register files.
	 *   Suggested fix: Set the Data Matching Disable bit (bit 2) of the
	 *   DCTL.  This bit is not documented in the user's manual. This bit
	 *   is only present for debug purposes and its functionality should
	 *   not be depended upon long term.''
	 *
	 * 88110 errata #5 (5.1.1):
	 * ``Setting the xmem bit in the dctl register to perform st/ld
	 *   xmems can cause the cpu to hang if a st instruction follows the
	 *   xmem.
	 *   Work-Around: do not set the xmem bit in dctl, or separate st
	 *   from xmem instructions.''
	 */
	dctl = BATC_512K | CMMU_DCTL_CEN | CMMU_DCTL_ADS;
	dctl |= CMMU_DCTL_RSVD1; /* Data Matching Disable */

	/*
	 * 88110 rev 4.2 errata #1:
	 * ``Under certain conditions involving exceptions, with branch
	 *   prediction enabled, the CPU may hang.
	 *   Suggested fix: Clear the PREN bit of the ICTL.  This will
	 *   disable branch prediction.''
	 *
	 * ...but this errata becomes...
	 *
	 * 88110 rev 5.1 errata #1:
	 * ``Under certain conditions involving exceptions, with branch
	 *   prediction enabled and decoupled loads/stores enabled, load
	 *   instructions may complete incorrectly or stores may execute
	 *   to the wrong. address.
	 *   Suggested fix: Clear the PREN bit of the ICTL or the DEN bit
	 *   of the DCTL.''
	 *
	 * So since branch prediction appears to give better performance
	 * than data cache decoupling, and it is not known whether the
	 * problem has been understood better and thus the conditions
	 * narrowed on 5.1, or changes between 4.2 and 5.1 only restrict
	 * the conditions on which it may occur, we'll enable branch
	 * prediction on 5.1 processors and data cache decoupling on
	 * earlier versions.
	 */
	if (procvers >= 0xf)	/* > 0xb ? */
		ictl |= CMMU_ICTL_PREN;
	else
		dctl |= CMMU_DCTL_DEN;

	mc88110_inval_inst();		/* clear instruction cache & TIC */
	mc88110_inval_data();		/* clear data cache */

	set_ictl(ictl);
	set_dctl(dctl);

	set_isr(0);
	set_dsr(0);
}

void
m88410_initialize_cpu(cpuid_t cpu)
{
	u_int dctl;

	m88110_initialize_cpu(cpu);
	dctl = get_dctl();
	dctl |= CMMU_DCTL_SEN;
	set_dctl(dctl);
	CMMU_LOCK;
#if 0
	mc88410_inval();	/* clear external data cache */
#else
	/*
	 * We can't invalidate the 88410 cache without flushing it first;
	 * this is probably due to either an error in the cpu-to-88410
	 * communication protocol, or to a bug in the '410 (but since I
	 * do not know how to get its revision, I can't tell whether this
	 * is the obscure v1 bug or not).
	 *
	 * Since we can't flush random data either, fill the secondary
	 * cache first, before flushing it.
	 *
	 * The smallest 88410 cache line is 32 bytes, and the largest size
	 * is 1MB.
	 */
	{
		vaddr_t va;
		uint32_t junk = 0;

		for (va = 0; va < 1024 * 1024; va += 32)
			junk += *(uint32_t *)va;

		/* to make sure the above loop isn't optimized away */
		mc88110_sync_data_page(junk & PAGE_SIZE);
	}
	mc88410_flush();
	mc88410_inval();
#endif
	CMMU_UNLOCK;
}

/*
 * Just before poweroff or reset....
 */
void
m88110_shutdown(void)
{
}

void
m88110_set_sapr(apr_t ap)
{
	u_int ictl, dctl;

	set_icmd(CMMU_ICMD_INV_SATC);
	set_dcmd(CMMU_DCMD_INV_SATC);

	ictl = get_ictl();
	dctl = get_dctl();

	set_isap(ap);
	set_dsap(ap);

	patc_clear();

	set_icmd(CMMU_ICMD_INV_UATC);
	set_icmd(CMMU_ICMD_INV_SATC);
	set_dcmd(CMMU_DCMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_SATC);

	/* Enable translation */
	ictl |= CMMU_ICTL_MEN | CMMU_ICTL_HTEN;
	dctl |= CMMU_DCTL_MEN | CMMU_DCTL_HTEN;
	set_ictl(ictl);
	set_dctl(dctl);
}

void
m88110_set_uapr(apr_t ap)
{
	set_iuap(ap);
	set_duap(ap);

	set_icmd(CMMU_ICMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_UATC);

	/* We need to at least invalidate the TIC, as it is va-addressed */
	set_icmd(CMMU_ICMD_INV_TIC);
}

/*
 *	Functions that invalidate TLB entries.
 */

/*
 *	flush any tlb
 */
void
m88110_flush_tlb(cpuid_t cpu, u_int kernel, vaddr_t vaddr, u_int count)
{
	u_int32_t psr;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();

	if (cpu != ci->ci_cpuid) {
		m197_send_ipi(kernel ?
		    CI_IPI_TLB_FLUSH_KERNEL : CI_IPI_TLB_FLUSH_USER, cpu);
		return;
	}
#endif

	psr = get_psr();
	set_psr(psr | PSR_IND);

	if (kernel) {
		set_icmd(CMMU_ICMD_INV_SATC);
		set_dcmd(CMMU_DCMD_INV_SATC);
	} else {
		set_icmd(CMMU_ICMD_INV_UATC);
		set_dcmd(CMMU_DCMD_INV_UATC);
	}

	set_psr(psr);
}

/*
 *	Functions that invalidate caches.
 */

/*
 * 88110 general information #22:
 * ``Issuing a command to flush and invalidate the data cache while the
 *   dcache is disabled (CEN = 0 in dctl) will cause problems.  Do not
 *   flush a disabled data cache.  In general, there is no reason to
 *   perform this operation with the cache disabled, since it may be
 *   incoherent with the proper state of memory.  Before 5.0 the flush
 *   command was treated like a nop when the cache was disabled.  This
 *   is no longer the case.''
 *
 * Since we always enable the data cache, and no cmmu cache operation
 * will occur before we do, it is not necessary to pay attention to this.
 */

#ifdef MULTIPROCESSOR
/*
 * 88110 rev 4.2 errata #17:
 * ``A copyback initiated by a flush page/line with invalidate command,
 *   which is retried on the external bus, and is preceded by a snoop
 *   copyback before the retry cycle occurs, can be incorrectly marked
 *   as invalid, and not copied back to memory.
 *   Suggested fix: Use flush page/line followed by flush page/line with
 *   invalidate to avoid this condition.''
 *
 * This really only matters to us when running a MULTIPROCESSOR kernel
 * (otherwise there is no snooping happening), and given the intrusive
 * changes it requires (see the comment about invalidates being turned
 * into flushes with invalidate in m88110_cmmu_inval_cache below), as
 * well as the small performance impact it has), we define a specific
 * symbol to enable the suggested workaround.
 *
 * Also, note that m88110_dma_cachectl() is never invoked when running on
 * a multiprocessor system, therefore does not need to implement this
 * workaround.
 */
#define	ENABLE_88110_ERRATA_17
#endif

#define	trunc_cache_line(a)	((a) & ~(MC88110_CACHE_LINE - 1))
#define	round_cache_line(a)	trunc_cache_line((a) + MC88110_CACHE_LINE - 1)

/*
 * Flush both Instruction and Data caches
 */

void
m88110_flush_cache(cpuid_t cpu, paddr_t pa, psize_t size)
{
	u_int32_t psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	mc88110_inval_inst();
	mc88110_flush_data();

	set_psr(psr);
}

void
m88410_flush_cache(cpuid_t cpu, paddr_t pa, psize_t size)
{
	u_int32_t psr;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();

	if (cpu != ci->ci_cpuid) {
		m197_send_complex_ipi(CI_IPI_CACHE_FLUSH, cpu, pa, size);
		return;
	}
#endif

	psr = get_psr();
	set_psr(psr | PSR_IND);

	mc88110_inval_inst();
	/* flush all data to avoid errata invalidate */
	mc88110_flush_data();
	CMMU_LOCK;
	mc88410_flush();
	CMMU_UNLOCK;

	set_psr(psr);
}

/*
 * Flush Instruction caches
 */

void
m88110_flush_inst_cache(cpuid_t cpu, paddr_t pa, psize_t size)
{
	/* atomic so no psr games */
	mc88110_inval_inst();
}

void
m88410_flush_inst_cache(cpuid_t cpu, paddr_t pa, psize_t size)
{
	u_int32_t psr;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();

	if (cpu != ci->ci_cpuid) {
		m197_send_complex_ipi(CI_IPI_ICACHE_FLUSH, cpu, pa, size);
		return;
	}
#endif

	psr = get_psr();
	set_psr(psr | PSR_IND);

	mc88110_inval_inst();
	CMMU_LOCK;
	mc88410_flush();
	CMMU_UNLOCK;

	set_psr(psr);
}

/*
 * Sync dcache - icache is never dirty but needs to be invalidated as well.
 */

void
m88110_cmmu_sync_cache(paddr_t pa, psize_t size)
{
#ifdef ENABLE_88110_ERRATA_17
	mc88110_flush_data_page(pa);
#else
	if (size <= MC88110_CACHE_LINE)
		mc88110_flush_data_line(pa);
	else
		mc88110_flush_data_page(pa);
#endif
}

void
m88110_cmmu_sync_inval_cache(paddr_t pa, psize_t size)
{
#ifdef ENABLE_88110_ERRATA_17
	mc88110_flush_data_page(pa);
	mc88110_sync_data_page(pa);
#else
	if (size <= MC88110_CACHE_LINE)
		mc88110_sync_data_line(pa);
	else
		mc88110_sync_data_page(pa);
#endif
}

void
m88110_cmmu_inval_cache(paddr_t pa, psize_t size)
{
	/*
	 * I'd love to do this...

	if (size <= MC88110_CACHE_LINE)
		mc88110_inval_data_line(pa);
	else
		mc88110_inval_data_page(pa);

	 * ... but there is no mc88110_inval_data_page(). Callers know
	 * this and turn invalidates into syncs with invalidate for page
	 * or larger areas.
	 */
	mc88110_inval_data_line(pa);
}

/*
 * High level cache handling functions (used by bus_dma).
 */

void
m88110_dma_cachectl(paddr_t _pa, psize_t _size, int op)
{
	u_int32_t psr;
	paddr_t pa;
	psize_t size, count;
	void (*flusher)(paddr_t, psize_t);

	pa = trunc_cache_line(_pa);
	size = round_cache_line(_pa + _size) - pa;

	switch (op) {
	case DMA_CACHE_SYNC:
		flusher = m88110_cmmu_sync_cache;
		break;
	case DMA_CACHE_SYNC_INVAL:
		flusher = m88110_cmmu_sync_inval_cache;
		break;
	default:
		if (pa != _pa || size != _size || size >= PAGE_SIZE)
			flusher = m88110_cmmu_sync_inval_cache;
		else
			flusher = m88110_cmmu_inval_cache;
		break;
	}

	psr = get_psr();
	set_psr(psr | PSR_IND);

	if (op != DMA_CACHE_SYNC)
		mc88110_inval_inst();
	while (size != 0) {
		count = (pa & PAGE_MASK) == 0 && size >= PAGE_SIZE ?
		    PAGE_SIZE : MC88110_CACHE_LINE;

		(*flusher)(pa, count);

		pa += count;
		size -= count;
	}

	set_psr(psr);
}

void
m88410_dma_cachectl_local(paddr_t pa, psize_t size, int op)
{
	u_int32_t psr;
	psize_t count;
	void (*flusher)(paddr_t, psize_t);
	void (*ext_flusher)(void);

	switch (op) {
	case DMA_CACHE_SYNC:
#if 0
		flusher = m88110_cmmu_sync_cache;
		ext_flusher = mc88410_flush;
#endif
		break;
	case DMA_CACHE_SYNC_INVAL:
		flusher = m88110_cmmu_sync_inval_cache;
		ext_flusher = mc88410_sync;
		break;
	default:
#ifdef ENABLE_88110_ERRATA_17
		flusher = m88110_cmmu_sync_inval_cache;
#else
		flusher = m88110_cmmu_inval_cache;
#endif
#ifdef notyet
		ext_flusher = mc88410_inval;
#else
		ext_flusher = mc88410_sync;
#endif
		break;
	}

	psr = get_psr();
	set_psr(psr | PSR_IND);

	if (op == DMA_CACHE_SYNC) {
		CMMU_LOCK;
		while (size != 0) {
			m88110_cmmu_sync_cache(pa, PAGE_SIZE);
			mc88410_flush_page(pa);
			pa += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
		CMMU_UNLOCK;
	} else {
		mc88110_inval_inst();
		while (size != 0) {
#ifdef ENABLE_88110_ERRATA_17
			count = PAGE_SIZE;
#else
			count = (pa & PAGE_MASK) == 0 && size >= PAGE_SIZE ?
			    PAGE_SIZE : MC88110_CACHE_LINE;
#endif

			(*flusher)(pa, count);

			pa += count;
			size -= count;
		}
		CMMU_LOCK;
		(*ext_flusher)();
		CMMU_UNLOCK;
	}

	set_psr(psr);
}

void
m88410_dma_cachectl(paddr_t _pa, psize_t _size, int op)
{
	paddr_t pa;
	psize_t size;

#ifdef ENABLE_88110_ERRATA_17
	pa = trunc_page(_pa);
	size = round_page(_pa + _size) - pa;

#if 0 /* not required since m88410_dma_cachectl_local() behaves identically */
	if (op == DMA_CACHE_INV)
		op = DMA_CACHE_SYNC_INVAL;
#endif
#else
	if (op == DMA_CACHE_SYNC) {
		pa = trunc_page(_pa);
		size = round_page(_pa + _size) - pa;
	} else {
		pa = trunc_cache_line(_pa);
		size = round_cache_line(_pa + _size) - pa;

		if (op == DMA_CACHE_INV) {
			if (pa != _pa || size != _size || size >= PAGE_SIZE)
				op = DMA_CACHE_SYNC_INVAL;
		}
	}
#endif

	m88410_dma_cachectl_local(pa, size, op);
#ifdef MULTIPROCESSOR
	m197_broadcast_complex_ipi(CI_IPI_DMA_CACHECTL, pa, size | op);
#endif
}

#ifdef MULTIPROCESSOR
void
m88110_dma_cachectl_local(paddr_t _pa, psize_t _size, int op)
{
	/* Obviously nothing to do. */
}
#endif
