/*	$NetBSD: swapgeneric.c,v 1.7 1995/08/09 03:22:50 briggs Exp $	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)swapgeneric.c	7.5 (Berkeley) 5/7/91
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <machine/pte.h>

#include "sd.h"
#include "cd.h"

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

struct	swdevt	swdevt[] = {
	{ NODEV,	1,	0 },
	{ NODEV,	0,	0 },
};

#if NSD > 0
extern struct cfdriver sdcd;
#endif
#if NCD > 0
extern struct cfdriver cdcd;
#endif

struct	genericconf {
	struct cfdriver	*gc_driver;
	char		*gc_name;
	dev_t		gc_root;
} genericconf[] = {
#if NSD > 0
	{ &sdcd,  "sd",  makedev(4,0) },
#endif
#if NCD > 0
	{ &cdcd,  "cd",  makedev(6,0) },
#endif
	{ 0 }
};

extern int ffs_mountroot();
int (*mountroot)() = ffs_mountroot;

setconf()
{
	register struct genericconf *gc = NULL;
	int	unit, swaponroot = 0;
	char	*root_swap;

	if (genericconf[0].gc_driver == 0)
		goto verybad;

	if (boothowto & RB_MINIROOT)
		root_swap = "swap";
	else {
		if (rootdev != NODEV)
			goto doswap;
		root_swap = "root";
	}

	if (boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("%s device? ", root_swap);
		gets(name);
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_name[0] == name[0] &&
			    gc->gc_name[1] == name[1])
				goto gotit;
		printf("use one of:");
		for (gc = genericconf; gc->gc_driver; gc++)
			printf(" %s%%d", gc->gc_name);
		printf("\n");
		goto retry;
gotit:
		if (name[3] == '*') {
			name[3] = name[4];
			swaponroot++;
		}
		if (name[2] >= '0' && name[2] <= '7' && name[3] == 0) {
			unit = name[2] - '0';
			goto found;
		}
		printf("bad/missing unit number\n");
	}
	unit = 0;
	for (gc = genericconf; gc->gc_driver; gc++) {
		if (gc->gc_driver->cd_ndevs > unit &&
		    gc->gc_driver->cd_devs[unit]) {
			printf("Trying %s on %s0\n", root_swap, gc->gc_name);
			goto found;
		}
	}
verybad:
	printf("no suitable %s", root_swap);
	if (root_swap[0] == 's') {
		printf("\n");
		goto doswap;
	}
	printf(" -- hit any key to reboot\n", root_swap);
	cngetc();
	doboot();
	printf("      Automatic reboot failed.\n");
	printf("You may reboot or turn the machine off, now.\n");
	for(;;);

found:
	gc->gc_root = makedev(major(gc->gc_root), unit * MAXPARTITIONS);
	if ((boothowto & RB_MINIROOT) == 0) {
		rootdev = gc->gc_root;
	}
doswap:
	if (gc)
		swdevt[0].sw_dev = dumpdev =
			makedev(major(gc->gc_root), minor(gc->gc_root) + 1);
	else
		swdevt[0].sw_dev = dumpdev =
			makedev(major(rootdev), minor(rootdev) + 1);
	/* swap size and dumplo set during autoconfigure */
	if (swaponroot)
	 	rootdev = dumpdev;
}

gets(cp)
	char *cp;
{
	register char	*lp;
	register int	c;

	lp = cp;
	for (;;) {
		cnputc(c=cngetc());
		switch (c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				lp--;
				cnputc(' ');
				cnputc('\b');
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u'&037:
			lp = cp;
			cnputc('\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}
