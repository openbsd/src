/*	$NetBSD: alloc.c,v 1.1 1996/09/30 16:35:00 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Substitute alloc.c for Openfirmware machines
 */
#include <sys/param.h>

#include <openfirm.h>
#include <stand.h>

/*
 * al tracks the allocated regions, fl tracks the free list
 */
struct ml {
	struct ml *next;
	unsigned int size;
} *al, *fl;

void *
alloc(size)
	unsigned size;
{
	struct ml **fp, *f;
	unsigned rsz;
	
	size = ALIGN(size) + ALIGN(sizeof(struct ml));
	for (fp = &fl; f = *fp; fp = &f->next)
		if (f->size >= size)
			break;
	if (!f) {
		rsz = roundup(size, NBPG);
		f = OF_claim(0, rsz, NBPG);
		if (f == (void *)-1)
			panic("alloc");
		f->size = rsz;
	} else
		*fp = f->next;
		
	f->next = al;
	al = f;
	return (void *)f + ALIGN(sizeof(struct ml));
}

void
free(ptr, size)
	void *ptr;
	unsigned size;
{
	struct ml *f = (struct ml *)(ptr - ALIGN(sizeof(struct ml)));
	
	if (f->size != roundup(ALIGN(size) + ALIGN(sizeof(struct ml)), NBPG))
		panic("free: wrong size (%x != %x)",
		      f->size,
		      roundup(ALIGN(size) + ALIGN(sizeof(struct ml)), NBPG));
	f->next = fl;
	fl = f;
}

void
freeall()
{
#ifdef	__notyet__		/* looks like there is a bug in Motorola OFW */
	struct ml *m1, *m2;

	for (m1 = fl; m1; m1 = m2) {
		m2 = m1->next;
		OF_release(m1, m1->size);
	}
	for (m1 = al; m1; m1 = m2) {
		m2 = m1->next;
		OF_release(m1, m1->size);
	}
#endif
}

#ifdef	__notdef__
#ifdef	FIREPOWERBUGS
/*
 * Since firmware insists on running virtual, we manage memory ourselves,
 * hoping that OpenFirmware will not need extra memory.
 * (But then, the callbacks don't work anyway).
 */
#define	OFMEM_REGIONS	32
static struct {
	u_int start;
	u_int size;
} OFavail[OFMEM_REGIONS];

void *
OF_claim(virt, size, align)
	void *virt;
	u_int size, align;
{
	static int init;
	int i;
	u_int addr = -1;
	
	if (!init) {
		int phandle;
		
		init = 1;

		if ((phandle = OF_finddevice("/memory")) == -1
		    || OF_getprop(phandle, "available",
				  OFavail, sizeof OFavail) <= 0)
			return (void *)-1;
	}
	if (align) {
		/* Due to the above, anything is page aligned here */
		for (i = 0; i < OFMEM_REGIONS; i++) {
			if (!OFavail[i].size)
				break;
			if (OFavail[i].size > size) {
				addr = OFavail[i].start;
				OFavail[i].start += size;
				OFavail[i].size -= size;
				break;
			}
		}
	} else {
		addr = (u_int)virt;
		for (i = 0; i < OFMEM_REGIONS; i++) {
			if (!OFavail[i].size) {
				addr = -1;
				break;
			}
			if (OFavail[i].start <= addr
			    && addr + size - OFavail[i].start <= OFavail[i].size) {
				/* Be lazy here, just cut off anything below addr */
				size += addr - OFavail[i].start;
				OFavail[i].start += size;
				OFavail[i].size -= size;
				break;
			}
		}
	}
	return (void *)addr;
}

/* Since this is called solely immediately before chain, we ignore it. */
void
OF_release(virt, size)
	void *virt;
	u_int size;
{
}
#endif	/* FIREPOWERBUGS */
#endif
