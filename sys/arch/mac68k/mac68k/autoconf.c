/*	$NetBSD: autoconf.c,v 1.20 1995/11/01 04:59:31 briggs Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * from: Utah $Hdr: autoconf.c 1.31 91/01/21$
 *
 *	@(#)autoconf.c	7.5 (Berkeley) 5/7/91
 */

/*
   ALICE 
      05/23/92 BG
      I've started to re-write this procedure to use our devices and strip 
      out all the useless HP stuff, but I only got to line 120 or so 
      before I had a really bad attack of kompernelphobia and blacked out.
*/

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>

#include <sys/disklabel.h>
#include <sys/disk.h>

#include <machine/vmparam.h>
#include <machine/param.h>
#include <machine/cpu.h>
#include <machine/pte.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;		    /* if 1 (locore.s), still working on cold-start */

#ifdef DEBUG
int	acdebug = 0;
#endif

/*
 * Determine mass storage and memory configuration for a machine.
 */
configure()
{
	int found;

	VIA_initialize();

	mrg_init();		/* Init Mac ROM Glue */

	adb_init();		/* ADB device subsystem & driver */

	startrtclock();

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("No main device!");

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
	register int nblks, tblks;

	for (swp = swdevt; swp->sw_dev != NODEV ; swp++) {
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

u_long	bootdev;		/* should be dev_t, but not until 32 bits */
struct	device *bootdv = NULL;

#define	PARTITIONMASK	0x7
#define	UNITSHIFT	3

/*
 * Map a SCSI bus, target, lun to a device number.
 * This could be tape, disk, CD.  The calling routine, though,
 * assumes DISK.  It would be nice to allow CD, too...
 */
static int
target_to_unit(u_long bus, u_long target, u_long lun)
{
	int			targ;
	struct scsibus_softc	*scsi;
	struct scsi_link	*sc_link;
	struct device		*sc_dev;
extern	struct cfdriver		scsibuscd;

	if (target < 0 || target > 7 || lun < 0 || lun > 7) {
		printf("scsi target to unit, target (%d) or lun (%d)"
			" out of range.\n", target, lun);
		return -1;
	}

	if (bus == -1) {
		for (bus = 0 ; bus < scsibuscd.cd_ndevs ; bus++) {
			if (scsibuscd.cd_devs[bus]) {
				scsi = (struct scsibus_softc *)
						scsibuscd.cd_devs[bus];
				if (scsi->sc_link[target][lun]) {
					sc_link = scsi->sc_link[target][lun];
					sc_dev = (struct device *)
							sc_link->device_softc;
					return sc_dev->dv_unit;
				}
			}
		}
		return -1;
	}
	if (bus < 0 || bus >= scsibuscd.cd_ndevs) {
		printf("scsi target to unit, bus (%d) out of range.\n", bus);
		return -1;
	}
	if (scsibuscd.cd_devs[bus]) {
		scsi = (struct scsibus_softc *) scsibuscd.cd_devs[bus];
		if (scsi->sc_link[target][lun]) {
			sc_link = scsi->sc_link[target][lun];
			sc_dev = (struct device *) sc_link->device_softc;
			return sc_dev->dv_unit;
		}
	}
	return -1;
}

/* swiped from sparc/sparc/autoconf.c */
static int
findblkmajor(register struct dkdevice *dv)
{
	register int	i;

	for (i=0 ; i<nblkdev ; i++) {
		if ((void (*)(struct buf *))bdevsw[i].d_strategy ==
		    dv->dk_driver->d_strategy)
			return i;
	}
	return -1;
}

/*
 * Yanked from i386/i386/autoconf.c
 */
void
findbootdev()
{
	register struct device *dv;
	register int unit;
	int major;

	major = B_TYPE(bootdev);
	if (major < 0 || major >= nblkdev)
		return;

	unit = B_UNIT(bootdev);

	bootdev &= ~(B_UNITMASK << B_UNITSHIFT);
	unit = target_to_unit(-1, unit, 0);
	bootdev |= (unit << B_UNITSHIFT);

	for (dv = alldevs ; dv ; dv = dv->dv_next) {
		if (   (dv->dv_class == DV_DISK)
		    && (unit == dv->dv_unit)
		    && (major == findblkmajor((struct dkdevice *) (dv+1)))) {
			bootdv = dv;
			return;
		}
	}
}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
setroot()
{
	register struct swdevt	*swp;
	register int		majdev, mindev, part;
	dev_t			nrootdev, nswapdev, temp;

	if (boothowto & RB_DFLTROOT)
		return;
	findbootdev();
	if (bootdv == NULL) {
		printf("ARGH!!  No boot device????");
		delay(10000000);
/*		panic("ARGH!!  No boot device????"); */
	}
	switch (bootdv->dv_class) {
		case DV_DISK:
			nrootdev = makedev(B_TYPE(bootdev),
					   B_UNIT(bootdev) << UNITSHIFT
					   + B_PARTITION(bootdev));
			break;
		default:
			printf("Only supports DISK device for booting.\n");
			break;
	}

	if (rootdev == nrootdev)
		return;

	majdev = major(nrootdev);
	mindev = minor(nrootdev);
	part = mindev & PARTITIONMASK;
	mindev -= part;

	rootdev = nrootdev;
	printf("Changing root device to %s%c.\n", bootdv->dv_xname, part+'a');

	temp = NODEV;
	for (swp = swdevt ; swp->sw_dev != NODEV ; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev == (minor(swp->sw_dev) & ~PARTITIONMASK)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == NODEV)
		return;

	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
}

struct newconf_S {
	char	*name;
	int	req;
};

static int
mbprint(aux, name)
	void	*aux;
	char	*name;
{
	struct newconf_S	*c = (struct newconf_S *) aux;

	if (name)
		printf("%s at %s", c->name, name);
	return(UNCONF);
}

static int
root_matchbyname(parent, cf, aux)
	struct device	*parent;
	struct cfdata	*cf;
	void		*aux;
{
	return (strcmp(cf->cf_driver->cd_name, (char *)aux) == 0);
}

extern int
matchbyname(parent, match, aux)
	struct device	*parent;
	void		*match;
	void		*aux;
{
	struct newconf_S	*c = (struct newconf_S *) aux;
	struct device		*dv = (struct device *) match;

/*	printf("matchbyname: (%s) and (%s).\n", dv->dv_xname, c->name);
*/

	return (strcmp(dv->dv_xname, c->name) == 0);
}

static void
mainbus_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct newconf_S	conf_data[] = {
					{"ite0",       1},
					{"adb0",       1},
					{"ser0",       0},
					{"nubus0",     1},
					{"ncrscsi0",   0},
					{"ncr96scsi0", 0},
					{"asc0",       0},
					{"fpu0",       0},
					{"floppy0",    0},
					{NULL,         0}
			 	};
	struct newconf_S	*c;
	int			fail=0, warn=0;

	printf("\n");
	for (c=conf_data ; c->name ; c++) {
		if (config_found(self, c, mbprint)) {
		} else {
			if (c->req) {
				fail++;
			}
			warn++;
		}
	}

	if (fail) {
		printf("Failed to find %d required devices.\n", fail);
		panic("Can't continue.");
	}
}

struct cfdriver mainbuscd =
      { NULL, "mainbus", root_matchbyname, mainbus_attach,
	DV_DULL, sizeof(struct device), 1, 0 };
