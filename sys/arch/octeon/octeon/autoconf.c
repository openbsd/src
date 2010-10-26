/*	$OpenBSD: autoconf.c,v 1.2 2010/10/26 00:02:01 syuu Exp $	*/
/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/autoconf.h>

extern void dumpconf(void);
void	parsepmonbp(void);

int	cold = 1;
struct device *bootdv = NULL;
char    bootdev[16];
enum devclass bootdev_class = DV_DULL;

extern char pmon_bootp[];

void
cpu_configure(void)
{
	(void)splhigh();

	softintr_init();
	(void)config_rootfound("mainbus", NULL);

	splinit();
	cold = 0;
}

void
parsepmonbp(void)
{
	char *p, *q;
	size_t len;

	if (strncmp(pmon_bootp, "tftp://", 7) == 0) {
		bootdev_class = DV_IFNET;
		strlcpy(bootdev, "netboot", sizeof bootdev);
		return;
	}
	strlcpy(bootdev, "unknown", sizeof bootdev);

	if (strncmp(pmon_bootp, "/dev/disk/", 10) == 0) {
		/* kernel loaded by our boot blocks */
		p = pmon_bootp + 10;
		len = strlen(p);
	} else {
		/* kernel loaded by PMON */
		p = strchr(pmon_bootp, '@');
		if (p == NULL)
			return;
		p++;

		q = strchr(p, '/');
		if (q == NULL)
			return;
		len = q - p;
	}

	if (len <= 2 || len >= sizeof bootdev - 1)
		return;
	memcpy(bootdev, p, len);
	bootdev[len] = '\0';
	bootdev_class = DV_DISK;
}

void
diskconf(void)
{
	if (*pmon_bootp != '\0')
		printf("pmon bootpath: %s\n", pmon_bootp);

	if (bootdv != NULL)
		printf("boot device: %s\n", bootdv->dv_xname);

	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();
}

void
device_register(struct device *dev, void *aux)
{
	if (bootdv != NULL)
		return;

	const char *drvrname = dev->dv_cfdata->cf_driver->cd_name;
	const char *name = dev->dv_xname;

	if (dev->dv_class != bootdev_class)
		return;	

	/* 
	 * The device numbering must match. There's no way
	 * pmon tells us more info. Depending on the usb slot
	 * and hubs used you may be lucky. Also, assume umass/sd for usb
	 * attached devices.
	 */
	switch (bootdev_class) {
	case DV_DISK:
		if (strcmp(drvrname, "wd") == 0 && strcmp(name, bootdev) == 0)
			bootdv = dev;
		if (strcmp(drvrname, "octcf") == 0 && strcmp(name, bootdev) == 0)
			bootdv = dev;
		else {
			/* XXX this really only works safely for usb0... */
		    	if ((strcmp(drvrname, "sd") == 0 ||
			    strcmp(drvrname, "cd") == 0) &&
			    strncmp(bootdev, "usb", 3) == 0 &&
			    strcmp(name + 2, bootdev + 3) == 0)
				bootdv = dev;
		}
		break;
	case DV_IFNET:
		/*
		 * This relies on the onboard Ethernet interface being
		 * attached before any other (usb) interface.
		 */
		bootdv = dev;
		break;
	default:
		break;
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		0 },
	{ "cd",		3 },
	{ "wd",		4 },
	{ "rd",		8 },
	{ "vnd",	2 },
	{ "octcf",	15 },
	{ NULL,		-1 }
};
