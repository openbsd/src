/*	$NetBSD: swapgeneric.c,v 1.6 1996/01/28 12:09:37 ragge Exp $	*/

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
 *	@(#)swapgeneric.c	7.11 (Berkeley) 5/9/91
 */

#include "uda.h"
#include "hp.h"

#include "sys/param.h"
#include "sys/conf.h"
#include "sys/buf.h"
#include "sys/systm.h"
#include "sys/reboot.h"
#include "sys/device.h"

#include "machine/pte.h"
#include "machine/mtpr.h"

#include "../uba/ubareg.h"
#include "../uba/ubavar.h"

/*
 * Generic configuration;  all in one
 */
dev_t	rootdev = NODEV;
dev_t	argdev = NODEV;
dev_t	dumpdev = NODEV;
int	nswap;
struct	swdevt swdevt[] = {
	{ -1,	1,	0 },
	{ -1,	1,	0 },
	{ -1,	1,	0 },
	{ 0,	0,	0 },
};
long	dumplo;
int	dmmin, dmmax, dmtext;

extern int ffs_mountroot();
int (*mountroot)() = ffs_mountroot;

extern	struct uba_driver scdriver;
extern	struct uba_driver hkdriver;
extern	struct uba_driver idcdriver;
extern	struct uba_driver hldriver;
extern	struct uba_driver udadriver;
extern	struct uba_driver kdbdriver;

extern	struct cfdriver hpcd;

struct	ngcconf {
	struct	cfdriver *ng_cf;
	dev_t	ng_root;
} ngcconf[] = {
#if NHP > 0
	{ &hpcd,	makedev(0, 0), },
#endif
	{ 0 },
};

struct	genericconf {
	caddr_t	gc_driver;
	char	*gc_name;
	dev_t	gc_root;
} genericconf[] = {
/*	{ (caddr_t)&hpcd,	"hp",	makedev(0, 0),	},
	{ (caddr_t)&scdriver,	"up",	makedev(2, 0),	}, */
#if NUDA > 0
	{ (caddr_t)&udadriver,	"ra",	makedev(9, 0),	},
#endif
/*	{ (caddr_t)&idcdriver,	"rb",	makedev(11, 0),	},
	{ (caddr_t)&hldriver,	"rl",	makedev(14, 0),	},
	{ (caddr_t)&hkdriver,	"hk",	makedev(3, 0),	},
	{ (caddr_t)&hkdriver,	"rk",	makedev(3, 0),	},
	{ (caddr_t)&kdbdriver,	"kra",	makedev(16, 0), }, */
	{ 0 },
};

setconf()
{
#if NUDA > 0
	register struct uba_device *ui;
#endif
	register struct genericconf *gc;
	struct	ngcconf *nc;
	register char *cp, *gp;
	int unit, swaponroot = 0, i;
	char name[128];

	if (rootdev != NODEV)
		goto doswap;
	unit = 0;
	/*
	 * First try new config devices.
	 */
	if (boothowto & RB_ASKNAME) {
nretry:
		swaponroot = 0;
		printf("root device? ");
		gets(name);
		if (name[strlen(name) - 1] == '*')
			name[strlen(name) - 1] = swaponroot++;
		for (nc = ngcconf; nc->ng_cf; nc++)
			for (i = 0; i < nc->ng_cf->cd_ndevs; i++)
				if (nc->ng_cf->cd_devs[i] &&
				    strcmp(name, ((struct device *)
				    (nc->ng_cf->cd_devs[i]))->dv_xname) == 0)
					goto ngotit;
#ifdef notyet
		printf("Use one of ");
		for (nc = ngcconf; nc->ng_cf; nc++)
			printf("%s%%d ", nc->ng_cf->cd_name);
		printf("\n");
#endif
		goto gc2;
ngotit:
		rootdev = makedev(major(nc->ng_root), i * 8);
		goto doswap;

	} else {
		for (nc = ngcconf; nc->ng_cf; nc++)
			for (i = 0; i < nc->ng_cf->cd_ndevs; i++)
				if (nc->ng_cf->cd_devs[i]) {
					printf("root on %s%d\n",
					    nc->ng_cf->cd_name, i);
					rootdev = makedev(major(nc->ng_root),
					    i * 8);
					goto doswap;
				}

	}

	if (boothowto & RB_ASKNAME) {
retry:
		printf("root device? ");
		gets(name);
gc2:
		for (gc = genericconf; gc->gc_driver; gc++)
		    for (cp = name, gp = gc->gc_name; *cp == *gp; cp++)
			if (*++gp == 0)
				goto gotit;
		printf(
		  "use hp%%d, up%%d, ra%%d, rb%%d, rl%%d, hk%%d or kra%%d\n");
		goto nretry;
gotit:
		if (*++cp < '0' || *cp > '9') {
			printf("bad/missing unit number\n");
			goto retry;
		}
		while (*cp >= '0' && *cp <= '9')
			unit = 10 * unit + *cp++ - '0';
		if (*cp == '*')
			swaponroot++;
		goto found;
	}
	for (gc = genericconf; gc->gc_driver; gc++) {
#if NUDA > 0
		for (ui = ubdinit; ui->ui_driver; ui++) {
			if (ui->ui_alive == 0)
				continue;
			if (ui->ui_unit == unit && ui->ui_driver ==
			    (struct uba_driver *)gc->gc_driver) {
				printf("root on %s%d\n",
				    ui->ui_driver->ud_dname, unit);
				goto found;
			}
		}
#endif
	}

	printf("no suitable root\n");
	asm("halt");

found:
	gc->gc_root = makedev(major(gc->gc_root), unit*8);
	rootdev = gc->gc_root;
doswap:
	swdevt[0].sw_dev = argdev = dumpdev =
	    makedev(major(rootdev), minor(rootdev)+1);
	/* swap size and dumplo set during autoconfigure */
	if (swaponroot)
		rootdev = dumpdev;
}

gets(cp)
	char *cp;
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		cnputc(c = (cngetc()&0x7f));
		switch (c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '#':
		case '\177':
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
