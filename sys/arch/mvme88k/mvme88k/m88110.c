/*	$OpenBSD: m88110.c,v 1.23 2005/09/25 20:55:15 miod Exp $	*/
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
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/simplelock.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu_number.h>
#include <machine/cmmu.h>
#include <machine/m88110.h>
#include <machine/m88410.h>
#include <machine/locore.h>
#include <machine/trap.h>

#ifdef DEBUG
#define DB_CMMU		0x4000	/* MMU debug */
unsigned int debuglevel = 0;
#define dprintf(_L_,_X_) \
do { \
	if (debuglevel & (_L_)) { \
		unsigned int psr; \
		disable_interrupt(psr); \
		printf("%d: ", cpu_number()); \
		printf _X_; \
		set_psr(psr); \
	} \
} while (0)
#else
#define dprintf(_L_,_X_)
#endif

#ifdef DDB
#include <ddb/db_output.h>		/* db_printf()		*/
#define DEBUG_MSG db_printf
#define STATIC
#else
#define DEBUG_MSG printf
#define STATIC	static
#endif /* DDB */

void m88110_cmmu_init(void);
void m88110_setup_board_config(void);
void m88110_cpu_configuration_print(int);
void m88110_cmmu_shutdown_now(void);
void m88110_cmmu_parity_enable(void);
unsigned m88110_cmmu_cpu_number(void);
void m88110_cmmu_set_sapr(unsigned, unsigned);
void m88110_cmmu_set_uapr(unsigned);
void m88110_cmmu_flush_tlb(unsigned, unsigned, vaddr_t, u_int);
void m88110_cmmu_flush_cache(int, paddr_t, psize_t);
void m88110_cmmu_flush_inst_cache(int, paddr_t, psize_t);
void m88110_cmmu_flush_data_cache(int, paddr_t, psize_t);
void m88110_dma_cachectl(pmap_t, vaddr_t, vsize_t, int);
void m88110_dma_cachectl_pa(paddr_t, psize_t, int);
void m88110_cmmu_dump_config(void);
void m88110_cmmu_show_translation(unsigned, unsigned, unsigned, int);
void m88110_show_apr(unsigned);

/* This is the function table for the mc88110 built-in CMMUs */
struct cmmu_p cmmu88110 = {
        m88110_cmmu_init,
	m88110_setup_board_config,
	m88110_cpu_configuration_print,
	m88110_cmmu_shutdown_now,
	m88110_cmmu_parity_enable,
	m88110_cmmu_cpu_number,
	m88110_cmmu_set_sapr,
	m88110_cmmu_set_uapr,
	m88110_cmmu_flush_tlb,
	m88110_cmmu_flush_cache,
	m88110_cmmu_flush_inst_cache,
	m88110_cmmu_flush_data_cache,
	m88110_dma_cachectl,
	m88110_dma_cachectl_pa,
#ifdef DDB
	m88110_cmmu_dump_config,
	m88110_cmmu_show_translation,
#else
	NULL,
	NULL,
#endif
#ifdef DEBUG
        m88110_show_apr,
#else
	NULL,
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

#ifdef DEBUG
void
m88110_show_apr(unsigned value)
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
	else
		printf(", not valid");
	printf("\n");
}
#endif

void
m88110_setup_board_config(void)
{
	/* we could print something here... */
	max_cpus = 1;
	cpu_sets[0] = 1;   /* This cpu installed... */
}

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
 */
void
m88110_cpu_configuration_print(int master)
{
	int pid = read_processor_identification_register();
	int proctype = (pid & 0xff00) >> 8;
	int procvers = (pid & 0xe) >> 1;
	int cpu = cpu_number();
	struct simplelock print_lock;

	CMMU_LOCK;
	if (master)
		simple_lock_init(&print_lock);

	simple_lock(&print_lock);

	printf("cpu%d: ", cpu);
	if (proctype != 1) {
		printf("unknown model arch 0x%x version 0x%x\n",
		    proctype, procvers);
		simple_unlock(&print_lock);
		return;
	}

	printf("M88110 version 0x%x", procvers);
	if (mc88410_present())
		printf(", external M88410 cache controller");
	printf("\n");

	simple_unlock(&print_lock);
        CMMU_UNLOCK;
}

/*
 * CMMU initialization routine
 */
void
m88110_cmmu_init(void)
{
	int i;

	/* clear BATCs */
	for (i = 0; i < 8; i++) {
		set_dir(i);
		set_dbp(0);
		set_iir(i);
		set_ibp(0);
	}

	/* clear PATCs */
	patc_clear();

	/* Do NOT enable ICTL_PREN (branch prediction) */
	set_ictl(BATC_32M
		 | CMMU_ICTL_DID	/* Double instruction disable */
		 | CMMU_ICTL_MEN
		 | CMMU_ICTL_CEN
		 | CMMU_ICTL_BEN
		 | CMMU_ICTL_HTEN);

	set_dctl(BATC_32M
                 | CMMU_DCTL_RSVD1	/* Data Matching Disable */
                 | CMMU_DCTL_MEN
		 | CMMU_DCTL_CEN
		 | CMMU_DCTL_SEN
		 | CMMU_DCTL_ADS
		 | CMMU_DCTL_HTEN);

	mc88110_inval_inst();		/* clear instruction cache & TIC */
	mc88110_inval_data();		/* clear data cache */
	if (mc88410_present())
		mc88410_inval();	/* clear external data cache */

	set_dcmd(CMMU_DCMD_INV_SATC);	/* invalidate ATCs */

	set_isr(0);
	set_dsr(0);
}

/*
 * Just before poweroff or reset....
 */
void
m88110_cmmu_shutdown_now(void)
{
#if 0
	CMMU_LOCK;
        CMMU_UNLOCK;
#endif
}

/*
 * Enable parity
 */
void
m88110_cmmu_parity_enable(void)
{
#if 0
	CMMU_LOCK;
        CMMU_UNLOCK;
#endif
}

/*
 * Find out the CPU number from accessing CMMU
 * Better be at splhigh, or even better, with interrupts
 * disabled.
 */

unsigned
m88110_cmmu_cpu_number(void)
{
	return 0; /* to make compiler happy */
}

void
m88110_cmmu_set_sapr(unsigned cpu, unsigned ap)
{
	unsigned ictl, dctl;

	CMMU_LOCK;

	set_icmd(CMMU_ICMD_INV_SATC);
	set_dcmd(CMMU_DCMD_INV_SATC);

	ictl = get_ictl();
	dctl = get_dctl();

	/* disable translation */
	set_ictl((ictl & ~CMMU_ICTL_MEN));
	set_dctl((dctl & ~CMMU_DCTL_MEN));

	set_isap(ap);
	set_dsap(ap);

	patc_clear();

	set_icmd(CMMU_ICMD_INV_UATC);
	set_icmd(CMMU_ICMD_INV_SATC);
	set_dcmd(CMMU_DCMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_SATC);

	/* restore MMU settings */
	set_ictl(ictl);
	set_dctl(dctl);

	CMMU_UNLOCK;
}

void
m88110_cmmu_set_uapr(unsigned ap)
{
	CMMU_LOCK;
	set_iuap(ap);
	set_duap(ap);

	set_icmd(CMMU_ICMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_UATC);

	/* We need to at least invalidate the TIC, as it is va-addressed */
	mc88110_inval_inst();
	CMMU_UNLOCK;
}

/*
 *	Functions that invalidate TLB entries.
 */

/*
 *	flush any tlb
 */
void
m88110_cmmu_flush_tlb(unsigned cpu, unsigned kernel, vaddr_t vaddr, u_int count)
{
	u_int32_t psr;

	disable_interrupt(psr);

	CMMU_LOCK;
	if (kernel) {
		set_icmd(CMMU_ICMD_INV_SATC);
		set_dcmd(CMMU_DCMD_INV_SATC);
	} else {
		set_icmd(CMMU_ICMD_INV_UATC);
		set_dcmd(CMMU_DCMD_INV_UATC);
	}
	CMMU_UNLOCK;

	set_psr(psr);
}

/*
 *	Functions that invalidate caches.
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
 * Care must be taken to avoid flushing the data cache when
 * the data cache is not on!  From the 0F92L Errata Documentation
 * Package, Version 1.1
 */

/*
 * XXX These routines are really suboptimal because they invalidate
 * way too much...
 * Improve them once the 197 support is really working...
 */

/*
 *	flush both Instruction and Data caches
 */
void
m88110_cmmu_flush_cache(int cpu, paddr_t physaddr, psize_t size)
{
	u_int32_t psr;

	disable_interrupt(psr);

	mc88110_inval_inst();
	mc88110_flush_data();
	if (mc88410_present())
		mc88410_flush();
	set_psr(psr);
}

/*
 *	flush Instruction caches
 */
void
m88110_cmmu_flush_inst_cache(int cpu, paddr_t physaddr, psize_t size)
{
	u_int32_t psr;

	disable_interrupt(psr);

	mc88110_inval_inst();
	set_psr(psr);
}

/*
 * flush data cache
 */
void
m88110_cmmu_flush_data_cache(int cpu, paddr_t physaddr, psize_t size)
{
	u_int32_t psr;

	disable_interrupt(psr);

	mc88110_flush_data();
	if (mc88410_present())
		mc88410_flush();
	set_psr(psr);
}

/*
 * sync dcache (and icache too)
 */
void
m88110_cmmu_sync_cache(paddr_t physaddr, psize_t size)
{
	u_int32_t psr;

	disable_interrupt(psr);

	mc88110_inval_inst();
	mc88110_flush_data();
	if (mc88410_present())
		mc88410_flush();
	set_psr(psr);
}

void
m88110_cmmu_sync_inval_cache(paddr_t physaddr, psize_t size)
{
	u_int32_t psr;

	disable_interrupt(psr);

	mc88110_sync_data();
	if (mc88410_present())
		mc88410_sync();
	set_psr(psr);
}

void
m88110_cmmu_inval_cache(paddr_t physaddr, psize_t size)
{
	u_int32_t psr;

	disable_interrupt(psr);

	mc88110_inval_inst();
	mc88110_inval_data();
	if (mc88410_present())
		mc88410_inval();
	set_psr(psr);
}

void
m88110_dma_cachectl(pmap_t pmap, vaddr_t va, vsize_t size, int op)
{
	paddr_t pa;

	if (pmap_extract(pmap, va, &pa) == FALSE)
		return;	/* XXX */

	switch (op) {
	case DMA_CACHE_SYNC:
		m88110_cmmu_sync_cache(pa, size);
		break;
	case DMA_CACHE_SYNC_INVAL:
		m88110_cmmu_sync_inval_cache(pa, size);
		break;
	default:
		m88110_cmmu_inval_cache(pa, size);
		break;
	}
}

void
m88110_dma_cachectl_pa(paddr_t pa, psize_t size, int op)
{
	switch (op) {
	case DMA_CACHE_SYNC:
		m88110_cmmu_sync_cache(pa, size);
		break;
	case DMA_CACHE_SYNC_INVAL:
		m88110_cmmu_sync_inval_cache(pa, size);
		break;
	default:
		m88110_cmmu_inval_cache(pa, size);
		break;
	}
}

#ifdef DDB
void
m88110_cmmu_dump_config(void)
{
	/* dummy routine */
}

#undef	VEQR_ADDR
#define	VEQR_ADDR	0

/*
 * Show (for debugging) how the given CMMU translates the given ADDRESS.
 * If cmmu == -1, the data cmmu for the current cpu is used.
 */
void
m88110_cmmu_show_translation(unsigned address,
		unsigned supervisor_flag,
		unsigned verbose_flag,
		int cmmu_num)
{
	/*
	 * A virtual address is split into three fields. Two are used as
	 * indicies into tables (segment and page), and one is an offset into
	 * a page of memory.
	 */
	union {
		unsigned bits;
		struct {
			unsigned segment_table_index:SDT_BITS,
			page_table_index:PDT_BITS,
			page_offset:PG_BITS;
		} field;
	} virtual_address;
	unsigned value;
	unsigned result;
	unsigned probeaddr;

	if (verbose_flag)
		db_printf("-------------------------------------------\n");

	if (supervisor_flag)
		value = get_dsap();
	else
		value = get_duap();

	/******* SEE WHAT A PROBE SAYS (if not a thread) ***********/

	set_dsar(address);
	if (supervisor_flag) {
		set_dcmd(CMMU_DCMD_PRB_SUPR);
	} else {
		set_dcmd(CMMU_DCMD_PRB_USER);
	}
	result = get_dsr();
	probeaddr = get_dsar();
	if (verbose_flag > 1)
		DEBUG_MSG("probe of 0x%08x returns dsr=0x%08x\n",
			  address, result);
	if (result & CMMU_DSR_PH || result & CMMU_DSR_BH) {
		DEBUG_MSG("probe of 0x%08x returns phys=0x%x",
			  address, probeaddr);
		if (result & CMMU_DSR_CP) DEBUG_MSG(", copyback err");
		if (result & CMMU_DSR_BE) DEBUG_MSG(", bus err");
		if (result & CMMU_DSR_TBE) DEBUG_MSG(", table search bus error");
		if (result & CMMU_DSR_SU) DEBUG_MSG(", sup prot");
		if (result & CMMU_DSR_WE) DEBUG_MSG(", write prot");
		if (result & CMMU_DSR_PH) DEBUG_MSG(", PATC");
		if (result & CMMU_DSR_BH) DEBUG_MSG(", BATC");
	} else {
		DEBUG_MSG("probe of 0x%08x missed the ATCs", address);
	}
	DEBUG_MSG(".\n");

	/******* INTERPRET AREA DESCRIPTOR *********/
	{
		if (verbose_flag > 1) {
			DEBUG_MSG(" %cAPR is 0x%08x\n",
				  supervisor_flag ? 'S' : 'U', value);
		}
		DEBUG_MSG(" %cAPR: SegTbl: 0x%x000p",
			  supervisor_flag ? 'S' : 'U', PG_PFNUM(value));
		if (value & CACHE_WT)
			DEBUG_MSG(", WTHRU");
		if (value & CACHE_GLOBAL)
			DEBUG_MSG(", GLOBAL");
		if (value & CACHE_INH)
			DEBUG_MSG(", INHIBIT");
		if (value & APR_V)
			DEBUG_MSG(", VALID");
		DEBUG_MSG("\n");

		/* if not valid, done now */
		if ((value & APR_V) == 0) {
			DEBUG_MSG("<would report an error, valid bit not set>\n");
			return;
		}
		value &= PG_FRAME;	/* now point to seg page */
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		DEBUG_MSG("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	virtual_address.bits = address;

	/****** ACCESS SEGMENT TABLE AND INTERPRET SEGMENT DESCRIPTOR  *******/
	{
		sdt_entry_t sdt;
		if (verbose_flag)
			DEBUG_MSG("will follow to entry %d of page at 0x%x...\n",
				  virtual_address.field.segment_table_index, value);
		value |= virtual_address.field.segment_table_index *
			 sizeof(sdt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			DEBUG_MSG("ERROR: unable to access page at 0x%08x.\n", value);
			return;
		}

		sdt = *(sdt_entry_t *)value;
		if (verbose_flag > 1)
			DEBUG_MSG("SEG DESC @0x%x is 0x%08x\n", value, sdt);
		DEBUG_MSG("SEG DESC @0x%x: PgTbl: 0x%x000",
			  value, PG_PFNUM(sdt));
		if (sdt & CACHE_WT)		    DEBUG_MSG(", WTHRU");
		else				    DEBUG_MSG(", !wthru");
		if (sdt & SG_SO)		    DEBUG_MSG(", S-PROT");
		else				    DEBUG_MSG(", UserOk");
		if (sdt & CACHE_GLOBAL)		    DEBUG_MSG(", GLOBAL");
		else				    DEBUG_MSG(", !global");
		if (sdt & CACHE_INH)		    DEBUG_MSG(", $INHIBIT");
		else				    DEBUG_MSG(", $ok");
		if (sdt & SG_PROT)		    DEBUG_MSG(", W-PROT");
		else				    DEBUG_MSG(", WriteOk");
		if (sdt & SG_V)			    DEBUG_MSG(", VALID");
		else				    DEBUG_MSG(", !valid");
		DEBUG_MSG(".\n");

		/* if not valid, done now */
		if (!(sdt & SG_V)) {
			DEBUG_MSG("<would report an error, STD entry not valid>\n");
			return;
		}
		value = ptoa(PG_PFNUM(sdt));
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		DEBUG_MSG("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	/******* PAGE TABLE *********/
	{
		pt_entry_t pte;
		if (verbose_flag)
			DEBUG_MSG("will follow to entry %d of page at 0x%x...\n",
				  virtual_address.field.page_table_index, value);
		value |= virtual_address.field.page_table_index *
			 sizeof(pt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			DEBUG_MSG("error: unable to access page at 0x%08x.\n", value);
			return;
		}

		pte = *(pt_entry_t *)value;
		if (verbose_flag > 1)
			DEBUG_MSG("PAGE DESC @0x%x is 0x%08x.\n", value, pte);
		DEBUG_MSG("PAGE DESC @0x%x: page @%x000",
			  value, PG_PFNUM(pte));
		if (pte & PG_W)			DEBUG_MSG(", WIRE");
		else				DEBUG_MSG(", !wire");
		if (pte & CACHE_WT)		DEBUG_MSG(", WTHRU");
		else				DEBUG_MSG(", !wthru");
		if (pte & PG_SO)		DEBUG_MSG(", S-PROT");
		else				DEBUG_MSG(", UserOk");
		if (pte & CACHE_GLOBAL)		DEBUG_MSG(", GLOBAL");
		else				DEBUG_MSG(", !global");
		if (pte & CACHE_INH)		DEBUG_MSG(", $INHIBIT");
		else				DEBUG_MSG(", $ok");
		if (pte & PG_M)			DEBUG_MSG(", MOD");
		else				DEBUG_MSG(", !mod");
		if (pte & PG_U)			DEBUG_MSG(", USED");
		else				DEBUG_MSG(", !used");
		if (pte & PG_PROT)		DEBUG_MSG(", W-PROT");
		else				DEBUG_MSG(", WriteOk");
		if (pte & PG_V)			DEBUG_MSG(", VALID");
		else				DEBUG_MSG(", !valid");
		DEBUG_MSG(".\n");

		/* if not valid, done now */
		if (!(pte & PG_V)) {
			DEBUG_MSG("<would report an error, PTE entry not valid>\n");
			return;
		}

		value = ptoa(PG_PFNUM(pte));
		if (verbose_flag)
			DEBUG_MSG("will follow to byte %d of page at 0x%x...\n",
				  virtual_address.field.page_offset, value);
		value |= virtual_address.field.page_offset;

		if (badwordaddr((vaddr_t)value)) {
			DEBUG_MSG("error: unable to access page at 0x%08x.\n", value);
			return;
		}
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		DEBUG_MSG("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	DEBUG_MSG("WORD at 0x%x is 0x%08x.\n", value, *(unsigned *)value);
}
#endif /* DDB */
