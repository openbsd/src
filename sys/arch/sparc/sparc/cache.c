/*	$OpenBSD: cache.c,v 1.19 2010/06/29 21:24:41 miod Exp $	*/
/*	$NetBSD: cache.c,v 1.34 1997/09/26 22:17:23 pk Exp $	*/

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cache.c	8.2 (Berkeley) 10/30/93
 *
 */

/*
 * Cache routines.
 *
 * TODO:
 *	- rework range flush
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/ctlreg.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/cpuvar.h>

#if defined(solbourne)
#include <machine/idt.h>
#include <machine/kap.h>
#endif

struct cachestats cachestats;

int cache_alias_dist;		/* Cache anti-aliasing constants */
int cache_alias_bits;

/*
 * Enable the cache.
 * We need to clear out the valid bits first.
 */
void
sun4_cache_enable()
{
	u_int i, lim, ls, ts;

	cache_alias_bits = CPU_ISSUN4
				? CACHE_ALIAS_BITS_SUN4
				: CACHE_ALIAS_BITS_SUN4C;
	cache_alias_dist = CPU_ISSUN4
				? CACHE_ALIAS_DIST_SUN4
				: CACHE_ALIAS_DIST_SUN4C;

	ls = CACHEINFO.c_linesize;
	ts = CACHEINFO.c_totalsize;

	for (i = AC_CACHETAGS, lim = i + ts; i < lim; i += ls)
		sta(i, ASI_CONTROL, 0);

	stba(AC_SYSENABLE, ASI_CONTROL,
	     lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_CACHE);
	CACHEINFO.c_enabled = 1;

	printf("cache enabled\n");

#ifdef notyet
	if (cpuinfo.flags & SUN4_IOCACHE) {
		stba(AC_SYSENABLE, ASI_CONTROL,
		     lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_IOCACHE);
		printf("iocache enabled\n");
	}
#endif
}

#if defined(SUN4M)
void
ms1_cache_enable()
{
	u_int pcr;

	cache_alias_dist = max(
	    CACHEINFO.ic_totalsize / CACHEINFO.ic_associativity,
	    CACHEINFO.dc_totalsize / CACHEINFO.dc_associativity);
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/* We "flash-clear" the I/D caches. */
	if ((pcr & MS1_PCR_ICE) == 0)
		sta(0, ASI_ICACHECLR, 0);
	if ((pcr & MS1_PCR_DCE) == 0)
		sta(0, ASI_DCACHECLR, 0);

	/* Turn on caches */
	sta(SRMMU_PCR, ASI_SRMMU, pcr | MS1_PCR_DCE | MS1_PCR_ICE);

	CACHEINFO.c_enabled = CACHEINFO.dc_enabled = 1;

	/*
	 * When zeroing or copying pages, there might still be entries in
	 * the cache, since we don't flush pages from the cache when
	 * unmapping them (`vactype' is VAC_NONE).  Fortunately, the
	 * MS1 cache is write-through and not write-allocate, so we can
	 * use cacheable accesses while not displacing cache lines.
	 */
	cpuinfo.flags |= CPUFLG_CACHE_MANDATORY;

	printf("cache enabled\n");
}

void
viking_cache_enable()
{
	u_int pcr;

	cache_alias_dist = max(
		CACHEINFO.ic_totalsize / CACHEINFO.ic_associativity,
		CACHEINFO.dc_totalsize / CACHEINFO.dc_associativity);
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	if ((pcr & VIKING_PCR_ICE) == 0) {
		/* I-cache not on; "flash-clear" it now. */
		sta(0x80000000, ASI_ICACHECLR, 0);	/* Unlock */
		sta(0, ASI_ICACHECLR, 0);		/* clear */
	}
	if ((pcr & VIKING_PCR_DCE) == 0) {
		/* D-cache not on: "flash-clear" it. */
		sta(0x80000000, ASI_DCACHECLR, 0);
		sta(0, ASI_DCACHECLR, 0);
	}

	/* Turn on caches via MMU */
	sta(SRMMU_PCR, ASI_SRMMU, pcr | VIKING_PCR_DCE | VIKING_PCR_ICE);

	CACHEINFO.c_enabled = CACHEINFO.dc_enabled = 1;

	/* Now turn on MultiCache if it exists */
	if (cpuinfo.mxcc && CACHEINFO.ec_totalsize > 0) {
		/* Multicache controller */
		stda(MXCC_ENABLE_ADDR, ASI_CONTROL,
		     ldda(MXCC_ENABLE_ADDR, ASI_CONTROL) |
		     (u_int64_t)MXCC_ENABLE_BIT);
		cpuinfo.flags |= CPUFLG_CACHEPAGETABLES; /* Ok to cache PTEs */
		CACHEINFO.ec_enabled = 1;
	}
	printf("cache enabled\n");
}

void
hypersparc_cache_enable()
{
	int i, ls, ts;
	u_int pcr, v;
	extern u_long dvma_cachealign;

	ls = CACHEINFO.c_linesize;
	ts = CACHEINFO.c_totalsize;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/*
	 * Setup the anti-aliasing constants and DVMA alignment constraint.
	 */
	cache_alias_dist = CACHEINFO.c_totalsize;
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;
	dvma_cachealign = cache_alias_dist;

	/* Now reset cache tag memory if cache not yet enabled */
	if ((pcr & HYPERSPARC_PCR_CE) == 0)
		for (i = 0; i < ts; i += ls) {
			sta(i, ASI_DCACHETAG, 0);
			while (lda(i, ASI_DCACHETAG))
				sta(i, ASI_DCACHETAG, 0);
		}

	pcr &= ~(HYPERSPARC_PCR_CE | HYPERSPARC_PCR_CM);

	hypersparc_cache_flush_all();

	pcr |= HYPERSPARC_PCR_CE;
	if (CACHEINFO.c_vactype == VAC_WRITEBACK)
		pcr |= HYPERSPARC_PCR_CM;

	sta(SRMMU_PCR, ASI_SRMMU, pcr);
	CACHEINFO.c_enabled = 1;

	/* XXX: should add support */
	if (CACHEINFO.c_hwflush)
		panic("cache_enable: can't handle 4M with hw-flush cache");

#ifdef notyet
	/*
	 * Enable instruction cache and, on single-processor machines,
	 * disable `Unimplemented Flush Traps'.
	 */
	v = HYPERSPARC_ICCR_ICE | (ncpu == 1 ? HYPERSPARC_ICCR_FTD : 0);
#else
	v = HYPERSPARC_ICCR_FTD | HYPERSPARC_ICCR_ICE;
#endif
	wrasr(v, HYPERSPARC_ASRNUM_ICCR);

	printf("cache enabled\n");
}

void
swift_cache_enable()
{
	int i, ls, ts;
	u_int pcr;

	cache_alias_dist = max(
		CACHEINFO.ic_totalsize / CACHEINFO.ic_associativity,
		CACHEINFO.dc_totalsize / CACHEINFO.dc_associativity);
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);
	pcr |= (SWIFT_PCR_ICE | SWIFT_PCR_DCE);
	sta(SRMMU_PCR, ASI_SRMMU, pcr);

	/* Now reset cache tag memory if cache not yet enabled */
	ls = CACHEINFO.ic_linesize;
	ts = CACHEINFO.ic_totalsize;
	if ((pcr & SWIFT_PCR_ICE) == 0)
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_ICACHETAG, 0);

	ls = CACHEINFO.dc_linesize;
	ts = CACHEINFO.dc_totalsize;
	if ((pcr & SWIFT_PCR_DCE) == 0)
		for (i = 0; i < ts; i += ls) {
			sta(i, ASI_DCACHETAG, 0);
			while (lda(i, ASI_DCACHETAG))
				sta(i, ASI_DCACHETAG, 0);
		}

	CACHEINFO.c_enabled = 1;
	printf("cache enabled\n");
}

void
cypress_cache_enable()
{
	int i, ls, ts;
	u_int pcr;
	extern u_long dvma_cachealign;

	cache_alias_dist = CACHEINFO.c_totalsize;
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;
	dvma_cachealign = cache_alias_dist;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);
	pcr &= ~CYPRESS_PCR_CM;

	/* Now reset cache tag memory if cache not yet enabled */
	if ((pcr & CYPRESS_PCR_CE) == 0) {
		ls = CACHEINFO.c_linesize;
		ts = CACHEINFO.c_totalsize;
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_DCACHETAG, 0);
		pcr |= CYPRESS_PCR_CE;
	}

	/* If put in write-back mode, turn it on */
	if (CACHEINFO.c_vactype == VAC_WRITEBACK)
		pcr |= CYPRESS_PCR_CM;

	sta(SRMMU_PCR, ASI_SRMMU, pcr);
	CACHEINFO.c_enabled = 1;
	printf("cache enabled\n");
}

void
turbosparc_cache_enable()
{
	int i, ls, ts;
	u_int pcr, pcf;

	cache_alias_dist = max(
		CACHEINFO.ic_totalsize / CACHEINFO.ic_associativity,
		CACHEINFO.dc_totalsize / CACHEINFO.dc_associativity);
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/* Now reset cache tag memory if cache not yet enabled */
	ls = CACHEINFO.ic_linesize;
	ts = CACHEINFO.ic_totalsize;
	if ((pcr & TURBOSPARC_PCR_ICE) == 0)
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_ICACHETAG, 0);

	ls = CACHEINFO.dc_linesize;
	ts = CACHEINFO.dc_totalsize;
	if ((pcr & TURBOSPARC_PCR_DCE) == 0)
		for (i = 0; i < ts; i += ls) {
			sta(i, ASI_DCACHETAG, 0);
			while (lda(i, ASI_DCACHETAG))
				sta(i, ASI_DCACHETAG, 0);
		}

	pcr |= (TURBOSPARC_PCR_ICE | TURBOSPARC_PCR_DCE);
	sta(SRMMU_PCR, ASI_SRMMU, pcr);

	pcf = lda(SRMMU_PCFG, ASI_SRMMU);
	if (pcf & TURBOSPARC_PCFG_SNP)
		printf("DVMA coherent ");

	CACHEINFO.c_enabled = 1;
	printf("cache enabled\n");
}
#endif /* defined(SUN4M) */

#if defined(SUN4) || defined(SUN4C)
/*
 * Flush the current context from the cache.
 *
 * This is done by writing to each cache line in the `flush context'
 * address space (or, for hardware flush, once to each page in the
 * hardware flush space, for all cache pages).
 */
void
sun4_vcache_flush_context()
{
	char *p;
	int i, ls;

	cachestats.cs_ncxflush++;
	p = (char *)0;	/* addresses 0..cacheinfo.c_totalsize will do fine */
	if (CACHEINFO.c_hwflush) {
		ls = NBPG;
		i = CACHEINFO.c_totalsize >> PGSHIFT;
		for (; --i >= 0; p += ls)
			sta(p, ASI_HWFLUSHCTX, 0);
	} else {
		ls = CACHEINFO.c_linesize;
		i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
		for (; --i >= 0; p += ls)
			sta(p, ASI_FLUSHCTX, 0);
	}
}

/*
 * Flush the given virtual region from the cache.
 *
 * This is also done by writing to each cache line, except that
 * now the addresses must include the virtual region number, and
 * we use the `flush region' space.
 *
 * This function is only called on sun4's with 3-level MMUs; there's
 * no hw-flush space.
 */
void
sun4_vcache_flush_region(vreg)
	int vreg;
{
	int i, ls;
	char *p;

	cachestats.cs_nrgflush++;
	p = (char *)VRTOVA(vreg);	/* reg..reg+sz rather than 0..sz */
	ls = CACHEINFO.c_linesize;
	i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
	for (; --i >= 0; p += ls)
		sta(p, ASI_FLUSHREG, 0);
}

/*
 * Flush the given virtual segment from the cache.
 *
 * This is also done by writing to each cache line, except that
 * now the addresses must include the virtual segment number, and
 * we use the `flush segment' space.
 *
 * Again, for hardware, we just write each page (in hw-flush space).
 */
void
sun4_vcache_flush_segment(vreg, vseg)
	int vreg, vseg;
{
	int i, ls;
	char *p;

	cachestats.cs_nsgflush++;
	p = (char *)VSTOVA(vreg, vseg);	/* seg..seg+sz rather than 0..sz */
	if (CACHEINFO.c_hwflush) {
		ls = NBPG;
		i = CACHEINFO.c_totalsize >> PGSHIFT;
		for (; --i >= 0; p += ls)
			sta(p, ASI_HWFLUSHSEG, 0);
	} else {
		ls = CACHEINFO.c_linesize;
		i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
		for (; --i >= 0; p += ls)
			sta(p, ASI_FLUSHSEG, 0);
	}
}

/*
 * Flush the given virtual page from the cache.
 * (va is the actual address, and must be aligned on a page boundary.)
 * Again we write to each cache line.
 */
void
sun4_vcache_flush_page(va)
	int va;
{
	int i, ls;
	char *p;

#ifdef DEBUG
	if (va & PGOFSET)
		panic("cache_flush_page: asked to flush misaligned va 0x%x",va);
#endif

	cachestats.cs_npgflush++;
	p = (char *)va;
	if (CACHEINFO.c_hwflush)
		sta(p, ASI_HWFLUSHPG, 0);
	else {
		ls = CACHEINFO.c_linesize;
		i = NBPG >> CACHEINFO.c_l2linesize;
		for (; --i >= 0; p += ls)
			sta(p, ASI_FLUSHPG, 0);
	}
}

/*
 * Flush a range of virtual addresses (in the current context).
 * The first byte is at (base&~PGOFSET) and the last one is just
 * before byte (base+len).
 *
 * We choose the best of (context,segment,page) here.
 */

#define CACHE_FLUSH_MAGIC	(CACHEINFO.c_totalsize / NBPG)

void
sun4_cache_flush(base, len)
	caddr_t base;
	u_int len;
{
	int i, ls, baseoff;
	char *p;

	if (CACHEINFO.c_vactype == VAC_NONE)
		return;

	/*
	 * Figure out how much must be flushed.
	 *
	 * If we need to do CACHE_FLUSH_MAGIC pages,  we can do a segment
	 * in the same number of loop iterations.  We can also do the whole
	 * region. If we need to do between 2 and NSEGRG, do the region.
	 * If we need to do two or more regions, just go ahead and do the
	 * whole context. This might not be ideal (e.g., fsck likes to do
	 * 65536-byte reads, which might not necessarily be aligned).
	 *
	 * We could try to be sneaky here and use the direct mapping
	 * to avoid flushing things `below' the start and `above' the
	 * ending address (rather than rounding to whole pages and
	 * segments), but I did not want to debug that now and it is
	 * not clear it would help much.
	 *
	 * (XXX the magic number 16 is now wrong, must review policy)
	 */
	baseoff = (int)base & PGOFSET;
	i = (baseoff + len + PGOFSET) >> PGSHIFT;

	cachestats.cs_nraflush++;
#ifdef notyet
	cachestats.cs_ra[min(i, MAXCACHERANGE)]++;
#endif

	if (i < CACHE_FLUSH_MAGIC) {
		/* cache_flush_page, for i pages */
		p = (char *)((int)base & ~baseoff);
		if (CACHEINFO.c_hwflush) {
			for (; --i >= 0; p += NBPG)
				sta(p, ASI_HWFLUSHPG, 0);
		} else {
			ls = CACHEINFO.c_linesize;
			i <<= PGSHIFT - CACHEINFO.c_l2linesize;
			for (; --i >= 0; p += ls)
				sta(p, ASI_FLUSHPG, 0);
		}
		return;
	}
	baseoff = (u_int)base & SGOFSET;
	i = (baseoff + len + SGOFSET) >> SGSHIFT;
	if (i == 1)
		sun4_vcache_flush_segment(VA_VREG(base), VA_VSEG(base));
	else {
		if (HASSUN4_MMU3L) {
			baseoff = (u_int)base & RGOFSET;
			i = (baseoff + len + RGOFSET) >> RGSHIFT;
			if (i == 1)
				sun4_vcache_flush_region(VA_VREG(base));
			else
				sun4_vcache_flush_context();
		} else
			sun4_vcache_flush_context();
	}
}
#endif /* defined(SUN4) || defined(SUN4C) */

#if defined(SUN4M)
/*
 * Flush the current context from the cache.
 *
 * This is done by writing to each cache line in the `flush context'
 * address space (or, for hardware flush, once to each page in the
 * hardware flush space, for all cache pages).
 */
void
srmmu_vcache_flush_context()
{
	char *p;
	int i, ls;

	cachestats.cs_ncxflush++;
	p = (char *)0;	/* addresses 0..cacheinfo.c_totalsize will do fine */
	ls = CACHEINFO.c_linesize;
	i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
	for (; --i >= 0; p += ls)
		sta(p, ASI_IDCACHELFC, 0);
}

/*
 * Flush the given virtual region from the cache.
 *
 * This is also done by writing to each cache line, except that
 * now the addresses must include the virtual region number, and
 * we use the `flush region' space.
 */
void
srmmu_vcache_flush_region(vreg)
	int vreg;
{
	int i, ls;
	char *p;

	cachestats.cs_nrgflush++;
	p = (char *)VRTOVA(vreg);	/* reg..reg+sz rather than 0..sz */
	ls = CACHEINFO.c_linesize;
	i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
	for (; --i >= 0; p += ls)
		sta(p, ASI_IDCACHELFR, 0);
}

/*
 * Flush the given virtual segment from the cache.
 *
 * This is also done by writing to each cache line, except that
 * now the addresses must include the virtual segment number, and
 * we use the `flush segment' space.
 *
 * Again, for hardware, we just write each page (in hw-flush space).
 */
void
srmmu_vcache_flush_segment(vreg, vseg)
	int vreg, vseg;
{
	int i, ls;
	char *p;

	cachestats.cs_nsgflush++;
	p = (char *)VSTOVA(vreg, vseg);	/* seg..seg+sz rather than 0..sz */
	ls = CACHEINFO.c_linesize;
	i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
	for (; --i >= 0; p += ls)
		sta(p, ASI_IDCACHELFS, 0);
}

/*
 * Flush the given virtual page from the cache.
 * (va is the actual address, and must be aligned on a page boundary.)
 * Again we write to each cache line.
 */
void
srmmu_vcache_flush_page(va)
	int va;
{
	int i, ls;
	char *p;

#ifdef DEBUG
	if (va & PGOFSET)
		panic("cache_flush_page: asked to flush misaligned va 0x%x",va);
#endif

	cachestats.cs_npgflush++;
	p = (char *)va;
	ls = CACHEINFO.c_linesize;
	i = NBPG >> CACHEINFO.c_l2linesize;
	for (; --i >= 0; p += ls)
		sta(p, ASI_IDCACHELFP, 0);
}

void
srmmu_cache_flush_all()
{
	srmmu_vcache_flush_context();
}

/*
 * Flush a range of virtual addresses (in the current context).
 * The first byte is at (base&~PGOFSET) and the last one is just
 * before byte (base+len).
 *
 * We choose the best of (context,segment,page) here.
 */

#define CACHE_FLUSH_MAGIC	(CACHEINFO.c_totalsize / NBPG)

void
srmmu_cache_flush(base, len)
	caddr_t base;
	u_int len;
{
	int i, ls, baseoff;
	char *p;

	/*
	 * Figure out how much must be flushed.
	 *
	 * If we need to do CACHE_FLUSH_MAGIC pages,  we can do a segment
	 * in the same number of loop iterations.  We can also do the whole
	 * region. If we need to do between 2 and NSEGRG, do the region.
	 * If we need to do two or more regions, just go ahead and do the
	 * whole context. This might not be ideal (e.g., fsck likes to do
	 * 65536-byte reads, which might not necessarily be aligned).
	 *
	 * We could try to be sneaky here and use the direct mapping
	 * to avoid flushing things `below' the start and `above' the
	 * ending address (rather than rounding to whole pages and
	 * segments), but I did not want to debug that now and it is
	 * not clear it would help much.
	 *
	 * (XXX the magic number 16 is now wrong, must review policy)
	 */
	baseoff = (int)base & PGOFSET;
	i = (baseoff + len + PGOFSET) >> PGSHIFT;

	cachestats.cs_nraflush++;
#ifdef notyet
	cachestats.cs_ra[min(i, MAXCACHERANGE)]++;
#endif

	if (i < CACHE_FLUSH_MAGIC) {
		/* cache_flush_page, for i pages */
		p = (char *)((int)base & ~baseoff);
		ls = CACHEINFO.c_linesize;
		i <<= PGSHIFT - CACHEINFO.c_l2linesize;
		for (; --i >= 0; p += ls)
			sta(p, ASI_IDCACHELFP, 0);
		return;
	}
	baseoff = (u_int)base & SGOFSET;
	i = (baseoff + len + SGOFSET) >> SGSHIFT;
	if (i == 1)
		srmmu_vcache_flush_segment(VA_VREG(base), VA_VSEG(base));
	else {
		baseoff = (u_int)base & RGOFSET;
		i = (baseoff + len + RGOFSET) >> RGSHIFT;
		if (i == 1)
			srmmu_vcache_flush_region(VA_VREG(base));
		else
			srmmu_vcache_flush_context();
	}
}

#ifndef	MS1_CACHEFLUSH_MAGIC
#define	MS1_CACHEFLUSH_MAGIC	0 /* 48 */
#endif
void
ms1_cache_flush(base, len)
	caddr_t base;
	u_int len;
{
	/*
	 * Although physically tagged, we still need to flush the
	 * data cache after (if we have a write-through cache) or before
	 * (in case of write-back caches) DMA operations.
	 */
#if MS1_CACHEFLUSH_MAGIC != 0
	if (len <= MS1_CACHEFLUSH_MAGIC) {
		/*
		 * If the range to be flushed is sufficiently small
		 * invalidate the covered cache lines by hand.
		 *
		 * The MicroSPARC I has a direct-mapped virtually addressed
		 * physically tagged data cache which is organised as
		 * 128 lines of 16 bytes. Virtual address bits [4-10]
		 * select the cache line. The cache tags are accessed
		 * through the standard DCACHE control space using the
		 * same address bits as those used to select the cache
		 * line in the virtual address.
		 *
		 * Note: we don't bother to compare the actual tags
		 * since that would require looking up physical addresses.
		 *
		 * The format of the tags we read from ASI_DCACHE control
		 * space is:
		 *
		 * 31     27 26            11 10         1 0
		 * +--------+----------------+------------+-+
		 * |  xxx   |    PA[26-11]   |    xxx     |V|
		 * +--------+----------------+------------+-+
		 *
		 * PA: bits 11-26 of the physical address
		 * V:  line valid bit
		 */
		int tagaddr = ((u_int)base & 0x7f0);

		len = roundup(len, 16);
		while (len != 0) {
			int tag = lda(tagaddr, ASI_DCACHETAG);
			if ((tag & 1) == 1) {
				/* Mark this cache line invalid */
				sta(tagaddr, ASI_DCACHETAG, 0);
			}
			len -= 16;
			tagaddr = (tagaddr + 16) & 0x7f0;
		}
	} else
#endif
		/* Flush entire data cache */
		sta(0, ASI_DCACHECLR, 0);
}

/*
 * Flush entire cache.
 */
void
ms1_cache_flush_all()
{

	/* Flash-clear both caches */
	sta(0, ASI_ICACHECLR, 0);
	sta(0, ASI_DCACHECLR, 0);
}

void
hypersparc_cache_flush_all()
{

	srmmu_vcache_flush_context();
	/* Flush instruction cache */
	hypersparc_pure_vcache_flush();
}

void
cypress_cache_flush_all()
{
	extern char kernel_text[];
	char *p;
	int i, ls;

	/* Fill the cache with known read-only content */
	p = (char *)kernel_text;
	ls = CACHEINFO.c_linesize;
	i = CACHEINFO.c_totalsize >> CACHEINFO.c_l2linesize;
	for (; --i >= 0; p += ls)
		(*(volatile char *)p);
}

void
viking_cache_flush(base, len)
	caddr_t base;
	u_int len;
{
	/*
	 * Although physically tagged, we still need to flush the
	 * data cache after (if we have a write-through cache) or before
	 * (in case of write-back caches) DMA operations.
	 */

}

void
viking_pcache_flush_line(va, pa)
	int va;
	int pa;
{
	/*
	 * Flush cache line corresponding to virtual address `va'
	 * which is mapped at physical address `pa'.
	 */
	extern char etext[];
	static char *base;
	int i;
	char *v;

	/*
	 * Construct a virtual address that hits the same cache line
	 * as PA, then read from 2*ASSOCIATIVITY-1 different physical
	 * locations (all different from PA).
	 */

#if 0
	if (base == 0) {
		cshift = CACHEINFO.ic_l2linesize;
		csize = CACHEINFO.ic_nlines << cshift;
		cmask = csize - 1;
		base = (char *)roundup((int)etext, csize);
	}

	v = base + (((va & cmask) >> cshift) << cshift);
	i = CACHEINFO.dc_associativity * 2 - 1;

	while (i--) {
		(*(volatile int *)v);
		v += csize;
	}
#else
#define cshift	5			/* CACHEINFO.ic_l2linesize */
#define csize	(128 << cshift)		/* CACHEINFO.ic_nlines << cshift */
#define cmask	(csize - 1)
#define cass	4			/* CACHEINFO.dc_associativity */

	if (base == 0)
		base = (char *)roundup((unsigned int)etext, csize);

	v = base + (((pa & cmask) >> cshift) << cshift);
	i = 2 * cass - 1;

	while (i--) {
		(*(volatile int *)v);
		v += csize;
	}
#undef cass
#undef cmask
#undef csize
#undef cshift
#endif
}

void
srmmu_pcache_flush_line(va, pa)
	int va;
	int pa;
{
	/*
	 * Flush cache line corresponding to virtual address `va'
	 * which is mapped at physical address `pa'.
	 */
	sta(va, ASI_IDCACHELFP, 0);
}
#endif /* SUN4M */

#if defined(solbourne)
void
kap_cache_enable()
{
	kap_cache_flush(NULL, 0);
	sta(0, ASI_ICACHE_INVAL, 0);

	sta(ICU_CONF, ASI_PHYS_IO,
	    lda(ICU_CONF, ASI_PHYS_IO) & ~CONF_ICACHE_DISABLE);
	CACHEINFO.c_enabled = 1;

	printf("cache enabled\n");
}

void
kap_vcache_flush_context()
{
	kap_cache_flush(0, 0);
	sta(0, ASI_DCACHE_INVAL, 0);
	sta(0, ASI_ICACHE_INVAL, 0);
}

void
kap_vcache_flush_page(va)
	int va;
{
	kap_cache_flush((caddr_t)va, PAGE_SIZE);
}

void
kap_cache_flush(base, len)
	caddr_t base;
	u_int len;
{
	u_int line;
	u_int32_t mmcr;

	/*
	 * Due to the small size of the data cache and the fact that we
	 * would be flushing 4 bytes by 4 bytes, it is faster to flush
	 * the whole cache instead.
	 */

	mmcr = lda(0, ASI_MMCR) & ~(MMCR_DSET0 | MMCR_DSET1);
	/* flush bank 0 */
	sta(0, ASI_MMCR, mmcr | MMCR_DSET0);
	for (line = 0; line < DCACHE_LINE(DCACHE_LINES); line += DCACHE_INCR)
		(void)lda(line, ASI_DCACHE_FLUSH);
	/* flush bank 1 */
	sta(0, ASI_MMCR, mmcr | MMCR_DSET1);
	for (line = 0; line < DCACHE_LINE(DCACHE_LINES); line += DCACHE_INCR)
		(void)lda(line, ASI_DCACHE_FLUSH);
}

#endif	/* solbourne */
