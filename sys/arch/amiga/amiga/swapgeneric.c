/*	$NetBSD: swapgeneric.c,v 1.20 1996/01/07 22:01:46 thorpej Exp $	*/

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
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>		/* XXXX and all that uses it */
#include <sys/proc.h>		/* XXXX and all that uses it */
#include <sys/disk.h>

#include "fd.h"
#include "sd.h"
#include "cd.h"

#if NCD > 0
int cd9660_mountroot();
#endif
int ffs_mountroot();
int (*mountroot)() = ffs_mountroot;

/*
 * Generic configuration;  all in one
 */
dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

struct	swdevt swdevt[] = {
	{ NODEV,	1,	0 },
	{ NODEV,	0,	0 },
};

#if NFD > 0
extern	struct cfdriver fdcd;
#endif
#if NSD > 0
extern	struct cfdriver sdcd;
#endif
#if NCD > 0
extern	struct cfdriver cdcd;
#endif

struct genericconf {
	struct cfdriver *gc_driver;
	dev_t gc_root;
};

/*
 * the system will assign rootdev to the first partition 'a' 
 * found with FS_BSDFFS fstype. so these should be ordered
 * in prefernece of boot. however it does walk units backwards
 * to remain compatible with the old amiga method of picking
 * the last root found.
 */
struct genericconf genericconf[] = {
#if NFD > 0
	{&fdcd,	makedev(2, 0)},
#endif
#if NSD > 0
	{&sdcd,	makedev(4, 0)},
#endif
#if NCD > 0
	{&cdcd,	makedev(7, 0)},
#endif
	{ 0 },
};

struct genericconf *
getgenconf(bp)
	char *bp;
{
	char *cp;
	struct genericconf *gc;

	for (;;) {
		printf("root device> ");
		gets(bp);
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_driver->cd_name[0] == bp[0] &&
			    gc->gc_driver->cd_name[1] == bp[1])
				break;
		if (gc->gc_driver == NULL) {
			printf("use one of:");
			for (gc = genericconf; gc->gc_driver; gc++)
				printf(" %s%%d", gc->gc_driver->cd_name);
			printf("\n");
			continue;
		}
		cp = bp + 2;
		if (*cp >= '0' && *cp <= '9')
			break;
		printf("bad/missing unit number\n");
	}
	return(gc);
}

setconf()
{
	struct disk *dkp;
	struct device **devpp;
	struct partition *pp;
	struct genericconf *gc;
	struct bdevsw *bdp;
	int unit, swaponroot;
	char name[128];
	char *cp;
	
	swaponroot = 0;

	if (rootdev != NODEV)
		goto justdoswap;

	unit = 0;
	if (boothowto & RB_ASKNAME) {
		gc = getgenconf(name);
		cp = name + 2;
		while (*cp >= '0' && *cp <= '9')
			unit = 10 * unit + *cp++ - '0';
		if (*cp == '*')
			swaponroot = 1;
		unit &= 0x7;
		goto found;
	}

	for (gc = genericconf; gc->gc_driver; gc++) {
		for (unit = gc->gc_driver->cd_ndevs - 1; unit >= 0; unit--) { 
			if (gc->gc_driver->cd_devs[unit] == NULL)
				continue;

			/*
			 * Find the disk corresponding to the current
			 * device.
			 */
			devpp = (struct device **)gc->gc_driver->cd_devs;
			if ((dkp = disk_find(devpp[unit]->dv_xname)) == NULL)
				continue;

			if (dkp->dk_driver == NULL ||
			    dkp->dk_driver->d_strategy == NULL)
				continue;
			for (bdp = bdevsw; bdp < (bdevsw + nblkdev); bdp++)
				if (bdp->d_strategy ==
				    dkp->dk_driver->d_strategy)
					break;
			if (bdp->d_open(MAKEDISKDEV(major(gc->gc_root),
			    unit, 0), FREAD | FNONBLOCK, 0, curproc))
				continue;
			bdp->d_close(MAKEDISKDEV(major(gc->gc_root), unit, 
			    0), FREAD | FNONBLOCK, 0, curproc);
			pp = &dkp->dk_label->d_partitions[0];
			if (pp->p_size == 0 || pp->p_fstype != FS_BSDFFS)
				continue;
			goto found;
		}
	}
	printf("no suitable root\n");
	asm("stop #0x2700");
	/*NOTREACHED*/
found:

	gc->gc_root = MAKEDISKDEV(major(gc->gc_root), unit, 0);
	rootdev = gc->gc_root;
#if NCD > 0
	if (major(rootdev) == 7)
		mountroot = cd9660_mountroot;
#endif

justdoswap:
	swdevt[0].sw_dev = dumpdev = MAKEDISKDEV(major(rootdev), 
	    DISKUNIT(rootdev), 1);
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
		cnputc(c = cngetc());
		switch (c) {
		case '\n':
		case '\r':
			*lp = 0;
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
