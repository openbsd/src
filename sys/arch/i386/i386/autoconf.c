/*	$OpenBSD: autoconf.c,v 1.12 1996/09/01 20:55:20 downsj Exp $	*/
/*	$NetBSD: autoconf.c,v 1.20 1996/05/03 19:41:56 christos Exp $	*/

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
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba 
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/cpu.h>

void swapconf __P((void));
void setroot __P((void));
void setconf __P((void));

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern int	cold;		/* cold start flag initialized in locore.s */

/*
 * Determine i/o configuration for a machine.
 */
void
configure()
{

	startrtclock();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	printf("biomask %x netmask %x ttymask %x\n",
	    (u_short)imask[IPL_BIO], (u_short)imask[IPL_NET],
	    (u_short)imask[IPL_TTY]);

	spl0();

	setconf();

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	swapconf();
	dumpconf();
	cold = 0;
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		int maj = major(swp->sw_dev);

		if (maj > nblkdev)
			break;
		if (bdevsw[maj].d_psize) {
			nblks = (*bdevsw[maj].d_psize)(swp->sw_dev);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}

#define	DOSWAP			/* change swdevt and dumpdev */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	{ 'w','d' },	/* 0 = wd */
	{ 's','w' },	/* 1 = sw */
	{ 'f','d' },	/* 2 = fd */
	{ 'w','t' },	/* 3 = wt */
	{ 's','d' },	/* 4 = sd -- new SCSI system */
};

dev_t	argdev = NODEV;
int	nswap;
long	dumplo;
int	dmmin, dmmax, dmtext;

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
setroot()
{
	int  majdev, mindev, unit, part, adaptor;
	dev_t orootdev;
#ifdef DOSWAP
	dev_t temp = 0;
#endif
	struct swdevt *swp;

	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	if (majdev > sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor = (bootdev >> B_ADAPTORSHIFT) & B_ADAPTORMASK;
	part = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	unit = (bootdev >> B_UNITSHIFT) & B_UNITMASK;
	mindev = (unit * MAXPARTITIONS) + part;
	orootdev = rootdev;
	rootdev = makedev(majdev, mindev);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;
	printf("root on %c%c%d%c\n",
	    devname[majdev][0], devname[majdev][1],
	    unit, part + 'a');

#ifdef DOSWAP
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    (mindev / MAXPARTITIONS) ==
		    (minor(swp->sw_dev) / MAXPARTITIONS)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == NODEV)
		return;

	/*
	 * If dumpdev was the same as the old primary swap device, move
	 * it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
#endif
}

#include "wdc.h"
#if NWDC > 0
extern	struct cfdriver wd_cd;
#endif
#include "fd.h"
#if NFD > 0
extern	struct cfdriver fd_cd;
#endif
#include "sd.h"
#if NSD > 0
extern	struct cfdriver sd_cd;
#endif
#include "cd.h"
#if NCD > 0
extern	struct cfdriver cd_cd;
#endif
#include "mcd.h"
#if NMCD > 0
extern	struct cfdriver mcd_cd;
#endif

struct	genericconf {
	struct cfdriver *gc_driver;
	char *gc_name;
	dev_t gc_major;
} genericconf[] = {
#if NWDC > 0
	{ &wd_cd,  "wd",  0 },
#endif
#if NSD > 0
	{ &sd_cd,  "sd",  4 },
#endif
#if NCD > 0
	{ &cd_cd,  "cd",  6 },
#endif
#if NMCD > 0
	{ &mcd_cd, "mcd", 7 },
#endif
#if NFDC > 0
	{ &fd_cd,  "fd",  2 },
#endif
	{ 0 }
};

void
setconf()
{
	extern int ffs_mountroot __P((void *));
	extern int (*mountroot) __P((void *));
	register struct genericconf *gc;
	int unit;
#if 0
	int swaponroot = 0;
#endif
	char *num;

	if (boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("root device? ");
		getsn(name, sizeof name);
		if (*name == '\0')
			goto noask;
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_driver->cd_ndevs &&
			    strncmp(gc->gc_name, name,
			    strlen(gc->gc_name)) == 0)
				break;
		if (gc->gc_driver) {
			num = &name[strlen(gc->gc_name)];
#if 0
			if (num[0] == '*') {
				strcpy(num, num+1);
				swaponroot++;
			}
#endif

			unit = 0;
			do {
				unit = (unit * 10) + *num - '0';
				if (*num < '0' || *num > '9')
					unit = -1;
			} while (unit != -1 && *++num);

			if (unit < 0) {
				printf("%s: not a unit number\n",
				    &name[strlen(gc->gc_name)]);
			} else if (unit > gc->gc_driver->cd_ndevs ||
			    gc->gc_driver->cd_devs[unit] == NULL) {
				printf("%d: no such unit\n", unit);
			} else {
				printf("root on %s%da\n", gc->gc_name, unit);
				rootdev = makedev(gc->gc_major,
				    unit * MAXPARTITIONS);
				goto doswap;
			}
		}
		printf("use one of: ");
		for (gc = genericconf; gc->gc_driver; gc++) {
			for (unit=0; unit < gc->gc_driver->cd_ndevs; unit++) {
				if (gc->gc_driver->cd_devs[unit])
					printf("%s%d ", gc->gc_name, unit);
			}
		}
		printf("\n");
		goto retry;
	}
noask:
	if (mountroot == NULL) {
		/* `swap generic' */
		setroot();
	} else {
		/* preconfigured */
		return;
	}

doswap:
	mountroot = ffs_mountroot;
	swdevt[0].sw_dev = argdev = dumpdev =
	    makedev(major(rootdev), minor(rootdev) + 1);
	/* swap size and dumplo set during autoconfigure */
#if 0
	if (swaponroot)
		rootdev = dumpdev;
#endif
}
