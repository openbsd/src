/*	$NetBSD: dvma.c,v 1.6 1996/01/31 17:20:39 gwr Exp $	*/

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
 *      This product includes software developed by Gordon W. Ross
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
 * The easiest way to deal with the need for DVMA mappings is to
 * create a DVMA alias mapping of the entire address range used by
 * the boot program.  That way, dvma_mapin can just compute the
 * DVMA alias address, and dvma_mapout does nothing.
 *
 * Note that this assumes that standalone programs will do I/O
 * operations only within range (SA_MIN_VA .. SA_MAX_VA) checked.
 */

#include <sys/param.h>
#include "stand.h"

/* XXX */
extern int  get_segmap __P((int));
extern void set_segmap __P((int, int));


#define	DVMA_BASE 0xFFf00000
#define DVMA_MAPLEN  0xE0000	/* 1 MB - 128K (save MONSHORTSEG) */

#define SA_MIN_VA	0x200000
#define SA_MAX_VA	(SA_MIN_VA + DVMA_MAPLEN)

/* This points to the end of the free DVMA space. */
u_int dvma_end = DVMA_BASE + DVMA_MAPLEN;

void
dvma_init()
{
	int segva, dmava, sme;

	segva = SA_MIN_VA;
	dmava = DVMA_BASE;

	while (segva < SA_MAX_VA) {
		sme = get_segmap(segva);
		set_segmap(dmava, sme);
		segva += NBSG;
		dmava += NBSG;
	}
}

/* Convert a local address to a DVMA address. */
char *
dvma_mapin(char *addr, int len)
{
	int va = (int)addr;

	/* Make sure the address is in the DVMA map. */
	if ((va < SA_MIN_VA) || (va >= SA_MAX_VA))
		panic("dvma_mapin");

	va -= SA_MIN_VA;
	va += DVMA_BASE;

	return ((char *) va);
}

/* Convert a DVMA address to a local address. */
char *
dvma_mapout(char *addr, int len)
{
	int va = (int)addr;

	/* Make sure the address is in the DVMA map. */
	if ((va < DVMA_BASE) || (va >= (DVMA_BASE + DVMA_MAPLEN)))
		panic("dvma_mapout");

	va -= DVMA_BASE;
	va += SA_MIN_VA;

	return ((char *) va);
}

char *
dvma_alloc(int len)
{
	len = sun3_round_page(len);
	dvma_end -= len;
	return((char*)dvma_end);
}

void
dvma_free(char *dvma, int len)
{
	/* not worth the trouble */
}
