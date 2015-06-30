/*	$OpenBSD: autoconf.c,v 1.6 2015/06/30 06:10:21 yasuoka Exp $	*/
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
void parse_uboot_root(void);

int	cold = 1;
struct device *bootdv = NULL;
char    bootdev[16];
enum devclass bootdev_class = DV_DULL;
extern char uboot_rootdev[];

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
parse_uboot_root(void)
{
	char *p;
	size_t len;

        /*
         * Turn the U-Boot root device (rootdev=/dev/octcf0) into a boot device.
         */
        p = strrchr(uboot_rootdev, '/');
        if (p == NULL)
                return;
	p++;

	len = strlen(p);
	if (len <= 2 || len >= sizeof bootdev - 1)
		return;

	memcpy(bootdev, p, len);
	bootdev[len] = '\0';
	bootdev_class = DV_DISK;
}

void
diskconf(void)
{
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

	switch (bootdev_class) {
	case DV_DISK:
		if ((strcmp(drvrname, "wd") == 0 ||
		    strcmp(drvrname, "sd") == 0 ||
		    strcmp(drvrname, "octcf") == 0) &&
		    strcmp(name, bootdev) == 0)
			bootdv = dev;
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
