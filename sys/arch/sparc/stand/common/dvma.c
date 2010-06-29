/*	$OpenBSD: dvma.c,v 1.4 2010/06/29 21:33:54 miod Exp $	*/
/*	$NetBSD: dvma.c,v 1.2 1995/09/17 00:50:56 pk Exp $	*/
/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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
 */

/*
 * The easiest way to deal with the need for DVMA mappings is to just
 * map the entire megabyte of RAM where we are loaded into DVMA space.
 * That way, dvma_mapin can just compute the DVMA alias address, and
 * dvma_mapout does nothing.  Note that this assumes all standalone
 * programs stay in the range `base_va' .. `base_va + DVMA_MAPLEN'
 */

#include <sys/param.h>
#include <machine/pte.h>
#include <machine/ctlreg.h>

#include <sparc/sparc/asm.h>

#define	DVMA_BASE	0xFFF00000
#define DVMA_MAPLEN	0xE0000	/* 1 MB - 128K (save MONSHORTSEG) */

static int base_va;

/*
 * This module is only used on sun4, so:
 */
#define getsegmap(va)		(lduha(va, ASI_SEGMAP))
#define setsegmap(va, pmeg)	do stha(va, ASI_SEGMAP, pmeg); while(0)

void
dvma_init(void)
{
	u_int segva, dmava;
	int nseg;
	extern int start;

	/*
	 * Align our address base with the DVMA segment.
	 * Allocate one DVMA segment to cover the stack, which
	 * grows downward from `start'.
	 */
	dmava = DVMA_BASE;
	base_va = segva = (((int)&start) & -NBPSG) - NBPSG;

	/* Then double-map the DVMA addresses */
	nseg = (DVMA_MAPLEN + NBPSG - 1) >> SGSHIFT;
	while (nseg-- > 0) {
		setsegmap(dmava, getsegmap(segva));
		segva += NBPSG;
		dmava += NBPSG;
	}
}

/*
 * Convert a local address to a DVMA address.
 */
char *
dvma_mapin(char *addr, size_t len)
{
	int va = (int)addr;

	va -= base_va;

#ifndef BOOTXX
	/* Make sure the address is in the DVMA map. */
	if (va < 0 || va >= DVMA_MAPLEN)
		panic("dvma_mapin");
#endif

	va += DVMA_BASE;

	return ((char *)va);
}

/*
 * Convert a DVMA address to a local address.
 */
char *
dvma_mapout(char *addr, size_t len)
{
	int va = (int)addr;

	va -= DVMA_BASE;

#ifndef BOOTXX
	/* Make sure the address is in the DVMA map. */
	if (va < 0 || va >= DVMA_MAPLEN)
		panic("dvma_mapout");
#endif

	va += base_va;

	return ((char *)va);
}

extern char *alloc(int);

char *
dvma_alloc(int len)
{
	char *mem;

	mem = alloc(len);
	if (!mem)
		return (mem);
	return (dvma_mapin(mem, len));
}

extern void free(void *ptr, int len);
void
dvma_free(char *dvma, int len)
{
	char *mem;

	mem = dvma_mapout(dvma, len);
	if (mem)
		free(mem, len);
}
