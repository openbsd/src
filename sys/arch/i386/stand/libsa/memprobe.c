/*	$OpenBSD: memprobe.c,v 1.23 1997/10/22 23:34:40 mickey Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner, Michael Shalayeff
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
 *	This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <machine/biosvar.h>
#include <stand/boot/bootarg.h>
#include "libsa.h"

static int addrprobe __P((u_int));
u_int cnvmem, extmem;		/* XXX - compatibility */
bios_memmap_t *memory_map;

/* BIOS int 15, AX=E820
 *
 * This is the "prefered" method.
 */
static __inline bios_memmap_t *
bios_E820(mp)
	register bios_memmap_t *mp;
{
	int rc, off = 0, sig, gotcha = 0;

	do {
		BIOS_regs.biosr_es = ((u_int)(mp) >> 4);
		__asm __volatile(DOINT(0x15) "; setc %b1"
				: "=a" (sig), "=d" (rc), "=b" (off)
				: "0" (0xE820), "1" (0x534d4150), "b" (off),
				  "c" (sizeof(*mp)), "D" (((u_int)mp) & 0xF)
				: "cc", "memory");
			off = BIOS_regs.biosr_bx;

			if (rc & 0xff || sig != 0x534d4150)
				break;
			gotcha++;
			mp->size >>= 10; /* / 1024 */
			if (mp->type == 0)
				mp->type = BIOS_MAP_RES;
			mp++;
	} while (off);

	if (!gotcha)
		return (NULL);
#ifdef DEBUG
	printf("0x15[E820]");
#endif
	return (mp);
}

/* BIOS int 15, AX=E801
 *
 * Only used if int 15, AX=E820 does not work.
 * This should work for more than 64MB.
 */
static __inline bios_memmap_t *
bios_E801(mp)
	register bios_memmap_t *mp;
{
	int rc, m1, m2;

	/* Test for 0xE801 */
	__asm __volatile(DOINT(0x15) "; setc %b1"
		: "=a" (m1), "=c" (rc), "=d" (m2) : "0" (0xE801));

	/* Make a memory map from info */
	if(rc & 0xff)
		return (NULL);
#ifdef DEBUG
	printf("0x15[E801]");
#endif
	/* Fill out BIOS map */
	mp->addr = (1024 * 1024);	/* 1MB */
	mp->size = (m1 & 0xffff);
	mp->type = BIOS_MAP_FREE;

	mp++;
	mp->addr = (1024 * 1024) * 16;	/* 16MB */
	mp->size = (m2 & 0xffff) * 64;
	mp->type = BIOS_MAP_FREE;

	return ++mp;
}


/* BIOS int 15, AX=8800
 *
 * Only used if int 15, AX=E801 does not work.
 * Machines with this are restricted to 64MB.
 */
static __inline bios_memmap_t *
bios_8800(mp)
	register bios_memmap_t *mp;
{
	int rc, mem;

	__asm __volatile(DOINT(0x15) "; setc %b0"
		: "=c" (rc), "=a" (mem) : "a" (0x8800));

	if(rc & 0xff)
		return (NULL);
#ifdef DEBUG
	printf("0x15[8800]");
#endif
	/* Fill out a BIOS_MAP */
	mp->addr = 1024 * 1024;		/* 1MB */
	mp->size = mem & 0xffff;
	mp->type = BIOS_MAP_FREE;

	return ++mp;
}

/* BIOS int 0x12 Get Conventional Memory
 *
 * Only used if int 15, AX=E820 does not work.
 */
static __inline bios_memmap_t *
bios_int12(mp)
	register bios_memmap_t *mp;
{
	int mem;
#ifdef DEBUG
	printf(", 0x12\n");
#endif
	__asm __volatile(DOINT(0x12) : "=a" (mem) :: "%ecx", "%edx", "cc");

	/* Fill out a bios_memmap_t */
	mp->addr = 0;
	mp->size = mem & 0xffff;
	mp->type = BIOS_MAP_FREE;

	return ++mp;
}


/* addrprobe(kloc): Probe memory at address kloc * 1024.
 *
 * This is a hack, but it seems to work ok.  Maybe this is
 * the *real* way that you are supposed to do probing???
 *
 * Modify the original a bit.  We write everything first, and
 * then test for the values.  This should croak on machines that
 * return values just written on non-existent memory...
 *
 * BTW: These machines are pretty broken IMHO.
 *
 * XXX - Does not detect aliased memory.
 */
static int
addrprobe(kloc)
	u_int kloc;
{
	__volatile u_int *loc;
	static const u_int pat[] = {
		0x00000000, 0xFFFFFFFF,
		0x01010101, 0x10101010,
		0x55555555, 0xCCCCCCCC
	};
	register u_int i, ret = 0;
	u_int save[NENTS(pat)];

	/* Get location */
	loc = (int *)(kloc * 1024);

	save[0] = *loc;
	/* Probe address */
	for(i = 0; i < NENTS(pat); i++){
		*loc = pat[i];
		if(*loc != pat[i])
			ret++;
	}
	*loc = save[0];

	if (!ret) {
		/* Write address */
		for(i = 0; i < NENTS(pat); i++) {
			save[i] = loc[i];
			loc[i] = pat[i];
		}

		/* Read address */
		for(i = 0; i < NENTS(pat); i++) {
			if(loc[i] != pat[i])
				ret++;
			loc[i] = save[i];
		}
	}

	return ret;
}

/* Probe for all extended memory.
 *
 * This is only used as a last resort.  If we resort to this
 * routine, we are getting pretty desparate.  Hopefully nobody
 * has to rely on this after all the work above.
 *
 * XXX - Does not detect aliases memory.
 * XXX - Could be destructive, as it does write.
 */
static __inline bios_memmap_t *
badprobe(mp)
	register bios_memmap_t *mp;
{
	int ram;
#ifdef DEBUG
	printf("Scan");
#endif
	/* probe extended memory
	 *
	 * There is no need to do this in assembly language.  This is
	 * much easier to debug in C anyways.
	 */
	for(ram = 1024; ram < 512 * 1024; ram += 4)
		if(addrprobe(ram))
			break;

	mp->addr = 1024 * 1024;
	mp->size = ram - 1024;
	mp->type = BIOS_MAP_FREE;

	return ++mp;
}

void
memprobe()
{
	static bios_memmap_t bm[32];	/* This is easier */
	bios_memmap_t *pm = bm, *im;
	int total = 0;
#ifdef DEBUG
	printf("Probing memory: ");
#endif
	if(!(pm = bios_E820(bm))) {
		im = bios_int12(bm);
		pm = bios_E801(im);
		if (!pm)
			pm = bios_8800(im);
		if (!pm)
			pm = badprobe(im);
		if (!pm) {
			printf ("No Extended memory detected.");
			pm = im;
		}
	}
#ifdef DEBUG
	printf("\n");
#endif
	pm->type = BIOS_MAP_END;
	/* Register in global var */
	addbootarg(BOOTARG_MEMMAP, (pm - bm + 1) * sizeof(*bm), bm);
	memory_map = bm; /* XXX for 'machine mem' command only */
	printf("mem0:");

	/* Get total free memory */
	for(im = bm; im->type != BIOS_MAP_END; im++) {
		if (im->type == BIOS_MAP_FREE) {
			total += im->size;
			printf(" %luK", (u_long)im->size);
		}
	}
	printf("\n");

	/* XXX - Compatibility, remove later */
	for(im = bm; im->type != BIOS_MAP_END; im++)
		if ((im->addr & 0xFFFFF) == im->addr &&
		   im->type == BIOS_MAP_FREE) {
			cnvmem = im->size; 
			break;		/* Take the first region */
		}

	extmem = total - cnvmem;
}
