/*	$NetBSD: autoconf.c,v 1.18 1996/10/13 03:39:44 christos Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: autoconf.c 1.31 91/01/21
 *
 *	@(#)autoconf.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <pmax/dev/device.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/turbochannel.h>

void setroot __P((void));
void swapconf __P((void));
void dumpconf __P((void)); 	/* XXX */

void xconsinit __P((void));	/* XXX console-init continuation */

#if 0
/*
 * XXX system-dependent, should call through a pointer.
 * (spl0 should _NOT_ enable TC interrupts on a 3MIN.)
 *
 */
int spl0 __P((void));
#endif

void	configure __P((void));
void	makebootdev __P((char *cp));



/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold = 1;	/* if 1, still working on cold-start */
int	cpuspeed = 30;	/* approx # instr per usec. */
extern	int pmax_boardtype;
extern	tc_option_t tc_slot_info[TC_MAX_LOGICAL_SLOTS];


extern int cputype;	/* glue for new-style config */
int cputype;

extern int initcpu __P((void));		/*XXX*/
void configure_scsi __P((void));

/*
 * Determine mass storage and memory configuration for a machine.
 * Print cpu type, and then iterate over an array of devices
 * found on the baseboard or in turbochannel option slots.
 * Once devices are configured, enable interrupts, and probe
 * for attached scsi devices.
 */
void
configure()
{
	int s;

	/*
	 * Set CPU type for new-style config. 
	 * Should support Decstations with CPUs on daughterboards,
	 * where system-type (board type) and CPU type aren't
	 * necessarily the same.
	 * (On hold until someone donates an r4400 daughterboard).
	 */
	cputype = pmax_boardtype;		/*XXX*/


	/*
	 * Kick off autoconfiguration
	 */
	s = splhigh();
	if (config_rootfound("mainbus", "mainbus") == NULL)
	    panic("no mainbus found");

#if 0
	printf("looking for non-PROM console driver\n");
#endif

	xconsinit();	/* do kludged-up console init */

#ifdef DEBUG
	if (cputype == DS_3MIN)
/*FIXME*/	printf("switched to non-PROM console\n");
#endif

	initcpu();

#ifdef DEBUG
	printf("autconfiguration done, spl back to 0x%x\n", s);
#endif
	/*
	 * Configuration is finished,  turn on interrupts.
	 * This is just spl0(), except on the 3MIN, where TURBOChannel
	 * option cards interrupt at IPLs 0-2, and some dumb drivers like
	 * the cfb want to just disable interrupts.
	 */
	if (cputype != DS_3MIN)
		spl0();

	/*
	 * Probe SCSI bus using old-style pmax configuration table.
	 * We do not yet have machine-independent SCSI support or polled
	 * SCSI.
	 */
	printf("Beginning old-style SCSI device autoconfiguration\n");
	configure_scsi();

#ifdef GENERIC
	if ((boothowto & RB_ASKNAME) == 0)
		setroot();
	setconf();
#else
	setroot();
#endif
	swapconf();
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
	dumpconf();
}

#define	DOSWAP			/* Change swdevt and dumpdev too */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	{  0,  0  },	/*  0 = 4.4bsd rz */
	{  0,  0  },	/*  1 = vax ht */
	{  0,  0  },	/*  2 = ?? */
	{ 'r','k' },	/*  3 = rk */
	{  0,  0  },	/*  4 = sw */
	{ 't','m' },	/*  5 = tm */
	{ 't','s' },	/*  6 = ts */
	{ 'm','t' },	/*  7 = mt */
	{ 'r','t' },	/*  8 = rt*/
	{  0,  0  },	/*  9 = ?? */
	{ 'u','t' },	/* 10 = ut */
	{ 'i','d' },	/* 11 = 11/725 idc */
	{ 'r','x' },	/* 12 = rx */
	{ 'u','u' },	/* 13 = uu */
	{ 'r','l' },	/* 14 = rl */
	{ 't','u' },	/* 15 = tmscp */
	{ 'c','s' },	/* 16 = cs */
	{ 'm','d' },	/* 17 = md */
	{ 's','t' },	/* 18 = st */
	{ 's','d' },	/* 19 = sd */
	{ 't','z' },	/* 20 = tz */
	{ 'r','z' },	/* 21 = rz */
	{  0,  0  },	/* 22 = ?? */
	{ 'r','a' },	/* 23 = ra */
};

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
setroot()
{
	int  majdev, mindev, unit, part, controller;
	dev_t  orootdev;
	struct swdevt *swp;
	register struct pmax_scsi_device *dp;

#ifdef DOSWAP
	dev_t temp;
#endif

	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != B_DEVMAGIC)
		return;
	majdev = B_TYPE(bootdev);
	if (majdev >= sizeof(devname) / sizeof(devname[0]))
		return;
	controller = B_CONTROLLER(bootdev);
	part = B_PARTITION(bootdev);
	unit = B_UNIT(bootdev);

	for (dp = scsi_dinit; ; dp++) {
		if (dp->sd_driver == 0)
			return;
		if (dp->sd_alive && dp->sd_drive == unit &&
		    dp->sd_ctlr == controller &&
		    dp->sd_driver->d_name[0] == devname[majdev][0] &&
		    dp->sd_driver->d_name[1] == devname[majdev][1]) {
			mindev = dp->sd_unit;
		    	break;
		}
	}
	/*
	 * Form a new rootdev
	 */
	orootdev = rootdev;
	rootdev = MAKEDISKDEV(majdev, mindev, part);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;

	printf("Changing root device to %c%c%d%c\n",
		devname[majdev][0], devname[majdev][1],
		mindev, part + 'a');

#ifdef DOSWAP
	part = mindev % MAXPARTITIONS;
	temp = 0;
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    part == DISKPART(swp->sw_dev)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == NODEV)
		return;

	/*
	 * If dumpdev was the same as the old primary swap
	 * device, move it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
#endif
}

/*
 * Look at the string 'cp' and decode the boot device.
 * Boot names can be something like 'rz(0,0,0)vmunix' or '5/rz0/vmunix'.
 */
void
makebootdev(cp)
	register char *cp;
{
	int majdev, unit, part, ctrl;

	if (*cp >= '0' && *cp <= '9') {
		/* XXX should be able to specify controller */
		if (cp[1] != '/' || cp[4] < '0' || cp[4] > '9')
			goto defdev;
		unit = cp[4] - '0';
		if (cp[5] >= 'a' && cp[5] <= 'h')
			part = cp[5] - 'a';
		else
			part = 0;
		cp += 2;
		for (majdev = 0; majdev < sizeof(devname)/sizeof(devname[0]);
		    majdev++) {
			if (cp[0] == devname[majdev][0] &&
			    cp[1] == devname[majdev][1]) {
				bootdev = MAKEBOOTDEV(majdev, 0, 0, unit, part);
				return;
			}
		}
		goto defdev;
	}
	for (majdev = 0; majdev < sizeof(devname)/sizeof(devname[0]); majdev++)
		if (cp[0] == devname[majdev][0] &&
		    cp[1] == devname[majdev][1] &&
		    cp[2] == '(')
			goto fndmaj;
defdev:
	bootdev = B_DEVMAGIC;
	return;

fndmaj:
	for (ctrl = 0, cp += 3; *cp >= '0' && *cp <= '9'; )
		ctrl = ctrl * 10 + *cp++ - '0';
	if (*cp == ',')
		cp++;
	for (unit = 0; *cp >= '0' && *cp <= '9'; )
		unit = unit * 10 + *cp++ - '0';
	if (*cp == ',')
		cp++;
	for (part = 0; *cp >= '0' && *cp <= '9'; )
		part = part * 10 + *cp++ - '0';
	if (*cp != ')')
		goto defdev;
	bootdev = MAKEBOOTDEV(majdev, 0, ctrl, unit, part);
}
