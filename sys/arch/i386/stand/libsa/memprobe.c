/*	$OpenBSD: memprobe.c,v 1.27 1998/04/18 07:39:54 deraadt Exp $	*/

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

u_int cnvmem, extmem;		/* XXX - compatibility */
bios_memmap_t *memory_map;


/* Check gateA20
 *
 * A sanity check.
 */
static __inline int
checkA20(void)
{
	char *p = (char *)0x100000;
	char *q = (char *)0x000000;
	int st;

	/* Simple check */
	if(*p != *q)
		return(1);

	/* Complex check */
	*p = ~(*p);
	st = (*p != *q);
	*p = ~(*p);

	return(st);
}

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
const u_int addrprobe_pat[] = {
	0x00000000, 0xFFFFFFFF,
	0x01010101, 0x10101010,
	0x55555555, 0xCCCCCCCC
};
static int
addrprobe(kloc)
	u_int kloc;
{
	__volatile u_int *loc;
	register u_int i, ret = 0;
	u_int save[NENTS(addrprobe_pat)];

	/* Get location */
	loc = (int *)(kloc * 1024);

	save[0] = *loc;
	/* Probe address */
	for(i = 0; i < NENTS(addrprobe_pat); i++){
		*loc = addrprobe_pat[i];
		if(*loc != addrprobe_pat[i])
			ret++;
	}
	*loc = save[0];

	if (!ret) {
		/* Write address */
		for(i = 0; i < NENTS(addrprobe_pat); i++) {
			save[i] = loc[i];
			loc[i] = addrprobe_pat[i];
		}

		/* Read address */
		for(i = 0; i < NENTS(addrprobe_pat); i++) {
			if(loc[i] != addrprobe_pat[i])
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

bios_memmap_t bios_memmap[32];	/* This is easier */
void
memprobe()
{
	bios_memmap_t *pm = bios_memmap, *im;
#ifdef DEBUG
	printf("Probing memory: ");
#endif
	if(!(pm = bios_E820(bios_memmap))) {
		im = bios_int12(bios_memmap);
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
	addbootarg(BOOTARG_MEMMAP, 
		(pm - bios_memmap + 1) * sizeof(*bios_memmap), bios_memmap);
	memory_map = bios_memmap; /* XXX for 'machine mem' command only */
	printf("memory:");

	/* XXX - Compatibility, remove later */
	extmem = cnvmem = 0;
	for(im = bios_memmap; im->type != BIOS_MAP_END; im++) {
		/* Count only "good" memory chunks 4K an up in size */
		if ((im->type == BIOS_MAP_FREE) && (im->size >= 4)) {
			printf(" %luK", (u_long)im->size);

			/* We ignore "good" memory in the 640K-1M hole */
			if(im->addr < 0xA0000)
				cnvmem += im->size;
			if(im->addr >= 0x100000)
				extmem += im->size;
		}
	}

	/* Check if gate A20 is on */
	if(checkA20())
		printf(" [A20 on]");
	else
		printf(" [A20 off!]");

	printf("\n");
}
