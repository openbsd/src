/*	$NetBSD: cache.c,v 1.8.4.1 1996/06/12 20:40:35 pk Exp $ */

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

enum vactype vactype;
struct cachestats cachestats;

#if defined(SUN4M)
int cache_alias_dist;		/* Cache anti-aliasing constants */
int cache_alias_bits;
#endif

/*
 * A few quick macros to allow for different ASI's between 4M/4/4C machines
 */
#define flushls_ctx(p)	do {			\
	CPU_ISSUN4M				\
		? sta(p, ASI_IDCACHELFC, 0)	\
		: sta(p, ASI_FLUSHCTX, 0);	\
} while (0)
#define flushls_reg(p)	do {			\
	CPU_ISSUN4M				\
		? sta(p, ASI_IDCACHELFR, 0)	\
		: sta(p, ASI_FLUSHREG, 0);	\
} while (0)
#define flushls_seg(p)	do {			\
	CPU_ISSUN4M				\
		? sta(p, ASI_IDCACHELFS, 0)	\
		: sta(p, ASI_FLUSHSEG, 0);	\
} while (0)
#define flushls_pag(p)	do {			\
	CPU_ISSUN4M				\
		? sta(p, ASI_IDCACHELFP, 0)	\
		: sta(p, ASI_FLUSHPG, 0);	\
} while (0)
#define flushls_usr(p)	do {			\
	if (CPU_ISSUN4M)			\
		sta(p, ASI_IDCACHELFU, 0);	\
while(0)

/* XXX: (ABB) need more generic Sun4m cache support */
#if !defined(SUN4) && !defined(SUN4C)
#define	ASI_HWFLUSHSEG	0x05	/* hardware assisted version of FLUSHSEG */
#define	ASI_HWFLUSHPG	0x06	/* hardware assisted version of FLUSHPG */
#define	ASI_HWFLUSHCTX	0x07	/* hardware assisted version of FLUSHCTX */
#endif

/*
 * Enable the cache.
 * We need to clear out the valid bits first.
 */
void
cache_enable()
{
	register u_int i, lim, ls, ts;

	ls = cacheinfo.c_linesize;
	ts = cacheinfo.c_totalsize;

	if (CPU_ISSUN4M) {
		i = lda(SRMMU_PCR, ASI_SRMMU);
		switch (mmumod) {
		case SUN4M_MMU_HS:	/* HyperSPARC */
			/*
			 * First we determine what type of cache we have, and
			 * setup the anti-aliasing constants appropriately.
			 */
			if (i & SRMMU_PCR_CS) {
				cache_alias_bits = CACHE_ALIAS_BITS_HS256k;
				cache_alias_dist = CACHE_ALIAS_DIST_HS256k;
			} else {
				cache_alias_bits = CACHE_ALIAS_BITS_HS128k;
				cache_alias_dist = CACHE_ALIAS_DIST_HS128k;
			}
			/* Now reset cache tag memory */
			for (i = 0, lim = ts; i < lim; i += ls)
				sta(i, ASI_DCACHETAG, 0);

			sta(SRMMU_PCR, ASI_SRMMU, /* Enable write-back cache */
			    lda(SRMMU_PCR, ASI_SRMMU) | SRMMU_PCR_CE |
				SRMMU_PCR_CM);
			cacheinfo.c_enabled = 1;
			vactype = VAC_NONE;
			/* HyperSPARC uses phys. tagged cache */

			/* XXX: should add support */
			if (cacheinfo.c_hwflush)
				panic("cache_enable: can't handle 4M with hw-flush cache");

			printf("cache enabled\n");
			break;

		case SUN4M_MMU_SS:	/* SuperSPARC */
			if ((cpumod & 0xf0) != (SUN4M_SS & 0xf0)) {
				printf(
			"cache NOT enabled for %x/%x cpu/mmu combination\n",
					cpumod, mmumod);
				break;	/* ugh, SS and MS have same MMU # */
			}
			cache_alias_bits = CACHE_ALIAS_BITS_SS;
			cache_alias_dist = CACHE_ALIAS_DIST_SS;

			/* We "flash-clear" the I/D caches. */
			sta(0x80000000, ASI_ICACHECLR, 0); /* Unlock */
			sta(0, ASI_ICACHECLR, 0);	/* clear */
			sta(0x80000000, ASI_DCACHECLR, 0);
			sta(0, ASI_DCACHECLR, 0);

			/* Turn on caches via MMU */
			sta(SRMMU_PCR, ASI_SRMMU,
			    lda(SRMMU_PCR,ASI_SRMMU) | SRMMU_PCR_DCE |
				SRMMU_PCR_ICE);
			cacheinfo.c_enabled = cacheinfo.dc_enabled = 1;

			/* Now try to turn on MultiCache if it exists */
			/* XXX (ABB) THIS IS BROKEN MUST FIX */
			if (0&&(lda(SRMMU_PCR, ASI_SRMMU) & SRMMU_PCR_MB) == 0
				&& cacheinfo.ec_totalsize > 0) {
				/* Multicache controller */
				sta(MXCC_ENABLE_ADDR, ASI_CONTROL,
				    lda(MXCC_ENABLE_ADDR, ASI_CONTROL) |
					MXCC_ENABLE_BIT);
				cacheinfo.ec_enabled = 1;
			}
			printf("cache enabled\n");
			break;
		case SUN4M_MMU_MS1: /* MicroSPARC */
			/* We "flash-clear" the I/D caches. */
			sta(0, ASI_ICACHECLR, 0);	/* clear */
			sta(0, ASI_DCACHECLR, 0);

			/* Turn on caches via MMU */
			sta(SRMMU_PCR, ASI_SRMMU,
			    lda(SRMMU_PCR,ASI_SRMMU) | SRMMU_PCR_DCE |
				SRMMU_PCR_ICE);
			cacheinfo.c_enabled = cacheinfo.dc_enabled = 1;

			printf("cache enabled\n");
			break;

		default:
			printf("Unknown MMU architecture...cache disabled.\n");
			/* XXX: not HyperSPARC -- guess for now */
			cache_alias_bits = GUESS_CACHE_ALIAS_BITS;
			cache_alias_dist = GUESS_CACHE_ALIAS_DIST;
		}

	} else {

		for (i = AC_CACHETAGS, lim = i + ts; i < lim; i += ls)
			sta(i, ASI_CONTROL, 0);

		stba(AC_SYSENABLE, ASI_CONTROL,
		     lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_CACHE);
		cacheinfo.c_enabled = 1;

		printf("cache enabled\n");

#ifdef notyet
		if (cpumod == SUN4_400) {
			stba(AC_SYSENABLE, ASI_CONTROL,
			     lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_IOCACHE);
			printf("iocache enabled\n");
		}
#endif
	}
}

/*
 * Flush the current context from the cache.
 *
 * This is done by writing to each cache line in the `flush context'
 * address space (or, for hardware flush, once to each page in the
 * hardware flush space, for all cache pages).
 */
void
cache_flush_context()
{
	register char *p;
	register int i, ls;

	cachestats.cs_ncxflush++;
	p = (char *)0;	/* addresses 0..cacheinfo.c_totalsize will do fine */
	if (cacheinfo.c_hwflush) {
		ls = NBPG;
		i = cacheinfo.c_totalsize >> PGSHIFT;
		for (; --i >= 0; p += ls)
			sta(p, ASI_HWFLUSHCTX, 0);
	} else {
		ls = cacheinfo.c_linesize;
		i = cacheinfo.c_totalsize >> cacheinfo.c_l2linesize;
		for (; --i >= 0; p += ls)
			flushls_ctx(p);
	}
}

#if defined(MMU_3L) || defined(SUN4M)
/*
 * Flush the given virtual region from the cache.
 *
 * This is also done by writing to each cache line, except that
 * now the addresses must include the virtual region number, and
 * we use the `flush region' space.
 *
 * This function is only called on sun4m's or sun4's with 3-level MMUs; there's
 * no hw-flush space.
 */
void
cache_flush_region(vreg)
	register int vreg;
{
	register int i, ls;
	register char *p;

	cachestats.cs_nrgflush++;
	p = (char *)VRTOVA(vreg);	/* reg..reg+sz rather than 0..sz */
	ls = cacheinfo.c_linesize;
	i = cacheinfo.c_totalsize >> cacheinfo.c_l2linesize;
	for (; --i >= 0; p += ls)
		flushls_reg(p);
}
#endif

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
cache_flush_segment(vreg, vseg)
	register int vreg, vseg;
{
	register int i, ls;
	register char *p;

	cachestats.cs_nsgflush++;
	p = (char *)VSTOVA(vreg, vseg);	/* seg..seg+sz rather than 0..sz */
	if (cacheinfo.c_hwflush) {
		ls = NBPG;
		i = cacheinfo.c_totalsize >> PGSHIFT;
		for (; --i >= 0; p += ls)
			sta(p, ASI_HWFLUSHSEG, 0);
	} else {
		ls = cacheinfo.c_linesize;
		i = cacheinfo.c_totalsize >> cacheinfo.c_l2linesize;
		for (; --i >= 0; p += ls)
			flushls_seg(p);
	}
}

/*
 * Flush the given virtual page from the cache.
 * (va is the actual address, and must be aligned on a page boundary.)
 * Again we write to each cache line.
 */
void
cache_flush_page(va)
	int va;
{
	register int i, ls;
	register char *p;

#ifdef DEBUG
	if (va & PGOFSET)
		panic("cache_flush_page: asked to flush misaligned va %x",va);
#endif

	cachestats.cs_npgflush++;
	p = (char *)va;
	if (cacheinfo.c_hwflush)
		sta(p, ASI_HWFLUSHPG, 0);
	else {
		ls = cacheinfo.c_linesize;
		i = NBPG >> cacheinfo.c_l2linesize;
		for (; --i >= 0; p += ls)
			flushls_pag(p);
	}
}

/*
 * Flush a range of virtual addresses (in the current context).
 * The first byte is at (base&~PGOFSET) and the last one is just
 * before byte (base+len).
 *
 * We choose the best of (context,segment,page) here.
 */

#define CACHE_FLUSH_MAGIC	(cacheinfo.c_totalsize / NBPG)

void
cache_flush(base, len)
	caddr_t base;
	register u_int len;
{
	register int i, ls, baseoff;
	register char *p;
#if defined(MMU_3L)
	extern int mmu_3l;
#endif

#if defined(SUN4M)
	/*
	 * Although physically tagged, we still need to flush the
	 * data cache after (if we have a write-through cache) or before
	 * (in case of write-back caches) DMA operations.
	 *
	 * SS10s and 20s (supersparcs) use cached DVMA, no need to flush.
	 */
	if (CPU_ISSUN4M && mmumod != SUN4M_MMU_SS) {
		/* XXX (ABB) - Need more generic cache interface */
		/* XXX above test conflicts on MicroSPARC (SUN4M_MMU_MS=_SS) */
		sta(0, ASI_DCACHECLR, 0);
		return;
	}
#endif

	if (vactype == VAC_NONE)
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
		if (cacheinfo.c_hwflush) {
			for (; --i >= 0; p += NBPG)
				sta(p, ASI_HWFLUSHPG, 0);
		} else {
			ls = cacheinfo.c_linesize;
			i <<= PGSHIFT - cacheinfo.c_l2linesize;
			for (; --i >= 0; p += ls)
				flushls_pag(p);
		}
		return;
	}
	baseoff = (u_int)base & SGOFSET;
	i = (baseoff + len + SGOFSET) >> SGSHIFT;
	if (i == 1)
		cache_flush_segment(VA_VREG(base), VA_VSEG(base));
	else {
#if defined(MMU_3L) || defined(SUN4M)
		baseoff = (u_int)base & RGOFSET;
		i = (baseoff + len + RGOFSET) >> RGSHIFT;
		if (i == 1
#if !defined(MMU_3L)
			&& CPU_ISSUN4M
#elif !defined(SUN4M)
			&& mmu_3l
#else
			&& (CPU_ISSUN4M || mmu_3l)
#endif
				)
			cache_flush_region(VA_VREG(base));
		else
#endif
			cache_flush_context();
	}
}
