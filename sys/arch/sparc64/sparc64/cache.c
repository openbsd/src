/*	$OpenBSD: cache.c,v 1.3 2003/02/17 01:29:20 henric Exp $	*/
/*	$NetBSD: cache.c,v 1.5 2000/12/06 01:47:50 mrg Exp $ */

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
 * UltraSPARC has VIPT D$ and PIPT I$.
 *
 * TODO:
 *	- rework range flush
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/ctlreg.h>
#include <machine/pte.h>

#include <sparc64/sparc64/cache.h>

enum vactype vactype;
struct cachestats cachestats;
int cachedebug = 0;

/*
 * Enable the cache.
 * The prom does this for us.
 */
void
cache_enable()
{
	/* 
	 * No point in implementing this unless we have a cache_disable().
	 * Anyway, sun4u ECC is generated in the E$, so we can't disable that
	 * and expect to use any RAM.
	 */
	cacheinfo.c_enabled = 1; /* enable cache flushing */
}

/*
 * Flush the given virtual page from the cache.
 * (va is the actual address, and must be aligned on a page boundary.)
 * To get the E$ we read to each cache line.  
 */
int
cache_flush_page(pa)
	paddr_t pa;
{
	register int i, j, ls;
	register char *p;
	register int *kp;

#ifdef DEBUG
	if (cachedebug)
		printf("cache_flush_page %llx\n", (unsigned long long)pa);
	if (pa & PGOFSET)
		panic("cache_flush_page: asked to flush misaligned pa %llx", (unsigned long long)pa);
#endif

	/* Don't flush if not enabled or not probed. */
	if (!cacheinfo.c_enabled) return 0;

	cachestats.cs_npgflush++;
	p = (char *)(u_long)pa;
	ls = cacheinfo.c_linesize;
	i = NBPG >> cacheinfo.dc_l2linesize;
	/* Assume E$ takes care of itself*/
	kp = (int *)(u_long)((pa & (cacheinfo.ec_totalsize - 1)) + KERNBASE);
	j = 0; /* defeat optimizer? */
	for (; --i >= 0; p += ls) {
		flush(p);	/* Take care of I$. */
		j += kp[i];	/* Take care of E$. */
	}
	return j;
}

/*
 * Flush a range of virtual addresses (in the current context).
 * The first byte is at (base&~PGOFSET) and the last one is just
 * before byte (base+len).
 *
 * We may need to get more complex if we need to flush E$ because
 * the virtual color may not match the physical color.  Assume cache
 * coherence is handled by H/W.
 */

#define CACHE_FLUSH_MAGIC	(cacheinfo.ec_totalsize / NBPG)

int
cache_flush(base, len)
	vaddr_t base;
	size_t len;
{
	int i, j, ls;
	vaddr_t baseoff;
	char *p;
	int *kp;

#ifdef DEBUG
	if (cachedebug)
		printf("cache_flush %p %x\n", (void *)(u_long)base, (u_int)len);
#endif

	/* Don't flush if not enabled or not probed. */
	if (!cacheinfo.c_enabled) return 0;

	baseoff = (vaddr_t)base & PGOFSET;
	i = (baseoff + len + PGOFSET) >> PGSHIFT;

	cachestats.cs_nraflush++;

	i = min(i,CACHE_FLUSH_MAGIC);

	p = (char *)((vaddr_t)base & ~baseoff);
	ls = cacheinfo.dc_linesize;
	i >>= cacheinfo.dc_l2linesize;
	/* Pick right physical color for E$ */
	kp = (int *)(((vaddr_t)p & (cacheinfo.ec_totalsize - 1)) + KERNBASE);
	j = 0; /* defeat optimizer? */
	for (; --i >= 0; p += ls) {
		flush(p);	/* Take care of I$. */
		j += kp[i];	/* Take care of E$. */
	}
	return j;
}
