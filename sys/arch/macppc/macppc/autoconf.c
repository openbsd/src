/*	$OpenBSD: autoconf.c,v 1.34 2007/09/09 15:24:53 deraadt Exp $	*/
/*
 * Copyright (c) 1996, 1997 Per Fogelstrom
 * Copyright (c) 1995 Theo de Raadt
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
 * from: Utah Hdr: autoconf.c 1.31 91/01/21
 *
 *	from: @(#)autoconf.c	8.1 (Berkeley) 6/10/93
 *      $Id: autoconf.c,v 1.34 2007/09/09 15:24:53 deraadt Exp $
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
#include <dev/cons.h>
#include <uvm/uvm_extern.h>
#include <machine/autoconf.h>
#include <machine/powerpc.h>

#include <sys/disk.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <scsi/sdvar.h>

void	dumpconf(void);
struct	device *getdevunit(char *, int);
static	struct devmap *findtype(char **);
void	makebootdev(char *cp);
int	getpno(char **);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold = 1;	/* if 1, still working on cold-start */
char	bootdev[16];	/* to hold boot dev name */
struct device *bootdv = NULL;

struct dumpmem dumpmem[VM_PHYSSEG_MAX];
u_int ndumpmem;

/*
 *  Configure all devices found that we know about.
 *  This is done at boot time.
 */
void
cpu_configure()
{
	(void)splhigh();	/* To be really sure.. */
	calc_delayconst();

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");
	(void)spl0();
	cold = 0;
}

struct devmap {
	char *att;
	char *dev;
	int   type;
};
#define	T_IFACE	0x10

#define	T_BUS	0x00
#define	T_SCSI	0x11
#define	T_IDE	0x12
#define	T_DISK	0x21

static struct devmap *
findtype(char **s)
{
	static struct devmap devmap[] = {
		{ "/ht",		NULL, T_BUS },
		{ "/ht@",		NULL, T_BUS },
		{ "/pci@",		NULL, T_BUS },
		{ "/pci",		NULL, T_BUS },
		{ "/AppleKiwi@",	NULL, T_BUS },
		{ "/AppleKiwi",		NULL, T_BUS },
		{ "/mac-io@",		NULL, T_BUS },
		{ "/mac-io",		NULL, T_BUS },
		{ "/@",			NULL, T_BUS },
		{ "/LSILogic,sas@",	"sd", T_SCSI },
		{ "/scsi@",		"sd", T_SCSI },
		{ "/ide",		"wd", T_IDE },
		{ "/ata",		"wd", T_IDE },
		{ "/k2-sata-root",	NULL, T_BUS },
		{ "/k2-sata",		"wd", T_IDE },
		{ "/disk@",		"sd", T_DISK },
		{ "/disk",		"wd", T_DISK },
		{ "/usb@",		"sd", T_SCSI },
		{ "/ADPT,2940U2B@",	"sd", T_SCSI },
		{ "/bcom5704@4",	"bge0", T_IFACE },
		{ "/bcom5704@4,1",	"bge1", T_IFACE },
		{ "/ethernet",		"gem0", T_IFACE },
		{ "/enet",		"mc0", T_IFACE },
		{ NULL, NULL }
	};
	struct devmap *dp = &devmap[0];

	while (dp->att) {
		if (strncmp(*s, dp->att, strlen(dp->att)) == 0) {
			*s += strlen(dp->att);
			break;
		}
		dp++;
	}
	if (dp->att == NULL)
		printf("string [%s] not found\n", *s);

	return(dp);
}

/*
 * Look at the string 'bp' and decode the boot device.
 * Boot names look like: '/pci/scsi@c/disk@0,0/bsd'
 *                       '/pci/mac-io/ide@20000/disk@0,0/bsd
 *                       '/pci/mac-io/ide/disk/bsd
 *			 '/ht@0,f2000000/pci@2/bcom5704@4/bsd'
 */
void
makebootdev(char *bp)
{
	int	unit, ptype;
	char   *dev, *cp;
	struct devmap *dp;

	cp = bp;
	do {
		while(*cp && *cp != '/')
			cp++;

		dp = findtype(&cp);
		if (!dp->att) {
			printf("Warning: bootpath unrecognized: %s\n", bp);
			return;
		}
	} while((dp->type & T_IFACE) == 0);

	if (dp->att && dp->type == T_IFACE) {
		snprintf(bootdev, sizeof bootdev, "%s", dp->dev);
		return;
	}
	dev = dp->dev;
	while(*cp && *cp != '/')
		cp++;
	ptype = dp->type;
	dp = findtype(&cp);
	if (dp->att && dp->type == T_DISK) {
		unit = getpno(&cp);
		if (ptype == T_SCSI) {
			struct device *dv;
			struct sd_softc *sd;

			TAILQ_FOREACH(dv, &alldevs, dv_list) {
				if (dv->dv_class != DV_DISK ||
				    strcmp(dv->dv_cfdata->cf_driver->cd_name, "sd"))
					continue;
				sd = (struct sd_softc *)dv;
				if (sd->sc_link->target != unit)
					continue;
				snprintf(bootdev, sizeof bootdev,
				    "%s%c", dv->dv_xname, 'a');
				return;
			}
		}
		snprintf(bootdev, sizeof bootdev, "%s%d%c", dev, unit, 'a');
		return;
	}
	printf("Warning: boot device unrecognized: %s\n", bp);
}

int
getpno(char **cp)
{
	int val = 0, digit;
	char *cx = *cp;

	while (*cx) {
		if (*cx >= '0' && *cx <= '9')
			digit = *cx - '0';
		else if (*cx >= 'a' && *cx <= 'f')
			digit = *cx - 'a' + 0x0a;
		else
			break;
		val = val * 16 + digit;
		cx++;
	}
	*cp = cx;
	return (val);
}

void
device_register(struct device *dev, void *aux)
{
}

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(char *name, int unit)
{
	struct device *dev = TAILQ_FIRST(&alldevs);
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	snprintf(num, sizeof num, "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strlcpy(fullname, name, sizeof fullname);
	strlcat(fullname, num, sizeof fullname);

	while (strcmp(dev->dv_xname, fullname) != 0)
		if ((dev = TAILQ_NEXT(dev, dv_list)) == NULL)
			return NULL;

	return dev;
}

/*
 * Now that we are fully operational, we can checksum the
 * disks, and using some heuristics, hopefully are able to
 * always determine the correct root disk.
 */
void
diskconf(void)
{
	dev_t temp;
	int part = 0;

	printf("bootpath: %s\n", bootpath);
	makebootdev(bootpath);

	/* Lookup boot device from boot if not set by configuration */
	bootdv = parsedisk(bootdev, strlen(bootdev), 0, &temp);
	setroot(bootdv, part, RB_USERREQ);
	dumpconf();
}

struct nam2blk nam2blk[] = {
	{ "wd",		0 },
	{ "sd",		2 },
	{ "raid",	19 },
	{ NULL,		-1 }
};
