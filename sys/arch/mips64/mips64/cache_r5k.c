/*	$OpenBSD: cache_r5k.c,v 1.11 2014/03/11 20:32:42 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
 * Copyright (c) 1998-2004 Opsycon AB (www.opsycon.se)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Cache handling for R5000 processor and close relatives:
 * R4600/R4700, R5000, RM52xx, RM7xxx, RM9xxx.
 *
 * The following assumptions are made:
 * - L1 I$ is 2-way or 4-way (RM7k/RM9k), VIPT, 32 bytes/line, up to 32KB
 * - L1 D$ is 2-way or 4-way (RM7k/RM9k), VIPT, write-back, 32 bytes/line,
 *   up to 32KB
 * - `internal' L2 on RM7k/RM9k is 4-way, PIPT, write-back, 32 bytes/line,
 *   256KB total
 * - L3 (on RM7k/RM9k) or `external' L2 (all others) may not exist. If it
 *   does, it is direct-mapped, PIPT, write-through
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <mips64/cache.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>

#include <uvm/uvm_extern.h>

#define	IndexInvalidate_I	0x00
#define	IndexWBInvalidate_D	0x01
#define	IndexWBInvalidate_S	0x03

#define	IndexStoreTag_S		0x0b

#define	HitInvalidate_D		0x11
#define	HitInvalidate_S		0x13

#define	HitWBInvalidate_D	0x15
#define	InvalidatePage_T	0x16	/* Only RM7k/RM9k */
#define	HitWBInvalidate_S	0x17
#define	InvalidatePage_S	0x17	/* Only RM527[0-1] */

/*
 *  R5000 and RM52xx config register bits.
 */
#define	CF_5_SE		(1U << 12)	/* Secondary cache enable */
#define	CF_5_SC		(1U << 17)	/* Secondary cache not present */
#define	CF_5_SS		(3U << 20)	/* Secondary cache size */
#define	CF_5_SS_AL	20		/* Shift to align */

/*
 *  RM7000 config register bits.
 */
#define	CF_7_SE		(1U << 3)	/* Secondary cache enable */
#define	CF_7_SC		(1U << 31)	/* Secondary cache not present */
#define	CF_7_TE		(1U << 12)	/* Tertiary cache enable */
#define	CF_7_TC		(1U << 17)	/* Tertiary cache not present */
#define	CF_7_TS		(3U << 20)	/* Tertiary cache size */
#define	CF_7_TS_AL	20		/* Shift to align */


#define	R5K_LINE	32UL		/* internal cache line */
#define	R5K_PAGE	4096UL		/* external cache page */


/*
 * Cache configuration
 */
#define	CTYPE_HAS_IL2		0x01	/* Internal L2 Cache present */
#define	CTYPE_HAS_XL2		0x02	/* External L2 Cache present */
#define	CTYPE_HAS_XL3		0x04	/* External L3 Cache present */

#define	nop4()			__asm__ __volatile__ \
	("nop; nop; nop; nop")
#define	nop10()			__asm__ __volatile__ \
	("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop")

#define	cache(op,offs,addr)	__asm__ __volatile__ \
	("cache %0, %1(%2)" :: "i"(op), "i"(offs), "r"(addr) : "memory")

#define	reset_taglo()		__asm__ __volatile__ \
	("mtc0 $zero, $28")	/* COP_0_TAG_LO */
#define	reset_taghi()		__asm__ __volatile__ \
	("mtc0 $zero, $29")	/* COP_0_TAG_HI */

static __inline__ register_t
get_config(void)
{
	register_t cfg;
	__asm__ __volatile__ ("mfc0 %0, $16" : "=r"(cfg)); /* COP_0_CONFIG */
	return cfg;
}

static __inline__ void
set_config(register_t cfg)
{
	__asm__ __volatile__ ("mtc0 %0, $16" :: "r"(cfg)); /* COP_0_CONFIG */
	/* MTC0_HAZARD */
#ifdef CPU_RM7000
	nop10();
#else
	nop4();
#endif
}

static __inline__ void	mips5k_hitinv_primary(vaddr_t, vsize_t);
static __inline__ void	mips5k_hitinv_secondary(vaddr_t, vsize_t);
static __inline__ void	mips5k_hitwbinv_primary(vaddr_t, vsize_t);
static __inline__ void	mips5k_hitwbinv_secondary(vaddr_t, vsize_t);

void mips5k_l2_init(register_t);
void mips7k_l2_init(register_t);
void mips7k_l3_init(register_t);
void mips5k_c0_cca_update(register_t);
static void run_uncached(void (*)(register_t), register_t);

/*
 * Invoke a simple routine from uncached space (either CKSEG1 or uncached
 * XKPHYS).
 */

static void
run_uncached(void (*fn)(register_t), register_t arg)
{
	vaddr_t va;
	paddr_t pa;

	va = (vaddr_t)fn;
	if (IS_XKPHYS(va)) {
		pa = XKPHYS_TO_PHYS(va);
		va = PHYS_TO_XKPHYS(pa, CCA_NC);
	} else {
		pa = CKSEG0_TO_PHYS(va);
		va = PHYS_TO_CKSEG1(pa);
	}
	fn = (void (*)(register_t))va;

	(*fn)(arg);
}


/*
 * Initialize the external L2 cache of an R5000 (or close relative) processor.
 * INTENDED TO BE RUN UNCACHED - BE SURE TO CHECK THAT IT WON'T STORE ANYTHING
 * ON THE STACK IN THE ASSEMBLY OUTPUT EVERYTIME YOU CHANGE IT.
 */
void
mips5k_l2_init(register_t l2size)
{
	register vaddr_t va, eva;
	register register_t cfg;

	cfg = get_config();
	cfg |= CF_5_SE;
	set_config(cfg);

	reset_taglo();
	reset_taghi();

	va = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = va + l2size;
	while (va != eva) {
		cache(InvalidatePage_S, 0, va);
		va += R5K_PAGE;
	}
}

/*
 * Initialize the internal L2 cache of an RM7000 (or close relative) processor.
 * INTENDED TO BE RUN UNCACHED - BE SURE TO CHECK THAT IT WON'T STORE ANYTHING
 * ON THE STACK IN THE ASSEMBLY OUTPUT EVERYTIME YOU CHANGE IT.
 */
void
mips7k_l2_init(register_t l2size)
{
	register vaddr_t va, eva;
	register register_t cfg;

	cfg = get_config();
	cfg |= CF_7_SE;
	set_config(cfg);

	reset_taglo();
	reset_taghi();

	va = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = va + l2size;
	while (va != eva) {
		cache(IndexStoreTag_S, 0, va);
		va += R5K_LINE;
	}
	mips_sync();

	va = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = va + l2size;
	while (va != eva) {
		__asm__ __volatile__
		    ("lw $zero, 0(%0)" :: "r"(va));
		va += R5K_LINE;
	}
	mips_sync();

	va = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = va + l2size;
	while (va != eva) {
		cache(IndexStoreTag_S, 0, va);
		va += R5K_LINE;
	}
	mips_sync();
}

/*
 * Initialize the external L3 cache of an RM7000 (or close relative) processor.
 * INTENDED TO BE RUN UNCACHED - BE SURE TO CHECK THAT IT WON'T STORE ANYTHING
 * ON THE STACK IN THE ASSEMBLY OUTPUT EVERYTIME YOU CHANGE IT.
 */
void
mips7k_l3_init(register_t l3size)
{
	register vaddr_t va, eva;
	register register_t cfg;

	cfg = get_config();
	cfg |= CF_7_TE;
	set_config(cfg);

	reset_taglo();
	reset_taghi();

	va = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = va + l3size;
	while (va != eva) {
		cache(InvalidatePage_T, 0, va);
		va += R5K_PAGE;
	}
}

/*
 * Update the coherency of KSEG0.
 * INTENDED TO BE RUN UNCACHED - BE SURE TO CHECK THAT IT WON'T STORE ANYTHING
 * ON THE STACK IN THE ASSEMBLY OUTPUT EVERYTIME YOU CHANGE IT.
 */
void
mips5k_c0_cca_update(register_t cfg)
{
	set_config(cfg);

#if defined(CPU_R5000) || defined(CPU_RM7000)
	/*
	 * RM52xx and RM7000 hazard: after updating the K0 field of the Config
	 * register, the KSEG0 and CKSEG0 address segments should not be used
	 * for 5 cycles. The register modification and the use of these address
	 * segments should be separated by at least five (RM52xx) or
	 * ten (RM7000) integer instructions.
	 */
	nop10();
#endif
}

/*
 * Discover cache configuration and update cpu_info accordingly.
 * Initialize L2 and L3 caches found if necessary.
 */
void
Mips5k_ConfigCache(struct cpu_info *ci)
{
	register_t cfg, ncfg;
	uint setshift;

	cfg = cp0_get_config();

	/* L1 cache */
	ci->ci_l1inst.size = (1 << 12) << ((cfg >> 9) & 0x07); /* IC */
	ci->ci_l1inst.linesize = R5K_LINE;
	ci->ci_l1data.size = (1 << 12) << ((cfg >> 6) & 0x07); /* DC */
	ci->ci_l1data.linesize = R5K_LINE;

	/* sane defaults */
	setshift = 1;
	memset(&ci->ci_l2, 0, sizeof(struct cache_info));
	memset(&ci->ci_l3, 0, sizeof(struct cache_info));
	ci->ci_cacheconfiguration = 0;

	switch ((cp0_get_prid() >> 8) & 0xff) {
	default:
		/* shouldn't happen, really; but we can't panic here */
		break;
#ifdef CPU_R4600
	case MIPS_R4600:
	case MIPS_R4700:
		/* no external L2 interface */
		break;
#endif
#ifdef CPU_R5000
	case MIPS_R5000:
	case MIPS_RM52X0:
		/* optional external direct L2 cache */
		if ((cfg & CF_5_SC) == 0) {
			ci->ci_l2.size = (1 << 19) <<
			    ((cfg & CF_5_SS) >> CF_5_SS_AL);
			ci->ci_l2.linesize = R5K_LINE;
			ci->ci_l2.setsize = ci->ci_l2.size;
			ci->ci_l2.sets = 1;
		}
		if (ci->ci_l2.size != 0) {
			ci->ci_cacheconfiguration |= CTYPE_HAS_XL2;
			cfg |= CF_5_SE;
			run_uncached(mips5k_l2_init, ci->ci_l2.size);
		}
		break;
#endif	/* CPU_R5000 */
#ifdef CPU_RM7000
	case MIPS_RM7000:
	case MIPS_RM9000:
		setshift = 2;
		/* optional external direct L3 cache */
		if ((cfg & CF_7_TC) == 0) {
#ifndef L3SZEXT
			/*
			 * Assume L3 size is provided in the system information
			 * field of the Config register. This is usually the
			 * case on systems where the RM7k/RM9k processor is
			 * an upgrade from an R5000/RM52xx processor, such as
			 * the SGI O2.
			 */
			ci->ci_l3.size = (1 << 19) <<
			    ((cfg & CF_7_TS) >> CF_7_TS_AL);
			ci->ci_l3.linesize = R5K_LINE;
			ci->ci_l3.setsize = ci->ci_l3.size;
			ci->ci_l3.sets = 1;
#else
			/*
			 * Assume machdep has initialized ci_l3 for us.
			 */
#endif
		}
		if (ci->ci_l3.size != 0) {
			ci->ci_cacheconfiguration |= CTYPE_HAS_XL3;
			cfg |= CF_7_TE;
			run_uncached(mips7k_l3_init, ci->ci_l3.size);

		}
		/* internal 4-way L2 cache */
		if ((cfg & CF_7_SC) == 0) {
			ci->ci_l2.size = 256 * 1024;	/* fixed size */
			ci->ci_l2.linesize = R5K_LINE;
			ci->ci_l2.setsize = ci->ci_l2.size / 4;
			ci->ci_l2.sets = 4;
		}
		if (ci->ci_l2.size != 0) {
			ci->ci_cacheconfiguration |= CTYPE_HAS_IL2;
			if ((cfg & CF_7_SE) == 0) {
				cfg |= CF_7_SE;
				run_uncached(mips7k_l2_init, ci->ci_l2.size);
			}
		}
		break;
#endif	/* CPU_RM7000 */
	}

	ci->ci_l1inst.setsize = ci->ci_l1inst.size >> setshift;
	ci->ci_l1inst.sets = setshift == 2 ? 4 : 2;
	ci->ci_l1data.setsize = ci->ci_l1data.size >> setshift;
	ci->ci_l1data.sets = setshift == 2 ? 4 : 2;

	cache_valias_mask =
	    (max(ci->ci_l1inst.setsize, ci->ci_l1data.setsize) - 1) &
	    ~PAGE_MASK;

	if (cache_valias_mask != 0) {
		cache_valias_mask |= PAGE_MASK;
		pmap_prefer_mask = cache_valias_mask;
	}

	ci->ci_SyncCache = Mips5k_SyncCache;
	ci->ci_InvalidateICache = Mips5k_InvalidateICache;
	ci->ci_SyncDCachePage = Mips5k_SyncDCachePage;
	ci->ci_HitSyncDCache = Mips5k_HitSyncDCache;
	ci->ci_HitInvalidateDCache = Mips5k_HitInvalidateDCache;
	ci->ci_IOSyncDCache = Mips5k_IOSyncDCache;

	ncfg = (cfg & ~CFGR_CCA_MASK) | CCA_CACHED;
	if (cfg != ncfg)
		run_uncached(mips5k_c0_cca_update, ncfg);
}

/*
 * Writeback and invalidate all caches.
 */
void
Mips5k_SyncCache(struct cpu_info *ci)
{
	vaddr_t sva, eva;
#ifdef CPU_R4600
	/*
	 * Revision 1 R4600 need to perform `Index' cache operations with
	 * interrupt disabled, to make sure both ways are correctly updated.
	 */
	register_t sr = disableintr();
#endif

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l1inst.size;
	while (sva != eva) {
		cache(IndexInvalidate_I, 0, sva);
		sva += R5K_LINE;
	}

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l1data.size;
	while (sva != eva) {
		cache(IndexWBInvalidate_D, 0, sva);
		sva += R5K_LINE;
	}

#ifdef CPU_R4600
	setsr(sr);
#endif

#ifdef CPU_RM7000
	if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2) {
		sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
		eva = sva + ci->ci_l2.size;
		while (sva != eva) {
			cache(IndexWBInvalidate_S, 0, sva);
			sva += R5K_LINE;
		}
	} else
#endif
#ifdef CPU_R5000
	if (ci->ci_cacheconfiguration & CTYPE_HAS_XL2) {
		sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
		eva = sva + ci->ci_l2.size;
		reset_taglo();
		while (sva != eva) {
			cache(InvalidatePage_S, 0, sva);
			sva += R5K_PAGE;
		}
	} else
#endif
	{
	}

#ifdef CPU_RM7000
	if (ci->ci_cacheconfiguration & CTYPE_HAS_XL3) {
		sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
		eva = sva + ci->ci_l3.size;
		reset_taglo();
		while (sva != eva) {
			cache(InvalidatePage_T, 0, sva);
			sva += R5K_PAGE;
		}
	}
#endif

	mips_sync();
}

/*
 * Invalidate I$ for the given range.
 */
void
Mips5k_InvalidateICache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va, sva, eva, iva;
	vsize_t sz, offs;
#ifdef CPU_R4600
	/*
	 * Revision 1 R4600 need to perform `Index' cache operations with
	 * interrupt disabled, to make sure both ways are correctly updated.
	 */
	register_t sr = disableintr();
#endif

	/* extend the range to integral cache lines */
	va = _va & ~(R5K_LINE - 1);
	sz = ((_va + _sz + R5K_LINE - 1) & ~(R5K_LINE - 1)) - va;

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	offs = ci->ci_l1inst.setsize;
	/* keep only the index bits */
	sva |= va & (offs - 1);
	eva = sva + sz;

	switch (ci->ci_l1inst.sets) {
	default:
#ifdef CPU_RM7000
	case 4:
		while (sva != eva) {
			iva = sva;
			cache(IndexInvalidate_I, 0, iva);
			iva += offs;
			cache(IndexInvalidate_I, 0, iva);
			iva += offs;
			cache(IndexInvalidate_I, 0, iva);
			iva += offs;
			cache(IndexInvalidate_I, 0, iva);
			sva += R5K_LINE;
		}
		break;
#endif
#if defined(CPU_R5000) || defined(CPU_R4600)
	case 2:
		iva = sva + offs;
		while (sva != eva) {
			cache(IndexInvalidate_I, 0, iva);
			cache(IndexInvalidate_I, 0, sva);
			iva += R5K_LINE;
			sva += R5K_LINE;
		}
		break;
#endif
	}

#ifdef CPU_R4600
	setsr(sr);
#endif
	mips_sync();
}

static __inline__ void
mips5k_hitwbinv_primary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitWBInvalidate_D, 0, va);
		va += R5K_LINE;
	}
}

static __inline__ void
mips5k_hitwbinv_secondary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitWBInvalidate_S, 0, va);
		cache(HitWBInvalidate_D, 0, va);	/* orphans in L1 */
		va += R5K_LINE;
	}
}

/*
 * Writeback D$ for the given page.
 */
void
Mips5k_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
{
#ifdef CPU_R4600
	/*
	 * Revision 1 R4600 need to perform `Index' cache operations with
	 * interrupt disabled, to make sure both ways are correctly updated.
	 */
	register_t sr = disableintr();
#endif

	switch (ci->ci_l1data.sets) {
	default:
#ifdef CPU_RM7000
	case 4:
		/*
		 * On RM7000 and RM9000, the D$ cache set is never larger than
		 * the page size, causing it to behave as a physically-indexed
		 * cache. We can thus use Hit operations on the physical
		 * address.
		 */
		if ((ci->ci_cacheconfiguration & CTYPE_HAS_IL2) == 0) {
			mips5k_hitwbinv_primary(PHYS_TO_XKPHYS(pa, CCA_CACHED),
			    PAGE_SIZE);
		} /* else
			done below */
		break;
#endif
#if defined(CPU_R5000) || defined(CPU_R4600)
	case 2:
	    {
		vaddr_t sva, eva, iva;
		vsize_t offs;

		sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
		offs = ci->ci_l1data.setsize;
		/* keep only the index bits */
		sva |= va & (offs - 1);
		eva = sva + PAGE_SIZE;

		iva = sva + offs;
		while (sva != eva) {
			cache(IndexWBInvalidate_D, 0, iva);
			cache(IndexWBInvalidate_D, 0, sva);
			iva += R5K_LINE;
			sva += R5K_LINE;
		}
	    }
		break;
#endif
	}

#ifdef CPU_R4600
	setsr(sr);
#endif

#ifdef CPU_RM7000
	if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2) {
		mips5k_hitwbinv_secondary(PHYS_TO_XKPHYS(pa, CCA_CACHED),
		    PAGE_SIZE);
	}
#endif

	mips_sync();
}

/*
 * Writeback D$ for the given range. Range is expected to be currently
 * mapped, allowing the use of `Hit' operations. This is less aggressive
 * than using `Index' operations.
 */

void
Mips5k_HitSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;

	/* extend the range to integral cache lines */
	va = _va & ~(R5K_LINE - 1);
	sz = ((_va + _sz + R5K_LINE - 1) & ~(R5K_LINE - 1)) - va;

#if 0
	if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2)
		mips5k_hitwbinv_secondary(va, sz);
	else
#endif
	     {
#ifdef CPU_R4600
		/*
		 * R4600 revision 2 needs to load from an uncached address
		 * before any Hit or CreateDEX operation. Alternatively, 12
		 * nop (cycles) will empty the cache load buffer.
		 * We are only putting 10 here, and hope the overhead of the
		 * code around will provide the rest.
		 */
		nop10();
#endif
		mips5k_hitwbinv_primary(va, sz);
	}

	mips_sync();
}

/*
 * Invalidate D$ for the given range. Range is expected to be currently
 * mapped, allowing the use of `Hit' operations. This is less aggressive
 * than using `Index' operations.
 */

static __inline__ void
mips5k_hitinv_primary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitInvalidate_D, 0, va);
		va += R5K_LINE;
	}
}

static __inline__ void
mips5k_hitinv_secondary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitInvalidate_S, 0, va);
		cache(HitInvalidate_D, 0, va);	/* orphans in L1 */
		va += R5K_LINE;
	}
}

void
Mips5k_HitInvalidateDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;

	/* extend the range to integral cache lines */
	va = _va & ~(R5K_LINE - 1);
	sz = ((_va + _sz + R5K_LINE - 1) & ~(R5K_LINE - 1)) - va;

#if 0
	if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2)
		mips5k_hitinv_secondary(va, sz);
	else
#endif
	     {
#ifdef CPU_R4600
		/*
		 * R4600 revision 2 needs to load from an uncached address
		 * before any Hit or CreateDEX operation. Alternatively, 12
		 * nop (cycles) will empty the cache load buffer.
		 * We are only putting 10 here, and hope the overhead of the
		 * code around will provide the rest.
		 */
		nop10();
#endif
		mips5k_hitinv_primary(va, sz);
	}

	mips_sync();
}

/*
 * Backend for bus_dmamap_sync(). Enforce coherency of the given range
 * by performing the necessary cache writeback and/or invalidate
 * operations.
 */
void
Mips5k_IOSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz, int how)
{
	vaddr_t va;
	vsize_t sz;
	int partial_start, partial_end;

	/*
	 * internal cache
	 */

	/* extend the range to integral cache lines */
	va = _va & ~(R5K_LINE - 1);
	sz = ((_va + _sz + R5K_LINE - 1) & ~(R5K_LINE - 1)) - va;

#ifdef CPU_R4600
	/*
	 * R4600 revision 2 needs to load from an uncached address
	 * before any Hit or CreateDEX operation. Alternatively, 12
	 * nop (cycles) will empty the cache load buffer.
	 * We are only putting 10 here, and hope the overhead of the
	 * code around will provide the rest.
	 */
	nop10();
#endif

	switch (how) {
	case CACHE_SYNC_R:
		/* writeback partial cachelines */
		if (((_va | _sz) & (R5K_LINE - 1)) != 0) {
			partial_start = va != _va;
			partial_end = va + sz != _va + _sz;
		} else {
			partial_start = partial_end = 0;
		}
		if (partial_start) {
#ifdef CPU_RM7000
			if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2)
				cache(HitWBInvalidate_S, 0, va);
#endif
			cache(HitWBInvalidate_D, 0, va);
			va += R5K_LINE;
			sz -= R5K_LINE;
		}
		if (sz != 0 && partial_end) {
			sz -= R5K_LINE;
#ifdef CPU_RM7000
			if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2)
				cache(HitWBInvalidate_S, 0, va + sz);
#endif
			cache(HitWBInvalidate_D, 0, va + sz);
		}

		if (sz != 0) {
#ifdef CPU_RM7000
			if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2)
				mips5k_hitinv_secondary(va, sz);
			else
#endif
				mips5k_hitinv_primary(va, sz);
		}
		break;

	case CACHE_SYNC_X:
	case CACHE_SYNC_W:
#ifdef CPU_RM7000
		if (ci->ci_cacheconfiguration & CTYPE_HAS_IL2)
			mips5k_hitwbinv_secondary(va, sz);
		else
#endif
			mips5k_hitwbinv_primary(va, sz);
		break;
	}

	/*
	 * external cache
	 */

	switch (how) {
	case CACHE_SYNC_W:
		break;
	case CACHE_SYNC_X:
	case CACHE_SYNC_R:
#ifdef CPU_R5000
		if (ci->ci_cacheconfiguration & CTYPE_HAS_XL2) {
			/* align on external page size */
			va = _va & ~(R5K_PAGE - 1);
			sz = ((_va + _sz + R5K_PAGE - 1) - va) / R5K_PAGE;
			reset_taglo();
			while (sz != 0) {
				cache(InvalidatePage_S, 0, va);
				va += R5K_PAGE;
				sz--;
			}
		} else
#endif
#ifdef CPU_RM7000
		if (ci->ci_cacheconfiguration & CTYPE_HAS_XL3) {
			/* align on external page size */
			va = _va & ~(R5K_PAGE - 1);
			sz = ((_va + _sz + R5K_PAGE - 1) - va) / R5K_PAGE;
			reset_taglo();
			while (sz != 0) {
				cache(InvalidatePage_T, 0, va);
				va += R5K_PAGE;
				sz--;
			}
		} else
#endif
		{
		}
		break;
	}

	mips_sync();
}
