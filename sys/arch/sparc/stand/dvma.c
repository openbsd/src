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
 * The easiest way to deal with the need for DVMA mappings is
 * to just map the entire third megabyte of RAM into DVMA space.
 * That way, dvma_mapin can just compute the DVMA alias address,
 * and dvma_mapout does nothing.  Note that this assumes all
 * standalone programs stay in the range SA_MIN_VA .. SA_MAX_VA
 */

#include <sys/param.h>
#include <machine/pte.h>
#include <machine/ctlreg.h>
#include "promdev.h"

#define	DVMA_BASE	0xFFF00000
#define DVMA_MAPLEN	0xE0000	/* 1 MB - 128K (save MONSHORTSEG) */

#define SA_MIN_VA	(RELOC - 0x40000)	/* XXX - magic constant */
#define SA_MAX_VA	(SA_MIN_VA + DVMA_MAPLEN)

void
dvma_init()
{
	register int segva, dmava;

	dmava = DVMA_BASE;
	for (segva = SA_MIN_VA; segva < SA_MAX_VA; segva += NBPSG) {
		setsegmap(dmava, getsegmap(segva));
		dmava += NBPSG;
	}
}

/*
 * Convert a local address to a DVMA address.
 */
char *
dvma_mapin(char *addr, size_t len)
{
	register int va = (int)addr;

	/* Make sure the address is in the DVMA map. */
	if ((va < SA_MIN_VA) || (va >= SA_MAX_VA))
		panic("dvma_mapin");

	va += DVMA_BASE - SA_MIN_VA;

	return ((char *)va);
}

/*
 * Convert a DVMA address to a local address.
 */
char *
dvma_mapout(char *addr, size_t len)
{
	int va = (int)addr;

	/* Make sure the address is in the DVMA map. */
	if ((va < DVMA_BASE) || (va >= (DVMA_BASE + DVMA_MAPLEN)))
		panic("dvma_mapout");

	va -= DVMA_BASE - SA_MIN_VA;

	return ((char *)va);
}

extern char *alloc __P((int));

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
