/*	$OpenBSD: boot.c,v 1.7 2001/09/10 04:28:52 bjc Exp $ */
/*	$NetBSD: boot.c,v 1.4 1999/10/23 14:42:22 ragge Exp $ */
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
#include "rpb.h"
#include "lib/libsa/stand.h"

#define V750UCODE(x)    ((x>>8)&255)

#include "vaxstand.h"

/*
 * Boot program... arguments passed in r10 and r11 determine
 * whether boot stops to ask for system name and which device
 * boot comes from.
 */

char line[100];
int	devtype, bootdev, howto, debug;
extern	unsigned opendev;
extern  unsigned *bootregs;
struct	rpb *rpb;

void	usage(), boot(), halt();

struct vals {
	char	*namn;
	void	(*func)();
	char	*info;
} val[] = {
	{"?", usage, "Show this help menu"},
	{"help", usage, "Same as '?'"},
	{"boot", boot, "Load and execute file"},
	{"halt", halt, "Halts the system"},
	{0, 0},
};

char *filer[] = {
	"bsd",
	"bsd.gz",
	"bsd.old",
	0,
};

int jbuf[10];
int sluttid, senast, skip;

Xmain()
{
	int io, type, filindex = 0;
	int j, nu;
	volatile int askname;

	/* make sure the rpb is out of the way so it does not get trampled;
	 * this will be the case if booting from net
	 */

	rpb = (struct rpb *)bootregs[11];
	bootdev = rpb->devtyp;
	io=0;
	skip = 1;
	autoconf();

	askname = howto & RB_ASKNAME;
	printf("\n\r>> OpenBSD/vax boot [%s %s] <<\n", __DATE__, __TIME__);
	printf(">> Press enter to autoboot now, or any other key to abort:  ");
	sluttid = getsecs() + 5;
	senast = 0;
	skip = 0;
	setjmp(jbuf);
	for (;;) {
		nu = sluttid - getsecs();
		if (senast != nu)
			printf("%c%d", 8, nu);
		if (nu <= 0)
			break;
		senast = nu;
		if ((j = (testkey() & 0177))) {
			skip = 1;
			if (j != 10 && j != 13) {
				printf("\nPress '?' for help");
				askname = 1;
			}
			break;
		}
	}
	skip = 1;
	printf("\n");

	/* First try to autoboot */
	if (askname == 0) {
		type = (devtype >> B_TYPESHIFT) & B_TYPEMASK;
		if ((unsigned)type < ndevs && devsw[type].dv_name)
			while (filer[filindex]) {
				errno = 0;
				printf("> boot %s\n", filer[filindex]);
				exec(filer[filindex++], 0, 0);
				printf("boot failed: %s\n", strerror(errno));
#if 0
				if (testkey())
					break;
#endif
			}
	}

	/* If any key pressed, go to conversational boot */
	for (;;) {
		struct vals *v = &val[0];
		char *c, *d;

		printf("> ");
		gets(line);

		c = line;
		while (*c == ' ')
			c++;

		if (c[0] == 0)
			continue;

		if ((d = index(c, ' ')))
			*d++ = 0;

		while (v->namn) {
			if (strcmp(v->namn, c) == 0)
				break;
			v++;
		}
		if (v->namn)
			(*v->func)(d);
		else
			printf("Unknown command: %s\n", c);
			
	}
}

void
halt()
{
	asm("halt");
}

void
boot(arg)
	char *arg;
{
	char *fn = "bsd";

	if (arg) {
		while (*arg == ' ')
			arg++;

		if (*arg != '-') {
			fn = arg;
			if ((arg = index(arg, ' '))) {
				*arg++ = 0;
				while (*arg == ' ')
					arg++;
			} else
				goto load;
		}
		if (*arg != '-') {
fail:			printf("usage: boot [filename] [-asd]\n");
			return;
		}

		while (*++arg) {
			if (*arg == 'a')
				howto |= RB_ASKNAME;
			else if (*arg == 'd')
				howto |= RB_KDB;
			else if (*arg == 's')
				howto |= RB_SINGLE;
			else
				goto fail;
		}
	}
load:	exec(fn, 0, 0);
	printf("Boot failed: %s\n", strerror(errno));
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
	asm __volatile (" extzv %0,%3,%1,(%2)+"	\
			:			\
			: "g"(one),"g"(two),"r"(three),"g"(four));	\
})


loadpcs()
{
	static int pcsdone = 0;
	int mid = mfpr(PR_SID);
	int i, j, *ip, *jp;
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

void
usage()
{
	struct vals *v = &val[0];

	printf("Commands:\n");
	while (v->namn) {
		printf("%s\t%s\n", v->namn, v->info);
		v++;
	}
}
