/*	$OpenBSD: boot.c,v 1.5 1998/05/11 07:36:26 niklas Exp $ */
/*	$NetBSD: boot.c,v 1.7 1997/06/08 17:49:16 ragge Exp $ */
/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)boot.c	7.15 (Berkeley) 5/4/91
 */

#include "sys/param.h"
#include "sys/reboot.h"
#include "lib/libsa/stand.h"

#define V750UCODE(x)    ((x>>8)&255)

#include <sys/exec.h>

/*
 * Boot program... arguments passed in r10 and r11 determine
 * whether boot stops to ask for system name and which device
 * boot comes from.
 */

char line[100];
volatile int devtype, bootdev;
extern	unsigned opendev;
extern  unsigned *bootregs;

Xmain()
{
	register howto asm("r11");
	register bdev  asm("r10");
	int io, retry, type;
	extern	char vers[];

	io=0;
	bootdev=bdev;
	autoconf();

	if ((howto & RB_ASKNAME) == 0) {
		type = (devtype >> B_TYPESHIFT) & B_TYPEMASK;
		if ((unsigned)type < ndevs && devsw[type].dv_name)
			strcpy(line, "/bsd");
		else
			howto |= RB_SINGLE|RB_ASKNAME;
	}

	for (retry = 0;;) {
		if (io >= 0)
			printf("\n%s\n", vers);
		if (howto & RB_ASKNAME) {
			printf(": ");
			gets(line);
			if (line[0] == 0) {
				strcpy(line, "/bsd");
				printf(": %s\n", line);
			}
		} else
			printf(": %s\n", line);
		io = open(line, 0);
		if (io >= 0) {
			loadpcs();
			copyunix(howto, opendev, io);
			close(io);
			howto |= RB_SINGLE|RB_ASKNAME;
		} else {
			printf("%s\n",strerror(errno));
		}
		if (++retry > 2)
			howto |= RB_SINGLE|RB_ASKNAME;
	}
}

/*ARGSUSED*/
copyunix(howto, devtype, aio)
	register howto, devtype;	/* howto=r11, devtype=r10 */
	int aio;
{
	register int esym;		/* must be r9 */
	struct exec x;
	register int io = aio, i;
	char *addr;

	if (read(io, (char *)&x, sizeof(x)) != sizeof(x) || N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}
	printf("%d", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC && lseek(io, 0x400, SEEK_SET) == -1)
		goto shread;
	if (read(io, (char *)0, x.a_text) != x.a_text)
		goto shread;
	addr = (char *)x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((int)addr & CLOFSET)
			*addr++ = 0;
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	printf("+%d", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;
	if (howto & RB_KDB && x.a_syms) {
		*(int *)addr = x.a_syms;		/* symbol table size */
		addr += sizeof (int);
		printf("[+%d", x.a_syms);
		if (read(io, addr, x.a_syms) != x.a_syms)
			goto shread;
		addr += x.a_syms;
		if (read(io, addr, sizeof (int)) != sizeof (int))
			goto shread;
		i = *(int *)addr - sizeof (int);	/* string table size */
		addr += sizeof (int);
		printf("+%d]", i);
		if (read(io, addr, i) != i)
			goto shread;
		addr += i;
		esym = roundup((int)addr, sizeof (int));
		x.a_bss = 0;
	} else
		howto &= ~RB_KDB;
	for (i = 0; i < 128*512; i++)	/* slop */
		*addr++ = 0;
	printf(" start 0x%x\n", (x.a_entry&0x7fffffff));
	hoppabort((x.a_entry&0x7fffffff),howto, devtype, esym);
	return;
shread:
	printf("\nShort read\n\n");
	return;
}

/* 750 Patchable Control Store magic */

#include "../include/mtpr.h"
#include "../include/cpu.h"
#include "../include/sid.h"
#define	PCS_BITCNT	0x2000		/* number of patchbits */
#define	PCS_MICRONUM	0x400		/* number of ucode locs */
#define	PCS_PATCHADDR	0xf00000	/* start addr of patchbits */
#define	PCS_PCSADDR	(PCS_PATCHADDR+0x8000)	/* start addr of pcs */
#define	PCS_PATCHBIT	(PCS_PATCHADDR+0xc000)	/* patchbits enable reg */
#define	PCS_ENABLE	0xfff00000	/* enable bits for pcs */

#define	extzv(one, two, three,four)	\
({			\
	asm __volatile (" extzv %0,%3,(%1),(%2)+"	\
			:			\
			: "g"(one),"g"(two),"g"(three),"g"(four));	\
})


loadpcs()
{
	register int *ip;	/* known to be r11 below */
	register int i;		/* known to be r10 below */
	register int *jp;	/* known to be r9 below */
	register int j;
	static int pcsdone = 0;
	int mid = mfpr(PR_SID);
	char pcs[100];
	char *cp;

	if ((mid >> 24) != VAX_750 || ((mid >> 8) & 255) < 95 || pcsdone)
		return;
	printf("Updating 11/750 microcode: ");
	for (cp = line; *cp; cp++)
		if (*cp == ')' || *cp == ':')
			break;
	if (*cp) {
		bcopy(line, pcs, 99);
		pcs[99] = 0;
		i = cp - line + 1;
	} else
		i = 0;
	strcpy(pcs + i, "pcs750.bin");
	i = open(pcs, 0);
	if (i < 0) {
		printf("bad luck - missing pcs750.bin :-(\n");
		return;
	}
	/*
	 * We ask for more than we need to be sure we get only what we expect.
	 * After read:
	 *	locs 0 - 1023	packed patchbits
	 *	 1024 - 11264	packed microcode
	 */
	if (read(i, (char *)0, 23*512) != 22*512) {
		printf("Error reading %s\n", pcs);
		close(i);
		return;
	}
	close(i);

	/*
	 * Enable patchbit loading and load the bits one at a time.
	 */
	*((int *)PCS_PATCHBIT) = 1;
	ip = (int *)PCS_PATCHADDR;
	jp = (int *)0;
	for (i=0; i < PCS_BITCNT; i++) {
		extzv(i,jp,ip,1);
	}
	*((int *)PCS_PATCHBIT) = 0;

	/*
	 * Load PCS microcode 20 bits at a time.
	 */
	ip = (int *)PCS_PCSADDR;
	jp = (int *)1024;
	for (i=j=0; j < PCS_MICRONUM * 4; i+=20, j++) {
		extzv(i,jp,ip,20);
	}

	/*
	 * Enable PCS.
	 */
	i = *jp;		/* get 1st 20 bits of microcode again */
	i &= 0xfffff;
	i |= PCS_ENABLE;	/* reload these bits with PCS enable set */
	*((int *)PCS_PCSADDR) = i;

	mid = mfpr(PR_SID);
	printf("new rev level=%d\n", V750UCODE(mid));
	pcsdone = 1;
}
