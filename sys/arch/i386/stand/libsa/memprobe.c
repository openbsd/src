/*	$OpenBSD: memprobe.c,v 1.34 2000/03/05 18:40:59 niklas Exp $	*/

/*
 * Copyright (c) 1997-1999 Michael Shalayeff
 * Copyright (c) 1997 Tobias Weingartner
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
#include <dev/isa/isareg.h>
#include <stand/boot/bootarg.h>
#include "libsa.h"

u_int cnvmem, extmem;		/* XXX - compatibility */


/* Check gateA20
 *
 * A sanity check.
 */
static __inline int
checkA20(void)
{
	register char *p = (char *)0x100000;
	register char *q = (char *)0x000000;
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
			if (!mp->type)
				mp->type = BIOS_MAP_RES;
			mp++;
	} while (off);

	if (!gotcha)
		return (NULL);
#ifdef DEBUG
	printf("0x15[E820] ");
#endif
	return (mp);
}

/* XXX Disabled until it is shown it is needed, and a version that does not
 * confuse the AT&T Globalyst 580 comes up.  Ask niklas@openbsd.org if you
 * want to know details.
 */
#if 0
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
		: "=a" (m1), "=b" (m2), "=c" (rc) : "0" (0xE801));

	/* Make a memory map from info */
	if(rc & 0xff)
		return (NULL);
#ifdef DEBUG
	printf("0x15[E801] ");
#endif
	/* Fill out BIOS map */
	mp->addr = (1024 * 1024);	/* 1MB */
	mp->size = (m1 & 0xffff) * 1024;
	mp->type = BIOS_MAP_FREE;

	mp++;
	mp->addr = (1024 * 1024) * 16;	/* 16MB */
	mp->size = (m2 & 0xffff) * 64 * 1024;
	mp->type = BIOS_MAP_FREE;

	return ++mp;
}
#endif

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
	printf("0x15[8800] ");
#endif
	/* Fill out a BIOS_MAP */
	mp->addr = 1024 * 1024;		/* 1MB */
	mp->size = (mem & 0xffff) * 1024;
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
	printf("0x12 ");
#endif
	__asm __volatile(DOINT(0x12) : "=a" (mem) :: "%ecx", "%edx", "cc");

	/* Fill out a bios_memmap_t */
	mp->addr = 0;
	mp->size = (mem & 0xffff) * 1024;
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
 * XXX - Does not detect aliased memory.
 * XXX - Could be destructive, as it does write.
 */
static __inline bios_memmap_t *
badprobe(mp)
	register bios_memmap_t *mp;
{
	int ram;
#ifdef DEBUG
	printf("scan ");
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
	mp->size = (ram - 1024) * 1024;
	mp->type = BIOS_MAP_FREE;

	return ++mp;
}

bios_memmap_t bios_memmap[32];	/* This is easier */
#ifndef _TEST
void
memprobe()
{
	bios_memmap_t *pm = bios_memmap, *im;

#ifdef DEBUG
	printf(" mem(");
#else
	printf(" mem[");
#endif

	if(!(pm = bios_E820(bios_memmap))) {
		im = bios_int12(bios_memmap);
#if 0
		pm = bios_E801(im);
		if (!pm)
#endif
			pm = bios_8800(im);
		if (!pm)
			pm = badprobe(im);
		if (!pm) {
			printf (" No Extended memory detected.");
			pm = im;
		}
	}

	pm->type = BIOS_MAP_END;
	/* gotta peephole optimize the list */

	apmcheck();

#ifdef DEBUG
	printf(")[");
#endif

	/* XXX - Compatibility, remove later (smpprobe() relies on it) */
	extmem = cnvmem = 0;
	for(im = bios_memmap; im->type != BIOS_MAP_END; im++) {
		/* Count only "good" memory chunks 12K and up in size */
		if ((im->type == BIOS_MAP_FREE) && (im->size >= 12*1024)) {
			if (im->size > 1024 * 1024)
				printf("%uM ", (u_int)im->size / (1024 * 1024));
			else
				printf("%uK ", (u_int)im->size / 1024);

			/*
			 * Compute compatibility values:
			 * cnvmem -- is the upper boundary of conventional
			 *	memory (below IOM_BEGIN (=640k))
			 * extmem -- is the size of the contignous extended
			 *	memory segment starting at 1M
			 *
			 * We ignore "good" memory in the 640K-1M hole.
			 * We drop "machine {cnvmem,extmem}" commands.
			 */
			if(im->addr < IOM_BEGIN)
				cnvmem = max(cnvmem, im->addr + im->size);
			if(im->addr >= IOM_END)
				extmem += im->size;
		}
	}
	cnvmem /= 1024;
	extmem /= 1024;

	/* Check if gate A20 is on */
	printf("a20=o%s] ", checkA20()? "n" : "ff!");
}
#endif

void
dump_biosmem(tm)
	bios_memmap_t *tm;
{
	register bios_memmap_t *p;
	register u_int total = 0;

	if (!tm)
		tm = bios_memmap;

	for(p = tm; p->type != BIOS_MAP_END; p++) {
		printf("Region %d: type %u at 0x%x for %uKB\n", p - tm,
			p->type, (u_int)p->addr, (u_int)p->size / 1024);

		if(p->type == BIOS_MAP_FREE)
			total += p->size / 1024;
	}

	printf("Low ram: %dKB  High ram: %dKB\n", cnvmem, extmem);
	printf("Total free memory: %uKB\n", total);
}

int
mem_delete(sa, ea)
	long sa, ea;
{
	register bios_memmap_t *p;

	for (p = bios_memmap; p->type != BIOS_MAP_END; p++) {
		if (p->type == BIOS_MAP_FREE) {
			register int32_t sp = p->addr, ep = p->addr + p->size;

			/* can we eat it as a whole? */
			if ((sa - sp) <= NBPG && (ep - ea) <= NBPG) {
				bcopy (p + 1, p, (char *)bios_memmap +
				       sizeof(bios_memmap) - (char *)p);
				break;
			/* eat head or legs */
			} else if (sa <= sp && sp < ea) {
				p->addr = ea;
				p->size = ep - ea;
				break;
			} else if (sa < ep && ep <= ea) {
				p->size = sa - sp;
				break;
			} else if (sp < sa && ea < ep) {
				/* bite in half */
				bcopy (p, p + 1, (char *)bios_memmap +
				       sizeof(bios_memmap) - (char *)p -
				       sizeof(bios_memmap[0]));
				p[1].addr = ea;
				p[1].size = ep - ea;
				p->size = sa - sp;
				break;
			}
		}
	}
	return 0;
}

int
mem_add(sa, ea)
	long sa, ea;
{
	register bios_memmap_t *p;

	for (p = bios_memmap; p->type != BIOS_MAP_END; p++) {
		if (p->type == BIOS_MAP_FREE) {
			register int32_t sp = p->addr, ep = p->addr + p->size;

			/* is it already there? */
			if (sp <= sa && ea <= ep) {
				break;
			/* join head or legs */
			} else if (sa < sp && sp <= ea) {
				p->addr = sa;
				p->size = ep - sa;
				break;
			} else if (sa <= ep && ep < ea) {
				p->size = ea - sp;
				break;
			} else if (ea < sp) {
				/* insert before */
				bcopy (p, p + 1, (char *)bios_memmap +
				       sizeof(bios_memmap) - (char *)(p - 1));
				p->addr = sa;
				p->size = ea - sa;
				break;
			}
		}
	}

	/* meaning add new item at the end of the list */
	if (p->type == BIOS_MAP_END) {
		p[1] = p[0];
		p->type = BIOS_MAP_FREE;
		p->addr = sa;
		p->size = ea - sa;
	}

	return 0;
}

void
mem_pass()
{
	bios_memmap_t *p;

	for (p = bios_memmap; p->type != BIOS_MAP_END; p++)
		;
	addbootarg(BOOTARG_MEMMAP, (p - bios_memmap + 1) * sizeof *bios_memmap,
		bios_memmap);
}
