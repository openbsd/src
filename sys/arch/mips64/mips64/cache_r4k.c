/*	$OpenBSD: cache_r4k.c,v 1.5 2012/05/27 19:13:04 miod Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>

#include <mips64/cache.h>
#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

#define	IndexInvalidate_I	0x00
#define	IndexWBInvalidate_D	0x01
#define	IndexWBInvalidate_S	0x03

#define	HitInvalidate_D		0x11
#define	HitInvalidate_S		0x13

#define	HitWBInvalidate_D	0x15
#define	HitWBInvalidate_S	0x17

#define	cache(op,addr) \
    __asm__ __volatile__ ("cache %0, 0(%1)" :: "i"(op), "r"(addr) : "memory")
#define	sync() \
    __asm__ __volatile__ ("sync" ::: "memory");

static __inline__ void	mips4k_hitinv_primary(vaddr_t, vsize_t, vsize_t);
static __inline__ void	mips4k_hitinv_secondary(vaddr_t, vsize_t, vsize_t);
static __inline__ void	mips4k_hitwbinv_primary(vaddr_t, vsize_t, vsize_t);
static __inline__ void	mips4k_hitwbinv_secondary(vaddr_t, vsize_t, vsize_t);

void
Mips4k_ConfigCache(struct cpu_info *ci)
{
	uint32_t cfg, ncfg;

	cfg = cp0_get_config();

	if (cfg & (1 << 5))	/* IB */
		ci->ci_l1instcacheline = 32;
	else
		ci->ci_l1instcacheline = 16;
	ci->ci_l1instcachesize = (1 << 12) << ((cfg >> 9) & 0x07); /* IC */

	if (cfg & (1 << 4))	/* DB */
		ci->ci_l1datacacheline = 32;
	else
		ci->ci_l1datacacheline = 16;
	ci->ci_l1datacachesize = (1 << 12) << ((cfg >> 6) & 0x07); /* DC */

	/* R4000 and R4400 L1 caches are direct */
	ci->ci_cacheways = 1;
	ci->ci_l1instcacheset = ci->ci_l1instcachesize;
	ci->ci_l1datacacheset = ci->ci_l1datacachesize;

	cache_valias_mask =
	    (max(ci->ci_l1instcachesize, ci->ci_l1datacachesize) - 1) &
	    ~PAGE_MASK;

	if ((cfg & (1 << 17)) == 0) {	/* SC */
		/*
		 * We expect the setup code to have set up ci->ci_l2size for
		 * us. Unfortunately we aren't allowed to panic() there,
		 * because the console is not available.
		 */

		/* fixed 32KB aliasing to avoid VCE */
		pmap_prefer_mask = ((1 << 15) - 1);
	} else {
		ci->ci_l2line = 0;
		ci->ci_l2size = 0;
	}
	ci->ci_l3size = 0;

	if (cache_valias_mask != 0) {
		cache_valias_mask |= PAGE_MASK;
		pmap_prefer_mask |= cache_valias_mask;
	}

	ncfg = (cfg & ~7) | CCA_CACHED;
	ncfg &= ~(1 << 4);
	if (cfg != ncfg) {
		void (*fn)(uint32_t);
		vaddr_t va;
		paddr_t pa;

		va = (vaddr_t)&cp0_set_config;
		if (IS_XKPHYS(va)) {
			pa = XKPHYS_TO_PHYS(va);
			va = PHYS_TO_XKPHYS(pa, CCA_NC);
		} else {
			pa = CKSEG0_TO_PHYS(va);
			va = PHYS_TO_CKSEG1(pa);
		}
		fn = (void (*)(uint32_t))va;

		(*fn)(ncfg);
	}
}

/*
 * Writeback and invalidate all caches.
 */
void
Mips4k_SyncCache(struct cpu_info *ci)
{
	vaddr_t sva, eva;
	vsize_t line;

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l1instcachesize;
	line = ci->ci_l1instcacheline;
	while (sva != eva) {
		cache(IndexInvalidate_I, sva);
		sva += line;
	}

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l1datacachesize;
	line = ci->ci_l1datacacheline;
	while (sva != eva) {
		cache(IndexWBInvalidate_D, sva);
		sva += line;
	}

	if (ci->ci_l2size != 0) {
		sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
		eva = sva + ci->ci_l2size;
		line = ci->ci_l2line;
		while (sva != eva) {
			cache(IndexWBInvalidate_S, sva);
			sva += line;
		}
	}

	sync();
}

/*
 * Invalidate I$ for the given range.
 */
void
Mips4k_InvalidateICache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va, sva, eva;
	vsize_t sz;
	vsize_t line;

	line = ci->ci_l1instcacheline;
	/* extend the range to integral cache lines */
	if (line == 16) {
		va = _va & ~(16UL - 1);
		sz = ((_va + _sz + 16 - 1) & ~(16UL - 1)) - va;
	} else {
		va = _va & ~(32UL - 1);
		sz = ((_va + _sz + 32 - 1) & ~(32UL - 1)) - va;
	}

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	/* keep only the index bits */
	sva += va & ((1UL << 15) - 1);
	eva = sva + sz;
	while (sva != eva) {
		cache(IndexInvalidate_I, sva);
		sva += line;
	}

	sync();
}

/*
 * Writeback D$ for the given page.
 */
void
Mips4k_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
{
	vaddr_t sva, eva;
	vsize_t line;

	line = ci->ci_l1datacacheline;
	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	/* keep only the index bits */
	sva += va & ((1UL << 15) - 1);
	eva = sva + PAGE_SIZE;
	while (sva != eva) {
		cache(IndexWBInvalidate_D, sva);
		sva += line;
	}

	if (ci->ci_l2size != 0) {
		line = ci->ci_l2line;
		sva = PHYS_TO_XKPHYS(pa, CCA_CACHED);
		eva = sva + PAGE_SIZE;
		while (sva != eva) {
			cache(IndexWBInvalidate_S, sva);
			sva += line;
		}
	}

	sync();
}

/*
 * Writeback D$ for the given range. Range is expected to be currently
 * mapped, allowing the use of `Hit' operations. This is less aggressive
 * than using `Index' operations.
 */

static __inline__ void
mips4k_hitwbinv_primary(vaddr_t va, vsize_t sz, vsize_t line)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitWBInvalidate_D, va);
		va += line;
	}
}

static __inline__ void
mips4k_hitwbinv_secondary(vaddr_t va, vsize_t sz, vsize_t line)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitWBInvalidate_S, va);
		va += line;
	}
}

void
Mips4k_HitSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;
	vsize_t line;

	line = ci->ci_l1datacacheline;
	/* extend the range to integral cache lines */
	if (line == 16) {
		va = _va & ~(16UL - 1);
		sz = ((_va + _sz + 16 - 1) & ~(16UL - 1)) - va;
	} else {
		va = _va & ~(32UL - 1);
		sz = ((_va + _sz + 32 - 1) & ~(32UL - 1)) - va;
	}
	mips4k_hitwbinv_primary(va, sz, line);

	if (ci->ci_l2size != 0) {
		line = ci->ci_l2line;
		/* extend the range to integral cache lines */
		va = _va & ~(line - 1);
		sz = ((_va + _sz + line - 1) & ~(line - 1)) - va;
		mips4k_hitwbinv_secondary(va, sz, line);
	}

	sync();
}

/*
 * Invalidate D$ for the given range. Range is expected to be currently
 * mapped, allowing the use of `Hit' operations. This is less aggressive
 * than using `Index' operations.
 */

static __inline__ void
mips4k_hitinv_primary(vaddr_t va, vsize_t sz, vsize_t line)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitInvalidate_D, va);
		va += line;
	}
}

static __inline__ void
mips4k_hitinv_secondary(vaddr_t va, vsize_t sz, vsize_t line)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitInvalidate_S, va);
		va += line;
	}
}

void
Mips4k_HitInvalidateDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;
	vsize_t line;

	line = ci->ci_l1datacacheline;
	/* extend the range to integral cache lines */
	if (line == 16) {
		va = _va & ~(16UL - 1);
		sz = ((_va + _sz + 16 - 1) & ~(16UL - 1)) - va;
	} else {
		va = _va & ~(32UL - 1);
		sz = ((_va + _sz + 32 - 1) & ~(32UL - 1)) - va;
	}
	mips4k_hitinv_primary(va, sz, line);

	if (ci->ci_l2size != 0) {
		line = ci->ci_l2line;
		/* extend the range to integral cache lines */
		va = _va & ~(line - 1);
		sz = ((_va + _sz + line - 1) & ~(line - 1)) - va;
		mips4k_hitinv_secondary(va, sz, line);
	}

	sync();
}

/*
 * Backend for bus_dmamap_sync(). Enforce coherency of the given range
 * by performing the necessary cache writeback and/or invalidate
 * operations.
 */
void
Mips4k_IOSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz, int how)
{
	vaddr_t va;
	vsize_t sz;
	vsize_t line;
	int partial_start, partial_end;

	/*
	 * L1
	 */

	line = ci->ci_l1datacacheline;
	/* extend the range to integral cache lines */
	if (line == 16) {
		va = _va & ~(16UL - 1);
		sz = ((_va + _sz + 16 - 1) & ~(16UL - 1)) - va;
	} else {
		va = _va & ~(32UL - 1);
		sz = ((_va + _sz + 32 - 1) & ~(32UL - 1)) - va;
	}

	switch (how) {
	case CACHE_SYNC_R:
		/* writeback partial cachelines */
		if (((_va | _sz) & (line - 1)) != 0) {
			partial_start = va != _va;
			partial_end = va + sz != _va + _sz;
		} else {
			partial_start = partial_end = 0;
		}
		if (partial_start) {
			cache(HitWBInvalidate_D, va);
			va += line;
			sz -= line;
		}
		if (sz != 0 && partial_end) {
			cache(HitWBInvalidate_D, va + sz - line);
			sz -= line;
		}
		mips4k_hitinv_primary(va, sz, line);
		break;
	case CACHE_SYNC_X:
	case CACHE_SYNC_W:
		mips4k_hitwbinv_primary(va, sz, line);
		break;
	}

	/*
	 * L2
	 */

	if (ci->ci_l2size != 0) {
		line = ci->ci_l2line;
		/* extend the range to integral cache lines */
		va = _va & ~(line - 1);
		sz = ((_va + _sz + line - 1) & ~(line - 1)) - va;

		switch (how) {
		case CACHE_SYNC_R:
			/* writeback partial cachelines */
			if (((_va | _sz) & (line - 1)) != 0) {
				partial_start = va != _va;
				partial_end = va + sz != _va + _sz;
			} else {
				partial_start = partial_end = 0;
			}
			if (partial_start) {
				cache(HitWBInvalidate_S, va);
				va += line;
				sz -= line;
			}
			if (sz != 0 && partial_end) {
				cache(HitWBInvalidate_S, va + sz - line);
				sz -= line;
			}
			mips4k_hitinv_secondary(va, sz, line);
			break;
		case CACHE_SYNC_X:
		case CACHE_SYNC_W:
			mips4k_hitwbinv_secondary(va, sz, line);
			break;
		}
	}
}
