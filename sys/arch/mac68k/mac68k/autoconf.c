/*	$OpenBSD: autoconf.c,v 1.33 2010/07/01 03:20:37 matthew Exp $	*/
/*	$NetBSD: autoconf.c,v 1.38 1996/12/18 05:46:09 scottr Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disk.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/viareg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

int target_to_unit(u_long, u_long, u_long);
void	findbootdev(void);

struct device	*booted_device;
int		booted_partition;
dev_t		bootdev;

/*
 * Yanked from i386/i386/autoconf.c (and tweaked a bit)
 */
void
findbootdev()
{
	struct device *dv;
	int major, unit;

	booted_device = NULL;
	booted_partition = 0;	/* Assume root is on partition a */

	major = B_TYPE(bootdev);
	if (major < 0 || major >= nblkdev)
		return;

	unit = B_UNIT(bootdev);

	bootdev &= ~(B_UNITMASK << B_UNITSHIFT);
	unit = target_to_unit(-1, unit, 0);
	bootdev |= (unit << B_UNITSHIFT);

	if (disk_count <= 0)
		return;

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class == DV_DISK && major == findblkmajor(dv) &&
		    unit == dv->dv_unit) {
			booted_device = dv;
			return;
		}
	}
}

void
cpu_configure()
{
	/* this couldn't be done in intr_init() because this uses malloc() */
	softintr_init();

	startrtclock();

	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("No mainbus found!");
	spl0();

	findbootdev();
	cold = 0;
}

void
device_register(struct device *dev, void *aux)
{
}

void
diskconf(void)
{
	setroot(booted_device, booted_partition, RB_USERREQ);
	dumpconf();
}

/*
 * Map a SCSI bus, target, lun to a device number.
 * This could be tape, disk, CD.  The calling routine, though,
 * assumes DISK.  It would be nice to allow CD, too...
 */
int
target_to_unit(bus, target, lun)
	u_long bus, target, lun;
{
	struct scsibus_softc	*scsi;
	struct scsi_link	*sc_link;
	struct device		*sc_dev;
	extern	struct cfdriver		scsibus_cd;

	if (target < 0 || target > 7 || lun < 0 || lun > 7) {
		printf("scsi target to unit, target (%ld) or lun (%ld)"
			" out of range.\n", target, lun);
		return -1;
	}

	if (bus == -1) {
		for (bus = 0 ; bus < scsibus_cd.cd_ndevs ; bus++) {
			if (scsibus_cd.cd_devs[bus]) {
				scsi = (struct scsibus_softc *)
						scsibus_cd.cd_devs[bus];
				sc_link = scsi_get_link(scsi, target, lun);
				if (sc_link != NULL) {
					sc_dev = (struct device *)
							sc_link->device_softc;
					return sc_dev->dv_unit;
				}
			}
		}
		return -1;
	}
	if (bus < 0 || bus >= scsibus_cd.cd_ndevs) {
		printf("scsi target to unit, bus (%ld) out of range.\n", bus);
		return -1;
	}
	if (scsibus_cd.cd_devs[bus]) {
		scsi = (struct scsibus_softc *) scsibus_cd.cd_devs[bus];
		sc_link = scsi_get_link(scsi, target, lun);
		if (sc_link != NULL) {
			sc_dev = (struct device *) sc_link->device_softc;
			return sc_dev->dv_unit;
		}
	}
	return -1;
}

struct nam2blk nam2blk[] = {
	{ "sd",         4 },
	{ "cd",         6 },
	{ "rd",		13 },
	{ "vnd",	8 },
	{ NULL,		-1 }
};
