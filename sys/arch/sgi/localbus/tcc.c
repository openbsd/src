/*	$OpenBSD: tcc.c,v 1.4 2014/03/29 18:09:30 guenther Exp $	*/

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
 * POWER Indigo2 TBus Cache Controller (Stream Cache) support code.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <mips64/archtype.h>
#include <mips64/cache.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>

#include <sgi/sgi/ip22.h>
#include <sgi/localbus/tccreg.h>
#include <sgi/localbus/tccvar.h>

int	tcc_match(struct device *, void *, void *);
void	tcc_attach(struct device *, struct device *, void *);

const struct cfattach tcc_ca = {
	sizeof(struct device), tcc_match, tcc_attach
};

struct cfdriver tcc_cd = {
	NULL, "tcc", DV_DULL
};

uint32_t tcc_bus_error(uint32_t, struct trap_frame *);

CACHE_PROTOS(tcc)

int
tcc_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *maa = (void *)aux;

	switch (sys_config.system_type) {
	case SGI_IP26:
		return strcmp(maa->maa_name, tcc_cd.cd_name) == 0;
	default:
		return 0;
	}
}

void
tcc_attach(struct device *parent, struct device *self, void *aux)
{
	uint32_t ctrl, rev;

	ctrl = (uint32_t)tcc_read(TCC_GCACHE_CTRL);
	rev = (ctrl & TCC_GCACHE_REV_MASK) >> TCC_GCACHE_REV_SHIFT;
	printf(": streaming cache revision %d\n", rev);

	tcc_bus_reset();

	/* Enable bus error and machine check interrupts. */
	set_intr(INTPRI_BUSERR_TCC, CR_BERR, tcc_bus_error);
	tcc_write(TCC_INTR, TCC_INTR_MCHECK_ENAB | TCC_INTR_BERR_ENAB);

	/* Enable all cache sets. */
	tcc_write(TCC_GCACHE_CTRL, (ctrl | TCC_GCACHE_SET_ALL) &
	    ~TCC_GCACHE_DISABLE_WB);

	/* Enable prefetching. */
	tcc_prefetch_enable();
}

void
tcc_bus_reset()
{
	tcc_write(TCC_INTR, (tcc_read(TCC_INTR) & TCC_INTR_ENABLE_MASK) |
	    TCC_INTR_MCHECK | TCC_INTR_BERR);
	tcc_write(TCC_ERROR, TCC_ERROR_NESTED_MCHECK | TCC_ERROR_NESTED_BERR);
}

uint32_t
tcc_bus_error(uint32_t hwpend, struct trap_frame *tf)
{
	uint64_t intr, error, addr, errack;

	intr = tcc_read(TCC_INTR);
	error = tcc_read(TCC_ERROR);
	addr = tcc_read(TCC_BERR_ADDR);

	printf("tcc bus error: intr %lx error %lx (%d) addr %08lx\n",
	    intr, error, (error & TCC_ERROR_TYPE_MASK) >> TCC_ERROR_TYPE_SHIFT,
	    addr);

	/* Ack error condition */
	errack = 0;
	if (intr & TCC_INTR_MCHECK)
		errack |= TCC_ERROR_NESTED_MCHECK;
	if (intr & TCC_INTR_BERR)
		errack |= TCC_ERROR_NESTED_BERR;
	tcc_write(TCC_INTR, (intr & TCC_INTR_ENABLE_MASK) |
	    (intr & (TCC_INTR_MCHECK | TCC_INTR_BERR)));
	tcc_write(TCC_ERROR, errack);

	cp0_reset_cause(CR_BERR);
	return hwpend;
}

/*
 * Cache maintainance routines
 */

#define	tcc_cache_hit(addr,op) \
__asm__ volatile ("lw $0, %0(%1)" :: "i" (TCC_CACHEOP_HIT), \
    "r" (PHYS_TO_XKPHYS(TCC_CACHEOP_BASE | (addr) | (op), CCA_NC)) : "memory")
#define	tcc_cache_index(s,i,op) \
__asm__ volatile ("lw $0, %0(%1)" :: "i" (TCC_CACHEOP_INDEX | (op)), \
    "r" (PHYS_TO_XKPHYS(TCC_CACHEOP_BASE | ((s) << TCC_CACHEOP_SET_SHIFT) | \
	 ((i) << TCC_CACHEOP_INDEX_SHIFT), CCA_NC)) : "memory")

void tcc_virtual(struct cpu_info *, vaddr_t, vsize_t, uint64_t);

void
tcc_ConfigCache(struct cpu_info *ci)
{
	struct cache_info l2;

	l2 = ci->ci_l2;

	tfp_ConfigCache(ci);

	if (l2.size != 0) {
		ci->ci_l2 = l2;

		ci->ci_SyncCache = tcc_SyncCache;
		ci->ci_SyncDCachePage = tcc_SyncDCachePage;
		ci->ci_HitSyncDCache = tcc_HitSyncDCache;
		ci->ci_HitInvalidateDCache = tcc_HitInvalidateDCache;
		ci->ci_IOSyncDCache = tcc_IOSyncDCache;
	}
}

void
tcc_SyncCache(struct cpu_info *ci)
{
	uint64_t idx;

	mips_sync();
	tfp_InvalidateICache(ci, 0, ci->ci_l1inst.size);

	/*
	 * The following relies upon the fact that the (line, set)
	 * fields are contiguous. Therefore by pretending there is
	 * a huge number of sets and only one line, we can span the
	 * whole cache.
	 */
	idx = (uint64_t)ci->ci_l2.size / TCC_CACHE_LINE;
	while (idx != 0) {
		idx--;
		tcc_cache_index(idx, 0,
		    TCC_CACHEOP_WRITEBACK | TCC_CACHEOP_INVALIDATE);
	}
	tcc_prefetch_invalidate();
}

void
tcc_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
{
	vaddr_t epa;

	mips_sync();
	epa = pa + PAGE_SIZE;
	do {
		tcc_cache_hit(pa,
		    TCC_CACHEOP_WRITEBACK | TCC_CACHEOP_INVALIDATE);
		pa += TCC_CACHE_LINE;
	} while (pa != epa);
	tcc_prefetch_invalidate();
}

void
tcc_virtual(struct cpu_info *ci, vaddr_t va, vsize_t sz, uint64_t op)
{
	paddr_t pa;

	if (IS_XKPHYS(va)) {
		pa = XKPHYS_TO_PHYS(va);

		while (sz != 0) {
			tcc_cache_hit(pa, op);
			pa += TCC_CACHE_LINE;
			sz -= TCC_CACHE_LINE;
		}
		return;
	}

	while (sz != 0) {
		/* get the proper physical address */
		if (pmap_extract(pmap_kernel(), va, &pa) == 0) {
#ifdef DIAGNOSTIC
			panic("%s: invalid va %p", __func__, va);
#else
			/* should not happen */
#endif
		}

		while (sz != 0) {
			tcc_cache_hit(pa, op);
			pa += TCC_CACHE_LINE;
			va += TCC_CACHE_LINE;
			sz -= TCC_CACHE_LINE;
			if (sz == 0)
				return;
			if ((va & PAGE_MASK) == 0)
				break;	/* need new pmap_extract() */
		}
	}
}

void
tcc_HitSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;

	mips_sync();

	/* extend the range to integral cache lines */
	va = _va & ~(TCC_CACHE_LINE - 1);
	sz = ((_va + _sz + TCC_CACHE_LINE - 1) & ~(TCC_CACHE_LINE - 1)) - va;

	tcc_virtual(ci, va, sz, TCC_CACHEOP_WRITEBACK | TCC_CACHEOP_INVALIDATE);
	tcc_prefetch_invalidate();
}

void
tcc_HitInvalidateDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;

	mips_sync();

	/* extend the range to integral cache lines */
	va = _va & ~(TCC_CACHE_LINE - 1);
	sz = ((_va + _sz + TCC_CACHE_LINE - 1) & ~(TCC_CACHE_LINE - 1)) - va;

	tcc_virtual(ci, va, sz, TCC_CACHEOP_INVALIDATE);
	tcc_prefetch_invalidate();
}

void
tcc_IOSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz, int how)
{
	vaddr_t va;
	vsize_t sz;
	int partial_start, partial_end;

	mips_sync();

	/* extend the range to integral cache lines */
	va = _va & ~(TCC_CACHE_LINE - 1);
	sz = ((_va + _sz + TCC_CACHE_LINE - 1) & ~(TCC_CACHE_LINE - 1)) - va;

	switch (how) {
	default:
	case CACHE_SYNC_R:
		/* writeback partial cachelines */
		if (((_va | _sz) & (TCC_CACHE_LINE - 1)) != 0) {
			partial_start = va != _va;
			partial_end = va + sz != _va + _sz;
		} else {
			partial_start = partial_end = 0;
		}
		if (partial_start) {
			tcc_virtual(ci, va, TCC_CACHE_LINE,
			    TCC_CACHEOP_WRITEBACK | TCC_CACHEOP_INVALIDATE);
			va += TCC_CACHE_LINE;
			sz -= TCC_CACHE_LINE;
		}
		if (sz != 0 && partial_end) {
			sz -= TCC_CACHE_LINE;
			tcc_virtual(ci, va + sz, TCC_CACHE_LINE,
			    TCC_CACHEOP_WRITEBACK | TCC_CACHEOP_INVALIDATE);
		}
		if (sz != 0)
			tcc_virtual(ci, va, sz, TCC_CACHEOP_INVALIDATE);

		tcc_prefetch_invalidate();
		break;

	case CACHE_SYNC_X:
		tcc_virtual(ci, va, sz, TCC_CACHEOP_WRITEBACK);
		break;

	case CACHE_SYNC_W:
		tcc_virtual(ci, va, sz,
		    TCC_CACHEOP_WRITEBACK | TCC_CACHEOP_INVALIDATE);
		tcc_prefetch_invalidate();
		break;
	}
}
