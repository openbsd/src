/*	$NetBSD: swapgeneric.c,v 1.1 1995/08/25 07:49:14 phil Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)swapgeneric.c	5.5 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include "xd.h"

static void gets __P((char *cp));
int cngetc __P((void));

/*
 * Generic configuration;  all in one
 */
dev_t	rootdev = NODEV;
dev_t	argdev = NODEV;
dev_t	dumpdev = NODEV;
int	nswap;
struct	swdevt swdevt[] = {
	{ NODEV,	1,	0 },
	{ NODEV,	0,	0 },
};

#if NSD > 0
extern	struct cfdriver sd_cd;
#endif
#if NXD > 0
extern	struct cfdriver xd_cd;
#endif

struct	genericconf {
	struct cfdriver *gc_driver;
	char *gc_name;
	dev_t gc_major;
} genericconf[] = {
#if NSD > 0
	{ &sd_cd,  "sd",  7 },
#endif
#if NXD > 0
	{ &xd_cd,  "xd",  10 },
#endif
	{ 0 }
};

#ifdef FFS
extern int ffs_mountroot __P((void));
#endif
#ifdef NFSCLIENT
extern int nfs_mountroot __P((void));
#endif
int (*mountroot) __P((void)) = ffs_mountroot;

void setconf __P((void));

void
setconf ()
{
	register struct genericconf *gc;
	int unit;

	if (rootdev != NODEV)
		goto doswap;

	/* There must be at least one entry in genericconf.  */
	if (genericconf[0].gc_driver == 0)
		goto verybad;

	if (1 || boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("root device? ");
		gets(name);
#ifdef NFSCLIENT
		/* A very special root file...  */
		if (name[0] == 'n' && name[1] == 'f' && name[2] == 's')
		  {
		    mountroot = nfs_mountroot;
		    return;
		  }
#endif
		    
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_name[0] == name[0] &&
			    gc->gc_name[1] == name[1])
				goto gotit;
		goto bad;
gotit:
		if (name[2] >= '0' && name[2] <= '7' && name[3] == 0) {
			unit = name[2] - '0';
			goto found;
		}
		printf("bad/missing unit number\n");
bad:
		printf("use:\n");	
		for (gc = genericconf; gc->gc_driver; gc++)
			printf("\t%s%%d\n", gc->gc_name);
		goto retry;
	}
	unit = 0;
	for (gc = genericconf; gc->gc_driver; gc++) {
		if (gc->gc_driver->cd_ndevs > unit &&
		    gc->gc_driver->cd_devs[unit]) {
			printf("root on %s0\n", gc->gc_name);
			goto found;
		}
	}
verybad:
	printf("no suitable root \n");
	for (;;) ;

found:
	rootdev = makedev(gc->gc_major, unit * MAXPARTITIONS);
doswap:
	swdevt[0].sw_dev = argdev = dumpdev =
	    makedev(major(rootdev), minor(rootdev) + 1);
}

static void
gets(cp)
	char *cp;
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc()&0177);
		switch (c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			printf("\r\n");
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
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
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}

