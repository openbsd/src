/*	$OpenBSD: cache_tfp.c,v 1.4 2014/03/31 20:21:19 miod Exp $	*/

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
 * R8000 cache routines.
 *
 * These routines only handle the L1 cache found onboard the R8000.
 * The L2 (Streaming Cache) cache handling is apparently quite different
 * accross R8000-based designs (well... the two of them: IP21 and IP26),
 * and is handled on a per-platform basis.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <mips64/cache.h>
#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

void	tfp_dctw_zero(vaddr_t);
void	tfp_inval_icache(vsize_t);

#define	TFP_DCTW_STEP	16UL			/* 4 words per tag */

void
tfp_ConfigCache(struct cpu_info *ci)
{
	register_t cfg;

	cfg = cp0_get_config();

	/*
	 * XXX It would make sense to trust the configuration register,
	 * XXX but at least on my system it would report a 32KB I$, while
	 * XXX ARCS reports the architected 16KB.
	 * XXX Better not trust anything from the configuration register,
	 * XXX then.
	 */
#if 0
	ci->ci_l1inst.size = (1 << 11) << ((cfg >> 9) & 0x07); /* IC */
	if (cfg & (1 << 5))	/* IB */
		ci->ci_l1inst.linesize = 32;
	else
		ci->ci_l1inst.linesize = 16;

	ci->ci_l1data.size = (1 << 12) << ((cfg >> 6) & 0x07); /* DC */
	if (cfg & (1 << 4))	/* DB */
		ci->ci_l1data.linesize = 32;
	else
		ci->ci_l1data.linesize = 16;
#else
	ci->ci_l1inst.size = 16384;
	ci->ci_l1inst.linesize = 32;
	ci->ci_l1data.size = 16384;
	ci->ci_l1data.linesize = 32;
#endif

	/* R8000 L1 caches are direct */
	ci->ci_l1inst.setsize = ci->ci_l1inst.size;
	ci->ci_l1inst.sets = 1;
	ci->ci_l1data.setsize = ci->ci_l1data.size;
	ci->ci_l1data.sets = 1;

	cache_valias_mask =
	    (max(ci->ci_l1inst.size, ci->ci_l1data.size) - 1) &
	    ~PAGE_MASK;

	/* R8000 L2 cache are platform-specific, and not covered here */
	memset(&ci->ci_l2, 0, sizeof(struct cache_info));
	memset(&ci->ci_l3, 0, sizeof(struct cache_info));

	ci->ci_SyncCache = tfp_SyncCache;
	ci->ci_InvalidateICache = tfp_InvalidateICache;
	ci->ci_InvalidateICachePage = tfp_InvalidateICachePage;
	ci->ci_SyncICache = tfp_SyncICache;
	ci->ci_SyncDCachePage = tfp_SyncDCachePage;
	ci->ci_HitSyncDCache = tfp_HitSyncDCache;
	ci->ci_HitInvalidateDCache = tfp_HitInvalidateDCache;
	ci->ci_IOSyncDCache = tfp_IOSyncDCache;
}

/*
 * Writeback and invalidate all caches.
 */
void
tfp_SyncCache(struct cpu_info *ci)
{
	vaddr_t va, eva;
	register_t sr;

	tfp_InvalidateICache(ci, 0, ci->ci_l1inst.size);

	sr = disableintr();
	eva = ci->ci_l1data.size;
	for (va = 0; va < eva; va += TFP_DCTW_STEP)
		tfp_dctw_zero(va);
	setsr(sr);
}

/*
 * Invalidate I$ for the given range.
 */
void
tfp_InvalidateICache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;
	void (*inval_subr)(vsize_t);

	if (_sz >= ci->ci_l1inst.size) {
		tfp_inval_icache(ci->ci_l1inst.size);
	} else {
		/* extend the range to multiple of 32 bytes */
		va = _va & ~(32UL - 1);
		sz = ((_va + _sz + 32 - 1) & ~(32UL - 1)) - va;

		/* compute cache offset */
		va &= (ci->ci_l1inst.size - 1);
		inval_subr = (void (*)(vsize_t))
		    ((vaddr_t)tfp_inval_icache + va);
		(*inval_subr)(sz);
	}
}

/*
 * Register a given page for I$ invalidation.
 */
void
tfp_InvalidateICachePage(struct cpu_info *ci, vaddr_t va)
{
	/*
	 * Since the page size matches the size of the instruction cache,
	 * all we need to do here is remember there are postponed flushes.
	 */
	ci->ci_cachepending_l1i = 1;
}

/*
 * Perform postponed I$ invalidation.
 */
void
tfp_SyncICache(struct cpu_info *ci)
{
	if (ci->ci_cachepending_l1i != 0) {
		tfp_inval_icache(ci->ci_l1inst.size);
		ci->ci_cachepending_l1i = 0;
	}
}

/*
 * Writeback D$ for the given page.
 */
void
tfp_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
{
	/* nothing to do, D$ is write-through */
}

/*
 * Writeback D$ for the given range.
 */
void
tfp_HitSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	/* nothing to do, D$ is write-through */
}

/*
 * Invalidate D$ for the given range.
 */
void
tfp_HitInvalidateDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va, eva;
	vsize_t sz;
	register_t sr;

	/* extend the range to multiples of the D$ tag span */
	va = _va & ~(TFP_DCTW_STEP - 1);
	sz = ((_va + _sz + TFP_DCTW_STEP - 1) & ~(TFP_DCTW_STEP - 1)) - va;

	sr = disableintr();
	for (eva = va + sz; va < eva; va += TFP_DCTW_STEP)
		tfp_dctw_zero(va);
	setsr(sr);
}

/*
 * Backend for bus_dmamap_sync(). Enforce coherency of the given range
 * by performing the necessary cache writeback and/or invalidate
 * operations.
 */
void
tfp_IOSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz, int how)
{
	switch (how) {
	case CACHE_SYNC_R:
	case CACHE_SYNC_X:
		tfp_HitInvalidateDCache(ci, _va, _sz);
		break;
	case CACHE_SYNC_W:
		/* nothing to do */
		break;
	}
}
