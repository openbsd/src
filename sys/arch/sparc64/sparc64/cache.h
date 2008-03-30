/*	$OpenBSD: cache.h,v 1.6 2008/03/30 12:30:01 kettenis Exp $	*/
/*	$NetBSD: cache.h,v 1.3 2000/08/01 00:28:02 eeh Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	@(#)cache.h	8.1 (Berkeley) 6/11/93
 */

enum vactype { VAC_NONE, VAC_WRITETHROUGH, VAC_WRITEBACK };

extern enum vactype vactype;	/* XXX  move into cacheinfo struct */

/*
 * Cache tags can be written in control space, and must be set to 0
 * (or invalid anyway) before turning on the cache.  The tags are
 * addressed as an array of 32-bit structures of the form:
 *
 *	struct cache_tag {
 *		u_int	:7,		(unused; must be zero)
 *			ct_cid:3,	(context ID)
 *			ct_w:1,		(write flag from PTE)
 *			ct_s:1,		(supervisor flag from PTE)
 *			ct_v:1,		(set => cache entry is valid)
 *			:3,		(unused; must be zero)
 *			ct_tid:14,	(cache tag ID)
 *			:2;		(unused; must be zero)
 *	};
 *
 * The SPARCstation 1 cache sees virtual addresses as:
 *
 *	struct cache_va {
 *		u_int	:2,		(unused; probably copies of va_tid<13>)
 *			cva_tid:14,	(tag ID)
 *			cva_line:12,	(cache line number)
 *			cva_byte:4;	(byte in cache line)
 *	};
 *
 * (The SS2 cache is similar but has half as many lines, each twice as long.)
 *
 * Note that, because the 12-bit line ID is `wider' than the page offset,
 * it is possible to have one page map to two different cache lines.
 * This can happen whenever two different physical pages have the same bits
 * in the part of the virtual address that overlaps the cache line ID, i.e.,
 * bits <15:12>.  In order to prevent cache duplication, we have to
 * make sure that no one page has more than one virtual address where
 * (va1 & 0xf000) != (va2 & 0xf000).  (The cache hardware turns off ct_v
 * when a cache miss occurs on a write, i.e., if va1 is in the cache and
 * va2 is not, and you write to va2, va1 goes out of the cache.  If va1
 * is in the cache and va2 is not, reading va2 also causes va1 to become
 * uncached, and the [same] data is then read from main memory into the
 * cache.)
 *
 * The other alternative, of course, is to disable caching of aliased
 * pages.  (In a few cases this might be faster anyway, but we do it
 * only when forced.)
 *
 * The Sun4, since it has an 8K pagesize instead of 4K, needs to check
 * bits that are one position higher.
 */

/* 
 * The spitfire has a 16K two-way set associative level-1 I$ and a separate 
 * 16K level-1 D$.  The I$ can be invalidated using the FLUSH instructions, 
 * so we don't really need to worry about it much.  The D$ is 16K write-through
 * direct mapped virtually addressed cache with two 16-byte sub-blocks per line.  
 * The E$ is a 512KB-4MB direct mapped physically indexed physically tagged cache.
 * Since the level-1 caches are write-through, they don't need flushing and can be
 * invalidated directly.  
 *
 * The spitfire sees virtual addresses as:
 *
 *	struct cache_va {
 *		u_int64_t	:22,		(unused; we only have 40-bit addresses)
 *				cva_tag:28,	(tag ID)
 *				cva_line:9,	(cache line number)
 *				cva_byte:5;	(byte within line)
 *	};
 *
 * Since there is one bit of overlap between the page offset and the line index,
 * all we need to do is make sure that bit 14 of the va remains constant and we have
 * no aliasing problems.  
 *
 * Let me try again.  Page size is 8K, cache size is 16K so if (va1&0x3fff != va2&0x3fff)
 * we have a problem.  Bit 14 *must* be the same for all mappings of a page to be cacheable
 * in the D$.  (The I$ is 16K 2-way associative--each bank is 8K.  No conflict there.)
 */

/* Some more well-known values: */
#define CACHE_ALIAS_MASK	0x7fff	
#define CACHE_ALIAS_BITS	0x4000

/*
 * True iff a1 and a2 are `bad' aliases (will cause cache duplication).
 */
#define	BADALIAS(a1, a2) (((int)(a1) ^ (int)(a2)) & CACHE_ALIAS_BITS)

/*
 * Routines for dealing with the cache.
 */
void	cache_enable(void);		/* turn it on */
int 	cache_flush_page(paddr_t);	/* flush page from E$ */
int	cache_flush(vaddr_t, vsize_t);	/* flush region */

/* The following are for D$ flushes and are in locore.s */
#define dcache_flush_page(pa) cacheinfo.c_dcache_flush_page(pa)
void 	us_dcache_flush_page(paddr_t);	/* flush page from D$ */
void 	us3_dcache_flush_page(paddr_t);	/* flush page from D$ */
void	no_dcache_flush_page(paddr_t);

/* The following flush a range from the D$ and I$ but not E$. */
void	cache_flush_virt(vaddr_t, vsize_t);
void	cache_flush_phys(paddr_t, psize_t, int);

/*
 * Cache control information.
 */
struct cacheinfo {
	void	(*c_dcache_flush_page)(paddr_t);

	int	c_totalsize;		/* total size, in bytes */
					/* if split, MAX(icache,dcache) */
	int	c_enabled;		/* true => cache is enabled */
	int	c_hwflush;		/* true => have hardware flush */
	int	c_linesize;		/* line size, in bytes */
	int	c_l2linesize;		/* log2(linesize) */
	int	c_physical;		/* true => cache is physical */
	int 	c_split;		/* true => cache is split */
	int 	ic_totalsize;		/* instruction cache */
	int 	ic_enabled;
	int 	ic_linesize;
	int 	ic_l2linesize;
	int 	dc_totalsize;		/* data cache */
	int 	dc_enabled;
	int 	dc_linesize;
	int 	dc_l2linesize;
	int	ec_totalsize;		/* external cache info */
	int 	ec_enabled;
	int	ec_linesize;
	int	ec_l2linesize;
};
extern struct cacheinfo cacheinfo;

/*
 * Cache control statistics.
 */
struct cachestats {
	int	cs_npgflush;		/* # page flushes */
	int	cs_nraflush;		/* # range flushes */
#ifdef notyet
	int	cs_ra[65];		/* pages/range */
#endif
};
