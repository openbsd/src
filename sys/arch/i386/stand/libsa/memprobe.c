/*	$OpenBSD: memprobe.c,v 1.17 1997/10/20 14:47:42 mickey Exp $	*/

/*
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
#include "libsa.h"

static int addrprobe __P((u_int));
u_int cnvmem, extmem;		/* XXX - remove */
bios_memmap_t *memory_map;

struct E820_desc_t {
	u_int32_t addr_lo;
	u_int32_t addr_hi;
	u_int32_t size_lo;
	u_int32_t size_hi;
	u_int32_t type;
} __attribute__ ((packed));
static struct E820_desc_t Desc;


#define E820_MAX_MAPENT	10

/* BIOS int 15, AX=E820
 *
 * This is the "prefered" method.
 */
bios_memmap_t *
bios_E820()
{
	static bios_memmap_t bm[E820_MAX_MAPENT];		/* This is easier */
	int E820Present = 0;
	int eax = 0, count = 0;
	volatile int ebx = 0;

	do {
		BIOS_regs.biosr_es = ((unsigned)(&Desc) >> 4);
		__asm __volatile(
			"movl %2, %%ebx\n\t"
			"movl $0x534D4150, %%edx\n\t"
			DOINT(0x15) "\n\t"
			"movl %%eax, %%edx\n\t"
			"setc %b0\n\t"
			"movzbl %b0, %0\n\t"
			"cmpl $0x534D4150, %%edx\n\t"
			"je 1f\n\t"
			"incl %%eax\n\t"
			"1:\n\t"
			: "=a" (eax)
			: "0" (0xE820),
			  "1" (ebx),
			  "c" (sizeof(Desc)),
			  "D" ((unsigned)(&Desc) & 0xF)
			: "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "cc", "memory");
			ebx = BIOS_regs.biosr_bx;

			if(eax) break;		/* Not present, or done */

			/* We are ignoring the upper 32 bits in the 64 bit
			 * integer returned.  If anyone has a PC where the
			 * upper 32 bits are not zeros, *please* tell me! ;)
			 *
			 * NOTE: the BIOS_MAP_* numbers are the same, save for
			 * the "zero" case, which we remap to reserved.
			 */
			bm[count].addr = Desc.addr_lo;
			bm[count].size = Desc.size_lo;
			bm[count].type = (Desc.type)?Desc.type:BIOS_MAP_RES;

			E820Present = 1;
	} while((++count < E820_MAX_MAPENT) && (ebx != 0));	/* Do not re-oder! */

	if(E820Present){
		printf("int 0x15 [E820]\n");
		bm[count].type = BIOS_MAP_END;
		return(bm);
	}

	return(NULL);
}


/* BIOS int 15, AX=E801
 *
 * Only used if int 15, AX=E820 does not work.
 * This should work for more than 64MB.
 */
bios_memmap_t *
bios_E801()
{
	static bios_memmap_t bm[3];
	int eax, edx = 0;

	/* Test for 0xE801 */
	__asm __volatile(
		DOINT(0x15) "\n\t"
		"setc %b0\n\t"
		"movzbl %b0, %0\n\t"
		: "=a" (eax)
		: "a" (0xE801)
		);

	/* Make a memory map from info */
	if(!eax){
		printf("int 0x15 [E801], ");

		__asm __volatile(
			DOINT(0x15) "\n\t"
			: "=a" (eax), "=d" (edx)
			: "a" (0xE801)
			);

		/* Make sure only valid bits */
		eax &= 0xFFFF;
		edx &= 0xFFFF;

		/* Fill out BIOS map */
		bm[0].addr = (1024 * 1024);			/* 1MB */
		bm[0].size = eax * 1024;
		bm[0].type = BIOS_MAP_FREE;

		bm[1].addr = (1024 * 1024) * 16;	/* 16MB */
		bm[1].size = (edx * 1024) * 64;
		bm[1].type = BIOS_MAP_FREE;

		bm[2].type = BIOS_MAP_END;

		return(bm);
	}

	return(NULL);
}


/* BIOS int 15, AX=8800
 *
 * Only used if int 15, AX=E801 does not work.
 * Machines with this are restricted to 64MB.
 */
bios_memmap_t *
bios_8800()
{
	static bios_memmap_t bm[2];
	int eax, mem;

	__asm __volatile(
		DOINT(0x15) "\n\t"
		"movl %%eax, %%ecx\n\t"
		"setc %b0\n\t"
		"movzbl %b0, %0\n\t"
		: "=a" (eax), "=c" (mem)
		: "a" (0x8800)
		);

	if(eax) return(NULL);

	printf("int 0x15 [8800], ");

	/* Fill out a BIOS_MAP */
	bm[0].addr = 1024*1024;		/* 1MB */
	bm[0].size = (mem & 0xFFFF) * 1024;
	bm[0].type = BIOS_MAP_FREE;

	bm[1].type = BIOS_MAP_END;

	return(bm);
}


/* BIOS int 0x12 Get Conventional Memory
 *
 * Only used if int 15, AX=E820 does not work.
 */
bios_memmap_t *
bios_int12()
{
	static bios_memmap_t bm[2];
	int mem;

	printf("int 0x12\n");

	__asm __volatile(DOINT(0x12) : "=a" (mem)
			 :: "%ecx", "%edx", "cc");
	mem &= 0xffff;

	/* Fill out a BIOS_MAP */
	bm[0].addr = 0;
	bm[0].size = mem * 1024;
	bm[0].type = BIOS_MAP_FREE;

	bm[1].type = BIOS_MAP_END;

	return(bm);
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
bios_memmap_t *
badprobe()
{
	static bios_memmap_t bm[2];
	int ram;

	printf("Physical, ");
	/* probe extended memory
	 *
	 * There is no need to do this in assembly language.  This is
	 * much easier to debug in C anyways.
	 */
	for(ram = 1024; ram < 512*1024; ram += 4)
		if(addrprobe(ram))
			break;

	bm[0].addr = 1024 * 1024;
	bm[0].size = (ram - 1024) * 1024;
	bm[0].type = BIOS_MAP_FREE;

	bm[1].type = BIOS_MAP_END;

	return(bm);
}


int
count(map)
	bios_memmap_t *map;
{
	int i;

	for(i = 0; map[i].type != BIOS_MAP_END; i++) ;

	return(i);
}


bios_memmap_t *
combine(a, b)
	bios_memmap_t *a, *b;
{
	bios_memmap_t *res;
	int size, i;

	/* Sanity checks */
	if(!b) return(a);
	if(!a) return(b);

	size = (count(a) + count(b) + 1) * sizeof(bios_memmap_t);
	res = alloc(size);

	/* Again */
	if(!res) return(NULL);		/* We are in deep doggie-doo */

	for(i = 0; a[i].type != BIOS_MAP_END; i++)
		res[i] = a[i];
	size = count(a);
	for(; b[i - size].type != BIOS_MAP_END; i++)
		res[i] = b[i - size];

	res[i].type = BIOS_MAP_END;
	return(res);
}


void
memprobe()
{
	bios_memmap_t *tm, *em, *bm;	/* total, extended, base */
	int count, total = 0;

	printf("Probing memory: ");
	tm = em = bm = NULL;

	tm = bios_E820();
	if(!tm){
		em = bios_E801();
		if(!em) em = bios_8800();
		if(!em) em = badprobe();
		bm = bios_int12();

		tm = combine(bm, em);
	}

	/* Register in global var */
	memory_map = tm;
	printf("mem0:");

	/* Get total free memory */
	for(count = 0; tm[count].type != BIOS_MAP_END; count++) {
		if(tm[count].type == BIOS_MAP_FREE) {
			total += tm[count].size;

			printf(" %luKB", (long)tm[count].size/1024);
		}
	}
	printf("\n");

	/* XXX - Compatibility, remove later */
	cnvmem = extmem = 0;
	for(count = 0; tm[count].type != BIOS_MAP_END; count++) {
		if((tm[count].addr < 0xFFFFF) && (tm[count].type == BIOS_MAP_FREE)){

			cnvmem += tm[count].size; 
		}
		if((tm[count].addr > 0xFFFFF) && (tm[count].type == BIOS_MAP_FREE)){

			extmem += tm[count].size; 
		}
	}
	cnvmem /= 1024;
	extmem /= 1024;
}

