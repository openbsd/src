/*	$NetBSD: autoconf.c,v 1.16 1995/12/28 19:16:55 thorpej Exp $	*/

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
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */

extern int cold;	/* cold start flag initialized in locore.s */

/*
 * Determine i/o configuration for a machine.
 */
configure()
{
	extern int safepri;
	int i;
	static char *ipl_names[] = IPL_NAMES;

	/* Start the clocks. */
	startrtclock();

	/* Find out what the hardware configuration looks like! */
	if (config_rootfound("membus", "membus") == 0)
		panic ("No mem bus found!");

	for (i = 0; i < NIPL; i++)
		printf("%s%s=%x", i?", ":"", ipl_names[i], imask[i]);
	printf("\n");

	safepri = imask[IPL_ZERO];
	spl0();

#if GENERIC
	if ((boothowto & RB_ASKNAME) == 0)
		setroot();
	setconf();
#else
	setroot();
#endif

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
swapconf()
{
	register struct swdevt *swp;
	register int nblks;
	extern int Maxmem;


	for (swp = swdevt; swp->sw_dev > 0; swp++)
	{
		unsigned d = major(swp->sw_dev);

		if (d >= nblkdev) break;
		if (bdevsw[d].d_psize) {
			nblks = (*bdevsw[d].d_psize)(swp->sw_dev);
			if (nblks > 0 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			else
				swp->sw_nblks = 0;
		}
		swp->sw_nblks = ctod(dtoc(swp->sw_nblks));

	}
}

#define	DOSWAP			/* change swdevt and dumpdev */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	's','d',	/* 0 = sd */
	's','w',	/* 1 = sw */
	's','t',	/* 2 = st */
	'r','d',	/* 3 = rd */
	'c','d',	/* 4 = cd */
};

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
setroot()
{
	int  majdev, mindev, unit, part, adaptor;
	dev_t temp, orootdev;
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
	printf("changing root device to %c%c%d%c\n",
		devname[majdev][0], devname[majdev][1],
		unit, part + 'a');

#ifdef DOSWAP
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    (mindev / MAXPARTITIONS)
		    == (minor(swp->sw_dev) / MAXPARTITIONS)) {
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

/* mem bus stuff */

static int membusprobe();
static void membusattach();

struct cfdriver membuscd =
      {	NULL, "membus", membusprobe, membusattach,
	DV_DULL, sizeof(struct device), NULL, 0 };

static int
membusprobe(parent, cf, aux)
	struct device	*parent;
	struct cfdata	*cf;
	void		*aux;
{
	return (strcmp(cf->cf_driver->cd_name, "membus") == 0);
}

static void
membusattach(parent, self, args)
	struct device *parent, *self;
 	void *args;
{
	printf ("\n");
	while (config_found(self, NULL, NULL))
		;
}
