/*	$NetBSD: swapgeneric.c,v 1.2 1995/11/30 21:55:01 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <machine/disklabel.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>		/* XXXX and all that uses it */
#include <sys/proc.h>		/* XXXX and all that uses it */
#include <sys/disk.h>

#include "ramd.h"
#include "fd.h"
#include "sd.h"
#include "cd.h"

/*
 * Only boot on ufs. (XXX?)
 */
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
#if NRAMD > 0
extern	struct cfdriver ramdcd;
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
 * in preference of boot.
 */
static struct genericconf genericconf[] = {
#if NRAMD > 0
	{&ramdcd,makedev(1, 0)},
#endif
#if NFD > 0
	{&fdcd,	makedev(2, 0)},
#endif
#if NSD > 0
	{&sdcd,	makedev(4, 0)},
#endif
#if NCD > 0
	{&cdcd,	makedev(6, 0)},
#endif
	{ 0 },
};

static void			gets __P((char *));
static struct genericconf	*getgenconf __P((char *));
static struct genericconf	*guess_gc __P((int, int *));


static struct genericconf *
getgenconf(bp)
char	*bp;
{
	char			*cp;
	struct genericconf	*gc;

	for(;;) {
		printf("format: <dev-name><unit> [ ':'<part> ] [ * ]\n");
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
	return (gc);
}

setconf()
{
	struct genericconf	*gc;
	int			unit, swaponroot, part;
	char			name[128];
	char			*cp;
	
	swaponroot = 0;
	part       = 0;
	unit       = 0;

	if (rootdev != NODEV)
		goto justdoswap;

	if (boothowto & RB_ASKNAME) {
		gc = getgenconf(name);
		cp = name + 2;

		/*
		 * Get unit number
		 */
		while (*cp >= '0' && *cp <= '9')
			unit = 10 * unit + *cp++ - '0';

		/*
		 * And maybe partition...
		 * This is only usefull when booting from floppy. So it is
		 * possible to select the correct density.
		 */
		if (*cp == ':') {
			cp++;
			while (*cp >= '0' && *cp <= '9')
				part = 10 * part + *cp++ - '0';
		}

		if (*cp == '*')
			swaponroot = 1;
		unit &= 0x7;
	}
	else {
		gc = guess_gc(1, &unit);

		if (gc == NULL) {
			printf("no suitable root\n");
			asm("stop #0x2700");
		}
		/*NOTREACHED*/
	}

	gc->gc_root = MAKEDISKDEV(major(gc->gc_root), unit, part);
	rootdev     = gc->gc_root;


justdoswap:
	if (!swaponroot) {
		/* Find a suitable swap device */
		if ((gc = guess_gc(0, &unit)) == NULL) {
			swdevt[0].sw_dev = dumpdev =
				MAKEDISKDEV(major(rootdev),DISKUNIT(rootdev),1);
		}
		else {
			swdevt[0].sw_dev = dumpdev =
				MAKEDISKDEV(major(gc->gc_root),unit,1);
		}
	}

	/* swap size and dumplo set during autoconfigure */
	if (swaponroot)
		rootdev = dumpdev;
}

static void
gets(cp)
char	*cp;
{
	register char	*lp;
	register int	c;

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

static struct genericconf *
guess_gc(search_root, rv_unit)
int	search_root;
int	*rv_unit;
{
	struct genericconf	*gc;
	struct partition	*pp;
	struct dkdevice		*dkp;
	struct bdevsw		*bdp;
	int			unit;

	for (gc = genericconf; gc->gc_driver; gc++) {
	     for (unit = 0; unit < gc->gc_driver->cd_ndevs; unit++) {
		if (gc->gc_driver->cd_devs[unit] == NULL)
			continue;
		/*
		 * this is a hack these drivers should use
		 * dk_dev and not another instance directly above.
		 */
		dkp = (struct dkdevice *)
		   ((struct device *)gc->gc_driver->cd_devs[unit] + 1);
		if (dkp->dk_driver==NULL || dkp->dk_driver->d_strategy==NULL)
			continue;
		for (bdp = bdevsw; bdp < (bdevsw + nblkdev); bdp++)
			if (bdp->d_strategy == dkp->dk_driver->d_strategy)
				break;
		if (bdp->d_open(MAKEDISKDEV(major(gc->gc_root),
			    unit, 3), FREAD | FNONBLOCK, 0, curproc))
			continue;
		bdp->d_close(MAKEDISKDEV(major(gc->gc_root), unit, 3),
			    FREAD | FNONBLOCK, 0, curproc);
		if (search_root) {
			pp = &dkp->dk_label.d_partitions[0];
			if (pp->p_size == 0 || pp->p_fstype != FS_BSDFFS)
				continue;
		}
		else { /* must be swap */
			pp = &dkp->dk_label.d_partitions[1];
			if (pp->p_size == 0 || pp->p_fstype != FS_SWAP)
				continue;
		}
		goto found;
	    }
	}
	return (NULL);
found:
	*rv_unit = unit;
	return (gc);
}
