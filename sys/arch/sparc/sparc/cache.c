/*	$OpenBSD: cache.c,v 1.6 1999/03/15 17:55:31 deraadt Exp $	*/
/*	$NetBSD: cache.c,v 1.33 1997/07/29 09:41:56 fair Exp $ */

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

#include <machine/ctlreg.h>
#include <machine/pte.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/cpuvar.h>

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
	register u_int i, lim, ls, ts;

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

void
ms1_cache_enable()
{
	u_int pcr;

	cache_alias_bits = GUESS_CACHE_ALIAS_BITS;
	cache_alias_dist = GUESS_CACHE_ALIAS_DIST;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/* We "flash-clear" the I/D caches. */
	if ((pcr & MS1_PCR_ICE) == 0)
		sta(0, ASI_ICACHECLR, 0);
	if ((pcr & MS1_PCR_DCE) == 0)
		sta(0, ASI_DCACHECLR, 0);

	/* Turn on caches */
	sta(SRMMU_PCR, ASI_SRMMU, pcr | MS1_PCR_DCE | MS1_PCR_ICE);

	cpuinfo.flags |= CPUFLG_CACHEPAGETABLES;

	CACHEINFO.c_enabled = CACHEINFO.dc_enabled = 1;

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
	u_int pcr;

	ls = CACHEINFO.c_linesize;
	ts = CACHEINFO.c_totalsize;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);

	/*
	 * First we determine what type of cache we have, and
	 * setup the anti-aliasing constants appropriately.
	 */
	if (pcr & HYPERSPARC_PCR_CS) {
		cache_alias_bits = CACHE_ALIAS_BITS_HS256k;
		cache_alias_dist = CACHE_ALIAS_DIST_HS256k;
	} else {
		cache_alias_bits = CACHE_ALIAS_BITS_HS128k;
		cache_alias_dist = CACHE_ALIAS_DIST_HS128k;
	}

	/* Now reset cache tag memory if cache not yet enabled */
	if ((pcr & HYPERSPARC_PCR_CE) == 0)
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_DCACHETAG, 0);

	/* Enable write-back cache */
	pcr |= (HYPERSPARC_PCR_CE | HYPERSPARC_PCR_CM);
	sta(SRMMU_PCR, ASI_SRMMU, pcr);
	CACHEINFO.c_enabled = 1;

	/* XXX: should add support */
	if (CACHEINFO.c_hwflush)
		panic("cache_enable: can't handle 4M with hw-flush cache");

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
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_DCACHETAG, 0);

	/* XXX - assume that an MS2 with ecache is really a turbo in disguise */
	if (CACHEINFO.ec_totalsize == 0)
		cpuinfo.flags |= CPUFLG_CACHEPAGETABLES; /* Ok to cache PTEs */
	CACHEINFO.c_enabled = 1;
	printf("cache enabled\n");
}

void
cypress_cache_enable()
{
	int i, ls, ts;
	u_int pcr;

	cache_alias_dist = CACHEINFO.c_totalsize;
	cache_alias_bits = (cache_alias_dist - 1) & ~PGOFSET;

	pcr = lda(SRMMU_PCR, ASI_SRMMU);
	pcr &= ~(CYPRESS_PCR_CE | CYPRESS_PCR_CM);

	/* Now reset cache tag memory if cache not yet enabled */
	ls = CACHEINFO.c_linesize;
	ts = CACHEINFO.c_totalsize;
	if ((pcr & CYPRESS_PCR_CE) == 0)
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_DCACHETAG, 0);

	pcr |= CYPRESS_PCR_CE;

#if 1
	pcr &= ~CYPRESS_PCR_CM;		/* XXX Disable write-back mode */
#else
	/* If put in write-back mode, turn it on */
	if (CACHEINFO.c_vactype == VAC_WRITEBACK)
		pcr |= CYPRESS_PCR_CM;
#endif

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
		for (i = 0; i < ts; i += ls)
			sta(i, ASI_DCACHETAG, 0);

	pcr |= (TURBOSPARC_PCR_ICE | TURBOSPARC_PCR_DCE);
	sta(SRMMU_PCR, ASI_SRMMU, pcr);

	pcf = lda(SRMMU_PCFG, ASI_SRMMU);
	if (pcf & TURBOSPARC_PCFG_SNP)
		printf("DVMA coherent ");

	CACHEINFO.c_enabled = 1;
	printf("cache enabled\n");
}

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
	register char *p;
	register int i, ls;

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
	register int vreg;
{
	register int i, ls;
	register char *p;

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
	register int vreg, vseg;
{
	register int i, ls;
	register char *p;

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
	register int i, ls;
	register char *p;

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
	register u_int len;
{
	register int i, ls, baseoff;
	register char *p;

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
	register char *p;
	register int i, ls;

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
	register int vreg;
{
	register int i, ls;
	register char *p;

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
	register int vreg, vseg;
{
	register int i, ls;
	register char *p;

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
	register int i, ls;
	register char *p;

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
	register u_int len;
{
	register int i, ls, baseoff;
	register char *p;

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

void
ms1_cache_flush(base, len)
	caddr_t base;
	register u_int len;
{
	/*
	 * Although physically tagged, we still need to flush the
	 * data cache after (if we have a write-through cache) or before
	 * (in case of write-back caches) DMA operations.
	 */

	/* XXX investigate other methods instead of blowing the entire cache */
	sta(0, ASI_DCACHECLR, 0);
}

void
viking_cache_flush(base, len)
	caddr_t base;
	register u_int len;
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
