/*	$OpenBSD: autoconf.c,v 1.13 2007/05/04 19:30:55 deraadt Exp $	*/
/*
 * Copyright (c) 1996 Per Fogelstrom
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

#include <uvm/uvm_extern.h>

#include <dev/cons.h>
#include <machine/autoconf.h>
#include <mips64/archtype.h>

void	diskconf(void);
extern void dumpconf(void);
struct device *getdevunit(char *, int);
const struct devmap *boot_findtype(char *);
int makebootdev(const char *, int);
const char *boot_get_path_component(const char *, char *, int *);
const char *boot_getnr(const char *, int *);

/* Struct translating from ARCS to bsd. */
struct devmap {
	const char	*att;
	const char	*dev;
	int		what;
};
#define	DEVMAP_TYPE	0x01
#define	DEVMAP_UNIT	0x02
#define	DEVMAP_PART	0x04

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold = 1;			/* if 1, still working on cold-start */
char	bootdev[16] = "unknown";	/* to hold boot dev name */
struct device *bootdv = NULL;

/*
 *  Configure all devices found that we know about.
 *  This is done at boot time.
 */
void
cpu_configure(void)
{
	(void)splhigh();	/* Set mask to what we intend. */
	if (config_rootfound("mainbus", "mainbus") == 0) {
		panic("no mainbus found");
	}

	splinit();		/* Initialized, fire up interrupt system */

	md_diskconf = diskconf;
	cold = 0;
}

void
diskconf(void)
{
	dev_t tmpdev;

	/* Lookup boot device from boot if not set by configuration */
	if (bootdv == NULL)
		bootdv = parsedisk(bootdev, strlen(bootdev), 0, &tmpdev);
	if (bootdv == NULL)
		printf("boot device: lookup '%s' failed.\n", bootdev);
	else
		printf("boot device: %s\n", bootdv->dv_xname);

	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();
}

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(name, unit)
	char *name;
	int unit;
{
	struct device *dev = TAILQ_FIRST(&alldevs);
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	snprintf(num, sizeof(num), "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strlcpy(fullname, name, sizeof(fullname));
	strlcat(fullname, num, sizeof(fullname));

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = TAILQ_NEXT(dev, dv_list)) == NULL)
			return NULL;
	}
	return dev;
}

const struct devmap *
boot_findtype(char *s)
{
	const struct devmap devmap[] = {
		{ "scsi",	"sd",	DEVMAP_TYPE },
		{ "disk",	"",	DEVMAP_UNIT },
		{ "part",	"",	DEVMAP_PART },
		{ "partition",	"",	DEVMAP_PART },
		{ NULL, NULL }
	};
	const struct devmap *dp = &devmap[0];

	while (dp->att) {
		if (strcmp (s, dp->att) == 0) {
			break;
		}
		dp++;
	}
	if (dp->att)
		return dp;
	else
		return NULL;
}

/*
 * Look at the string 'bp' and decode the boot device.
 * Boot devices look like: 'scsi()disk(n)rdisk()partition(0)'
 *	 		  or
 *			   'dksc(0,1,0)'
 */
int
makebootdev(const char *bp, int offs)
{
	char namebuf[256];
	const char *cp, *ncp, *ecp, *devname;
	int	i, unit, partition;
	const struct devmap *dp;

	if (bp == NULL)
		return -1;

	ecp = cp = bp;
	unit = partition = 0;
	devname = NULL;

	if (strncmp(cp, "dksc(", 5) == 0) {
		devname = "sd";
		cp += 5;
		cp = boot_getnr(cp, &i);
		if (*cp == ',') {
			cp = boot_getnr(cp, &i);
			unit = i - 1;
			if (*cp == ',') {
				cp = boot_getnr(cp, &i);
				partition = i;
			}
		}
	} else {
		ncp = boot_get_path_component(cp, namebuf, &i);
		while (ncp != NULL) {
			if ((dp = boot_findtype(namebuf)) != NULL) {
				switch(dp->what) {
				case DEVMAP_TYPE:
					devname = dp->dev;
					break;
				case DEVMAP_UNIT:
					unit = i - 1 + offs;
					break;
			case DEVMAP_PART:
					partition = i;
					break;
				}
			}
			cp = ncp;
			ncp = boot_get_path_component(cp, namebuf, &i);
		}
	}

	if (devname == NULL) {
		return -1;
	}

	snprintf(bootdev, sizeof(bootdev), "%s%d%c", devname, unit, 'a');
	return 0;
}

const char *
boot_get_path_component(const char *p, char *comp, int *no)
{
	while (*p && *p != '(') {
		*comp++ = *p++;
	}
	*comp = '\0';

	if (*p == NULL)
		return NULL;

	*no = 0;
	p++;
	while (*p && *p != ')') {
		if (*p >= '0' && *p <= '9')
			*no = *no * 10 + *p++ - '0';
		else
			return NULL;
	}
	return ++p;
}

const char *
boot_getnr(const char *p, int *no)
{
	*no = 0;
	while (*p >= '0' && *p <= '9')
		*no = *no * 10 + *p++ - '0';
	return p;
}

struct nam2blk nam2blk[] = {
	{ "sd",		0 },
	{ "wd",		4 },
	{ NULL,		-1 }
};
