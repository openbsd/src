/*	$NetBSD: swapgeneric.c,v 1.12 1995/09/24 02:18:11 thorpej Exp $	*/

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
#include <sys/disklabel.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <hp300/dev/device.h>

#include "sd.h"
#include "rd.h"
#include "le.h"

/*
 * Generic configuration;  all in one
 */

dev_t	rootdev = NODEV;
dev_t	argdev = NODEV;
dev_t	dumpdev = NODEV;
int	nswap;
struct	swdevt swdevt[] = {
	{ -1,	1,	0 },
	{ NODEV,	0,	0 },
};
int	dmmin, dmmax, dmtext;

#if NRD > 0
extern	struct driver rddriver;
#endif
#if NSD > 0
extern	struct driver sddriver;
#endif
#if NLE > 0
extern	struct driver ledriver;
#endif

extern struct hp_ctlr hp_cinit[];
extern struct hp_device hp_dinit[];

extern u_long bootdev;

static	int no_mountroot __P((void));

#ifdef FFS
extern int ffs_mountroot();
#else
#define ffs_mountroot		no_mountroot
#endif /* FFS */

#ifdef NFSCLIENT
extern char *nfsbootdevname;	/* from nfs_boot.c */
extern int nfs_mountroot();	/* nfs_vfsops.c */
static char nfsbootdevname_buf[128];
#else
static char *nfsbootdevname;
#define nfs_mountroot		no_mountroot
#endif /* NFSCLIENT */

/* XXX: should eventually ask for root fs type. */

struct	genericconf {
	caddr_t	gc_driver;
	char	*gc_name;
	dev_t	gc_root;
	int	(*gc_mountroot)();
} genericconf[] = {
#if NRD > 0
	{ (caddr_t)&rddriver,	"rd",	makedev(2, 0),	ffs_mountroot },
#endif
#if NSD > 0
	{ (caddr_t)&sddriver,	"sd",	makedev(4, 0),	ffs_mountroot },
#endif
#if NLE > 0
	{ (caddr_t)&ledriver,	"le",	NODEV,		nfs_mountroot },
#endif
	{ 0 },
};

int (*mountroot)() = ffs_mountroot;

setconf()
{
	register struct hp_ctlr *hc;
	register struct hp_device *hd;
	register struct genericconf *gc;
	register char *cp;
	int unit, swaponroot = 0;

	if (rootdev != NODEV)
		goto doswap;
	unit = 0;
	if (boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("root device? ");
		gets(name);
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_name[0] == name[0] &&
			    gc->gc_name[1] == name[1])
				goto gotit;
		printf("use one of:");
		for (gc = genericconf; gc->gc_driver; gc++)
			printf(" %s?", gc->gc_name);
		printf("\n");
		goto retry;
gotit:
		cp = &name[1];
		if (*++cp < '0' || *cp > '9') {
			printf("bad/missing unit number\n");
			goto retry;
		}
		while (*cp >= '0' && *cp <= '9')
			unit = 10 * unit + *cp++ - '0';
		if (*cp == '*')
			swaponroot++;

#ifdef NFSCLIENT
		if (gc->gc_root == NODEV) {
			/*
			 * Tell nfs_mountroot if it's a network interface.
			 */
			bzero(nfsbootdevname_buf, sizeof(nfsbootdevname_buf));
			sprintf(nfsbootdevname_buf, "%s%d", gc->gc_name, unit);
			nfsbootdevname = nfsbootdevname_buf;
		}
#endif /* NFSCLIENT */
		goto found;
	}
	for (gc = genericconf; gc->gc_driver; gc++) {
		if (bootdev == 0 && gc->gc_root != NODEV)
			continue;
		for (hd = hp_dinit; hd->hp_driver; hd++) {
			if (hd->hp_alive == 0)
				continue;
			if (hd->hp_unit == 0 && hd->hp_driver ==
			    (struct driver *)gc->gc_driver) {
				printf("root on %s0\n", hd->hp_driver->d_name);
				goto found;
			}
		}
	}
	printf("No suitable root, halting.\n");
	asm("stop #0x2700");
found:
	if (gc->gc_root != NODEV)
		gc->gc_root = makedev(major(gc->gc_root), unit*8);
	mountroot = gc->gc_mountroot;	/* XXX: should ask for fs type. */
	rootdev = gc->gc_root;
doswap:
	if (rootdev == NODEV)
		swdevt[0].sw_dev = argdev = dumpdev = NODEV;
	else {
		/*
		 * Primary swap is always in the `b' partition.
		 */
		swdevt[0].sw_dev = argdev = dumpdev =
		    MAKEDISKDEV(major(rootdev), DISKUNIT(rootdev), 1);
		/* swap size and dumplo set during autoconfigure */
		if (swaponroot)
			rootdev = dumpdev;
	}
}

gets(cp)
	char *cp;
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		cnputc(c = cngetc());
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

static int
no_mountroot()
{

	printf("root/swap configuration error, halting.\n");
	asm("stop #0x2700");
	/* NOTREACHED */
}
